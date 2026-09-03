/******************************************************************************
 * Intercept plugin for OpenCPN -- cross-toolchain shims.
 *
 * Include this FIRST -- before <cmath> or any header that pulls it in -- in
 * any .cpp that uses <cmath> constants (M_PI and friends) or other
 * not-quite-portable bits.
 *
 * Why: the code is written and built on a Linux sandbox (GCC / libstdc++,
 * lenient about these) but must also compile for the Windows target (MSVC,
 * which is not). The sandbox cannot run MSVC, so these divergences only
 * surface in Windows CI -- see the homelab repo's ci_watcher / apply_ci_fix.
 * Files that include a wxWidgets header (`wx/wxprec.h` etc.) get M_PI for
 * free because wx sets this itself; a lean pure-computation .cpp does not.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_PORTABILITY_H__
#define INTERCEPT_PI_PORTABILITY_H__

// MSVC's <cmath> only defines M_PI, M_PI_2, M_SQRT2, ... when this macro is
// set before <cmath> is first seen in the translation unit. GCC and Clang
// define them unconditionally, which is why this is easy to miss on Linux.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>

// Belt and braces: if some standard-library configuration still did not
// provide M_PI, define it here.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#endif  // INTERCEPT_PI_PORTABILITY_H__
