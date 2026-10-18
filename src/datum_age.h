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
 * The craft categories the leeway model distinguishes. The panel maps its
 * dropdown entry to one of these (CraftTypeChoices), so a translated label
 * can't break the lookup.
 */
enum class CraftType { kUnknown, kRubber, kWooden };

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
  // Leeway diverges from downwind by up to +/- this, side unknown.
  double divergence_deg = 0.0;
};

/**
 * Leeway coefficients by craft. Wooden: 4 % of wind, +/-50 deg (the
 * published fishing-vessel example). Rubber and unknown: 3.6 % of wind,
 * +/-30 deg (liferaft range, Allen & Plourde 1999) -- an unknown craft gets
 * the higher-leeway figures, the more cautious assumption for a search.
 * Unverified -- see docs/LEEWAY_NEEDS_VERIFICATION.md.
 */
LeewayCoefficients LookupLeewayCoefficients(CraftType craft);

/** Where ComputeAgedDatum took the drift from. */
enum class DriftSource { kNone, kManual, kGrib };

/** Result of ComputeAgedDatum: the drifted position and how long it drifted for. */
struct AgedDatum {
  double lat = 0.0;
  double lon = 0.0;
  wxTimeSpan elapsed;

  // What the drift was built from -- the uncertainty estimate depends on it.
  DriftSource source = DriftSource::kNone;
  // kGrib only: the GRIB supplied wind / current at every integration step
  // (false when no step ran, since then nothing was sampled).
  // A step without one (grid doesn't cover the position, no such field)
  // drifts on what is left, so that component counts as unknown.
  bool wind_every_step = false;
  bool current_every_step = false;
  // kGrib only: leeway was part of the drift (ComputeAgedDatum's
  // include_leeway). When false the target drifted on current alone, and its
  // leeway counts as unknown in the uncertainty.
  bool leeway_applied = false;
  // How far the leeway component alone moved the target (NM): the length of
  // the summed leeway vector, not of its path. Divergence turns every step's
  // leeway by the same angle, which turns the sum by it too, so this is what
  // the divergence widening scales with -- a wind that backs and undoes its
  // own leeway adds no divergence.
  double leeway_nm = 0.0;
};

/**
 * Ages a reported position forward from time_of_report to now by
 * integrating surface current and wind-driven leeway in fixed time steps.
 *
 * Environmental source, in priority order:
 *  1. grib (non-null): at each step, LookupWind()/LookupCurrent() at the
 *     current datum position and time. Leeway is derived from the wind
 *     sample via craft's coefficients, drifting downwind (wind FROM
 *     + 180 deg); current is used as looked up. A lookup that comes back
 *     unavailable at a step contributes zero rather than aborting the run
 *     -- e.g. a GRIB whose grid does not cover the current position.
 *  2. manual (available), used only when grib is null: a constant total
 *     drift vector for every step. No separate leeway is added on top of
 *     it, since there is no wind sample to derive one from.
 *  3. Neither: zero drift; the aged datum equals the reported position.
 *
 * include_leeway false drops the wind-driven leeway from the GRIB source:
 * the target drifts with the current alone -- for an operator who doesn't
 * trust the (unreviewed) leeway model. No effect on the manual source.
 *
 * time_of_report or now not valid, or now not after time_of_report,
 * returns the reported position unchanged with zero elapsed time.
 */
AgedDatum ComputeAgedDatum(double reported_lat, double reported_lon,
                           const wxDateTime& time_of_report,
                           const wxDateTime& now, CraftType craft,
                           const GribReader* grib,
                           const ManualSetAndDrift& manual,
                           bool include_leeway = true);

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
 * Uncertainty around an aged datum. The water current (TWCe) and the leeway
 * (LWe) each get IAMSAR's default probable error, 0.3 kt (as in the Vol II
 * App. Q example), when the GRIB covers them at every step, and 0.5 kt when
 * it doesn't -- so missing data widens the circle instead of shrinking it.
 * The wind's own error (ASWDVe, 0.3 kt) only adds when the GRIB had wind;
 * without wind data the unknown-leeway term already stands for it. With
 * leeway switched off (AgedDatum::leeway_applied false) leeway counts as
 * unknown too: the target is still pushed by the wind, the estimate just
 * doesn't model it, so the circle must widen, not shrink. A
 * manually entered set & drift is not data: both components stay unknown.
 * initial_error_nm is X.
 */
DatumUncertainty ComputeDatumUncertainty(const AgedDatum& aged,
                                         CraftType craft,
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
