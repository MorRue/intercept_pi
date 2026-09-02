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
- **Linux CI builds in a `debian:bookworm` container** (glibc 2.36) so the
  artifact loads on more crew machines than one built on the newer CI host or
  a dev box. Never ship a locally built `.so` — release artifacts come from CI.
- **GPLv3+**, inherited from the template.

## State

Working skeleton, confirmed loading in OpenCPN 5.8.4 on Linux — it appears
in Options > Plugins and its toolbar button renders. It subscribes to
`WANTS_NMEA_EVENTS` for own-ship position and displays that position on
click. This is a placeholder that proves the toolbar wiring and the position
feed — the two things every later feature depends on.

`src/` is `intercept_pi.{h,cpp}`, `case_dialog.{h,cpp}` (the case-intake
dialog, merged), and the `plug_utils` icon helpers. ShipDriver's AIS maker,
GRIB record classes and simulator GUI were deleted; the GRIB classes come
back with the drift work (see "Next").

## Planned direction (IAMSAR Vol. III mechanics)

1. Case intake: position (DD/DDM/DMS), time of report, craft type, POB — done
2. Datum: age the position forward using surface current + leeway
   (Allen & Plourde coefficients). With no environmental data at all,
   datum = reported position. **The leeway model in `src/datum_age.cpp` is
   NOT verified — see `docs/LEEWAY_NEEDS_VERIFICATION.md`; `kLeewayRubber = 0.36`
   is almost certainly ~10× too large.** Do not tighten it or build on its
   numbers without a human checking them against IAMSAR / Allen & Plourde.
3. Uncertainty radius: position error + drift error
4. Intercept: course onto a *moving* datum, ETA that updates as it drifts.
   **Leeway drift is optional in this calculation.** When wind/current data is
   available, compute the datum both *with* and *without* leeway drift, and let
   the operator toggle (a checkbox) which one the displayed intercept route
   uses — so an operator who doesn't trust the unverified leeway model can fall
   back to current-only, and with no environmental data at all the route is
   just bearing-to-reported-position. Wind and drift are both optional inputs.
5. Search patterns: expanding square, sector, parallel track, as routes

**Wind/current source — decided: an optional operator-selected GRIB file.**
The case dialog has a "GRIB file" picker. If a file is given, datum ageing
reads 10 m wind and surface current from it at the datum position and time.
If none is given, the operator may enter set & drift by hand, and every step
downstream must still work with no environmental input (treat drift as zero,
widen the uncertainty radius to say so). Chosen for: standard format, works
offline with a GRIB downloaded ashore, no dependency on `grib_pi` being
installed, no credentials. Querying `grib_pi` over the JSON message bus
remains a possible *additional* source later. Direct CMEMS/Copernicus fetch
is rejected — credentials and offline caching for little gain over a
pre-downloaded GRIB.

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
target triggers. Build the default target (or `tarball`). Any tool that lists
targets will offer `intercept_pi` — it is not a usable entry point.

**Editing a `CACHE` variable in `Plugin.cmake` does not affect an existing
build tree.** `set(... CACHE STRING ...)` only assigns when the cache has no
value, so `CMakeCache.txt` wins on every reconfigure. Either pass `-D` to
override it, or delete the cache:

    cmake -B build -DOCPN_TEST_REPO=MorRue/intercept-alpha

Check what is actually in force with `grep OCPN_ build/CMakeCache.txt` — the
configure output prints the selected upload repository too. Do this in every
build tree; each carries its own cache.

Use a separate build tree per configuration. Only a Debug tree carries
`debug_info`, so build and install from that one when attaching a debugger:

    cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-debug -j$(nproc)
    ./scripts/install-local.sh build-debug

`build-*` is gitignored, so any number of trees can coexist. **A build tree
cannot be moved or renamed** — CMake stores absolute paths in the cache and
refuses to reconfigure. If this directory is ever renamed, delete the build
trees and configure again; git itself survives a rename untouched.

## Open items

- **CI** (`build.yml`): Linux builds in a `debian:bookworm` container (wx 3.2
  in apt) on every push and PR. Windows (MSVC Win32, prebuilt wxWidgets 3.2.6,
  gettext from Poedit) also runs on every push/PR for now — it's ~2 min with
  the wx binaries cached, but it counts 2× against the private-repo Actions
  budget, so consider narrowing it to tags + `workflow_dispatch` if minutes
  get tight.
- **This will be developed on a sandbox machine** that reaches GitHub through
  a per-repository *deploy key*. Deploy keys authenticate git over SSH but not
  the GitHub API, so `gh` cannot work there at all — no pull requests, no
  issues. Anything needing the API happens on the Mac. See the `homelab` repo,
  ADR-0009.
- The `OCPN_*_REPO` values in `Plugin.cmake` name Cloudsmith repositories
  under `MorRue/` that do not exist yet. Harmless: the upload step only runs
  when CI has Cloudsmith credentials.
- `po/` still holds the template's translations. Harmless — unmatched
  strings fall back to English — but stale.

## Next

1. **Retro-review follow-up on PR #5's datum-ageing code.** The local reviewer
   raised three HIGH flags (`homelab/orchestrator/runs/needs-human/pr5.md`).
   Reading the merged code, two are weak — `manual.available` *is* checked
   before use, and the null-`grib` path *is* the handled no-file case — and
   `atan2(0,0)` is defined (returns 0), not UB. What is actually worth doing,
   code only:
   - **Validate `ManualSetAndDrift` values**, not just its `available` flag:
     a negative `drift_kt` or an out-of-range `set_deg` currently flows
     straight into the integration. Clamp `drift_kt` at 0 and normalise
     `set_deg` to [0,360) in `ComputeAgedDatum` (or reject → drift 0).
   - **Guard `CombineVectors` against a zero resultant** — return
     `speed 0, dir 0` when `x == 0 && y == 0` rather than depending on the
     platform `atan2(0,0)`.
   - **Widen `test_datum_age.cpp`.** It has 3 closed-form cases; add: zero
     elapsed time, a southward (bearing ~180°) manual drift, a high-latitude
     (>70°) start, and current+leeway that cancel (exercises the
     `CombineVectors` zero guard).
   Keep the public API unchanged. **Do not touch the leeway coefficients or the
   `wind + 180°` direction — those are unverified domain questions for a human
   (`docs/LEEWAY_NEEDS_VERIFICATION.md`), not part of this task.**

2. **Uncertainty radius** (Planned direction #3): position error + drift error
   around the aged datum. Drift error should widen when there is no
   environmental input, per the "treat drift as zero, widen the radius to say
   so" rule in Planned direction #2.

*Landed:* datum ageing (PR #5) — `ComputeAgedDatum` / `FinalizeDatum` on the
`Case`, GRIB and manual set & drift as sources, drift optional. Correctness of
the leeway model itself is still unverified — see `docs/LEEWAY_NEEDS_VERIFICATION.md`.
