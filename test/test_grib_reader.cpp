/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone test for GribReader.
 *
 * Builds without the OpenCPN plugin SDK or the wxWidgets GUI toolkit:
 * links grib_reader.cpp directly and only needs wxBase (wxFileName,
 * wxDateTime, wxString), not wxCore. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "grib_reader.h"

#include <wx/datetime.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
  } else {
    std::printf("ok:   %s\n", what);
  }
}

wxString JoinPath(const wxString& dir, const wxString& file) {
  wxString d = dir;
  if (!d.IsEmpty() && d.Last() != wxFileName::GetPathSeparator()) {
    d += wxFileName::GetPathSeparator();
  }
  return d + file;
}

}  // namespace

int main(int argc, char** argv) {
  // The fixture directory is passed in by CMake (see add_test in
  // CMakeLists.txt) so this doesn't depend on ctest's working directory;
  // default to a path relative to a manual invocation from the repo root.
  wxString data_dir = (argc > 1) ? wxString(argv[1]) : wxString("test/data");

  // Both fixtures are a GFS 2026-09-01 00Z analysis (forecast hour 0), so
  // their valid time is the cycle time itself -- see test/data/README.md.
  // Built via wxDateTime(time_t), which is always UTC, the same way
  // GribReader's own MakeUtcDateTime is -- wxDateTime::Set() would instead
  // read its arguments as local wall-clock time.
  const wxDateTime kValidTime(static_cast<time_t>(1788220800));

  // (1) Fixture (a): simple-packed (Data Representation Template 5.0),
  // 10 m wind -- the encoding GribReader implements, so this should
  // decode. Not produced by every environment (milestone 1's README notes
  // wgrib2/eccodes were unobtainable there without root); skip cleanly
  // with a note rather than failing when it's missing.
  wxFileName fixture_a(
      JoinPath(data_dir, "gfs_10m_wind_simple_drt5.0.grib2"));
  if (!fixture_a.FileExists()) {
    std::printf(
        "SKIP: fixture (a) gfs_10m_wind_simple_drt5.0.grib2 not present "
        "(see test/data/README.md)\n");
  } else {
    GribReader reader(fixture_a);
    // Grid is 30-40N/10-20E at 0.25 degree spacing starting at (30, 10);
    // (35, 15) lands exactly on a grid point.
    EnvSample wind = reader.LookupWind(35.0, 15.0, kValidTime);
    Check(wind.available, "fixture (a): LookupWind available");
    Check(wind.speed_kt >= 0.0 && wind.speed_kt <= 150.0,
          "fixture (a): speed_kt in [0,150]");
    Check(wind.direction_deg >= 0.0 && wind.direction_deg < 360.0,
          "fixture (a): direction_deg in [0,360)");
  }

  // (2) Fixture (b): complex-packed (Data Representation Template 5.3) --
  // an encoding GribReader does not implement. It must be skipped
  // cleanly (no matching record found), not crash or abort.
  wxFileName fixture_b(
      JoinPath(data_dir, "gfs_pbl_wind_complex_drt5.3.grib2"));
  Check(fixture_b.FileExists(), "fixture (b) file present");
  {
    GribReader reader(fixture_b);
    EnvSample wind = reader.LookupWind(35.0, 15.0, kValidTime);
    Check(!wind.available,
          "fixture (b): LookupWind unavailable (unsupported packing)");
  }

  // (3) Nonexistent path: must not crash, must report unavailable.
  {
    wxFileName missing(JoinPath(data_dir, "does_not_exist.grib2"));
    GribReader reader(missing);
    EnvSample wind = reader.LookupWind(35.0, 15.0, kValidTime);
    Check(!wind.available, "nonexistent file: LookupWind unavailable");
  }

  // (4) Malformed input must be rejected cleanly -- no crash, no
  // out-of-bounds read -- and simply yield "not available". The parser
  // reads a hand-rolled binary format, so this is the safety net for a
  // user pointing the file picker at a corrupt or non-GRIB file.
  {
    struct BadCase {
      const char* name;
      std::vector<unsigned char> bytes;
    };
    std::vector<BadCase> cases = {
        {"empty file", {}},
        {"random bytes, no GRIB magic",
         {0x00, 0xFF, 0x42, 0x13, 0x37, 0xAB, 0xCD, 0xEF, 0x01, 0x02}},
        {"GRIB magic then nothing",
         {'G', 'R', 'I', 'B', 0, 0, 0, 2}},
        {"GRIB magic, message length far past EOF",
         {'G', 'R', 'I', 'B', 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF,
          0x00, 0x01, 0x02}},
        {"GRIB magic, section length past message end",
         {'G', 'R', 'I', 'B', 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0x28,
          0xFF, 0xFF, 0xFF, 0xFF, 0x01}},
        {"0xFF filler", std::vector<unsigned char>(64, 0xFF)},
    };

    for (const auto& bad : cases) {
      wxString tmp = wxFileName::CreateTempFileName("grib_bad");
      FILE* f = std::fopen(tmp.mb_str(), "wb");
      if (f) {
        if (!bad.bytes.empty())
          std::fwrite(bad.bytes.data(), 1, bad.bytes.size(), f);
        std::fclose(f);
        GribReader reader{wxFileName(tmp)};
        EnvSample w = reader.LookupWind(35.0, 15.0, kValidTime);
        EnvSample c = reader.LookupCurrent(35.0, 15.0, kValidTime);
        Check(!w.available && !c.available, bad.name);
        wxRemoveFile(tmp);
      }
    }
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
