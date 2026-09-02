/******************************************************************************
 * Intercept plugin for OpenCPN -- IAMSAR reference test for datum ageing.
 *
 * Encodes the fully worked example in IAMSAR Vol II Appendix Q ("F/V
 * SAMPLE") -- see docs/LEEWAY_NEEDS_VERIFICATION.md -- as an external-truth
 * regression test: (a) the leeway coefficient the default (non-"wooden")
 * craft_type resolves to, applied to the example's average surface wind,
 * must reproduce the example's leeway speed; (b) ComputeAgedDatum, given
 * the example's wind and current, must land near the example's datum.
 *
 * Part (b)'s tolerance is wider than the position-error tolerances used
 * elsewhere (9 NM, not 2): docs/LEEWAY_NEEDS_VERIFICATION.md's suggested
 * datum (37 44'N 065 03'W) is the arithmetic midpoint of IAMSAR's own two
 * divergence-bounded datums (021T/2.21kt and 060T/3.15kt, i.e. downwind
 * +-50 deg), and that midpoint is provably not the point a pure-downwind
 * (0 deg divergence) vector-sum produces: averaging two vectors of equal
 * magnitude L at +-50 deg from a center bearing yields a vector of
 * magnitude L*cos(50deg) (~=0.64L) at the center bearing, not L. Even
 * IAMSAR's own textbook leeway of exactly 1.3 kt at pure downwind (014T)
 * lands ~8.7 NM from that midpoint; this code's 0.036-coefficient leeway
 * (1.14 kt) lands ~5.8 NM away. A tight (2 NM) tolerance is therefore not
 * achievable against this particular reference point for a no-divergence
 * model -- only a leeway near 0.0263 x wind (~0.84 kt), not otherwise
 * motivated by IAMSAR Fig N-2, would land within 2 NM of it. 9 NM covers
 * the code's actual (correct-model, correct-coefficient) result with
 * headroom, while still failing hard (by two orders of magnitude) if the
 * coefficient regresses to the old 0.36: that lands ~198 NM away.
 *
 * ComputeAgedDatum only reads wind and current from a GribReader, so part
 * (b) builds a minimal synthetic GRIB2 file in memory -- four one-point
 * messages (10 m wind U/V, surface current U/V) holding the example's
 * constant values -- rather than shipping a binary fixture. Data
 * Representation Template 5.0 with num_bits=0 makes every sampled point
 * equal to ref_value exactly, so no bit-packing is needed; Section 6
 * (bitmap) is omitted, which GribReader treats as "every point present".
 *
 * Builds without the OpenCPN plugin SDK or the wxWidgets GUI toolkit, same
 * as test_datum_age.cpp: links datum_age.cpp and grib_reader.cpp directly
 * and only needs wxBase. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "datum_age.h"

#include "grib_reader.h"

#include <wx/datetime.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

// Matches kEarthRadiusNm in datum_age.cpp.
constexpr double kEarthRadiusNm = 3440.065;
// Matches kMetersPerSecondToKnots in grib_reader.cpp.
constexpr double kMetersPerSecondToKnots = 1.9438444924406;

double ToRad(double deg) { return deg * M_PI / 180.0; }

// Great-circle distance, close enough for a several-NM regression tolerance.
double DistanceNm(double lat1, double lon1, double lat2, double lon2) {
  double phi1 = ToRad(lat1), phi2 = ToRad(lat2);
  double dphi = ToRad(lat2 - lat1);
  double dlambda = ToRad(lon2 - lon1);
  double a = std::sin(dphi / 2) * std::sin(dphi / 2) +
             std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2) *
                 std::sin(dlambda / 2);
  double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
  return kEarthRadiusNm * c;
}

void AppendU(std::vector<unsigned char>* buf, uint64_t value, int nbytes) {
  for (int i = nbytes - 1; i >= 0; --i) {
    buf->push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
  }
}

// Sign-magnitude big-endian, mirroring ReadS() in grib_reader.cpp -- GRIB2
// scale factors and grid coordinates are sign-magnitude, not two's
// complement.
void AppendS(std::vector<unsigned char>* buf, int64_t value, int nbytes) {
  uint64_t magnitude = static_cast<uint64_t>(value < 0 ? -value : value);
  uint64_t sign_bit = 1ull << (nbytes * 8 - 1);
  AppendU(buf, magnitude | (value < 0 ? sign_bit : 0), nbytes);
}

void AppendFloatBE(std::vector<unsigned char>* buf, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  AppendU(buf, bits, 4);
}

void AppendZeros(std::vector<unsigned char>* buf, size_t n) {
  buf->insert(buf->end(), n, 0);
}

/**
 * One GRIB2 message: a single field at a single grid point, using Data
 * Representation Template 5.0 with num_bits=0 so the decoded value is
 * exactly ref_value (see UnpackSimple in grib_reader.cpp) -- no bitmap
 * (Section 6) is emitted, which GribReader treats as "every point
 * present". The grid is a 1x1 point at (la1, lo1) with a wide spacing
 * (di/dj), so every lookup during the drift integration -- which moves the
 * sample position by well under di/dj -- resolves to that same point.
 */
