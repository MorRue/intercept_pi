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

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "intercept_pi.h"
#include "plug_utils.h"

namespace {

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

void intercept_pi::OnToolbarToolCallback(int id) {
  // Placeholder: proves the toolbar wiring and the own-ship feed both work.
  // Replaced by the case-entry dialog in the next step.
  wxString msg;
  if (m_have_fix) {
    msg = wxString::Format(
        _("Own ship position\n\n"
          "Latitude:   %s\n"
          "Longitude:  %s\n"
          "COG: %.1f deg    SOG: %.1f kn"),
        toSDMM_PlugIn(1, m_own_lat, true), toSDMM_PlugIn(2, m_own_lon, true),
        m_own_cog, m_own_sog);
  } else {
    msg = _("No position fix received yet.\n\n"
            "Connect a GPS source under Options > Connections.");
  }
  wxMessageBox(msg, _("Intercept"), wxOK | wxICON_INFORMATION, m_parent_window);
}
