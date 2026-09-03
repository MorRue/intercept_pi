# Changelog

All notable changes to the Intercept plugin. This project follows
[Semantic Versioning](https://semver.org/).

## [Unreleased]

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
