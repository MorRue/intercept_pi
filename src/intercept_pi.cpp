/******************************************************************************
 * Intercept plugin for OpenCPN -- plugin entry points.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "datum_age.h"
#include "grib_reader.h"
#include "intercept_panel.h"
#include "intercept_pi.h"
#include "plug_utils.h"
#include "route_helper.h"

// ocpn_plugin.h WX_DECLARE_LIST's Plugin_WaypointList but does not define its
// node methods; OpenCPN's own shared library provides them on Linux, but the
// MSVC import lib (opencpn-libs/api-18/msvc-wx32/opencpn.lib) does not, so
// `new Plugin_WaypointList` + Append fails to link (LNK2001
// wxPlugin_WaypointListNode::DeleteData). Provide them locally on MSVC only.
#if defined(_MSC_VER)
#include <wx/listimpl.cpp>
WX_DEFINE_LIST(Plugin_WaypointList)
#endif

namespace {

// Fixed so every recompute replaces the same mark / route (delete-by-GUID,
// then add) instead of piling a new one onto the chart each time a case is
// confirmed. Arbitrary but stable -- not looked up against anything else.
const wxString kEstimatedMarkGuid = wxT("a41a6b0e-70d1-4b7e-9c2c-49f0b0a1a001");
const wxString kCourseRouteGuid = wxT("a41a6b0e-70d1-4b7e-9c2c-49f0b0a1a002");
const wxString kTargetMarkGuid = wxT("a41a6b0e-70d1-4b7e-9c2c-49f0b0a1a003");
const wxString kDriftTrackGuid = wxT("a41a6b0e-70d1-4b7e-9c2c-49f0b0a1a004");

// Surface current + wind-driven leeway realistically stays well under this
// even in a gale; a hand-typed drift_kt above it is a data-entry mistake
// (e.g. a stray digit), not a real target drift.
constexpr double kMaxPlausibleManualDriftKt = 20.0;

/**
 * Sanitises the operator's hand-entered set & drift before it reaches the
 * datum-ageing integrator: a non-finite value (a stray NaN/Inf from a bad
 * text-field parse) or a negative speed disables manual drift outright
 * rather than feeding a bogus vector into ComputeAgedDatum, and an
 * implausibly large speed is clamped down rather than dragging the datum
 * far off-track.
 */
ManualSetAndDrift SanitizeManualDrift(bool has_manual_drift, double set_deg,
                                      double drift_kt) {
  ManualSetAndDrift manual;
  if (!has_manual_drift) return manual;
  if (!std::isfinite(drift_kt) || !std::isfinite(set_deg) || drift_kt < 0.0) {
    return manual;  // available stays false: treat as not supplied.
  }
  manual.available = true;
  manual.set_deg = set_deg;
  manual.drift_kt = std::min(drift_kt, kMaxPlausibleManualDriftKt);
  return manual;
}

