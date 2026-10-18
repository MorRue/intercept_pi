# Changelog

All notable changes to the Intercept plugin. This project follows
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Leeway on/off: with a GRIB file, "Leeway: include" chooses whether the
  target drifts with the current plus leeway, or with the current alone for
  an operator who doesn't trust the unverified leeway figures. With leeway
  off the uncertainty ring grows so that it also contains the leeway
  estimate's ring -- switching the model off never makes the result look
  more certain.
- Uncertainty circle: a ring around the "Estimated position" mark, drawn by
  OpenCPN as a range ring on that mark, shows how far from it the target may
  be. The radius is IAMSAR's total probable error of position (reported
  position error plus a drift error that grows with time and with missing
  wind or current data) plus an allowance for leeway diverging to either
  side of downwind. The panel shows it as "Uncertainty radius". Checked
  against the published IAMSAR worked example; the default error values are
  unverified (see `docs/LEEWAY_NEEDS_VERIFICATION.md`).
- Automatic recalculation: "Auto-recalculate every N min" (1-60) at the
  bottom of the Result box repeats Recalculate on a timer, with the live GPS
  fix and the current time, and keeps updating the chart while the panel is
  closed. A "Last calculated" time shows when it last ran. An automatic run
  never opens a dialog: if an input is invalid, or a position is still being
  typed, it skips that run, keeps the last result on the chart and shows the
  reason in red under the row.
- Position inputs also accept degree, minute and second marks
  (`45° 30.5' N`, `45° 30' 30" N`), so a position copied from OpenCPN
  (for example from a mark's properties) can be pasted straight in.
- Unit tests for position parsing (`test_position_parse`) -- the parser had
  none before.

### Fixed
- The panel no longer mixes languages: a few labels that happen to match
  OpenCPN's own texts ("Own ship", "Clear") were shown in OpenCPN's
  translation, for example German, while the rest stayed English. The
  plugin now only uses its own translations, so it is consistently English
  until it gets a translation of its own.
- The intercept is found whenever own ship can reach the target, also when
  the target is the faster of the two but drifting toward or across own
  ship's path; the solver used to give up whenever the target was faster.
  When there is no intercept, no "Intercept" mark is drawn on top of the
  estimated position any more, the course line is labelled "Toward
  estimated position (no intercept)", and the ETA reads "-- (no intercept)"
  instead of a time to a place the target will have left.
- Recalculating (by hand, automatically, or by toggling a display checkbox)
  no longer cancels navigation: if OpenCPN is navigating along the "Course
  to steer" route or to one of the plugin's marks, that object is left as it
  is, and the panel says so. Before, it was deleted and re-added, which
  OpenCPN treats as deactivating the route.

### Changed
- The toolbar button is redrawn in the style of OpenCPN's own toolbar
  icons (24x24, light-grey lines on a dark square, a lighter square when
  the panel is open). It used to be dark lines on a transparent background,
  hard to see on the dark toolbar, and red when toggled. It now also stays
  pressed (showing the toggled icon) while the panel is open; it was
  registered as a plain button, so it never showed its toggled state.
- The chart mark at the reported position is labelled "Reported position"
  instead of "Target" -- it marks where the target was reported, not where
  it is.
- Own ship: the speed's "lock" checkbox is now "use GPS speed", with a
  read-only field next to it showing the speed OpenCPN currently gets from
  the GPS ("--" when there is none), so you can see whether it is worth
  using or enter a speed by hand instead. The position's "lock" checkbox is
  now "use GPS position".
- The panel never opens taller or wider than the screen it is on; on a small
  screen (a 1366x768 laptop, a 7" chart-plotter display) its contents scroll
  instead of running off the bottom edge. Scrolling with the mouse wheel
  over a number field or the craft dropdown scrolls the panel rather than
  changing the value, unless you have clicked into that field.
- Positions in the panel (estimated position, intercept point, and the
  own-ship field when it is unlocked) are shown in the latitude/longitude
  format chosen in OpenCPN's options -- degrees and decimal minutes,
  decimal degrees, or degrees-minutes-seconds -- exactly as OpenCPN shows
  positions everywhere else. Before, they were always degrees and decimal
  minutes.

## [0.1.0-alpha] — 2026-09-04

First public build. Alpha: functional, but the drift model is unreviewed
and the Windows build has not been run inside a real OpenCPN yet.

### Added
- Non-modal input/output panel (`InterceptPanel`) toggled from the toolbar;
  OpenCPN stays interactive while it is open.
- Reported position parsing (decimal degrees, degrees-minutes,
  degrees-minutes-seconds) and time of report.
- Datum ageing: the reported position drifted forward to the current time
  using leeway + total water current, summed as vectors. Environment from an
  operator-selected GRIB2 file (10 m wind and/or surface current) or a
  hand-entered set and drift; zero drift with neither.
- Moving-target intercept solve: the course to steer leads the drifting
  target to the point where own-ship meets it, with distance and ETA.
- Own-ship position and speed from OpenCPN's GPS fix or entered by hand,
  each independently.
- Chart objects: reported-position mark, estimated-position mark, intercept
  mark, target-drift track, and an activatable course-to-steer route, each
  independently show/hide-able.
- Standalone unit tests for coordinate parsing, datum ageing, the GRIB2
  reader, the intercept solve, output formatting, and a leeway/datum
  reference check against published SAR figures.

### Notes
- The drift model is **not verified for operational use** — see
  `docs/LEEWAY_NEEDS_VERIFICATION.md`.
- The Windows build passes CI but has not yet been loaded into a real
  OpenCPN; the `WX_DEFINE_LIST` workaround for the plugin API's route list
  is compile-tested only.

[Unreleased]: https://github.com/MorRue/intercept_pi/compare/v0.1.0-alpha...HEAD
[0.1.0-alpha]: https://github.com/MorRue/intercept_pi/releases/tag/v0.1.0-alpha
