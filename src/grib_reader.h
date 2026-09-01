/******************************************************************************
 * Intercept plugin for OpenCPN -- GRIB2 wind/current lookup.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_GRIB_READER_H__
#define INTERCEPT_PI_GRIB_READER_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/datetime.h>
#include <wx/filename.h>

#include <vector>

/**
 * One decoded field at a point: 10 m wind (speed + direction the wind
 * blows FROM) or surface current (drift speed + direction the current
 * sets TOWARD), both true degrees, speed in knots. available is false --
 * the "not available" sentinel -- when the file had no matching record,
 * the record used an encoding this reader does not support, or the
 * requested position/time fell outside the grid it did decode.
 */
struct EnvSample {
  bool available = false;
  double speed_kt = 0.0;
  double direction_deg = 0.0;
};

/**
 * Reads 10 m wind and surface current out of a GRIB2 file, for use as an
 * optional environmental source during datum ageing. This is a pure
 * lookup: nearest grid point, nearest record in time to what is asked
 * for. No interpolation, no ageing.
 *
 * Supports the subset of GRIB2 that marine forecast extracts use in
 * practice: a regular latitude/longitude grid (Grid Definition Template
 * 3.0), simple packing (Data Representation Template 5.0), and Product
 * Definition Template 4.0 (instantaneous field at a horizontal level).
 * A record using a different grid, packing, or product template is
 * skipped rather than failing the whole file -- a GRIB can carry other
 * parameters this plugin has no use for.
 */
class GribReader {
public:
  /** Parses path immediately; a missing or unreadable file leaves every
   *  lookup returning "not available" rather than throwing. */
  explicit GribReader(const wxFileName& path);

  EnvSample LookupWind(double lat, double lon, const wxDateTime& time) const;
  EnvSample LookupCurrent(double lat, double lon,
                           const wxDateTime& time) const;

private:
  struct Message {
    int discipline = 0;
    int category = 0;
    int number = 0;
    int level_type = 0;
    double level_value = 0.0;
    wxDateTime valid_time;

    bool grid_ok = false;
    int ni = 0;
    int nj = 0;
    double la1 = 0.0;
    double lo1 = 0.0;
    double di = 0.0;
    double dj = 0.0;
    bool i_increasing = true;
    bool j_increasing = false;

    bool data_ok = false;
    // Row-major, ni*nj values starting at (la1, lo1); NaN marks a point
    // the file's bitmap marked missing.
    std::vector<double> values;
  };

  wxFileName m_path;
  std::vector<Message> m_messages;

  void Load();
  const Message* FindNearest(int discipline, int category, int number,
                              bool require_10m_level,
                              const wxDateTime& time) const;
  static bool SampleAt(const Message& msg, double lat, double lon,
                        double* out_value);
};

#endif  // INTERCEPT_PI_GRIB_READER_H__