/** Parses token as a number, requiring the whole token to be consumed. */
bool ParseNumber(const std::string& token, double* out) {
  if (token.empty()) return false;
  try {
    size_t consumed = 0;
    double value = std::stod(token, &consumed);
    if (consumed != token.size()) return false;
    *out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

/**
 * Splits a trailing hemisphere letter off the last token, either as its
 * own token ("N") or attached to a number ("30.5N"). Returns '\0' (and
 * leaves tokens untouched) if the last token does not end in a letter.
 */
char ExtractHemisphere(std::vector<std::string>* tokens) {
  if (tokens->empty()) return '\0';
  std::string& last = tokens->back();
  if (last.empty() || !std::isalpha(static_cast<unsigned char>(last.back())))
    return '\0';
  char letter = std::toupper(static_cast<unsigned char>(last.back()));
  if (last.size() == 1)
    tokens->pop_back();
  else
    last.pop_back();
  return letter;
}

}  // namespace

PositionParseResult ParseCoordinate(const wxString& text, bool is_latitude) {
  PositionParseResult result;

  std::istringstream iss(text.ToStdString());
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) tokens.push_back(token);

  if (tokens.empty()) {
    result.error = _("Position is empty.");
    return result;
  }

  char hemisphere = ExtractHemisphere(&tokens);
  bool has_hemisphere = hemisphere != '\0';
  if (has_hemisphere && hemisphere != 'N' && hemisphere != 'S' &&
      hemisphere != 'E' && hemisphere != 'W') {
    result.error =
        wxString::Format(_("'%c' is not a hemisphere letter (use N, S, E "
                            "or W)."),
                          hemisphere);
    return result;
  }
  if (has_hemisphere) {
    bool letter_is_lat = (hemisphere == 'N' || hemisphere == 'S');
    if (letter_is_lat != is_latitude) {
      result.error = is_latitude
          ? wxString::Format(
                _("'%c' is not valid for latitude; use N or S."), hemisphere)
          : wxString::Format(
                _("'%c' is not valid for longitude; use E or W."),
                hemisphere);
      return result;
    }
  }

  if (tokens.empty() || tokens.size() > 3) {
    result.error = wxString::Format(
        _("Expected 1 to 3 numbers (degrees[, minutes[, seconds]]), "
          "found %zu."),
        tokens.size());
    return result;
  }

  std::vector<double> values;
  for (const auto& t : tokens) {
    double v;
    if (!ParseNumber(t, &v)) {
      result.error =
          wxString::Format(_("'%s' is not a number."), t.c_str());
      return result;
    }
    values.push_back(v);
  }

  double max_degrees = is_latitude ? 90.0 : 180.0;
  double degrees = 0.0;

  if (values.size() == 1) {
    // DD: a single signed value, hemisphere letter is optional.
    degrees = values[0];
    if (has_hemisphere) {
      if (degrees < 0) {
        result.error = _(
            "A negative value cannot be combined with a hemisphere letter.");
        return result;
      }
      if (hemisphere == 'S' || hemisphere == 'W') degrees = -degrees;
    }
  } else {
    // DDM or DMS: hemisphere letter is required, degrees/minutes/seconds
    // are all non-negative magnitudes.
    if (!has_hemisphere) {
      result.error = is_latitude
          ? _("Latitude in degrees-minutes(-seconds) format needs a "
              "hemisphere letter (N or S).")
          : _("Longitude in degrees-minutes(-seconds) format needs a "
              "hemisphere letter (E or W).");
      return result;
    }
    double deg_part = values[0];
    if (deg_part < 0) {
      result.error =
          _("Degrees must not be negative; use a hemisphere letter "
            "instead.");
      return result;
    }
    double min_part = values[1];
    if (min_part < 0 || min_part >= 60.0) {
      result.error = wxString::Format(
          _("Minutes must be within [0, 60), got %g."), min_part);
      return result;
    }
    double sec_part = 0.0;
    if (values.size() == 3) {
      sec_part = values[2];
      if (sec_part < 0 || sec_part >= 60.0) {
        result.error = wxString::Format(
            _("Seconds must be within [0, 60), got %g."), sec_part);
        return result;
      }
    }
    if (deg_part > max_degrees) {
      result.error = wxString::Format(
          _("Degrees must be within [0, %g], got %g."), max_degrees,
          deg_part);
      return result;
    }
    degrees = deg_part + min_part / 60.0 + sec_part / 3600.0;
    if (hemisphere == 'S' || hemisphere == 'W') degrees = -degrees;
  }

  if (std::fabs(degrees) > max_degrees) {
    result.error = is_latitude
        ? wxString::Format(_("Latitude must be within [-90, 90], got %g."),
                            degrees)
        : wxString::Format(
              _("Longitude must be within [-180, 180], got %g."), degrees);
    return result;
  }

  result.ok = true;
  result.degrees = degrees;
  return result;
}

bool SplitPosition(const wxString& text, wxString* lat_text,
                   wxString* lon_text) {
  wxString trimmed = text;
  trimmed.Trim(true).Trim(false);

  int comma = trimmed.Find(',');
  if (comma != wxNOT_FOUND) {
    *lat_text = trimmed.Mid(0, comma);
    *lon_text = trimmed.Mid(comma + 1);
  } else {
    size_t split_at = wxString::npos;
    for (size_t i = 0; i < trimmed.Length(); ++i) {
      wxChar c = wxToupper(trimmed[i]);
      if (c == 'N' || c == 'S') {
        split_at = i + 1;
        break;
      }
    }
    if (split_at == wxString::npos) return false;
    *lat_text = trimmed.Mid(0, split_at);
    *lon_text = trimmed.Mid(split_at);
  }
  lat_text->Trim(true).Trim(false);
  lon_text->Trim(true).Trim(false);
  return !lat_text->IsEmpty() && !lon_text->IsEmpty();
}

std::vector<wxString> CraftTypeLabels() {
  // "Unknown" is the default; LookupLeewayCoefficients() matches only
  // "wooden" and falls everything else through to the rubber-hull
  // coefficient, so an unknown craft gets that (the more cautious one).
  return {_("Unknown / not specified"),
          _("Rubber boat (inflatable, RIB, liferaft)"),
          _("Wooden boat (displacement hull)")};
}

