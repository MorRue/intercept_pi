/******************************************************************************
 * Intercept plugin for OpenCPN -- number/coordinate formatting.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_FORMAT_H__
#define INTERCEPT_PI_FORMAT_H__

#include <string>

// Free functions only, over plain doubles and std::string -- no wxWidgets
// dependency, so this is usable from a standalone CTest binary and from
// the GUI readout alike (Next #2a in CLAUDE.md).

// Decimal degrees: "<abs value, 6 decimals> <hemisphere>", e.g.
// "34.500000 N", "118.250000 W". Zero is reported as N / E.
std::string FormatLatDD(double lat_deg);
std::string FormatLonDD(double lon_deg);

// Degrees + decimal minutes: "<deg> <mm.mmm>' <hemisphere>", e.g.
// "34 30.000' N".
std::string FormatLatDDM(double lat_deg);
std::string FormatLonDDM(double lon_deg);

// Degrees, minutes, decimal seconds: "<deg> <mm> <ss.s>\" <hemisphere>",
// e.g. "34 30 45.0\" N".
std::string FormatLatDMS(double lat_deg);
std::string FormatLonDMS(double lon_deg);

// Bearing normalized to [0, 360) and formatted to 1 decimal, e.g. "45.0".
// An input of 360.0 (or anything that rounds up to it) formats as "0.0".
std::string FormatBearingDeg(double bearing_deg);

// Distance in nautical miles, formatted to 1 decimal, e.g. "12.3".
std::string FormatDistanceNm(double distance_nm);

// Elapsed time in hours, formatted "HH:MM" (HH is total elapsed hours, not
// clamped to a 24h clock -- this is a duration, not a time of day).
// Negative input is clamped to zero.
std::string FormatEtaHhMm(double eta_hours);

#endif  // INTERCEPT_PI_FORMAT_H__
