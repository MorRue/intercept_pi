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
#include <wx/timer.h>

#include "intercept_pi.h"

/**
 * A non-modal tool window that stays open on top of OpenCPN while the chart
 * remains fully interactive. Inputs are in titled group boxes -- own ship,
 * target drift, environment (GRIB) + craft, report -- followed by the
 * computed outputs (datum, drift source, elapsed, bearing/distance/ETA).
 * [Recalculate] re-runs the datum ageing and course-to-steer and refreshes
 * the chart marks + route, and can also run on a timer ("Auto-recalculate
 * every N min"). Closing it with the [x] just hides it (state is kept, and a
 * running auto-recalculation keeps updating the chart) and clears the
 * toolbar toggle -- the plugin destroys it in DeInit().
 */
class InterceptPanel : public wxFrame {
public:
  InterceptPanel(wxWindow* parent, intercept_pi* plugin);

  // Called by the plugin's DeInit() before Destroy(), which only schedules
  // the deletion: stops the auto-recalculation timer and forgets the plugin,
  // so nothing in between can touch the chart or a plugin being unloaded.
  void Detach();

private:
  void OnRecalculate(wxCommandEvent& event);
  void OnAutoTimer(wxTimerEvent& event);
  void OnAutoToggled(wxCommandEvent& event);
  void OnAutoIntervalChanged(wxSpinEvent& event);

  // Shared by the button and the timer. Catches any exception so it can't
  // escape the wx event loop into OpenCPN (which would abort the process),
  // and skips a timer tick that arrives while an error dialog is still open.
  void RunRecalculate(bool automatic);
  void DoRecalculate(bool automatic);
  // A manual run shows the error in a dialog. An automatic run never opens
  // a dialog: it skips this tick, keeps the last good result on the chart and
  // shows the reason in the note line -- auto mode stays on and picks up
  // again once the input is valid.
  void ReportInputError(const wxString& message, bool automatic);
  // Draws the case on the chart and shows, in the note line, whether
  // anything was left in place because OpenCPN is navigating with it.
  void ApplyToChart(const Case& c, const std::optional<OwnShipState>& own);
  // The line under the auto-recalculate row; empty hides it. A warning shows
  // in red.
  void SetNote(const wxString& text, bool warning);
  // Re-lays out after the outputs or note changed. Manual runs resize the
  // window to fit; automatic ones and display toggles only grow it, so a size
  // the operator chose survives.
  void RefreshLayout(bool automatic);
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
  wxStaticText* m_out_intercept;
  wxStaticText* m_out_bearing;
  wxStaticText* m_out_distance;
  wxStaticText* m_out_eta;

  wxCheckBox* m_auto_recalc;
  wxSpinCtrl* m_auto_minutes;
  wxStaticText* m_out_last_calc;  // Time of the last successful calculation.
  wxStaticText* m_out_note;       // Why a tick was skipped / something kept.
  wxTimer m_auto_timer;
  bool m_recalc_running = false;
};

#endif  // INTERCEPT_PI_INTERCEPT_PANEL_H__
