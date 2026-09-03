/******************************************************************************
 * Intercept plugin for OpenCPN -- GRIB2 wind/current lookup.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "grib_reader.h"

#include <wx/file.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

constexpr double kMetersPerSecondToKnots = 1.9438444924406;

double NormalizeDegrees(double deg) {
  double d = std::fmod(deg, 360.0);
  if (d < 0.0) d += 360.0;
  return d;
}

// GRIB2 unsigned big-endian integer, nbytes in [1,4]. Every call site
// length-checks its section first; this is belt-and-braces so a single
// missed guard on a malformed or truncated file yields 0, not an
// out-of-bounds read.
uint32_t ReadU(const std::vector<unsigned char>& buf, size_t off,
               int nbytes) {
  if (nbytes <= 0 || off + static_cast<size_t>(nbytes) > buf.size()) return 0;
  uint32_t v = 0;
  for (int i = 0; i < nbytes; ++i) v = (v << 8) | buf[off + i];
  return v;
}

// GRIB2 signed integer: sign-magnitude, the leftmost bit of the field is
// the sign rather than two's complement. Used for scale factors and grid
// coordinates. nbytes in [1,4].
int32_t ReadS(const std::vector<unsigned char>& buf, size_t off,
              int nbytes) {
  uint32_t raw = ReadU(buf, off, nbytes);
  uint32_t sign_bit = 1u << (nbytes * 8 - 1);
  int32_t magnitude = static_cast<int32_t>(raw & (sign_bit - 1));
  return (raw & sign_bit) ? -magnitude : magnitude;
}

float ReadFloatBE(const std::vector<unsigned char>& buf, size_t off) {
  uint32_t bits = ReadU(buf, off, 4);
  float f;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}

// Days from the civil epoch (1970-01-01) -- Howard Hinnant's algorithm.
// Used instead of wxDateTime::Set() so reference/valid times are built as
// unambiguous UTC instants rather than local-timezone wall-clock values.
wxLongLong DaysFromCivil(int y, int m, int d) {
  y -= (m <= 2) ? 1 : 0;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = static_cast<unsigned>(y - era * 400);
  unsigned mp = static_cast<unsigned>(m + (m > 2 ? -3 : 9));
  unsigned doy = (153 * mp + 2) / 5 + static_cast<unsigned>(d) - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return wxLongLong(era) * 146097 + static_cast<long>(doe) - 719468;
}

wxDateTime MakeUtcDateTime(int year, int month1to12, int day, int hour,
                            int minute, int second) {
  wxLongLong days = DaysFromCivil(year, month1to12, day);
  wxLongLong secs =
      days * 86400 + hour * 3600 + minute * 60 + second;
  return wxDateTime(static_cast<time_t>(secs.ToLong()));
}

bool TimeUnitToSeconds(int indicator, long* seconds_per_unit) {
  switch (indicator) {
    case 0: *seconds_per_unit = 60; return true;         // Minute.
    case 1: *seconds_per_unit = 3600; return true;        // Hour.
    case 2: *seconds_per_unit = 86400; return true;       // Day.
    case 10: *seconds_per_unit = 3 * 3600; return true;   // 3 hours.
    case 11: *seconds_per_unit = 6 * 3600; return true;   // 6 hours.
    case 12: *seconds_per_unit = 12 * 3600; return true;  // 12 hours.
    case 13: *seconds_per_unit = 1; return true;          // Second.
    default: return false;
  }
}

// Unpacks a Data Representation Template 5.0 (simple packing) field into
// total_points values, row-major. bitmap is null when section 6 said every
// point is present; otherwise a NaN marks a point the bitmap excluded, and
// a truncated data section leaves the remaining points NaN.
std::vector<double> UnpackSimple(const unsigned char* data, size_t data_len,
                                  int num_bits, float ref_value,
                                  int bin_scale, int dec_scale,
                                  const unsigned char* bitmap,
                                  size_t total_points) {
  std::vector<double> out(total_points,
                           std::numeric_limits<double>::quiet_NaN());
  const double bin_factor = std::pow(2.0, bin_scale);
  const double dec_factor = std::pow(10.0, -dec_scale);

  if (num_bits == 0) {
    const double y = ref_value * dec_factor;
    for (size_t i = 0; i < total_points; ++i) {
      if (bitmap && !(bitmap[i / 8] & (0x80 >> (i % 8)))) continue;
      out[i] = y;
    }
    return out;
  }

  size_t bit_pos = 0;
  for (size_t i = 0; i < total_points; ++i) {
    if (bitmap && !(bitmap[i / 8] & (0x80 >> (i % 8)))) continue;
    uint64_t raw = 0;
    for (int b = 0; b < num_bits; ++b) {
      size_t byte_idx = bit_pos / 8;
      if (byte_idx >= data_len) return out;  // Truncated data; bail.
      int bit_idx = 7 - static_cast<int>(bit_pos % 8);
      raw = (raw << 1) | static_cast<uint64_t>((data[byte_idx] >> bit_idx) & 1);
      ++bit_pos;
    }
    out[i] = (ref_value + static_cast<double>(raw) * bin_factor) * dec_factor;
  }
  return out;
}

}  // namespace

GribReader::GribReader(const wxFileName& path) : m_path(path) { Load(); }

void GribReader::Load() {
  if (!m_path.IsOk() || !m_path.FileExists()) return;

  wxFile file(m_path.GetFullPath());
  if (!file.IsOpened()) return;
  wxFileOffset len = file.Length();
  if (len <= 0) return;

  std::vector<unsigned char> buf(static_cast<size_t>(len));
  if (file.Read(buf.data(), buf.size()) != len) return;
  file.Close();

  const size_t total = buf.size();
  size_t pos = 0;

  while (pos + 16 <= total) {
    if (buf[pos] != 'G' || buf[pos + 1] != 'R' || buf[pos + 2] != 'I' ||
        buf[pos + 3] != 'B') {
      break;
    }
    int discipline = buf[pos + 6];
    int edition = buf[pos + 7];
    if (edition != 2) break;  // GRIB1 or unknown; this reader is GRIB2 only.

    uint64_t msg_len = 0;
    for (int i = 0; i < 8; ++i) msg_len = (msg_len << 8) | buf[pos + 8 + i];
    if (msg_len < 16 || pos + msg_len > total) break;
    const size_t msg_end = pos + static_cast<size_t>(msg_len);

    // Fields accumulated across the section-3..7 group(s) inside this
    // message. Grid (section 3) can be shared by more than one field, so
    // it persists across groups; the rest resets after each section 7.
    bool grid_ok = false;
    int ni = 0, nj = 0;
    double la1 = 0, lo1 = 0, di = 0, dj = 0;
    bool i_increasing = true, j_increasing = false;

    bool have_pdt = false;
    int category = 0, number = 0, level_type = 0;
    double level_value = 0.0;
    wxDateTime valid_time;

    bool have_drt = false;
    float ref_value = 0.0f;
    int bin_scale = 0, dec_scale = 0, num_bits = 0;

    bool bitmap_supported = true;
    bool no_bitmap = true;
    const unsigned char* bitmap_ptr = nullptr;

    size_t sp = pos + 16;
    while (sp + 4 <= msg_end) {
      if (buf[sp] == '7' && buf[sp + 1] == '7' && buf[sp + 2] == '7' &&
          buf[sp + 3] == '7') {
        break;  // Section 8, end of message.
      }
      if (sp + 5 > msg_end) break;
      uint32_t sect_len = ReadU(buf, sp, 4);
      if (sect_len < 5 || sp + sect_len > msg_end) break;
      int sect_num = buf[sp + 4];

      switch (sect_num) {
        case 1: {
          if (sect_len >= 19) {
            int year = static_cast<int>(ReadU(buf, sp + 12, 2));
            int month = buf[sp + 14];
            int day = buf[sp + 15];
            int hour = buf[sp + 16];
            int minute = buf[sp + 17];
            int second = buf[sp + 18];
            if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
              valid_time = MakeUtcDateTime(year, month, day, hour, minute,
                                            second);
            }
          }
          break;
        }
        case 3: {
          grid_ok = false;
          if (sect_len >= 72 && ReadU(buf, sp + 12, 2) == 0) {
            // Grid Definition Template 3.0: regular lat/lon grid.
            ni = static_cast<int>(ReadU(buf, sp + 30, 4));
            nj = static_cast<int>(ReadU(buf, sp + 34, 4));
            la1 = ReadS(buf, sp + 46, 4) * 1e-6;
            lo1 = ReadS(buf, sp + 50, 4) * 1e-6;
            di = ReadU(buf, sp + 63, 4) * 1e-6;
            dj = ReadU(buf, sp + 67, 4) * 1e-6;
            int scan_mode = buf[sp + 71];
            i_increasing = !(scan_mode & 0x80);
            j_increasing = (scan_mode & 0x40) != 0;
            bool row_major = !(scan_mode & 0x20);
            bool consistent_scan = !(scan_mode & 0x10);
            grid_ok = ni > 0 && nj > 0 && di > 0.0 && dj > 0.0 && row_major &&
                      consistent_scan;
          }
          break;
        }
        case 4: {
          have_pdt = false;
          if (sect_len >= 34 && ReadU(buf, sp + 7, 2) == 0) {
            // Product Definition Template 4.0: point-in-time field at a
            // horizontal level.
            category = buf[sp + 9];
            number = buf[sp + 10];
            int unit_indicator = buf[sp + 17];
            int32_t forecast_time = static_cast<int32_t>(ReadU(buf, sp + 18, 4));
            level_type = buf[sp + 22];
            int level_scale = ReadS(buf, sp + 23, 1);
            int32_t level_scaled = ReadS(buf, sp + 24, 4);
            level_value = level_scaled / std::pow(10.0, level_scale);

            long unit_seconds = 0;
            if (valid_time.IsValid() &&
                TimeUnitToSeconds(unit_indicator, &unit_seconds)) {
              valid_time = valid_time +
                            wxTimeSpan::Seconds(forecast_time * unit_seconds);
              have_pdt = true;
            }
          }
          break;
        }
        case 5: {
          have_drt = false;
          if (sect_len >= 21 && ReadU(buf, sp + 9, 2) == 0) {
            // Data Representation Template 5.0: simple packing.
            ref_value = ReadFloatBE(buf, sp + 11);
            bin_scale = ReadS(buf, sp + 15, 2);
            dec_scale = ReadS(buf, sp + 17, 2);
            num_bits = buf[sp + 19];
            have_drt = num_bits <= 32;
          }
          break;
        }
        case 6: {
          int indicator = buf[sp + 5];
          if (indicator == 255) {
            no_bitmap = true;
            bitmap_ptr = nullptr;
          } else if (indicator == 0) {
            no_bitmap = false;
            bitmap_ptr = buf.data() + sp + 6;
            size_t bitmap_bytes = sect_len - 6;
            size_t needed = grid_ok
                                ? (static_cast<size_t>(ni) * nj + 7) / 8
                                : 0;
            if (bitmap_bytes < needed) {
              // Malformed/truncated bitmap; safer to treat as absent than
              // to read past the section.
              no_bitmap = true;
              bitmap_ptr = nullptr;
            }
          } else {
            // Predefined bitmap by reference -- not supported.
            bitmap_supported = false;
          }
          break;
        }
        case 7: {
          if (grid_ok && have_pdt && have_drt && bitmap_supported) {
            const unsigned char* data_ptr = buf.data() + sp + 5;
            size_t data_len = sect_len - 5;
            size_t total_points = static_cast<size_t>(ni) * nj;

            Message msg;
            msg.discipline = discipline;
            msg.category = category;
            msg.number = number;
            msg.level_type = level_type;
            msg.level_value = level_value;
            msg.valid_time = valid_time;
            msg.grid_ok = true;
            msg.ni = ni;
            msg.nj = nj;
            msg.la1 = la1;
            msg.lo1 = lo1;
            msg.di = di;
            msg.dj = dj;
            msg.i_increasing = i_increasing;
            msg.j_increasing = j_increasing;
            msg.values = UnpackSimple(data_ptr, data_len, num_bits, ref_value,
                                       bin_scale, dec_scale,
                                       no_bitmap ? nullptr : bitmap_ptr,
                                       total_points);
            msg.data_ok = true;
            m_messages.push_back(std::move(msg));
          }
          // Reset per-field state; grid (section 3) may be reused by a
          // following field group within the same message.
          have_pdt = false;
          have_drt = false;
          bitmap_supported = true;
          no_bitmap = true;
          bitmap_ptr = nullptr;
          break;
        }
        default:
          break;  // Sections 0, 2, and anything unrecognized: skip.
      }

      sp += sect_len;
    }

    pos = msg_end;
  }
}

const GribReader::Message* GribReader::FindNearest(
    int discipline, int category, int number, bool require_10m_level,
    const wxDateTime& time) const {
  const Message* best = nullptr;
  double best_time_diff = 0.0;
  double best_level = 0.0;

  for (const auto& msg : m_messages) {
    if (!msg.grid_ok || !msg.data_ok) continue;
    if (msg.discipline != discipline || msg.category != category ||
        msg.number != number) {
      continue;
    }
    if (require_10m_level) {
      if (msg.level_type != 103) continue;
      if (std::fabs(msg.level_value - 10.0) > 0.5) continue;
    }
    if (!msg.valid_time.IsValid() || !time.IsValid()) continue;

    double diff =
        std::fabs((msg.valid_time - time).GetSeconds().ToDouble());
    if (!best || diff < best_time_diff ||
        (diff == best_time_diff && msg.level_value < best_level)) {
      best = &msg;
      best_time_diff = diff;
      best_level = msg.level_value;
    }
  }
  return best;
}

bool GribReader::SampleAt(const Message& msg, double lat, double lon,
                           double* out_value) {
  double lon_diff = NormalizeDegrees(lon) - NormalizeDegrees(msg.lo1);
  while (lon_diff > 180.0) lon_diff -= 360.0;
  while (lon_diff < -180.0) lon_diff += 360.0;
  double effective_di = msg.i_increasing ? msg.di : -msg.di;
  if (effective_di == 0.0) return false;
  int col = static_cast<int>(std::lround(lon_diff / effective_di));

  double effective_dj = msg.j_increasing ? msg.dj : -msg.dj;
  if (effective_dj == 0.0) return false;
  int row = static_cast<int>(std::lround((lat - msg.la1) / effective_dj));

  if (col < 0 || col >= msg.ni || row < 0 || row >= msg.nj) return false;

  double v = msg.values[static_cast<size_t>(row) * msg.ni + col];
  if (std::isnan(v)) return false;
  *out_value = v;
  return true;
}

EnvSample GribReader::LookupWind(double lat, double lon,
                                  const wxDateTime& time) const {
  EnvSample result;
  const Message* u_msg = FindNearest(0, 2, 2, /*require_10m_level=*/true, time);
  const Message* v_msg = FindNearest(0, 2, 3, /*require_10m_level=*/true, time);
  if (!u_msg || !v_msg) return result;

  double u, v;
  if (!SampleAt(*u_msg, lat, lon, &u) || !SampleAt(*v_msg, lat, lon, &v)) {
    return result;
  }

  result.available = true;
  result.speed_kt = std::sqrt(u * u + v * v) * kMetersPerSecondToKnots;
  result.direction_deg =
      NormalizeDegrees(std::atan2(-u, -v) * 180.0 / M_PI);  // Blowing FROM.
  return result;
}

EnvSample GribReader::LookupCurrent(double lat, double lon,
                                     const wxDateTime& time) const {
  EnvSample result;
  const Message* u_msg =
      FindNearest(10, 1, 2, /*require_10m_level=*/false, time);
  const Message* v_msg =
      FindNearest(10, 1, 3, /*require_10m_level=*/false, time);
  if (!u_msg || !v_msg) return result;

  double u, v;
  if (!SampleAt(*u_msg, lat, lon, &u) || !SampleAt(*v_msg, lat, lon, &v)) {
    return result;
  }

  result.available = true;
  result.speed_kt = std::sqrt(u * u + v * v) * kMetersPerSecondToKnots;
  result.direction_deg =
      NormalizeDegrees(std::atan2(u, v) * 180.0 / M_PI);  // Setting TOWARD.
  return result;
}
