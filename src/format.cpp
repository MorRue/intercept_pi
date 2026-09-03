/******************************************************************************
 * Intercept plugin for OpenCPN -- number/coordinate formatting.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "format.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

std::string FormatBuf(const char* fmt, ...) {
  char buf[64];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  return std::string(buf);
}

std::string FormatDD(double value_deg, char pos_hemi, char neg_hemi) {
  const char hemi = value_deg < 0.0 ? neg_hemi : pos_hemi;
  return FormatBuf("%.6f %c", std::fabs(value_deg), hemi);
}

std::string FormatDDM(double value_deg, char pos_hemi, char neg_hemi) {
  const char hemi = value_deg < 0.0 ? neg_hemi : pos_hemi;
  const double a = std::fabs(value_deg);
  const int deg = static_cast<int>(a);
  const double min = (a - deg) * 60.0;
  return FormatBuf("%d %06.3f' %c", deg, min, hemi);
}

std::string FormatDMS(double value_deg, char pos_hemi, char neg_hemi) {
  const char hemi = value_deg < 0.0 ? neg_hemi : pos_hemi;
  const double a = std::fabs(value_deg);
  const int deg = static_cast<int>(a);
  const double min_full = (a - deg) * 60.0;
  const int min = static_cast<int>(min_full);
  const double sec = (min_full - min) * 60.0;
  return FormatBuf("%d %02d %04.1f\" %c", deg, min, sec, hemi);
}

}  // namespace

std::string FormatLatDD(double lat_deg) { return FormatDD(lat_deg, 'N', 'S'); }
std::string FormatLonDD(double lon_deg) { return FormatDD(lon_deg, 'E', 'W'); }

std::string FormatLatDDM(double lat_deg) {
  return FormatDDM(lat_deg, 'N', 'S');
}
std::string FormatLonDDM(double lon_deg) {
  return FormatDDM(lon_deg, 'E', 'W');
}

std::string FormatLatDMS(double lat_deg) {
  return FormatDMS(lat_deg, 'N', 'S');
}
std::string FormatLonDMS(double lon_deg) {
  return FormatDMS(lon_deg, 'E', 'W');
}

std::string FormatBearingDeg(double bearing_deg) {
  double normalized = std::fmod(bearing_deg, 360.0);
  if (normalized < 0.0) normalized += 360.0;
  // Round before re-checking the wraparound: rounding e.g. 359.96 up to
  // one decimal gives 360.0, which must fold back to 0.0.
  double rounded = std::round(normalized * 10.0) / 10.0;
  if (rounded >= 360.0) rounded -= 360.0;
  return FormatBuf("%.1f", rounded);
}

std::string FormatDistanceNm(double distance_nm) {
  return FormatBuf("%.1f", distance_nm);
}

std::string FormatEtaHhMm(double eta_hours) {
  if (eta_hours < 0.0) eta_hours = 0.0;
  const long total_minutes = std::lround(eta_hours * 60.0);
  const long hh = total_minutes / 60;
  const long mm = total_minutes % 60;
  return FormatBuf("%02ld:%02ld", hh, mm);
}
