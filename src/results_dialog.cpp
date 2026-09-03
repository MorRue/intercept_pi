/******************************************************************************
 * Intercept plugin for OpenCPN -- intercept results readout.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "results_dialog.h"

#include "format.h"
#include "intercept.h"

namespace {

wxStaticText* AddRow(wxWindow* parent, wxSizer* sizer, const wxString& text) {
  auto* label = new wxStaticText(parent, wxID_ANY, text);
  sizer->Add(label, 0, wxALL, 4);
  return label;
}

// format.h's formatters return std::string (kept free of any wxWidgets
// dependency, per CLAUDE.md); wxString::Format's vararg machinery does not
// accept std::string directly (wxArgNormalizer<std::string> is declared but
// not defined, so it fails to compile), so every value routes through here.
wxString Wx(const std::string& s) { return wxString(s.c_str(), wxConvUTF8); }

}  // namespace

InterceptResultsDialog::InterceptResultsDialog(wxWindow* parent,
                                                const Case& case_data,
                                                bool own_fix_valid,
                                                double own_lat, double own_lon,
                                                double own_sog_kt)
    : wxDialog(parent, wxID_ANY, _("Intercept"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
  auto* main_sizer = new wxBoxSizer(wxVERTICAL);

  auto* box =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Datum and course to steer"));

  AddRow(this, box,
         wxString::Format(_("Datum: %s, %s"),
                           Wx(FormatLatDDM(case_data.aged_lat)),
                           Wx(FormatLonDDM(case_data.aged_lon))));

  // Which drift, if any, was applied to age the reported position.
  if (case_data.has_manual_drift) {
    AddRow(this, box,
           wxString::Format(_("Target drift: %s deg, %.1f kt (entered)"),
                             Wx(FormatBearingDeg(case_data.manual_set_deg)),
                             case_data.manual_drift_kt));
  } else if (!case_data.grib_file_path.IsEmpty()) {
    AddRow(this, box, _("Target drift: from GRIB file"));
  } else {
    AddRow(this, box,
           _("Target drift: none (datum = reported position)"));
  }

  // FinalizeDatum() leaves elapsed at zero when there was no environmental
  // data to age the position with -- only show it when drift actually
  // happened.
  if (!case_data.elapsed.IsNull()) {
    double elapsed_hours = case_data.elapsed.GetSeconds().ToDouble() / 3600.0;
    AddRow(this, box,
           wxString::Format(_("Elapsed since report: %s"),
                             Wx(FormatEtaHhMm(elapsed_hours))));
  }

  // Bearing and distance need only the two positions; ETA additionally
  // needs a speed. CourseToSteer() already returns eta absent when
  // own_sog_kt <= 0, so a position with no speed still gives a useful
  // course to steer.
  if (own_fix_valid) {
    InterceptResult result = CourseToSteer(
        own_lat, own_lon, case_data.aged_lat, case_data.aged_lon, own_sog_kt);
    AddRow(this, box,
           wxString::Format(_("Bearing: %s deg"),
                             Wx(FormatBearingDeg(result.bearing_deg))));
    AddRow(this, box,
           wxString::Format(_("Distance: %s NM"),
                             Wx(FormatDistanceNm(result.distance_nm))));
    if (result.eta.has_value()) {
      AddRow(this, box,
             wxString::Format(_("ETA: %s"), Wx(FormatEtaHhMm(*result.eta))));
    } else {
      AddRow(this, box, _("ETA: -- (own-ship speed not set)"));
    }
  } else {
    auto* unavailable = AddRow(
        this, box,
        _("Course to steer unavailable: no own-ship position (no GPS fix, "
          "and none entered in the case dialog)."));
    unavailable->SetForegroundColour(*wxRED);
  }

  main_sizer->Add(box, 0, wxEXPAND | wxALL, 10);

  auto* buttons = CreateStdDialogButtonSizer(wxOK);
  main_sizer->Add(buttons, 0, wxEXPAND | wxALL, 10);

  SetSizerAndFit(main_sizer);
}