void Case::FinalizeDatum() {
  std::unique_ptr<GribReader> grib;
  if (!grib_file_path.IsEmpty()) {
    grib = std::make_unique<GribReader>(wxFileName(grib_file_path));
  }

  // Hand-entered set & drift, used by ComputeAgedDatum() only when there is
  // no GRIB file. With neither, drift is zero and the datum is the reported
  // position.
  ManualSetAndDrift manual =
      SanitizeManualDrift(has_manual_drift, manual_set_deg, manual_drift_kt);

  AgedDatum aged =
      ComputeAgedDatum(lat, lon, time_of_report, wxDateTime::Now(),
                        craft_type, grib.get(), manual);
  aged_lat = aged.lat;
  aged_lon = aged.lon;
  elapsed = aged.elapsed;
}

/*
 * OpenCPN dlopen()s the shared library and looks up these two C symbols.
 * They are the only exported entry points; everything else is reached
 * through the returned object's vtable.
 */
extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new intercept_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

intercept_pi::intercept_pi(void* ppimgr)
    : opencpn_plugin_118(ppimgr),
      m_parent_window(nullptr),
      m_leftclick_tool_id(-1),
      m_have_fix(false),
      m_own_lat(0.0),
      m_own_lon(0.0),
      m_own_cog(0.0),
      m_own_sog(0.0) {
  // The panel icon is shown in Options > Plugins, before Init() runs.
  auto icon_path = GetPluginIcon("intercept_panel_icon", PKG_NAME);
  if (icon_path.type == IconPath::Type::Svg)
    m_panel_bitmap = LoadSvgIcon(icon_path.path.c_str());
  else if (icon_path.type == IconPath::Type::Png)
    m_panel_bitmap = LoadPngIcon(icon_path.path.c_str());
  else
    wxLogWarning("intercept_pi: no icon found for 'intercept_panel_icon'");
}

intercept_pi::~intercept_pi() = default;

int intercept_pi::Init() {
  m_parent_window = GetOCPNCanvasWindow();

  auto icon = GetPluginIcon("intercept_pi", PKG_NAME);
  auto toggled = GetPluginIcon("intercept_pi_toggled", PKG_NAME);

  if (icon.type == IconPath::Type::Svg) {
    m_leftclick_tool_id = InsertPlugInToolSVG(
        "Intercept", icon.path, icon.path, toggled.path, wxITEM_NORMAL,
        _("Intercept"), "", nullptr, INTERCEPT_TOOL_POSITION, 0, this);
  } else if (icon.type == IconPath::Type::Png) {
    auto bitmap = LoadPngIcon(icon.path.c_str());
    m_leftclick_tool_id =
        InsertPlugInTool("", &bitmap, &bitmap, wxITEM_NORMAL, _("Intercept"),
                         "", nullptr, INTERCEPT_TOOL_POSITION, 0, this);
  } else {
    wxLogWarning("intercept_pi: no toolbar icon found for 'intercept_pi'");
  }

  // WANTS_NMEA_EVENTS is what causes SetPositionFix() to be called.
  return (WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL | WANTS_NMEA_EVENTS |
          WANTS_CONFIG);
}

bool intercept_pi::DeInit() {
  if (m_leftclick_tool_id != -1) RemovePlugInTool(m_leftclick_tool_id);
  if (m_panel) {
    m_panel->Destroy();
    m_panel = nullptr;
  }
  wxString g;
  g = kEstimatedMarkGuid; DeleteSingleWaypoint(g);
  g = kTargetMarkGuid;    DeleteSingleWaypoint(g);
  g = kDriftTrackGuid;    DeletePlugInTrack(g);
  g = kCourseRouteGuid;   DeletePlugInRoute(g);
  return true;
}

int intercept_pi::GetAPIVersionMajor() { return atoi(API_VERSION); }

int intercept_pi::GetAPIVersionMinor() {
  std::string v(API_VERSION);
  size_t dotpos = v.find('.');
  return atoi(v.substr(dotpos + 1).c_str());
}

int intercept_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }

int intercept_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap* intercept_pi::GetPlugInBitmap() { return &m_panel_bitmap; }

wxString intercept_pi::GetCommonName() { return PLUGIN_API_NAME; }

wxString intercept_pi::GetShortDescription() { return PKG_SUMMARY; }

wxString intercept_pi::GetLongDescription() { return PKG_DESCRIPTION; }

void intercept_pi::SetPositionFix(PlugIn_Position_Fix& pfix) {
  m_own_lat = pfix.Lat;
  m_own_lon = pfix.Lon;
  m_own_cog = pfix.Cog;
  m_own_sog = pfix.Sog;
  m_have_fix = true;
}

