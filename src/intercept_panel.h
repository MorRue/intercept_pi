/******************************************************************************
 * Intercept plugin for OpenCPN -- persistent input/output panel.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_INTERCEPT_PANEL_H__
#define INTERCEPT_PI_INTERCEPT_PANEL_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/datectrl.h>
#include <wx/spinctrl.h>
#include <wx/timectrl.h>

#include "intercept_pi.h"

/**
 * A non-modal tool window that stays open on top of OpenCPN while the chart
 * remains fully interactive. Inputs are in titled group boxes -- own ship,
 * target drift, environment (GRIB) + craft, report -- followed by the
 * computed outputs (datum, drift source, elapsed, bearing/distance/ETA).
 * [Recalculate] re-runs the datum ageing and course-to-steer and refreshes
 * the chart marks + route; closing it with the [x] just hides it (state is
 * kept) and clears the toolbar toggle -- the plugin destroys it in DeInit().
 */
class InterceptPanel : public wxFrame {
public:
  InterceptPanel(wxWindow* parent, intercept_pi* plugin);

private:
  void OnRecalculate(wxCommandEvent& event);
  void OnBrowseGrib(wxCommandEvent& event);
  void OnClearGrib(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  // Disables the manual "Target drift" fields while a GRIB file is set.
  void UpdateGribLock();
  // Every "lock" checkbox works the same way: checked disables its field(s).
  void OnLockToggled(wxCommandEvent& event);
  // Re-applies the last computed case with the current show/hide checkboxes so
  // the chart redraws immediately, without a full recalculation.
  void OnDisplayToggled(wxCommandEvent& event);

  // Fills the output rows from a finalised case + effective own-ship, or
  // shows why the course to steer is unavailable.
  void ShowOutputs(const Case& c, const std::optional<OwnShipState>& own);

  intercept_pi* m_plugin;
  wxPanel* m_content;

  // "lock" checkboxes, one per input, sitting to the right of the field. All
  // identical in behaviour: checked => the field is disabled. The reported
  // position and time start unlocked (you type them in each time); the
  // own-ship fields start locked, so by default the live GPS fix is used and
  // you unlock only the one(s) you want to enter by hand.
  wxCheckBox* m_lock_position;
  wxCheckBox* m_lock_time;
  wxCheckBox* m_lock_own_pos;
  wxCheckBox* m_lock_own_sog;
  wxCheckBox* m_show_target;     // "Target" mark at the reported position.
  wxCheckBox* m_show_estimated;  // "Estimated position" mark at the datum.
  wxCheckBox* m_show_routes;     // Drift track + course-to-steer route.

  wxTextCtrl* m_position_ctrl;
  wxDatePickerCtrl* m_date_ctrl;
  wxTimePickerCtrl* m_time_ctrl;
  wxChoice* m_craft_choice;
  wxSpinCtrl* m_pob_ctrl;
  wxTextCtrl* m_grib_path_ctrl;
  wxSpinCtrlDouble* m_set_ctrl;
  wxSpinCtrlDouble* m_drift_ctrl;
  wxTextCtrl* m_own_pos_ctrl;
  wxSpinCtrlDouble* m_own_sog_ctrl;

  // The most recent finalised case and effective own-ship, kept so the display
  // checkboxes can redraw the chart without re-parsing the inputs.
  std::optional<Case> m_last_case;
  std::optional<OwnShipState> m_last_own;

  wxStaticText* m_out_datum;
  wxStaticText* m_out_drift;
  wxStaticText* m_out_moved;
  wxStaticText* m_out_elapsed;
  wxStaticText* m_out_bearing;
  wxStaticText* m_out_distance;
  wxStaticText* m_out_eta;
};

#endif  // INTERCEPT_PI_INTERCEPT_PANEL_H__
