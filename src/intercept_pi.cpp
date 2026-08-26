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

#include "config.h"
#include "intercept_pi.h"
#include "plug_utils.h"

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
