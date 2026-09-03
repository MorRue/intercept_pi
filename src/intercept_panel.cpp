/******************************************************************************
 * Intercept plugin for OpenCPN -- persistent input/output panel.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/filedlg.h>
#include <wx/statline.h>

#include "intercept_panel.h"

#include "format.h"
#include "intercept.h"

namespace {

wxStaticText* AddRow(wxWindow* parent, wxFlexGridSizer* grid,
                     const wxString& label) {
  grid->Add(new wxStaticText(parent, wxID_ANY, label), 0,
            wxALIGN_CENTER_VERTICAL);
  auto* value = new wxStaticText(parent, wxID_ANY, wxT("--"));
  grid->Add(value, 0, wxALIGN_CENTER_VERTICAL);
  return value;
}

// format.h returns std::string kept clear of wxWidgets; wxString::Format's
// varargs do not accept std::string, so route through here.
wxString Wx(const std::string& s) { return wxString(s.c_str(), wxConvUTF8); }

// Parses the free-form "lat, lon" text; on failure shows a message box on
// `parent` and returns false.
bool ParsePos(const wxString& text, wxWindow* parent, const wxString& what,
              double* lat, double* lon) {
  wxString lat_text, lon_text;
  PositionParseResult la, lo;
  if (!SplitPosition(text, &lat_text, &lon_text) ||
      !(la = ParseCoordinate(lat_text, true)).ok ||
      !(lo = ParseCoordinate(lon_text, false)).ok) {
    wxString detail = !la.ok && !la.error.IsEmpty()   ? la.error
                      : !lo.ok && !lo.error.IsEmpty() ? lo.error
                      : _("expected e.g. '45 30.5 N, 015 20.3 E'");
    wxMessageBox(wxString::Format(_("%s is not valid: %s"), what, detail),
                 _("Invalid position"), wxOK | wxICON_ERROR, parent);
    return false;
  }
  *lat = la.degrees;
  *lon = lo.degrees;
  return true;
}

}  // namespace

InterceptPanel::InterceptPanel(wxWindow* parent, intercept_pi* plugin)
    : wxFrame(parent, wxID_ANY, _("Intercept"), wxDefaultPosition,
              wxDefaultSize,
              wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER |
                  wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW),
      m_plugin(plugin),
      m_content(new wxPanel(this)) {
  wxPanel* panel = m_content;
  auto* outer = new wxBoxSizer(wxVERTICAL);

  // -- inputs --
  auto* in = new wxFlexGridSizer(0, 2, 6, 8);
  in->AddGrowableCol(1);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Reported position:")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_position_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_position_ctrl->SetHint(_("e.g. 45 30.5 N, 015 20.3 E"));
  in->Add(m_position_ctrl, 1, wxEXPAND);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Time of report:")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_time_ctrl = new wxTimePickerCtrl(panel, wxID_ANY, wxDateTime::Now());
  in->Add(m_time_ctrl, 0);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Craft type:")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_craft_choice = new wxChoice(panel, wxID_ANY);
  for (const auto& label : CraftTypeLabels()) m_craft_choice->Append(label);
  m_craft_choice->SetSelection(0);
  in->Add(m_craft_choice, 1, wxEXPAND);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Persons on board:")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_pob_ctrl = new wxSpinCtrl(panel, wxID_ANY, "1", wxDefaultPosition,
                              wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 1);
  in->Add(m_pob_ctrl, 0);

  in->Add(new wxStaticText(panel, wxID_ANY, _("GRIB file (optional):")), 0,
          wxALIGN_CENTER_VERTICAL);
  auto* grib_row = new wxBoxSizer(wxHORIZONTAL);
  m_grib_path_ctrl = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY);
  grib_row->Add(m_grib_path_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
  auto* browse = new wxButton(panel, wxID_ANY, _("Browse..."));
  grib_row->Add(browse, 0, wxLEFT, 6);
  in->Add(grib_row, 1, wxEXPAND);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Target drift set (deg true):")),
          0, wxALIGN_CENTER_VERTICAL);
  m_set_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 0.0, 359.9, 0.0, 1.0);
  in->Add(m_set_ctrl, 0);

  in->Add(new wxStaticText(panel, wxID_ANY,
                           _("Target drift rate (kt, 0 = use GRIB/none):")),
          0, wxALIGN_CENTER_VERTICAL);
  m_drift_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.1);
  in->Add(m_drift_ctrl, 0);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Own ship (optional):")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_own_pos_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_own_pos_ctrl->SetHint(_("position, blank = use GPS fix"));
  in->Add(m_own_pos_ctrl, 1, wxEXPAND);

  in->Add(new wxStaticText(panel, wxID_ANY, _("Own ship speed (kt):")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_own_sog_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.0, 99.0, 0.0, 0.1);
  in->Add(m_own_sog_ctrl, 0);

  outer->Add(in, 0, wxEXPAND | wxALL, 10);

  auto* recalc = new wxButton(panel, wxID_ANY, _("Recalculate"));
  outer->Add(recalc, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 10);

  outer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

  // -- outputs --
  auto* out = new wxFlexGridSizer(0, 2, 4, 12);
  m_out_datum = AddRow(panel, out, _("Datum:"));
  m_out_drift = AddRow(panel, out, _("Target drift:"));
  m_out_elapsed = AddRow(panel, out, _("Elapsed since report:"));
  m_out_bearing = AddRow(panel, out, _("Bearing to steer:"));
  m_out_distance = AddRow(panel, out, _("Distance:"));
  m_out_eta = AddRow(panel, out, _("ETA:"));
  outer->Add(out, 0, wxEXPAND | wxALL, 10);

  panel->SetSizerAndFit(outer);
  auto* frame_sizer = new wxBoxSizer(wxVERTICAL);
  frame_sizer->Add(panel, 1, wxEXPAND);
  SetSizerAndFit(frame_sizer);

  recalc->Bind(wxEVT_BUTTON, &InterceptPanel::OnRecalculate, this);
  browse->Bind(wxEVT_BUTTON, &InterceptPanel::OnBrowseGrib, this);
  Bind(wxEVT_CLOSE_WINDOW, &InterceptPanel::OnClose, this);
}

void InterceptPanel::OnBrowseGrib(wxCommandEvent& WXUNUSED(event)) {
  wxFileDialog fd(this, _("Select GRIB file"), wxEmptyString, wxEmptyString,
                  _("GRIB files (*.grb;*.grb2;*.bin)|*.grb;*.grb2;*.bin|"
                    "All files (*.*)|*.*"),
                  wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fd.ShowModal() == wxID_OK) m_grib_path_ctrl->SetValue(fd.GetPath());
}

void InterceptPanel::OnClose(wxCloseEvent& event) {
  // The [x] hides the panel and clears the toolbar toggle; the plugin owns
  // the object and destroys it in DeInit().
  Hide();
  if (m_plugin) m_plugin->OnPanelClosed();
  event.Veto();
}

void InterceptPanel::OnRecalculate(wxCommandEvent& WXUNUSED(event)) {
  Case c;
  if (!ParsePos(m_position_ctrl->GetValue(), this, _("Reported position"),
                &c.lat, &c.lon)) {
    return;
  }

  std::optional<OwnShipState> own;
  wxString own_text = m_own_pos_ctrl->GetValue();
  own_text.Trim(true).Trim(false);
  if (!own_text.IsEmpty()) {
    double olat, olon;
    if (!ParsePos(own_text, this, _("Own-ship position"), &olat, &olon)) return;
    own = OwnShipState{olat, olon, m_own_sog_ctrl->GetValue()};
  } else {
    own = m_plugin ? m_plugin->LiveFix() : std::nullopt;
  }

  c.time_of_report = m_time_ctrl->GetValue();
  c.craft_type = m_craft_choice->GetStringSelection();
  c.pob = m_pob_ctrl->GetValue();
  c.grib_file_path = m_grib_path_ctrl->GetValue();
  double drift_kt = m_drift_ctrl->GetValue();
  c.has_manual_drift = drift_kt > 0.0;
  c.manual_drift_kt = drift_kt;
  c.manual_set_deg = m_set_ctrl->GetValue();
  c.FinalizeDatum();

  ShowOutputs(c, own);
  if (m_plugin) m_plugin->ApplyCase(c, own);
  m_content->Layout();
  Fit();  // outputs may have grown/shrunk the panel's best size
}

void InterceptPanel::ShowOutputs(const Case& c,
                                 const std::optional<OwnShipState>& own) {
  m_out_datum->SetLabel(wxString::Format("%s, %s",
                                         Wx(FormatLatDDM(c.aged_lat)),
                                         Wx(FormatLonDDM(c.aged_lon))));

  if (c.has_manual_drift) {
    m_out_drift->SetLabel(wxString::Format(_("%s deg, %.1f kt (entered)"),
                                           Wx(FormatBearingDeg(c.manual_set_deg)),
                                           c.manual_drift_kt));
  } else if (!c.grib_file_path.IsEmpty()) {
    m_out_drift->SetLabel(_("from GRIB file"));
  } else {
    m_out_drift->SetLabel(_("none (datum = reported position)"));
  }

  m_out_elapsed->SetLabel(
      c.elapsed.IsNull()
          ? wxString(wxT("--"))
          : Wx(FormatEtaHhMm(c.elapsed.GetSeconds().ToDouble() / 3600.0)));

  if (own) {
    InterceptResult r = CourseToSteer(own->lat, own->lon, c.aged_lat,
                                      c.aged_lon, own->sog_kt);
    m_out_bearing->SetLabel(
        wxString::Format(_("%s deg"), Wx(FormatBearingDeg(r.bearing_deg))));
    m_out_distance->SetLabel(
        wxString::Format(_("%s NM"), Wx(FormatDistanceNm(r.distance_nm))));
    m_out_eta->SetLabel(r.eta.has_value()
                            ? Wx(FormatEtaHhMm(*r.eta))
                            : wxString(_("-- (own-ship speed not set)")));
  } else {
    const wxString none = _("-- (no own-ship position)");
    m_out_bearing->SetLabel(none);
    m_out_distance->SetLabel(none);
    m_out_eta->SetLabel(none);
  }
}
