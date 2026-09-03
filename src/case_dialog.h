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

#include <optional>

#include "intercept_pi.h"

/**
 * Collects the data for a new search-and-rescue case: reported position,
 * time of report, craft type, persons on board, and an optional GRIB file
 * to source wind/current from. On OK, the position is validated with
 * ParseCoordinate(); bad input shows an error message box and keeps the
 * dialog open, so GetCase() is only meaningful once ShowModal() has
 * returned wxID_OK. The GRIB file is optional -- leaving it unset is a
 * valid state and Case::grib_file_path stays empty.
 *
 * The "Own ship" position/speed fields are an optional override of OpenCPN's
 * live fix: pre-filled from `live_fix` when one is passed, editable, and can
 * be cleared to plan from a hypothetical position. GetOwnShipOverride()
 * returns parsed values only when the position field is non-empty and valid.
 */
class CaseDialog : public wxDialog {
public:
  explicit CaseDialog(wxWindow* parent,
                      std::optional<OwnShipState> live_fix = std::nullopt);

  const Case& GetCase() const { return m_case; }
  std::optional<OwnShipState> GetOwnShipOverride() const {
    return m_own_override;
  }

private:
  void OnOK(wxCommandEvent& event);
  void OnBrowseGrib(wxCommandEvent& event);

  wxTextCtrl* m_position_ctrl;
  wxTimePickerCtrl* m_time_ctrl;
  wxChoice* m_craft_choice;
  wxSpinCtrl* m_pob_ctrl;
  wxTextCtrl* m_grib_path_ctrl;
  wxTextCtrl* m_own_pos_ctrl;
  wxSpinCtrlDouble* m_own_sog_ctrl;

  Case m_case;
  std::optional<OwnShipState> m_own_override;
};

#endif  // INTERCEPT_PI_CASE_DIALOG_H__
