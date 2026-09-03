/******************************************************************************
 * Intercept plugin for OpenCPN
 *
 * Computes a course to steer onto a reported position, for use when closing
 * a vessel in distress.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_H__
#define INTERCEPT_PI_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/datetime.h>

#include <optional>
#include <vector>

#include "ocpn_plugin.h"

class InterceptPanel;

/** Let OpenCPN choose where the toolbar button lands. */
#define INTERCEPT_TOOL_POSITION -1

/** A search-and-rescue case: the reported position and circumstances. */
struct Case {
  double lat = 0.0;
  double lon = 0.0;
  wxDateTime time_of_report;
  wxString craft_type;
  int pob = 0;  // Persons on board.
  wxString grib_file_path;  // Empty means no GRIB file was selected.

  // Hand-entered drift of the target: the set (true direction it is moving
  // TOWARD) and the rate (knots). Used by FinalizeDatum() only when no GRIB
  // file is given -- with a GRIB, wind + current from the file win.
  bool has_manual_drift = false;
  double manual_set_deg = 0.0;
  double manual_drift_kt = 0.0;

  // Set by FinalizeDatum(): the reported position aged forward to now.
  // Equal to (lat, lon) with a zero elapsed until FinalizeDatum() runs, and
  // still equal to it afterwards if there was no environmental data to
  // drift with -- see datum_age.h.
  double aged_lat = 0.0;
  double aged_lon = 0.0;
  wxTimeSpan elapsed;

  /**
   * Ages (lat, lon) forward to now using GribReader on grib_file_path when
   * one is set, otherwise zero drift, and stores the result in aged_lat/
   * aged_lon/elapsed. Call once the rest of the case is populated.
   */
  void FinalizeDatum();
};

/**
 * Own-ship position and speed to compute the course to steer from. Normally
 * this comes from OpenCPN's live fix (intercept_pi::SetPositionFix); the case
 * dialog can also override it with hand-entered values, for planning from a
 * hypothetical position or when there is no GPS. sog_kt <= 0 means the speed
 * is unknown -- bearing and distance are still computable, ETA is not.
 */
struct OwnShipState {
  double lat = 0.0;
  double lon = 0.0;
  double sog_kt = 0.0;
};

/** Outcome of parsing one coordinate (latitude or longitude) from text. */
struct PositionParseResult {
  bool ok = false;
  double degrees = 0.0;  // Valid only when ok is true. Signed: +N/+E, -S/-W.
  wxString error;         // Valid only when ok is false.
};

/**
 * Parses a single latitude or longitude given as DD ("45.5"),
 * DDM ("45 30.5 N") or DMS ("45 30 30 N"). The caller states which axis
 * is expected via is_latitude, since that fixes both the valid hemisphere
 * letters (N/S vs E/W) and the degree range (0-90 vs 0-180).
 */
PositionParseResult ParseCoordinate(const wxString& text, bool is_latitude);

/**
 * Splits free-form position text into a latitude and a longitude part. A
 * comma is the preferred separator ("45 30.5 N, 015 20.3 E"); without one,
 * the split falls right after the first N/S hemisphere letter. Returns false
 * if it cannot find two non-empty parts.
 */
bool SplitPosition(const wxString& text, wxString* lat_text,
                   wxString* lon_text);

/** The craft-type labels offered in the input panel, in menu order. */
std::vector<wxString> CraftTypeLabels();

class intercept_pi : public opencpn_plugin_118 {
public:
  explicit intercept_pi(void* ppimgr);
  ~intercept_pi() override;

  // --- Lifecycle -----------------------------------------------------------
  int Init() override;
  bool DeInit() override;

  // --- Identification, shown in OpenCPN's plugin manager --------------------
  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

  // --- Events we subscribe to in Init() ------------------------------------
  void SetPositionFix(PlugIn_Position_Fix& pfix) override;
  void OnToolbarToolCallback(int id) override;

  // --- Called by the InterceptPanel ---------------------------------------
  /** OpenCPN's live own-ship fix, or nullopt until the first one arrives. */
  std::optional<OwnShipState> LiveFix() const;
  /** Store the case and (re)draw the target/intercept marks, drift track and
   *  course route on the chart. */
  void ApplyCase(const Case& c, const std::optional<OwnShipState>& own);
  /** The panel was closed by its own [x] -- clear the toolbar toggle. */
  void OnPanelClosed();

private:
  /**
   * (Re)places the four chart objects for the current case, each on a fixed
   * GUID (delete-before-add): "Target" mark at the reported position,
   * "Intercept" mark at the aged datum, "Target drift" track between them,
   * and (if own-ship is known) the "Course to steer" route.
   */
  void UpdateChartObjects(const Case& c,
                          const std::optional<OwnShipState>& own);

  /**
   * (Re)builds the two-point course-to-steer route (own-ship → datum) under
   * kCourseRouteGuid, replacing any previous one. `own` is the effective
   * own-ship position (live fix or the case dialog's manual override); with
   * none, any stale route is removed and nothing is drawn.
   */
  void UpdateCourseRoute(const Case& c,
                         const std::optional<OwnShipState>& own);

  wxWindow* m_parent_window;
  wxBitmap m_panel_bitmap;
  int m_leftclick_tool_id;

  /** Most recent own-ship fix; m_have_fix is false until the first one. */
  bool m_have_fix;
  double m_own_lat;
  double m_own_lon;
  double m_own_cog;
  double m_own_sog;

  /** The current case, if one has been entered. */
  std::optional<Case> m_case;

  /** The persistent, non-modal input/output panel. Created on first toolbar
   *  click, hidden (not destroyed) on close, destroyed in DeInit(). */
  InterceptPanel* m_panel = nullptr;
};

#endif  // INTERCEPT_PI_H__
