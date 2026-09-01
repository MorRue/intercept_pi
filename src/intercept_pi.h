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

#include "ocpn_plugin.h"

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

private:
  wxWindow* m_parent_window;
  wxBitmap m_panel_bitmap;
  int m_leftclick_tool_id;

  /** Most recent own-ship fix; m_have_fix is false until the first one. */
  bool m_have_fix;
  double m_own_lat;
  double m_own_lon;
  double m_own_cog;
  double m_own_sog;

  /** The current case, if one has been entered via the case dialog. */
  std::optional<Case> m_case;
};

#endif  // INTERCEPT_PI_H__
