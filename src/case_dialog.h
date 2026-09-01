/******************************************************************************
 * Intercept plugin for OpenCPN -- case-intake dialog.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_CASE_DIALOG_H__
#define INTERCEPT_PI_CASE_DIALOG_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/spinctrl.h>
#include <wx/timectrl.h>

#include "intercept_pi.h"

/**
 * Collects the data for a new search-and-rescue case: reported position,
 * time of report, craft type, persons on board, and an optional GRIB file
 * to source wind/current from. On OK, the position is validated with
 * ParseCoordinate(); bad input shows an error message box and keeps the
 * dialog open, so GetCase() is only meaningful once ShowModal() has
 * returned wxID_OK. The GRIB file is optional -- leaving it unset is a
 * valid state and Case::grib_file_path stays empty.
 */
class CaseDialog : public wxDialog {
public:
  explicit CaseDialog(wxWindow* parent);

  const Case& GetCase() const { return m_case; }

private:
  void OnOK(wxCommandEvent& event);
  void OnBrowseGrib(wxCommandEvent& event);

  wxTextCtrl* m_position_ctrl;
  wxTimePickerCtrl* m_time_ctrl;
  wxChoice* m_craft_choice;
  wxSpinCtrl* m_pob_ctrl;
  wxTextCtrl* m_grib_path_ctrl;

  Case m_case;
};

#endif  // INTERCEPT_PI_CASE_DIALOG_H__
