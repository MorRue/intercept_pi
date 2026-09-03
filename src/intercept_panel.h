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
#include <wx/spinctrl.h>
#include <wx/timectrl.h>

#include "intercept_pi.h"

/**
 * A non-modal tool window that stays open on top of OpenCPN while the chart
 * remains fully interactive. It carries both the case inputs (position, time,
 * craft, POB, GRIB, target drift, own-ship override) and the computed outputs
 * (datum, drift source, elapsed, bearing/distance/ETA). [Recalculate] re-runs
 * the datum ageing and course-to-steer and refreshes the chart mark + route;
 * closing it with the [x] just hides it (state is kept) and clears the
 * toolbar toggle -- the plugin destroys it in DeInit().
 */
class InterceptPanel : public wxFrame {
public:
  InterceptPanel(wxWindow* parent, intercept_pi* plugin);

private:
  void OnRecalculate(wxCommandEvent& event);
  void OnBrowseGrib(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  // Fills the output rows from a finalised case + effective own-ship, or
  // shows why the course to steer is unavailable.
  void ShowOutputs(const Case& c, const std::optional<OwnShipState>& own);

  intercept_pi* m_plugin;
  wxPanel* m_content;

  wxTextCtrl* m_position_ctrl;
  wxTimePickerCtrl* m_time_ctrl;
  wxChoice* m_craft_choice;
  wxSpinCtrl* m_pob_ctrl;
  wxTextCtrl* m_grib_path_ctrl;
  wxSpinCtrlDouble* m_set_ctrl;
  wxSpinCtrlDouble* m_drift_ctrl;
  wxTextCtrl* m_own_pos_ctrl;
  wxSpinCtrlDouble* m_own_sog_ctrl;

  wxStaticText* m_out_datum;
  wxStaticText* m_out_drift;
  wxStaticText* m_out_elapsed;
  wxStaticText* m_out_bearing;
  wxStaticText* m_out_distance;
  wxStaticText* m_out_eta;
};

#endif  // INTERCEPT_PI_INTERCEPT_PANEL_H__