std::vector<unsigned char> BuildGribMessage(
    int discipline, int category, int number, int level_type,
    double level_value, float ref_value, double la1, double lo1, double di,
    double dj) {
  std::vector<unsigned char> sec1;
  AppendU(&sec1, 19, 4);   // Section length.
  AppendU(&sec1, 1, 1);    // Section number.
  AppendU(&sec1, 0, 2);    // Originating center.
  AppendU(&sec1, 0, 2);    // Originating subcenter.
  AppendU(&sec1, 2, 1);    // Master tables version.
  AppendU(&sec1, 1, 1);    // Local tables version.
  AppendU(&sec1, 1, 1);    // Significance of reference time.
  AppendU(&sec1, 2000, 2);  // Year.
  AppendU(&sec1, 1, 1);     // Month.
  AppendU(&sec1, 25, 1);    // Day.
  AppendU(&sec1, 21, 1);    // Hour.
  AppendU(&sec1, 45, 1);    // Minute.
  AppendU(&sec1, 0, 1);     // Second.

  std::vector<unsigned char> sec3;
  AppendU(&sec3, 72, 4);  // Section length.
  AppendU(&sec3, 3, 1);   // Section number.
  AppendZeros(&sec3, 7);  // Source, number of points, list octets/interp.
  AppendU(&sec3, 0, 2);   // Grid Definition Template number: 3.0.
  AppendZeros(&sec3, 16);  // Shape/scale of earth, unused offsets.
  AppendU(&sec3, 1, 4);    // Ni.
  AppendU(&sec3, 1, 4);    // Nj.
  AppendZeros(&sec3, 8);   // Basic angle etc, unused offsets.
  AppendS(&sec3, static_cast<int64_t>(std::llround(la1 * 1e6)), 4);  // La1.
  AppendS(&sec3, static_cast<int64_t>(std::llround(lo1 * 1e6)), 4);  // Lo1.
  AppendZeros(&sec3, 9);  // Resolution flags, La2, Lo2, unused offsets.
  AppendU(&sec3, static_cast<uint64_t>(std::llround(di * 1e6)), 4);  // Di.
  AppendU(&sec3, static_cast<uint64_t>(std::llround(dj * 1e6)), 4);  // Dj.
  AppendU(&sec3, 0, 1);  // Scan mode: rows increase in i, row-major.

  std::vector<unsigned char> sec4;
  AppendU(&sec4, 34, 4);  // Section length.
  AppendU(&sec4, 4, 1);   // Section number.
  AppendZeros(&sec4, 2);  // Number of coordinate values after template.
  AppendU(&sec4, 0, 2);   // Product Definition Template number: 4.0.
  AppendU(&sec4, category, 1);
  AppendU(&sec4, number, 1);
  AppendZeros(&sec4, 6);  // Generating process / data cutoff fields.
  AppendU(&sec4, 1, 1);   // Time range unit: hours.
  AppendU(&sec4, 0, 4);   // Forecast time: 0 (analysis).
  AppendU(&sec4, level_type, 1);
  AppendS(&sec4, 0, 1);  // Level scale factor.
  AppendS(&sec4, static_cast<int64_t>(std::llround(level_value)), 4);
  AppendZeros(&sec4, 6);  // Second fixed surface fields, unused.

  std::vector<unsigned char> sec5;
  AppendU(&sec5, 21, 4);  // Section length.
  AppendU(&sec5, 5, 1);   // Section number.
  AppendZeros(&sec5, 4);  // Number of data points.
  AppendU(&sec5, 0, 2);   // Data Representation Template number: 5.0.
  AppendFloatBE(&sec5, ref_value);
  AppendS(&sec5, 0, 2);  // Binary scale factor.
  AppendS(&sec5, 0, 2);  // Decimal scale factor.
  AppendU(&sec5, 0, 1);  // Number of bits: 0 -> every point equals ref_value.
  AppendZeros(&sec5, 1);

  std::vector<unsigned char> sec7;
  AppendU(&sec7, 5, 4);  // Section length: header only, no packed data.
  AppendU(&sec7, 7, 1);  // Section number.

  uint64_t total_len =
      16 + sec1.size() + sec3.size() + sec4.size() + sec5.size() +
      sec7.size() + 4;

  std::vector<unsigned char> msg;
  msg.push_back('G');
  msg.push_back('R');
  msg.push_back('I');
  msg.push_back('B');
  AppendZeros(&msg, 2);  // Reserved.
  AppendU(&msg, discipline, 1);
  AppendU(&msg, 2, 1);  // Edition: GRIB2.
  AppendU(&msg, total_len, 8);
  msg.insert(msg.end(), sec1.begin(), sec1.end());
  msg.insert(msg.end(), sec3.begin(), sec3.end());
  msg.insert(msg.end(), sec4.begin(), sec4.end());
  msg.insert(msg.end(), sec5.begin(), sec5.end());
  msg.insert(msg.end(), sec7.begin(), sec7.end());
  msg.push_back('7');
  msg.push_back('7');
  msg.push_back('7');
  msg.push_back('7');
  return msg;
}

}  // namespace

