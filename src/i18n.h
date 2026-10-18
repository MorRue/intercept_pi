/******************************************************************************
 * Intercept plugin for OpenCPN -- translation lookup.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_I18N_H__
#define INTERCEPT_PI_I18N_H__

#include <wx/translation.h>

// The plugin's own message catalog. Must match the name the build installs
// translations under: opencpn-${PACKAGE_NAME}.mo (PluginLocalization.cmake).
#define INTERCEPT_PI_CATALOG "opencpn-intercept_pi"

// wxWidgets' _() looks a string up in every loaded catalog, OpenCPN's own
// included -- so a label that happens to match an OpenCPN string comes out
// in OpenCPN's translation ("Own ship" -> "Eigenes Schiff") while the rest
// stays English. Look only in this plugin's catalog: all English until the
// plugin has a translation of its own, then all translated. Include this
// after the wx headers in every file that uses _().
#undef _
#define _(s) wxGetTranslation((s), wxS(INTERCEPT_PI_CATALOG))

#endif  // INTERCEPT_PI_I18N_H__
