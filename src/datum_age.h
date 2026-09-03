/******************************************************************************
 * Intercept plugin for OpenCPN -- datum ageing.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_DATUM_AGE_H__
#define INTERCEPT_PI_DATUM_AGE_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/datetime.h>

class GribReader;

/**
 * Operator-entered set (true direction the current sets TOWARD) and drift
 * (speed in knots), used as the environmental source when no GRIB file is
 * available. available is false -- the "not supplied" sentinel -- when the
 * operator left this blank.
 */
struct ManualSetAndDrift {
  bool available = false;
  double set_deg = 0.0;
  double drift_kt = 0.0;
};

/**
 * Allen & Plourde-style leeway coefficients for one craft category:
 * downwind leeway speed (kt) = speed_pct_of_wind * 10 m wind speed (kt) +
 * speed_constant_kt. The figures LookupLeewayCoefficients() returns are
 * representative approximations for two broad categories (rubber-hulled
 * vs. wooden-hulled) meant to make the ageing model directionally correct
 * -- a rubber/inflatable hull has much higher windage relative to its
 * underwater profile than a wooden displacement hull. Verify against the
 * published Allen & Plourde (2000) table before relying on this
 * operationally; a later milestone may need a finer-grained table.
 */
struct LeewayCoefficients {
  double speed_pct_of_wind = 0.0;
  double speed_constant_kt = 0.0;
};

/**
 * Looks up leeway coefficients by craft_type, matched case-insensitively
 * against "rubber"/"wooden" so it survives the input labels changing or
 * (per CLAUDE.md, po/ being stale) an unmatched translation falling back
 * to English. A craft_type matching neither gets the rubber-hulled
 * (higher-leeway) coefficients -- the more cautious assumption for a
 * search when the craft is unknown.
 */
LeewayCoefficients LookupLeewayCoefficients(const wxString& craft_type);

/** Result of ComputeAgedDatum: the drifted position and how long it drifted for. */
struct AgedDatum {
  double lat = 0.0;
  double lon = 0.0;
  wxTimeSpan elapsed;
};

/**
 * Ages a reported position forward from time_of_report to now by
 * integrating surface current and wind-driven leeway in fixed time steps.
 *
 * Environmental source, in priority order:
 *  1. grib (non-null): at each step, LookupWind()/LookupCurrent() at the
 *     current datum position and time. Leeway is derived from the wind
 *     sample via craft_type's coefficients, drifting downwind (wind FROM
 *     + 180 deg); current is used as looked up. A lookup that comes back
 *     unavailable at a step contributes zero rather than aborting the run
 *     -- e.g. a GRIB whose grid does not cover the current position.
 *  2. manual (available), used only when grib is null: a constant total
 *     drift vector for every step. No separate leeway is added on top of
 *     it, since there is no wind sample to derive one from.
 *  3. Neither: zero drift; the aged datum equals the reported position.
 *
 * time_of_report or now not valid, or now not after time_of_report,
 * returns the reported position unchanged with zero elapsed time.
 */
AgedDatum ComputeAgedDatum(double reported_lat, double reported_lon,
                           const wxDateTime& time_of_report,
                           const wxDateTime& now, const wxString& craft_type,
                           const GribReader* grib,
                           const ManualSetAndDrift& manual);

/**
 * Vector sum of two (speed, true direction the vector points TOWARD)
 * pairs, e.g. surface current and wind-driven leeway. Exposed for unit
 * testing (the zero-resultant guard, in particular); not meant as a
 * general-purpose API outside datum ageing.
 */
void CombineVectors(double speed1_kt, double dir1_deg, double speed2_kt,
                     double dir2_deg, double* out_speed_kt,
                     double* out_dir_deg);

#endif  // INTERCEPT_PI_DATUM_AGE_H__
