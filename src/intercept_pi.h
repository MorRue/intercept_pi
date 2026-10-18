/******************************************************************************
 * Intercept plugin for OpenCPN
 *
 * Computes a course to steer onto a reported position, including the case
 * where that position is itself drifting with wind and current.
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

#include "datum_age.h"
#include "ocpn_plugin.h"
#include "position_parse.h"

class InterceptPanel;
struct OwnShipState;

/** Let OpenCPN choose where the toolbar button lands. */
#define INTERCEPT_TOOL_POSITION -1

/** One tracking case: the reported position and its circumstances. */
struct Case {
  double lat = 0.0;
  double lon = 0.0;
  wxDateTime time_of_report;
  CraftType craft_type = CraftType::kUnknown;
  int pob = 0;  // People on board -- recorded only, not used in any calc.
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
  // Also set by FinalizeDatum(): how far from (aged_lat, aged_lon) the
  // target may be. uncertainty.radius_nm is the circle shown on the chart.
  DatumUncertainty uncertainty;

  // Set by SolveIntercept(): the moving-target intercept -- where own-ship
  // meets the target if the target keeps drifting during the transit. When
  // intercept_solved is false (no own-ship speed, or own-ship can never
  // reach the target) there is no meeting point.
  bool intercept_solved = false;
  double intercept_lat = 0.0;
  double intercept_lon = 0.0;
  double intercept_bearing_deg = 0.0;
  double intercept_distance_nm = 0.0;  // Own-ship track length to the meeting.
  double intercept_eta_hours = 0.0;

  /**
   * Ages (lat, lon) forward to now using GribReader on grib_file_path when
   * one is set, otherwise zero drift, and stores the result in aged_lat/
   * aged_lon/elapsed. Call once the rest of the case is populated.
   */
  void FinalizeDatum();

  /**
   * Solves the moving-target intercept from own-ship: own-ship steers a
   * straight course at own.sog_kt while the target keeps drifting (same
   * environmental model as FinalizeDatum, extrapolated forward). Fills the
   * intercept_* fields. Call after FinalizeDatum(). With own.sog_kt <= 0 or
   * no reachable solution, leaves intercept_solved false.
   */
  void SolveIntercept(const OwnShipState& own);
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

/** One craft-type dropdown entry: the leeway category and its label. */
struct CraftChoice {
  CraftType type;
  wxString label;
};

/** The craft-type dropdown entries, in menu order; the first is the default. */
std::vector<CraftChoice> CraftTypeChoices();

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
  /** Store the case and (re)draw the chart objects. show_target toggles the
   *  "Reported position" mark; show_estimated toggles the
   *  "Estimated position" mark at the drifted-forward datum; show_lines
   *  toggles the drift track, the course-to-steer route and the "Intercept"
   *  mark at the route's far end. Returns true if a mark or the route was
   *  left as it was because OpenCPN is navigating with it. */
  bool ApplyCase(const Case& c, const std::optional<OwnShipState>& own,
                 bool show_target, bool show_estimated, bool show_lines);
  /** The panel was closed by its own [x] -- clear the toolbar toggle. */
  void OnPanelClosed();

private:
  /**
   * (Re)places the chart objects for the current case, each on a fixed GUID
   * (delete-before-add): "Reported position" mark, "Estimated
   * position" mark at the aged datum, "Target drift" track between them, and
   * (if own-ship is known) the "Course to steer" route with an "Intercept"
   * mark at its far end. Each show_* flag suppresses its object(s), deleting
   * any stale copy.
   *
   * An object OpenCPN is navigating with (the active route, or a mark that
   * is the active waypoint) is left untouched: deleting it would cancel the
   * navigation, and api-18 has no call to re-activate a route. The route and
   * the "Intercept" mark are kept or replaced together. The exception is the
   * "Estimated position" mark: it carries the uncertainty ring, so when it is
   * the active waypoint it is moved and resized in place
   * (UpdateSingleWaypointEx) instead of kept. Returns true if anything was
   * kept.
   */
  bool UpdateChartObjects(const Case& c,
                          const std::optional<OwnShipState>& own,
                          bool show_target, bool show_estimated,
                          bool show_lines);

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
