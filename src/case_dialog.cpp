/******************************************************************************
 * Intercept plugin for OpenCPN -- case-intake dialog.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/filedlg.h>

#include "case_dialog.h"

namespace {

const wxString kCraftRubberBoat =
    _("Rubber boat (inflatable, RIB, liferaft)");
const wxString kCraftWoodenBoat = _("Wooden boat (displacement hull)");

/**
 * Splits free-form position text into a latitude and a longitude part.
 * A comma is the preferred separator ("45 30.5 N, 015 20.3 E"); without
 * one, the split falls right after the first N/S hemisphere letter, since
 * that always ends the latitude portion in DDM/DMS notation.
 */
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

}  // namespace

CaseDialog::CaseDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("New case"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
      m_position_ctrl(nullptr),
      m_time_ctrl(nullptr),
      m_craft_choice(nullptr),
      m_pob_ctrl(nullptr),
      m_grib_path_ctrl(nullptr) {
  auto* main_sizer = new wxBoxSizer(wxVERTICAL);

  auto* grid = new wxFlexGridSizer(5, 2, 8, 8);
  grid->AddGrowableCol(1);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Position:")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_position_ctrl = new wxTextCtrl(this, wxID_ANY);
  m_position_ctrl->SetHint(_("e.g. 45 30.5 N, 015 20.3 E"));
  grid->Add(m_position_ctrl, 1, wxEXPAND);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Time of report:")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_time_ctrl = new wxTimePickerCtrl(this, wxID_ANY, wxDateTime::Now());
  grid->Add(m_time_ctrl, 0);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Craft type:")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_craft_choice = new wxChoice(this, wxID_ANY);
  m_craft_choice->Append(kCraftRubberBoat);
  m_craft_choice->Append(kCraftWoodenBoat);
  m_craft_choice->SetSelection(0);
  grid->Add(m_craft_choice, 1, wxEXPAND);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Persons on board:")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_pob_ctrl = new wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition,
                               wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 1);
  grid->Add(m_pob_ctrl, 0);

  grid->Add(new wxStaticText(this, wxID_ANY, _("GRIB file (optional):")), 0,
            wxALIGN_CENTER_VERTICAL);
  auto* grib_row = new wxBoxSizer(wxHORIZONTAL);
  m_grib_path_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_READONLY);
  grib_row->Add(m_grib_path_ctrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
  auto* grib_browse_button = new wxButton(this, wxID_ANY, _("Browse..."));
  grib_row->Add(grib_browse_button, 0, wxLEFT, 8);
  grid->Add(grib_row, 1, wxEXPAND);

  main_sizer->Add(grid, 1, wxEXPAND | wxALL, 10);

  auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
  main_sizer->Add(buttons, 0, wxEXPAND | wxALL, 10);

  SetSizerAndFit(main_sizer);

  // Bound with Bind() rather than an event table entry, so this handler
  // runs instead of (not before) wxDialog's default OnOK -- returning
  // without calling EndModal() is what keeps the dialog open on bad input.
  Bind(wxEVT_BUTTON, &CaseDialog::OnOK, this, wxID_OK);
  grib_browse_button->Bind(wxEVT_BUTTON, &CaseDialog::OnBrowseGrib, this);
}

void CaseDialog::OnBrowseGrib(wxCommandEvent& WXUNUSED(event)) {
  wxFileDialog file_dialog(
      this, _("Select GRIB file"), wxEmptyString, wxEmptyString,
      _("GRIB files (*.grb;*.grb2;*.bin)|*.grb;*.grb2;*.bin|All files "
        "(*.*)|*.*"),
      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (file_dialog.ShowModal() == wxID_OK) {
    m_grib_path_ctrl->SetValue(file_dialog.GetPath());
  }
}

void CaseDialog::OnOK(wxCommandEvent& WXUNUSED(event)) {
  wxString lat_text, lon_text;
  if (!SplitPosition(m_position_ctrl->GetValue(), &lat_text, &lon_text)) {
    wxMessageBox(
        _("Enter latitude and longitude separated by a comma, e.g. "
          "'45 30.5 N, 015 20.3 E'."),
        _("Invalid position"), wxOK | wxICON_ERROR, this);
    return;
  }

  PositionParseResult lat = ParseCoordinate(lat_text, /*is_latitude=*/true);
  if (!lat.ok) {
    wxMessageBox(lat.error, _("Invalid position"), wxOK | wxICON_ERROR, this);
    return;
  }
  PositionParseResult lon = ParseCoordinate(lon_text, /*is_latitude=*/false);
  if (!lon.ok) {
    wxMessageBox(lon.error, _("Invalid position"), wxOK | wxICON_ERROR, this);
    return;
  }

  m_case.lat = lat.degrees;
  m_case.lon = lon.degrees;
  m_case.time_of_report = m_time_ctrl->GetValue();
  m_case.craft_type = m_craft_choice->GetStringSelection();
  m_case.pob = m_pob_ctrl->GetValue();
  m_case.grib_file_path = m_grib_path_ctrl->GetValue();
  m_case.FinalizeDatum();

  EndModal(wxID_OK);
}
