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

/** Where ComputeAgedDatum took the drift from. */
enum class DriftSource { kNone, kManual, kGrib };

/** Result of ComputeAgedDatum: the drifted position and how long it drifted for. */
struct AgedDatum {
  double lat = 0.0;
  double lon = 0.0;
  wxTimeSpan elapsed;

  // What the drift was built from -- the uncertainty estimate depends on it.
  DriftSource source = DriftSource::kNone;
  // kGrib only: the GRIB supplied wind / current at every integration step.
  // A step without one (grid doesn't cover the position, no such field)
  // drifts on what is left, so that component counts as unknown.
  bool wind_every_step = false;
  bool current_every_step = false;
  // Distance covered by the leeway component alone (NM), summed over the
  // steps; what the leeway divergence widening scales with.
  double leeway_nm = 0.0;
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
 * IAMSAR total probable drift velocity error (Vol II App. K):
 * DVe = sqrt(ASWDVe^2 + TWCe^2 + LWe^2), all in knots -- the drift error due
 * to the wind's error, the total water current error and the leeway error.
 */
double TotalDriftVelocityErrorKt(double aswdve_kt, double twce_kt,
                                 double lwe_kt);

/**
 * IAMSAR total probable error of position, E = sqrt(X^2 + De^2) in NM, with
 * X the initial position error and De = drift hours * DVe. IAMSAR's Y term
 * (search facility position error) is left out: there is no search facility
 * in this calculation.
 */
double TotalProbableErrorNm(double initial_error_nm, double drift_error_nm);

/**
 * Leeway diverges from downwind by up to +/- divergence_deg, side unknown,
 * so IAMSAR plots a left and a right datum. ComputeAgedDatum uses pure
 * downwind; both of those datums lie 2 * L * sin(divergence / 2) from it,
 * L being the leeway distance. Adding this to E makes a circle around the
 * pure-downwind datum that still contains both IAMSAR datums' E circles.
 */
double DivergenceOffsetNm(double leeway_nm, double divergence_deg);

/**
 * Leeway divergence angle by craft_type, matched like
 * LookupLeewayCoefficients: "wooden" -> 50 deg (displacement hull; the
 * published fishing-vessel example), anything else -> 30 deg (top of the
 * liferaft range, Allen & Plourde 1999). Unverified -- see
 * docs/LEEWAY_NEEDS_VERIFICATION.md.
 */
double LookupLeewayDivergenceDeg(const wxString& craft_type);

/** How far from the estimated position the target may be. */
struct DatumUncertainty {
  double initial_error_nm = 0.0;  // X
  double dve_kt = 0.0;            // total probable drift velocity error
  double drift_error_nm = 0.0;    // De = drift hours * DVe
  double e_nm = 0.0;              // IAMSAR E = sqrt(X^2 + De^2)
  double divergence_nm = 0.0;     // DivergenceOffsetNm(), 0 without leeway
  double radius_nm = 0.0;         // e_nm + divergence_nm: what to draw
};

/**
 * Uncertainty around an aged datum. Each drift component gets IAMSAR's
 * default probable error when it is known (0.3 kt each for the wind's
 * effect, the current and the leeway, as in the Vol II App. Q example) and
 * a larger one when it is not (0.5 kt for an unknown current, and for
 * unknown leeway), so missing data widens the circle instead of shrinking
 * it. A manually entered set & drift counts as known current + leeway with
 * no separate wind term. initial_error_nm is X.
 */
DatumUncertainty ComputeDatumUncertainty(const AgedDatum& aged,
                                         const wxString& craft_type,
                                         double initial_error_nm);

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
