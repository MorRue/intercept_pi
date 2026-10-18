/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for position parsing.
 *
 * position_parse.{h,cpp} only need wxBase (wxString), so this links it
 * directly rather than the full plugin build. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "position_parse.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Report(bool ok, const char* what, const wxString& detail) {
  if (ok) {
    std::printf("ok:   %s\n", what);
  } else {
    std::fprintf(stderr, "FAIL: %s (%s)\n", what,
                 detail.ToStdString().c_str());
    ++g_failures;
  }
}

// text is UTF-8, so the degree / minute / second marks below survive
// regardless of the compiler's execution character set.
void ExpectDeg(const char* text, bool is_lat, double want, const char* what) {
  PositionParseResult r = ParseCoordinate(wxString::FromUTF8(text), is_lat);
  if (!r.ok) {
    Report(false, what, wxString::Format("error: %s", r.error));
    return;
  }
  Report(std::fabs(r.degrees - want) < 1e-9, what,
         wxString::Format("got %.9f, want %.9f", r.degrees, want));
}

void ExpectError(const char* text, bool is_lat, const char* what) {
  PositionParseResult r = ParseCoordinate(wxString::FromUTF8(text), is_lat);
  Report(!r.ok && !r.error.IsEmpty(), what,
         wxString::Format("parsed as %.9f", r.degrees));
}

void ExpectSplit(const char* text, const char* want_lat, const char* want_lon,
                 const char* what) {
  wxString lat, lon;
  bool ok = SplitPosition(wxString::FromUTF8(text), &lat, &lon);
  Report(ok && lat == wxString::FromUTF8(want_lat) &&
             lon == wxString::FromUTF8(want_lon),
         what, wxString::Format("got ok=%d '%s' / '%s'", ok, lat, lon));
}

}  // namespace

int main() {
  // -------- Plain formats (unchanged behaviour) --------
  ExpectDeg("45.5", true, 45.5, "DD: signed, no letter");
  ExpectDeg("-15.25", false, -15.25, "DD: negative longitude");
  ExpectDeg("45.5 S", true, -45.5, "DD with letter: S negates");
  ExpectDeg("45 30.5 N", true, 45.0 + 30.5 / 60.0, "DDM");
  ExpectDeg("015 20.3 E", false, 15.0 + 20.3 / 60.0, "DDM: leading zero");
  ExpectDeg("118 45 W", false, -118.75, "DDM: W negates");
  ExpectDeg("45 30 30 N", true, 45.0 + 30.0 / 60.0 + 30.0 / 3600.0, "DMS");
  ExpectDeg("30.5N", true, 30.5, "letter attached to the number");

  // -------- As OpenCPN displays them (toSDMM) --------
  ExpectDeg("45° 30.5000' N", true, 45.0 + 30.5 / 60.0,
            "OpenCPN DDM, high precision");
  ExpectDeg("015° 20.3' E", false, 15.0 + 20.3 / 60.0,
            "OpenCPN DDM, low precision, 3-digit lon");
  ExpectDeg("45° 30' 30.000\" S", true,
            -(45.0 + 30.0 / 60.0 + 30.0 / 3600.0), "OpenCPN DMS");
  ExpectDeg("-15.200000", false, -15.2, "OpenCPN decimal degrees (no letter)");
  ExpectDeg("45°30.5'N", true, 45.0 + 30.5 / 60.0, "marks with no spaces");
  ExpectDeg("45º 30.5′ N", true, 45.0 + 30.5 / 60.0,
            "ordinal-o degree sign and typographic prime");
  ExpectDeg("45 30′ 30″ N", true, 45.0 + 30.0 / 60.0 + 30.0 / 3600.0,
            "typographic prime and double prime");

  // -------- Still rejected --------
  ExpectError("", true, "empty");
  ExpectError("45 60 N", true, "minutes must be < 60");
  ExpectError("45 30.5 E", true, "E is not a latitude letter");
  ExpectError("45 30.5", true, "DDM without a hemisphere letter");
  ExpectError("91", true, "latitude out of range");
  ExpectError("181° 00' W", false, "longitude out of range, with marks");
  ExpectError("-45 30 N", true, "negative degrees with a letter");
  ExpectError("45° 30' 30\" 10 N", true, "four numbers");
  ExpectError("abc N", true, "not a number");

  // -------- Splitting lat from lon --------
  ExpectSplit("45 30.5 N, 015 20.3 E", "45 30.5 N", "015 20.3 E",
              "split at the comma");
  ExpectSplit("45 30.5 N 015 20.3 E", "45 30.5 N", "015 20.3 E",
              "split after the N/S letter");
  ExpectSplit("45° 30.5000' N, 015° 20.3000' E", "45° 30.5000' N",
              "015° 20.3000' E", "split an OpenCPN-formatted pair");

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
