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

#include <cmath>
#include <vector>

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

  // -- inputs, in titled group boxes --
  auto plain = [&](wxWindow* p, const wxString& text) {
    return new wxStaticText(p, wxID_ANY, text);
  };

  // Opens a titled box holding a 2-column form grid. After the call, `g` is
  // the grid to add label/field pairs to and `gp` is the window to parent
  // the controls on.
  wxFlexGridSizer* g = nullptr;
  wxWindow* gp = nullptr;
  std::vector<wxStaticBox*> group_boxes;
  std::vector<wxStaticText*> sub_headers;  // Bold labels inside the drift box.
  auto begin_group = [&](const wxString& title) {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, title);
    gp = box->GetStaticBox();
    group_boxes.push_back(box->GetStaticBox());
    g = new wxFlexGridSizer(0, 2, 6, 8);
    g->AddGrowableCol(1);
    box->Add(g, 1, wxEXPAND | wxALL, 6);
    outer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  };

  // Field + a "lock" checkbox on its right; checked disables the field.
  auto with_lock = [&](wxWindow* p, wxSizer* field, wxCheckBox** out_cb,
                       const wxString& lock_tip) {
    auto* s = new wxBoxSizer(wxHORIZONTAL);
    s->Add(field, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    auto* cb = new wxCheckBox(p, wxID_ANY, _("lock"));
    cb->SetToolTip(lock_tip);
    s->Add(cb, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    *out_cb = cb;
    return s;
  };

  // --- Own ship ---
  begin_group(_("Own ship"));
  g->Add(plain(gp, _("Position:")), 0, wxALIGN_CENTER_VERTICAL);
  m_own_pos_ctrl = new wxTextCtrl(gp, wxID_ANY);
  m_own_pos_ctrl->SetHint(_("e.g. 45 10 N, 015 05 E"));
  m_own_pos_ctrl->SetToolTip(
      _("Your position to steer from, same formats as the reported "
        "position. Leave locked to use OpenCPN's GPS fix; unlock to type "
        "one in."));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_own_pos_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    g->Add(with_lock(gp, f, &m_lock_own_pos,
                     _("Locked: use OpenCPN's live GPS position. "
                       "Unlocked: use the position typed here.")),
           1, wxEXPAND);
  }
  g->Add(plain(gp, _("Speed (kt):")), 0, wxALIGN_CENTER_VERTICAL);
  m_own_sog_ctrl = new wxSpinCtrlDouble(gp, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.0, 99.0, 0.0, 0.1);
  m_own_sog_ctrl->SetToolTip(
      _("Your speed over ground, knots. Needed for an ETA. Leave locked to "
        "use the GPS speed."));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_own_sog_ctrl, 0, wxALIGN_CENTER_VERTICAL);
    g->Add(with_lock(gp, f, &m_lock_own_sog,
                     _("Locked: use OpenCPN's live GPS speed. "
                       "Unlocked: use the speed typed here.")),
           1, wxEXPAND);
  }

  // --- Report ---
  begin_group(_("Report"));
  g->Add(plain(gp, _("Reported position:")), 0, wxALIGN_CENTER_VERTICAL);
  m_position_ctrl = new wxTextCtrl(gp, wxID_ANY);
  m_position_ctrl->SetHint(_("e.g. 45 30.5 N, 015 20.3 E"));
  m_position_ctrl->SetToolTip(
      _("The target's last known position: latitude then longitude, "
        "separated by a comma. Accepts decimal degrees (45.51), degrees + "
        "minutes (45 30.5 N) or degrees-minutes-seconds (45 30 30 N)."));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_position_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    g->Add(with_lock(gp, f, &m_lock_position,
                     _("Lock the reported position so a later Recalculate "
                       "can't change it by accident.")),
           1, wxEXPAND);
  }
  g->Add(plain(gp, _("Time of report (local):")), 0, wxALIGN_CENTER_VERTICAL);
  m_date_ctrl = new wxDatePickerCtrl(gp, wxID_ANY, wxDateTime::Now());
  m_date_ctrl->SetToolTip(
      _("Date the position was reported, local time. With the time field it "
        "sets how long the target has been drifting."));
  m_time_ctrl = new wxTimePickerCtrl(gp, wxID_ANY, wxDateTime::Now());
  m_time_ctrl->SetToolTip(
      _("Time of day the position was reported, local 24-hour. Elapsed time "
        "from here to now is how far the datum is aged forward."));
  {
    auto* f = new wxBoxSizer(wxHORIZONTAL);
    f->Add(m_date_ctrl, 0, wxALIGN_CENTER_VERTICAL);
    f->Add(m_time_ctrl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    g->Add(with_lock(gp, f, &m_lock_time,
                     _("Lock the report date and time against accidental "
                       "edits.")),
           1, wxEXPAND);
  }

  // --- Target drift: explicit and GRIB-derived, one box, two sub-headers ---
  {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, _("Target drift"));
    wxWindow* bp = box->GetStaticBox();
    group_boxes.push_back(box->GetStaticBox());

    auto subhead = [&](const wxString& text) {
      auto* st = new wxStaticText(bp, wxID_ANY, text);
      sub_headers.push_back(st);
      box->Add(st, 0, wxLEFT | wxRIGHT | wxTOP, 6);
    };
    auto subgrid = [&]() {
      auto* fg = new wxFlexGridSizer(0, 2, 6, 8);
      fg->AddGrowableCol(1);
      box->Add(fg, 0, wxEXPAND | wxALL, 6);
      return fg;
    };

    subhead(_("Explicit drift (ignored when a GRIB file is set)"));
    auto* eg = subgrid();
    eg->Add(plain(bp, _("Set (deg true):")), 0, wxALIGN_CENTER_VERTICAL);
    m_set_ctrl = new wxSpinCtrlDouble(bp, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_ARROW_KEYS, 0.0, 359.9, 0.0, 1.0);
    m_set_ctrl->SetToolTip(
        _("Direction the target is drifting TOWARD, degrees true (0-359). "
          "Used only when no GRIB file is set."));
    eg->Add(m_set_ctrl, 0);
    eg->Add(plain(bp, _("Rate (kt):")), 0, wxALIGN_CENTER_VERTICAL);
    m_drift_ctrl = new wxSpinCtrlDouble(bp, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.1);
    m_drift_ctrl->SetToolTip(
        _("How fast the target is drifting, knots. 0 means no explicit "
          "drift (datum stays at the reported position). Used only when no "
          "GRIB file is set."));
    eg->Add(m_drift_ctrl, 0);

    subhead(_("Environment drift (from a GRIB file; craft type sets leeway)"));
    auto* ng = subgrid();
    ng->Add(plain(bp, _("GRIB file:")), 0, wxALIGN_CENTER_VERTICAL);
    {
      auto* grib_row = new wxBoxSizer(wxHORIZONTAL);
      m_grib_path_ctrl = new wxTextCtrl(bp, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxTE_READONLY);
      m_grib_path_ctrl->SetToolTip(
          _("Optional GRIB2 file holding 10 m wind and/or surface current. "
            "When set it drives the datum drift and the explicit-drift "
            "fields above are disabled."));
      grib_row->Add(m_grib_path_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
      auto* browse = new wxButton(bp, wxID_ANY, _("Browse..."));
      browse->SetToolTip(
          _("Choose a GRIB2 file (.grb, .grb2, .grib, .grib2, .gr2, .bin)."));
      grib_row->Add(browse, 0, wxLEFT, 6);
      auto* clear_grib = new wxButton(bp, wxID_ANY, _("Clear"));
      clear_grib->SetToolTip(
          _("Remove the GRIB file; re-enable the explicit-drift fields"));
      grib_row->Add(clear_grib, 0, wxLEFT, 6);
      ng->Add(grib_row, 1, wxEXPAND);
      browse->Bind(wxEVT_BUTTON, &InterceptPanel::OnBrowseGrib, this);
      clear_grib->Bind(wxEVT_BUTTON, &InterceptPanel::OnClearGrib, this);
    }
    ng->Add(plain(bp, _("Craft type:")), 0, wxALIGN_CENTER_VERTICAL);
    m_craft_choice = new wxChoice(bp, wxID_ANY);
    for (const auto& label : CraftTypeLabels()) m_craft_choice->Append(label);
    m_craft_choice->SetSelection(0);  // "Unknown / not specified"
    m_craft_choice->SetToolTip(
        _("The kind of object being tracked. Sets the leeway coefficient "
          "(percentage of wind speed) applied when a GRIB supplies wind."));
    ng->Add(m_craft_choice, 1, wxEXPAND);
    ng->Add(plain(bp, _("Persons on board (optional):")), 0,
            wxALIGN_CENTER_VERTICAL);
    m_pob_ctrl = new wxSpinCtrl(bp, wxID_ANY, "0", wxDefaultPosition,
                                wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 0);
    m_pob_ctrl->SetToolTip(
        _("Number of people aboard. Recorded with the case; it does not "
          "affect the calculation."));
    ng->Add(m_pob_ctrl, 0);

    outer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  }

  // Own-ship position and speed start locked: use the live GPS fix by default,
  // unlock the one(s) you want to enter by hand.
  m_lock_own_pos->SetValue(true);
  m_lock_own_sog->SetValue(true);
  m_own_pos_ctrl->Enable(false);
  m_own_sog_ctrl->Enable(false);

  // -- result box; Recalculate shares the ETA row, bottom-right --
  {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, _("Result"));
    wxWindow* bp = box->GetStaticBox();
    group_boxes.push_back(box->GetStaticBox());

    auto* out = new wxFlexGridSizer(0, 2, 4, 12);
    out->AddGrowableCol(1);
    m_out_datum = AddRow(bp, out, _("Datum (intercept point):"));
    m_out_drift = AddRow(bp, out, _("Drift applied:"));
    m_out_moved = AddRow(bp, out, _("Target moved from report:"));
    m_out_elapsed = AddRow(bp, out, _("Elapsed since report:"));
    m_out_bearing = AddRow(bp, out, _("Bearing to steer:"));
    m_out_distance = AddRow(bp, out, _("Distance:"));

    out->Add(new wxStaticText(bp, wxID_ANY, _("ETA:")), 0,
             wxALIGN_CENTER_VERTICAL);
    auto* eta_row = new wxBoxSizer(wxHORIZONTAL);
    m_out_eta = new wxStaticText(bp, wxID_ANY, wxT("--"));
    eta_row->Add(m_out_eta, 1, wxALIGN_CENTER_VERTICAL);
    auto* recalc = new wxButton(bp, wxID_ANY, _("Recalculate"));
    recalc->SetToolTip(
        _("Age the datum to the current time and recompute the course to "
          "steer from the inputs above, then redraw the chart."));
    eta_row->Add(recalc, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    out->Add(eta_row, 1, wxEXPAND);
    recalc->Bind(wxEVT_BUTTON, &InterceptPanel::OnRecalculate, this);

    box->Add(out, 1, wxEXPAND | wxALL, 6);
    outer->Add(box, 0, wxEXPAND | wxALL, 10);
  }

  // -- chart display toggles, at the very bottom, under the result box --
  auto* disp_row = new wxBoxSizer(wxHORIZONTAL);
  m_show_target = new wxCheckBox(panel, wxID_ANY, _("Show reported position"));
  m_show_target->SetValue(true);
  m_show_target->SetToolTip(
      _("Show or hide the 'Target' mark at the reported position on the "
        "chart."));
  m_show_estimated =
      new wxCheckBox(panel, wxID_ANY, _("Show estimated position"));
  m_show_estimated->SetValue(true);
  m_show_estimated->SetToolTip(
      _("Show or hide the 'Estimated position' mark where the target is "
        "estimated to be now."));
  m_show_routes = new wxCheckBox(panel, wxID_ANY, _("Show routes"));
  m_show_routes->SetValue(true);
  m_show_routes->SetToolTip(
      _("Show or hide the target-drift track, the course-to-steer route and "
        "the intercept mark."));
  disp_row->Add(m_show_target, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  disp_row->Add(m_show_estimated, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  disp_row->Add(m_show_routes, 0, wxALIGN_CENTER_VERTICAL);
  outer->Add(disp_row, 0, wxALL, 10);

  // Only the group-box titles shrink; the fields inside keep the body size.
  // Setting a font on a wxStaticBox propagates to its children, so after
  // shrinking each box we restore every child to the normal font (and the
  // drift sub-headers to bold).
  wxFont normal_font = panel->GetFont();
  if (normal_font.IsOk()) {
    wxFont small_font = normal_font.Smaller();
    wxFont bold_font = normal_font;
    bold_font.MakeBold();
    for (wxStaticBox* b : group_boxes) {
      b->SetFont(small_font);
      for (wxWindow* child : b->GetChildren()) child->SetFont(normal_font);
    }
    for (wxStaticText* h : sub_headers) h->SetFont(bold_font);
  }

  panel->SetSizerAndFit(outer);
  auto* frame_sizer = new wxBoxSizer(wxVERTICAL);
  frame_sizer->Add(panel, 1, wxEXPAND);
  SetSizerAndFit(frame_sizer);

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

void InterceptPanel::UpdateGribLock() {
  const bool have_grib = !m_grib_path_ctrl->GetValue().IsEmpty();
  // With a GRIB, wind + current from the file drive the datum -- hand-entered
  // set & drift would be ignored, so disable them to make that clear.
  m_set_ctrl->Enable(!have_grib);
  m_drift_ctrl->Enable(!have_grib);
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