int main() {
  // (a) Leeway sub-calculation: IAMSAR Vol II Appendix Q gives a leeway
  // speed of 1.3 kt for the example's average surface wind of 31.72 kt.
  // The code's two-bucket craft_type model does not have a specific
  // "fishing vessel" bucket, so this checks the default (non-"wooden")
  // bucket -- the same one test_datum_age.cpp's GRIB-sourced case uses.
  {
    const wxString craft_type = "Rubber boat (inflatable, RIB, liferaft)";
    LeewayCoefficients coeffs = LookupLeewayCoefficients(craft_type);
    double leeway_speed_kt =
        coeffs.speed_pct_of_wind * 31.72 + coeffs.speed_constant_kt;
    Check(std::fabs(leeway_speed_kt - 1.3) <= 0.4,
          "leeway sub-calc: default coefficient x 31.72 kt is near 1.3 kt");
  }

  // (b) Full datum: EIP 37 10.0'N 065 45.0'W, ASW 194T/31.72kt, TWC
  // 057T/1.86kt, 18.75 h drift interval -- expect a datum near
  // 37 44'N 065 03'W (the pure-downwind variant of the IAMSAR worked
  // example, which produces two divergence-bounded datums this code's
  // no-divergence model does not compute; see
  // docs/LEEWAY_NEEDS_VERIFICATION.md).
  {
    const double eip_lat = 37.0 + 10.0 / 60.0;
    const double eip_lon = -(65.0 + 45.0 / 60.0);
    const double wind_speed_kt = 31.72;
    const double wind_from_deg = 194.0;
    const double current_speed_kt = 1.86;
    const double current_toward_deg = 57.0;
    const double drift_hours = 18.75;
    const double expected_lat = 37.0 + 44.0 / 60.0;
    const double expected_lon = -(65.0 + 3.0 / 60.0);
    // See the file header: 9 NM, not the 2 NM used elsewhere, because this
    // reference point is the midpoint of IAMSAR's two divergence-bounded
    // datums, which a no-divergence vector-sum systematically lands ~6-9
    // NM away from regardless of how accurate the leeway figure is.
    const double tolerance_nm = 9.0;

    // Grid spacing wide enough that the ~50 NM drift never leaves the
    // single grid point sampled at every step.
    const double grid_spacing_deg = 10.0;

    // GribReader::LookupWind treats (u, v) as the vector the wind blows
    // TOWARD and reports direction_deg = atan2(-u, -v) as the FROM
    // direction; solve for (u, v) given a FROM direction.
    const double wind_ms = wind_speed_kt / kMetersPerSecondToKnots;
    const float wind_u =
        static_cast<float>(-wind_ms * std::sin(ToRad(wind_from_deg)));
    const float wind_v =
        static_cast<float>(-wind_ms * std::cos(ToRad(wind_from_deg)));

    // GribReader::LookupCurrent treats (u, v) as the vector the current
    // sets TOWARD directly (direction_deg = atan2(u, v)).
    const double current_ms = current_speed_kt / kMetersPerSecondToKnots;
    const float current_u =
        static_cast<float>(current_ms * std::sin(ToRad(current_toward_deg)));
    const float current_v =
        static_cast<float>(current_ms * std::cos(ToRad(current_toward_deg)));

    std::vector<unsigned char> buf;
    // 10 m wind: discipline 0 (meteorological), category 2 (momentum),
    // number 2/3 (UGRD/VGRD), level type 103 (height above ground) at 10 m.
    std::vector<unsigned char> wind_u_msg =
        BuildGribMessage(0, 2, 2, 103, 10.0, wind_u, eip_lat, eip_lon,
                          grid_spacing_deg, grid_spacing_deg);
    std::vector<unsigned char> wind_v_msg =
        BuildGribMessage(0, 2, 3, 103, 10.0, wind_v, eip_lat, eip_lon,
                          grid_spacing_deg, grid_spacing_deg);
    // Surface current: discipline 10 (oceanographic), category 1
    // (currents), number 2/3 (U/V component); level is not checked for
    // current lookups, so level_type/level_value are arbitrary.
    std::vector<unsigned char> current_u_msg =
        BuildGribMessage(10, 1, 2, 0, 0.0, current_u, eip_lat, eip_lon,
                          grid_spacing_deg, grid_spacing_deg);
    std::vector<unsigned char> current_v_msg =
        BuildGribMessage(10, 1, 3, 0, 0.0, current_v, eip_lat, eip_lon,
                          grid_spacing_deg, grid_spacing_deg);
    buf.insert(buf.end(), wind_u_msg.begin(), wind_u_msg.end());
    buf.insert(buf.end(), wind_v_msg.begin(), wind_v_msg.end());
    buf.insert(buf.end(), current_u_msg.begin(), current_u_msg.end());
    buf.insert(buf.end(), current_v_msg.begin(), current_v_msg.end());

    wxString temp_path = wxFileName::CreateTempFileName("iamsar_grib");
    Check(!temp_path.IsEmpty(), "synthetic GRIB: temp file created");

    FILE* f = std::fopen(temp_path.mb_str(), "wb");
    Check(f != nullptr, "synthetic GRIB: temp file opened for writing");
    if (f) {
      std::fwrite(buf.data(), 1, buf.size(), f);
      std::fclose(f);
    }

    {
      wxFileName fixture(temp_path);
      GribReader grib(fixture);

      const wxDateTime time_of_report(static_cast<time_t>(0));
      const wxDateTime now(
          static_cast<time_t>(std::llround(drift_hours * 3600.0)));

      EnvSample wind = grib.LookupWind(eip_lat, eip_lon, time_of_report);
      EnvSample current = grib.LookupCurrent(eip_lat, eip_lon, time_of_report);
      Check(wind.available,
            "synthetic GRIB: precondition -- wind sample available");
      Check(current.available,
            "synthetic GRIB: precondition -- current sample available");
      Check(std::fabs(wind.speed_kt - wind_speed_kt) < 0.05,
            "synthetic GRIB: precondition -- wind speed round-trips");
      Check(std::fabs(current.speed_kt - current_speed_kt) < 0.05,
            "synthetic GRIB: precondition -- current speed round-trips");

      AgedDatum aged = ComputeAgedDatum(
          eip_lat, eip_lon, time_of_report, now,
          "Rubber boat (inflatable, RIB, liferaft)", &grib,
          ManualSetAndDrift());

      double distance_nm =
          DistanceNm(aged.lat, aged.lon, expected_lat, expected_lon);
      Check(distance_nm <= tolerance_nm,
            "IAMSAR Appendix Q: datum lands within 9 NM of the pure-downwind "
            "expected position");
    }

    wxRemoveFile(temp_path);
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}