void intercept_pi::OnToolbarToolCallback(int WXUNUSED(id)) {
  if (!m_panel) m_panel = new InterceptPanel(m_parent_window, this);

  const bool show = !m_panel->IsShown();
  m_panel->Show(show);
  if (show) m_panel->Raise();
  if (m_leftclick_tool_id != -1)
    SetToolbarItemState(m_leftclick_tool_id, show);
}

std::optional<OwnShipState> intercept_pi::LiveFix() const {
  if (!m_have_fix) return std::nullopt;
  return OwnShipState{m_own_lat, m_own_lon, m_own_sog};
}

void intercept_pi::ApplyCase(const Case& c,
                             const std::optional<OwnShipState>& own,
                             bool show_target, bool show_estimated,
                             bool show_lines) {
  m_case = c;
  UpdateChartObjects(c, own, show_target, show_estimated, show_lines);
  RequestRefresh(m_parent_window);
}

void intercept_pi::UpdateChartObjects(const Case& c,
                                      const std::optional<OwnShipState>& own,
                                      bool show_target, bool show_estimated,
                                      bool show_lines) {
  // Four objects, all delete-before-add on fixed GUIDs so a recalculation
  // replaces rather than piles up, each independently suppressible:
  //   * "Target" mark  -- the reported position, left where it was reported
  //   * "Estimated position" mark -- the aged datum, where the target is now
  //   * "Target drift"  -- a track from the reported position to the datum
  //   * "Course to steer" -- an activatable route from own-ship to the datum
  // Route and track render in different colours (OpenCPN's route vs track
  // styles), so the two lines are visually distinct.
  wxString g;

  g = kTargetMarkGuid;
  DeleteSingleWaypoint(g);
  if (show_target) {
    PlugIn_Waypoint target(c.lat, c.lon, wxT("activepoint"), _("Target"),
                           kTargetMarkGuid);
    AddSingleWaypoint(&target, /*b_permanent=*/true);
  }

  g = kEstimatedMarkGuid;
  DeleteSingleWaypoint(g);
  if (show_estimated) {
    PlugIn_Waypoint estimated(c.aged_lat, c.aged_lon, wxT("circle"),
                              _("Estimated position"), kEstimatedMarkGuid);
    AddSingleWaypoint(&estimated, /*b_permanent=*/true);
  }

  g = kDriftTrackGuid;
  DeletePlugInTrack(g);
  const bool drifted =
      std::fabs(c.aged_lat - c.lat) > 1e-7 || std::fabs(c.aged_lon - c.lon) > 1e-7;
  if (show_lines && drifted) {
    auto* track = new PlugIn_Track();
    track->m_NameString = _("Target drift");
    track->m_GUID = kDriftTrackGuid;
    track->pWaypointList = new Plugin_WaypointList();
    track->pWaypointList->Append(
        new PlugIn_Waypoint(c.lat, c.lon, wxT("circle"), _("Reported")));
    track->pWaypointList->Append(new PlugIn_Waypoint(
        c.aged_lat, c.aged_lon, wxT("circle"), _("Estimated position")));
    AddPlugInTrack(track, /*b_permanent=*/true);
  }

  if (show_lines) {
    UpdateCourseRoute(c, own);
  } else {
    wxString route_guid = kCourseRouteGuid;
    DeletePlugInRoute(route_guid);
  }
}

void intercept_pi::OnPanelClosed() {
  if (m_leftclick_tool_id != -1)
    SetToolbarItemState(m_leftclick_tool_id, false);
}

void intercept_pi::UpdateCourseRoute(const Case& c,
                                     const std::optional<OwnShipState>& own) {
  wxString guid = kCourseRouteGuid;
  DeletePlugInRoute(guid);

  // No own-ship position means there is no course line to draw -- the marks
  // still show target and estimated position, and the panel still gives
  // bearing/distance if a position was entered in the panel.
  if (!own) return;

  std::vector<GeoPoint> points =
      BuildInterceptWaypoints(own->lat, own->lon, c.aged_lat, c.aged_lon);

  auto* route = new PlugIn_Route();
  route->m_NameString = _("Course to steer");
  route->m_StartString = _("Own ship");
  route->m_EndString = _("Estimated position");
  route->m_GUID = kCourseRouteGuid;
  route->pWaypointList = new Plugin_WaypointList();
  route->pWaypointList->Append(new PlugIn_Waypoint(
      points[0].lat, points[0].lon, wxT("circle"), _("Own ship")));
  route->pWaypointList->Append(new PlugIn_Waypoint(
      points[1].lat, points[1].lon, wxT("circle"), _("Estimated position")));

  // Ordinary activatable route, so the navigator can select and steer it.
  AddPlugInRoute(route, /*b_permanent=*/true);
}
