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

  // FinalizeDatum() leaves elapsed at zero when there was no environmental
  // data to age the position with -- only show it when drift actually
  // happened.
  if (!case_data.elapsed.IsNull()) {
    double elapsed_hours = case_data.elapsed.GetSeconds().ToDouble() / 3600.0;
    AddRow(this, box,
           wxString::Format(_("Elapsed since report: %s"),
                             Wx(FormatEtaHhMm(elapsed_hours))));
  }

  const bool have_course = own_fix_valid && own_sog_kt > 0.0;
  if (have_course) {
    InterceptResult result = CourseToSteer(
        own_lat, own_lon, case_data.aged_lat, case_data.aged_lon, own_sog_kt);
    AddRow(this, box,
           wxString::Format(_("Bearing: %s deg"),
                             Wx(FormatBearingDeg(result.bearing_deg))));
    AddRow(this, box,
           wxString::Format(_("Distance: %s NM"),
                             Wx(FormatDistanceNm(result.distance_nm))));
    AddRow(this, box,
           wxString::Format(_("ETA: %s"),
                             Wx(result.eta.has_value()
                                    ? FormatEtaHhMm(*result.eta)
                                    : std::string("--:--"))));
  } else {
    auto* unavailable = AddRow(
        this, box,
        _("Course to steer unavailable: no current own-ship position fix "
          "and speed."));
    unavailable->SetForegroundColour(*wxRED);
  }

  main_sizer->Add(box, 0, wxEXPAND | wxALL, 10);

  auto* buttons = CreateStdDialogButtonSizer(wxOK);
  main_sizer->Add(buttons, 0, wxEXPAND | wxALL, 10);

  SetSizerAndFit(main_sizer);
}
