/******************************************************************************
 * Intercept plugin for OpenCPN -- intercept results readout.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_RESULTS_DIALOG_H__
#define INTERCEPT_PI_RESULTS_DIALOG_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "intercept_pi.h"

/**
 * Read-only results panel shown once a case has been confirmed: the datum
 * (Case::aged_lat/aged_lon, already finalised by CaseDialog::OnOK before
 * this is constructed) plus the static course to steer to it (Planned
 * direction #4 in CLAUDE.md -- no lead angle).
 *
 * All fields are computed and formatted here, in the constructor -- but
 * since this dialog is itself only ever constructed after CaseDialog
 * returns wxID_OK (see intercept_pi::OnToolbarToolCallback), that already
 * satisfies "populate after OK, not at construction time": there is no
 * earlier point at which this dialog exists with stale data.
 *
 * own_fix_valid is the effective own-ship state passed by the caller (the
 * live fix, or a hand-entered override from the case dialog). When it is
 * false there is no own-ship position at all, so only the datum is shown.
 * When it is true but own_sog_kt <= 0, bearing and distance are still shown
 * and only the ETA is omitted.
 */
class InterceptResultsDialog : public wxDialog {
public:
  InterceptResultsDialog(wxWindow* parent, const Case& case_data,
                          bool own_fix_valid, double own_lat, double own_lon,
                          double own_sog_kt);
};

#endif  // INTERCEPT_PI_RESULTS_DIALOG_H__
