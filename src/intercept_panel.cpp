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
  auto plain = [&](wxWindow* p, const wxString& text) {
    return new wxStaticText(p, wxID_ANY, text);
  };
  // Right cell: the field, then a "lock" checkbox on its right. Every lock
  // checkbox behaves identically -- checked disables the field.
  auto with_lock = [&](wxSizer* field, wxCheckBox** out_cb) {
    auto* s = new wxBoxSizer(wxHORIZONTAL);
    s->Add(field, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    auto* cb = new wxCheckBox(panel, wxID_ANY, _("lock"));
    cb->SetToolTip(_("Checked: the field is disabled so it can't be changed"));
    s->Add(cb, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    *out_cb = cb;
    return s;
  };

  auto* in_top = new wxFlexGridSizer(0, 2, 6, 8);
  in_top->AddGrowableCol(1);

  in_top->Add(plain(panel, _("Reported position:")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_position_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_position_ctrl->SetHint(_("e.g. 45 30.5 N, 015 20.3 E"));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_position_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    in_top->Add(with_lock(f, &m_lock_position), 1, wxEXPAND);
  }

  in_top->Add(plain(panel, _("Time of report (local):")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_date_ctrl = new wxDatePickerCtrl(panel, wxID_ANY, wxDateTime::Now());
  m_time_ctrl = new wxTimePickerCtrl(panel, wxID_ANY, wxDateTime::Now());
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_date_ctrl, 0, wxALIGN_CENTER_VERTICAL);
    f->Add(m_time_ctrl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    in_top->Add(with_lock(f, &m_lock_time), 1, wxEXPAND);
  }
  outer->Add(in_top, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  // Collapsible "GRIB file" group. A GRIB is the alternative to hand-entered
  // drift, so craft type (drives leeway) and POB sit here too. While a file
  // is set, the manual "Target drift" fields are disabled and this group is
  // forced open so its Clear button stays reachable -- see UpdateGribLock().
  m_grib_pane = new wxCollapsiblePane(
      panel, wxID_ANY, _("GRIB file (wind & current) + craft details"));
  {
    wxWindow* gp = m_grib_pane->GetPane();
    auto* pg = new wxFlexGridSizer(0, 2, 6, 8);
    pg->AddGrowableCol(1);

    pg->Add(plain(gp, _("GRIB file:")), 0, wxALIGN_CENTER_VERTICAL);
    auto* grib_row = new wxBoxSizer(wxHORIZONTAL);
    m_grib_path_ctrl = new wxTextCtrl(gp, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxTE_READONLY);
    grib_row->Add(m_grib_path_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    auto* browse = new wxButton(gp, wxID_ANY, _("Browse..."));
    grib_row->Add(browse, 0, wxLEFT, 6);
    auto* clear_grib = new wxButton(gp, wxID_ANY, _("Clear"));
    clear_grib->SetToolTip(_("Remove the GRIB file; re-enable manual drift"));
    grib_row->Add(clear_grib, 0, wxLEFT, 6);
    pg->Add(grib_row, 1, wxEXPAND);

    pg->Add(plain(gp, _("Craft type:")), 0, wxALIGN_CENTER_VERTICAL);
    m_craft_choice = new wxChoice(gp, wxID_ANY);
    for (const auto& label : CraftTypeLabels()) m_craft_choice->Append(label);
    m_craft_choice->SetSelection(0);  // "Unknown / not specified"
    pg->Add(m_craft_choice, 1, wxEXPAND);

    pg->Add(plain(gp, _("Persons on board (optional):")), 0,
            wxALIGN_CENTER_VERTICAL);
    m_pob_ctrl = new wxSpinCtrl(gp, wxID_ANY, "0", wxDefaultPosition,
                                wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 0);
    pg->Add(m_pob_ctrl, 0);

    gp->SetSizer(pg);
    pg->SetSizeHints(gp);
    browse->Bind(wxEVT_BUTTON, &InterceptPanel::OnBrowseGrib, this);
    clear_grib->Bind(wxEVT_BUTTON, &InterceptPanel::OnClearGrib, this);
  }
  m_grib_pane->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED,
                    &InterceptPanel::OnGribPaneChanged, this);
  outer->Add(m_grib_pane, 0, wxEXPAND | wxALL, 10);

  auto* in_bot = new wxFlexGridSizer(0, 2, 6, 8);
  in_bot->AddGrowableCol(1);

  in_bot->Add(plain(panel, _("Target drift set (deg true):")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_set_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 0.0, 359.9, 0.0, 1.0);
  in_bot->Add(m_set_ctrl, 0);

  in_bot->Add(plain(panel, _("Target drift rate (kt, 0 = none):")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_drift_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.1);
  in_bot->Add(m_drift_ctrl, 0);

  in_bot->Add(plain(panel, _("Own ship position:")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_own_pos_ctrl = new wxTextCtrl(panel, wxID_ANY);
  m_own_pos_ctrl->SetHint(_("e.g. 45 10 N, 015 05 E"));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_own_pos_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    in_bot->Add(with_lock(f, &m_lock_own_pos), 1, wxEXPAND);
  }

  in_bot->Add(plain(panel, _("Own ship speed (kt):")), 0,
              wxALIGN_CENTER_VERTICAL);
  m_own_sog_ctrl = new wxSpinCtrlDouble(panel, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.0, 99.0, 0.0, 0.1);
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_own_sog_ctrl, 0, wxALIGN_CENTER_VERTICAL);
    in_bot->Add(with_lock(f, &m_lock_own_sog), 1, wxEXPAND);
  }

  // Own-ship position and speed start locked: use the live GPS fix by default,
  // unlock the one(s) you want to enter by hand.
  m_lock_own_pos->SetValue(true);
  m_lock_own_sog->SetValue(true);
  m_own_pos_ctrl->Enable(false);
  m_own_sog_ctrl->Enable(false);

  outer->Add(in_bot, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  auto* disp_row = new wxBoxSizer(wxHORIZONTAL);
  m_show_target = new wxCheckBox(panel, wxID_ANY, _("Show reported position"));
  m_show_target->SetValue(true);
  m_show_estimated =
      new wxCheckBox(panel, wxID_ANY, _("Show estimated position"));
  m_show_estimated->SetValue(true);
  m_show_routes = new wxCheckBox(panel, wxID_ANY, _("Show routes"));
  m_show_routes->SetValue(true);
  disp_row->Add(m_show_target, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  disp_row->Add(m_show_estimated, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  disp_row->Add(m_show_routes, 0, wxALIGN_CENTER_VERTICAL);
  outer->Add(disp_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

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
  m_show_target->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnDisplayToggled, this);
  m_show_estimated->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnDisplayToggled, this);
  m_show_routes->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnDisplayToggled, this);
  Bind(wxEVT_CLOSE_WINDOW, &InterceptPanel::OnClose, this);
  m_lock_position->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
  m_lock_time->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
  m_lock_own_pos->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
  m_lock_own_sog->Bind(wxEVT_CHECKBOX, &InterceptPanel::OnLockToggled, this);
}

void InterceptPanel::OnLockToggled(wxCommandEvent& WXUNUSED(event)) {
  m_position_ctrl->Enable(!m_lock_position->IsChecked());
  m_date_ctrl->Enable(!m_lock_time->IsChecked());
  m_time_ctrl->Enable(!m_lock_time->IsChecked());

  const bool own_pos_manual = !m_lock_own_pos->IsChecked();
  const bool own_sog_manual = !m_lock_own_sog->IsChecked();
  m_own_pos_ctrl->Enable(own_pos_manual);
  m_own_sog_ctrl->Enable(own_sog_manual);

  // Seed a just-unlocked, still-empty own-ship field from the live fix so the
  // operator adjusts a value rather than typing one from scratch.
  if (auto fix = m_plugin ? m_plugin->LiveFix() : std::nullopt) {
    if (own_pos_manual && m_own_pos_ctrl->IsEmpty()) {
      m_own_pos_ctrl->SetValue(wxString::Format(
          "%.5f %c, %.5f %c", std::fabs(fix->lat), fix->lat >= 0 ? 'N' : 'S',
          std::fabs(fix->lon), fix->lon >= 0 ? 'E' : 'W'));
    }
    if (own_sog_manual && m_own_sog_ctrl->GetValue() == 0.0) {
      m_own_sog_ctrl->SetValue(fix->sog_kt);
    }
  }
}

void InterceptPanel::OnBrowseGrib(wxCommandEvent& WXUNUSED(event)) {
  // GRIB extensions vary by source: .grb (Saildocs), .grb2 (wgrib2),
  // .grib/.grib2 (NCEP/Copernicus), .gr2, and .bin. The "All files" filter
  // covers anything else, including no extension at all.
  wxFileDialog fd(this, _("Select GRIB file"), wxEmptyString, wxEmptyString,
                  _("GRIB files|*.grb;*.grb2;*.grib;*.grib2;*.gr2;*.bin|"
                    "All files (*.*)|*.*"),
                  wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fd.ShowModal() == wxID_OK) {
    m_grib_path_ctrl->SetValue(fd.GetPath());
    UpdateGribLock();
  }
}

void InterceptPanel::OnClearGrib(wxCommandEvent& WXUNUSED(event)) {
  m_grib_path_ctrl->Clear();
  UpdateGribLock();
}

void InterceptPanel::OnGribPaneChanged(wxCollapsiblePaneEvent& WXUNUSED(event)) {
  // A GRIB file in use must stay visible so its Clear button is reachable --
  // snap the group back open if the user tries to collapse it.
  if (!m_grib_path_ctrl->GetValue().IsEmpty() && m_grib_pane->IsCollapsed()) {
    m_grib_pane->Expand();
  }
  RelayoutForPane();
}

void InterceptPanel::UpdateGribLock() {
  const bool have_grib = !m_grib_path_ctrl->GetValue().IsEmpty();
  // With a GRIB, wind + current from the file drive the datum -- hand-entered
  // set & drift would be ignored, so disable them to make that clear.
  m_set_ctrl->Enable(!have_grib);
  m_drift_ctrl->Enable(!have_grib);
  if (have_grib && m_grib_pane->IsCollapsed()) m_grib_pane->Expand();
  RelayoutForPane();
}

void InterceptPanel::RelayoutForPane() {
  m_content->Layout();
  m_content->Fit();
  Fit();
}

void InterceptPanel::OnDisplayToggled(wxCommandEvent& WXUNUSED(event)) {
  // Redraw from the last computed case; nothing to do before the first
  // recalculation.
  if (m_last_case && m_plugin) {
    m_plugin->ApplyCase(*m_last_case, m_last_own, m_show_target->IsChecked(),
                        m_show_estimated->IsChecked(),
                        m_show_routes->IsChecked());
  }
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

  // Own-ship position and speed are independent: each comes from its field
  // when unlocked, otherwise from the live GPS fix. A hand-entered position
  // at GPS speed (or vice versa) is fine.
  std::optional<OwnShipState> own;
  {
    std::optional<OwnShipState> fix =
        m_plugin ? m_plugin->LiveFix() : std::nullopt;
    bool have_pos = false;
    double olat = 0.0, olon = 0.0, osog = 0.0;

    if (!m_lock_own_pos->IsChecked()) {
      if (!ParsePos(m_own_pos_ctrl->GetValue(), this, _("Own-ship position"),
                    &olat, &olon)) {
        return;
      }
      have_pos = true;
    } else if (fix) {
      olat = fix->lat;
      olon = fix->lon;
      have_pos = true;
    }

    if (!m_lock_own_sog->IsChecked()) {
      osog = m_own_sog_ctrl->GetValue();
    } else if (fix) {
      osog = fix->sog_kt;
    }

    if (have_pos) own = OwnShipState{olat, olon, osog};
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

  m_last_case = c;
  m_last_own = own;
  ShowOutputs(c, own);
  if (m_plugin)
    m_plugin->ApplyCase(c, own, m_show_target->IsChecked(),
                        m_show_estimated->IsChecked(),
                        m_show_routes->IsChecked());
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
