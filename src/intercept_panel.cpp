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
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/statline.h>

#include <cmath>

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

  // Left cell: an optional lock/enable checkbox, then the label.
  auto label_with_check = [&](wxCheckBox* cb, const wxString& text) {
    auto* s = new wxBoxSizer(wxHORIZONTAL);
    s->Add(cb, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    s->Add(new wxStaticText(panel, wxID_ANY, text), 0, wxALIGN_CENTER_VERTICAL);
    return s;
  };
  auto plain = [&](const wxString& text) {
    return new wxStaticText(panel, wxID_ANY, text);
  };

  m_lock_position = new wxCheckBox(panel, wxID_ANY, wxEmptyString);
  m_lock_position->SetToolTip(_("Lock so it can't be changed by accident"));
  in->Add(label_with_check(m_lock_position, _("Reported position:")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_position_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_position_ctrl->SetHint(_("e.g. 45 30.5 N, 015 20.3 E"));
  in->Add(m_position_ctrl, 1, wxEXPAND);

  m_lock_time = new wxCheckBox(panel, wxID_ANY, wxEmptyString);
  m_lock_time->SetToolTip(_("Lock so it can't be changed by accident"));
  in->Add(label_with_check(m_lock_time, _("Time of report (local):")), 0,
          wxALIGN_CENTER_VERTICAL);
  auto* when_row = new wxBoxSizer(wxHORIZONTAL);
  m_date_ctrl = new wxDatePickerCtrl(panel, wxID_ANY, wxDateTime::Now());
  m_time_ctrl = new wxTimePickerCtrl(panel, wxID_ANY, wxDateTime::Now());
  when_row->Add(m_date_ctrl, 0);
  when_row->Add(m_time_ctrl, 0, wxLEFT, 6);
  in->Add(when_row, 0);

  in->Add(plain(_("Craft type:")), 0, wxALIGN_CENTER_VERTICAL);
  m_craft_choice = new wxChoice(panel, wxID_ANY);
  for (const auto& label : CraftTypeLabels()) m_craft_choice->Append(label);
  m_craft_choice->SetSelection(0);  // "Unknown / not specified"
  in->Add(m_craft_choice, 1, wxEXPAND);

  in->Add(plain(_("Persons on board (optional):")), 0, wxALIGN_CENTER_VERTICAL);
  m_pob_ctrl = new wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition,
                              wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 0);
  in->Add(m_pob_ctrl, 0);

  in->Add(plain(_("GRIB file (optional):")), 0, wxALIGN_CENTER_VERTICAL);
  auto* grib_row = new wxBoxSizer(wxHORIZONTAL);
  m_grib_path_ctrl = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY);
  grib_row->Add(m_grib_path_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
  auto* browse = new wxButton(panel, wxID_ANY, _("Browse..."));
  grib_row->Add(browse, 0, wxLEFT, 6);
  in->Add(grib_row, 1, wxEXPAND);

  in->Add(plain(_("Target drift set (deg true):")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_set_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 0.0, 359.9, 0.0, 1.0);
  in->Add(m_set_ctrl, 0);

  in->Add(plain(_("Target drift rate (kt, 0 = use GRIB/none):")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_drift_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.1);
  in->Add(m_drift_ctrl, 0);

  m_use_manual_own = new wxCheckBox(panel, wxID_ANY, wxEmptyString);
  m_use_manual_own->SetToolTip(
      _("Off: use OpenCPN's GPS fix. On: enter own-ship position and speed."));
  in->Add(label_with_check(m_use_manual_own, _("Own ship (manual):")), 0,
          wxALIGN_CENTER_VERTICAL);
  m_own_pos_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_own_pos_ctrl->SetHint(_("e.g. 45 10 N, 015 05 E"));
  m_own_pos_ctrl->Enable(false);
  in->Add(m_own_pos_ctrl, 1, wxEXPAND);

  in->Add(plain(_("Own ship speed (kt):")), 0, wxALIGN_CENTER_VERTICAL);
  m_own_sog_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.0, 99.0, 0.0, 0.1);
  m_own_sog_ctrl->Enable(false);
  in->Add(m_own_sog_ctrl, 0);

  outer->Add(in, 0, wxEXPAND | wxALL, 10);

  auto* recalc = new wxButton(panel, wxID_ANY, _("Recalculate"));
  outer->Add(recalc, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 10);

  outer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

  // -- outputs --
  auto* out = new wxFlexGridSizer(0, 2, 4, 12);
  m_out_datum = AddRow(panel, out, _("Datum (intercept point):"));
  m_out_drift = AddRow(panel, out, _("Drift applied:"));
  m_out_moved = AddRow(panel, out, _("Target moved from report:"));
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
  m_lock_position->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
  m_lock_time->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
  m_use_manual_own->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnManualOwnToggled,
                         this);
}

void InterceptPanel::OnLockToggled(wxCommandEvent& WXUNUSED(event)) {
  m_position_ctrl->Enable(!m_lock_position->IsChecked());
  m_date_ctrl->Enable(!m_lock_time->IsChecked());
  m_time_ctrl->Enable(!m_lock_time->IsChecked());
}

void InterceptPanel::OnManualOwnToggled(wxCommandEvent& WXUNUSED(event)) {
  const bool manual = m_use_manual_own->IsChecked();
  m_own_pos_ctrl->Enable(manual);
  m_own_sog_ctrl->Enable(manual);
  if (manual && m_own_pos_ctrl->IsEmpty()) {
    // Seed from the live fix so the operator tweaks rather than retypes.
    if (auto fix = m_plugin ? m_plugin->LiveFix() : std::nullopt) {
      m_own_pos_ctrl->SetValue(wxString::Format(
          "%.5f %c, %.5f %c", std::fabs(fix->lat), fix->lat >= 0 ? 'N' : 'S',
          std::fabs(fix->lon), fix->lon >= 0 ? 'E' : 'W'));
      m_own_sog_ctrl->SetValue(fix->sog_kt);
    }
  }
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
  if (m_use_manual_own->IsChecked()) {
    double olat, olon;
    if (!ParsePos(m_own_pos_ctrl->GetValue(), this, _("Own-ship position"),
                  &olat, &olon)) {
      return;
    }
    own = OwnShipState{olat, olon, m_own_sog_ctrl->GetValue()};
  } else {
    own = m_plugin ? m_plugin->LiveFix() : std::nullopt;
  }

  // Combine the date picker's date with the time picker's hour/minute --
  // a time-only value defaults its date inconsistently across platforms
  // (sometimes 1970), which then makes "elapsed since report" nonsense.
  wxDateTime when = m_date_ctrl->GetValue();
  wxDateTime tod = m_time_ctrl->GetValue();
  when.SetHour(tod.GetHour());
  when.SetMinute(tod.GetMinute());
  when.SetSecond(0);
  c.time_of_report = when;
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

  InterceptResult moved =
      CourseToSteer(c.lat, c.lon, c.aged_lat, c.aged_lon, 0.0);
  m_out_moved->SetLabel(
      moved.distance_nm < 0.05
          ? wxString(_("-- (datum = reported position)"))
          : wxString::Format(_("%s NM toward %s deg"),
                             Wx(FormatDistanceNm(moved.distance_nm)),
                             Wx(FormatBearingDeg(moved.bearing_deg))));

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
