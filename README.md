# Intercept — an OpenCPN plugin

Computes the course to steer to reach a reported position — including the
case where that position is itself drifting with wind and current, so the
bearing to the last known position is not the course to steer.

Enter the reported position and time; optionally a GRIB file (for wind and
surface current) or a hand-entered set and drift; and own-ship position and
speed. The plugin ages the position forward to now, solves the
moving-target intercept, and shows the course to steer, distance, and ETA,
drawing the marks and the course line on the chart.

## ⚠️ Disclaimer

**This software is not verified for operational use.** The drift model has
not been reviewed by anyone with search-and-rescue domain expertise — see
[`docs/LEEWAY_NEEDS_VERIFICATION.md`](docs/LEEWAY_NEEDS_VERIFICATION.md).
It is provided **without any warranty** (see [COPYING](COPYING), sections 15
and 16). Do not rely on it for navigation or for any life-safety decision.

Not affiliated with or endorsed by the IMO, ICAO, the U.S. Coast Guard, the
OpenCPN project, or any search-and-rescue authority.

## Status

**v0.1.0-alpha** — a working plugin with a non-modal input/output panel,
datum ageing (GRIB or manual drift), a moving-target intercept solve, and
chart marks and a course-to-steer route. Alpha: the drift model is
unreviewed and the Windows build has not been run inside a real OpenCPN
yet.

## Installing

Not in the OpenCPN plugin catalogue yet. Download the tarball for your
platform from the
[releases page](https://github.com/MorRue/intercept_pi/releases) and import
it through **Options → Plugins → Import plugin…** — full walkthrough with
screenshots, and how to remove it, in
[**docs/INSTALLING.md**](docs/INSTALLING.md).

Needs OpenCPN 5.8 or newer (see Platform support below).

## Building

Requires wxWidgets 3.2, CMake and a C++ compiler.

    git submodule update --init --recursive
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc) --target tarball

Release artifacts come from CI, never a local build — a locally built
library links against your machine's libraries and may not load elsewhere.
For a fast local test you can skip the plugin manager and copy the library
straight in (`./scripts/install-local.sh`); OpenCPN then treats it as
unmanaged, so there is no Uninstall button — see
[docs/INSTALLING.md](docs/INSTALLING.md#for-developers-quick-local-test-without-the-plugin-manager).

## Platform support

Linux and Windows, built in CI on every push. The Windows build is 32-bit
because OpenCPN's official Windows binary is 32-bit; a 64-bit plugin will
not load.

The plugin targets plugin API 1.18, so it loads on OpenCPN 5.8 and later.

## Licence

GPL-3.0-or-later — see [COPYING](COPYING) / [LICENSE](LICENSE).

Derived from the OpenCPN plugin template by Mike Rossiter and the OpenCPN
developers. Bundled and referenced third-party components, and their
licences, are listed in [THIRD-PARTY.md](THIRD-PARTY.md).
