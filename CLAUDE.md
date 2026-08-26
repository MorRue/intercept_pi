# Intercept — OpenCPN plugin

## What this is

An OpenCPN plugin that computes the course to steer to reach a reported
position. Built for civil search and rescue in the Mediterranean: a distress
position is reported, and the vessel needs to close it. The bearing to the
last known position is not the course to steer, because the target drifts
between the report and arrival.

The name is deliberately neutral — it describes the mathematics, not the
use case. The GitHub repository is named `intercept_pi` for the same
reason.

## Author background

Comes from Java. C++ and CMake explanations are welcome; programming
fundamentals are not needed. Useful mappings: `Plugin.cmake` is the
`pom.xml` (name, version, source list), the built `.so`/`.dll` is a jar
that OpenCPN `dlopen`s, and `create_pi`/`destroy_pi` are the only exported
symbols — like a `ServiceLoader` entry point.

## Decisions already made, and why

- **Based on `Rasbats/shipdriver_pi`**, the community plugin template. Its
  git history was dropped; this is a fresh repo. `update-templates` can pull
  upstream template improvements — its "shipdriver" references are correct,
  they name the upstream remote.
- **`api-18`, not the newest (`api-21`).** OpenCPN loads any plugin whose
  API is ≤ its own, so an older API runs on more installs. Nothing planned
  needs newer callbacks. Ubuntu ships OpenCPN 5.8.4; upstream is at 5.14.
- **Windows builds are 32-bit (`-A Win32`).** OpenCPN's official Windows
  binary is 32-bit. A 64-bit plugin will not load.
- **Never cross-compile Windows with MinGW.** OpenCPN is MSVC-built and the
  plugin exports a C++ class with virtual functions; MinGW and MSVC disagree
  on vtable layout, name mangling and exceptions. It builds, then crashes.
- **Linux CI builds on `ubuntu-22.04`, not 24.04**, for an older glibc so the
  artifact loads on more crew machines. Development is on 24.04 — do not ship
  the locally built `.so`.
- **GPLv3+**, inherited from the template.

## State

Working skeleton, confirmed loading in OpenCPN 5.8.4 on Linux — it appears
in Options > Plugins and its toolbar button renders. It subscribes to
`WANTS_NMEA_EVENTS` for own-ship position and displays that position on
click. This is a placeholder that proves the toolbar wiring and the position
feed — the two things every later feature depends on.

`src/` is just `intercept_pi.{h,cpp}` plus the `plug_utils` icon helpers.
ShipDriver's AIS maker, GRIB record classes and simulator GUI were deleted.
The GRIB classes will likely return when drift modelling starts.

## Planned direction (IAMSAR Vol. III mechanics)

1. Case intake: position (DD/DDM/DMS), time of report, craft type, POB
2. Datum: age the position forward using surface current + leeway
   (Allen & Plourde coefficients — rubber vs. wooden boats differ a lot)
3. Uncertainty radius: position error + drift error
4. Intercept: course onto a *moving* datum, ETA that updates as it drifts
5. Search patterns: expanding square, sector, parallel track, as routes

Wind/current source is undecided. Options weighed: query the existing
`grib_pi` over OpenCPN's JSON message bus (no download code, no keys —
this is how `weather_routing_pi` does it), fetch CMEMS/Copernicus directly
(better Mediterranean currents, needs credentials and offline caching), or
manual set & drift entry (crude, works with no connectivity).

## Portability rules

Windows is a target, so: no hardcoded paths (use `GetPluginDataDir()` and
`wxFileName`), no `system()` calls or shelling out, use `wxThread`/
`std::thread` rather than pthreads, size dialogs with sizers and never
absolute pixels, and prefer the `wxDC` render path over OpenGL where there
is a choice.

## Build and test

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc) --target tarball
    ./scripts/install-local.sh      # copies into ~/.local, outside this repo

Then restart OpenCPN and enable under Options > Plugins. Plugin load
failures are silent in the GUI — use `opencpn -l debug` to see them.

**Do not build the `intercept_pi` target on its own** — it fails with
"wx/wxprec.h: No such file". The template configures in two phases and the
wxWidgets include paths are only wired up in the sub-build that the default
target triggers. Build the default target (or `tarball`). This matters when
picking a target in CLion, which offers `intercept_pi` in its list.

**Editing a `CACHE` variable in `Plugin.cmake` does not affect an existing
build tree.** `set(... CACHE STRING ...)` only assigns when the cache has no
value, so `CMakeCache.txt` wins on every reconfigure. Either pass `-D` to
override it, or delete the cache:

    cmake -B build -DOCPN_TEST_REPO=MorRue/intercept-alpha

Check what is actually in force with `grep OCPN_ build/CMakeCache.txt` — the
configure output prints the selected upload repository too. This applies to
CLion's `cmake-build-debug/` as well; use "Reset Cache and Reload Project"
there rather than editing the tree from the shell, since CLion passes its own
`-D` flags.

Use a separate build tree per configuration: `build/` for Release, and
CLion's own `cmake-build-debug/` for Debug. Only the Debug tree carries
`debug_info`, so install from that one when attaching a debugger:

    ./scripts/install-local.sh cmake-build-debug

## Open items

- **The Windows CI job has never run.** `.github/workflows/build.yml` and
  `ci/github-build-win.bat` were written from the AppVeyor script, not
  inherited. Linux is verified locally. Expect a round or two of fixing.
- `gh` is not authenticated — `gh auth login` is interactive and must be run
  by hand. Pushing over HTTPS otherwise needs a personal access token.
- The `OCPN_*_REPO` values in `Plugin.cmake` name Cloudsmith repositories
  under `MorRue/` that do not exist yet. Harmless: the upload step only runs
  when CI has Cloudsmith credentials.
- `po/` still holds the template's translations. Harmless — unmatched
  strings fall back to English — but stale.
