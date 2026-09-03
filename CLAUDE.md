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
   datum = reported position. Method and leeway magnitude are confirmed
   against IAMSAR Vol II & III and pinned by `test/test_reference_iamsar.cpp`;
   the remaining simplifications (two craft buckets, no divergence, single
   default coefficient) still want a SAR-literate review —
   `docs/LEEWAY_NEEDS_VERIFICATION.md`.
3. Uncertainty radius: position error + drift error
4. Intercept: course to steer + range + ETA. The **static form** (steer to the
   datum, no lead angle — the datum already = reported position with no drift
   data) ships first as the usable v0.1; the **moving form** leads the drifting
   datum. **Leeway drift is optional in this calculation.** When wind/current data is
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

**MSVC vs the Linux sandbox.** The code is written and built on Linux
(GCC, lenient) and only compiled for Windows (MSVC, strict) in CI — the
sandbox has no MSVC, so toolchain divergences surface only there. The
recurring one is `<cmath>` constants: MSVC's `<cmath>` provides `M_PI` and
friends only if `_USE_MATH_DEFINES` is set before it is first included.
**A `.cpp` that uses `M_PI` and does not include a wxWidgets header (which
sets it for you) must `#include "portability.h"` first**, before `<cmath>`
or anything pulling it in. Also watch: MSVC narrowing-conversion errors GCC
lets pass, `snprintf`/`strcpy` deprecation-as-error, `#pragma`/`__attribute__`,
POSIX-only calls.

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

The priority is a **usable end-to-end plugin as early as possible**: enter a
case → see a course to steer on the chart. The static-target version (steer to
the datum, which already equals the reported position when there's no drift
data) is genuinely useful for short transits and is what a crew does first
anyway; datum ageing then *refines* it rather than being built in a vacuum.

1. **Course-to-steer computation** (Planned direction #4, static form).
   Add a pure function — `src/intercept.{h,cpp}`, or extend `datum_age` —
   `CourseToSteer(own_lat, own_lon, target_lat, target_lon, own_sog_kt)`
   returning `{ bearing_deg, distance_nm, eta }` (`eta` optional / absent when
   `own_sog_kt` <= 0). Great-circle bearing and distance; ETA = distance /
   SOG. The target is `Case::aged_lat`/`aged_lon` (= the reported position
   when there's no environmental data — so this works with or without a GRIB).
   No lead angle: a stationary datum's course to steer *is* the bearing to it.
   Own-ship position/SOG already arrive via `SetPositionFix()`
   (`m_own_lat` … `m_own_sog`). Unit-test with hand-computed great-circle
   bearings (e.g. due-N, due-E, a diagonal, an antimeridian crossing) and an
   ETA case. Keep the API additive.

2. **Show the course to steer — v0.1** (GUI; small steps). Two parts:
   - **(a) Readout** — after the case dialog OKs, run `FinalizeDatum()` +
     `CourseToSteer()` and show datum lat/lon, bearing, distance, ETA, and
     `elapsed` if the datum was aged. A read-only results dialog. Number
     formatting is pure — unit-test it. Own-ship comes from OpenCPN's live
     fix (`SetPositionFix`), but the case dialog also has optional
     **Own ship** position/speed fields (pre-filled from the live fix,
     editable, blank = use the fix) so you can plan from a hypothetical
     position or with no GPS. Position but no speed → bearing + distance,
     no ETA. No position at all → datum only.
   - **(b) A datum mark on the chart** via `AddSingleWaypoint` (fixed GUID,
     `DeleteSingleWaypoint` before re-add so marks don't pile up, removed on
     `DeInit`). *Done in the v0.1 PR.* A `PlugIn_Route` (activatable course
     *line*, own-ship → datum) was tried first but `Plugin_WaypointList`'s
     node methods are `WX_DECLARE_LIST`'d in `ocpn_plugin.h` and **not**
     exported by the api-18 `opencpn.lib` — `new Plugin_WaypointList` +
     `Append` fails to link on MSVC (LNK2001 `wxPlugin_WaypointListNode::
     DeleteData`). To use a route later, add `WX_DEFINE_LIST(Plugin_WaypointList);`
     in one `.cpp`, or use the `*_Ex` route API. `route_helper.h`'s
     `BuildInterceptWaypoints` is staged for that / for #4's overlay line.
   Do **not** build a `RenderOverlay` custom overlay yet — see #3.
   **This is the usable v0.1.**

3. **Headless overlay smoke-test harness** (infra, do before any custom
   chart drawing). A `RenderOverlay(wxDC&, PlugIn_ViewPort*)` overlay can't
   be verified by `ctest` today, so the loop is blind to it. Set that up:
   - Factor the drawing into a free function
     `DrawInterceptOverlay(wxDC& dc, const OverlayViewport& vp, const OverlayState& s)`
     where `OverlayViewport` is a tiny struct holding just what the drawing
     needs (pixel size, and a `LatLonToPix(lat, lon) -> wxPoint` supplied by
     the caller — OpenCPN's `GetCanvasPixLL` in the plugin, a fake in tests).
   - New CTest target `test_overlay`: render `DrawInterceptOverlay` into a
     `wxBitmap` via `wxMemoryDC`, then assert on pixels — a non-background
     pixel near where the datum projects, line endpoints at expected pixels,
     nothing drawn when `OverlayState` is empty, no crash for an off-screen
     datum. Add `xvfb-run` to the Linux CI step (`sudo apt-get install -y
     xvfb`, wrap the ctest invocation) — `wxBitmap`/`wxMemoryDC` need an X
     connection on Linux. Windows CI can skip this target.
   - This does not catch "looks ugly / wrong colour / labels collide" — that
     stays a human review in OpenCPN — but it catches wrong projection math,
     off-screen draws, crashes, and draw-when-shouldn't, which is most of the
     way there. It pays off across #4 and #6.

4. **Uncertainty radius on the `Case`** (Planned direction #3). Compute the
   total probable error of position `E` around the aged datum, per IAMSAR
   Vol II §4.5 / Appendix K (the "Total probable error of position" and
   "Datum" worksheets; the F/V SAMPLE worked example is Appendix Q). Method:

       E = sqrt(X^2 + De^2)          (drop IAMSAR's Y term — no search
                                      facility in this calculation)
       De = drift_interval_hours * DVe
       DVe = sqrt(LWe^2 + TWCe^2)    (leeway + current velocity errors)

   - `LWe` (leeway probable error) is craft-config dependent — 0.1–0.35 kt
     from the IAMSAR Fig N-2 table in `docs/LEEWAY_NEEDS_VERIFICATION.md`. For
     the two-bucket model use a single conservative value (~0.3 kt); a
     `LookupLeewayError(craft_type)` alongside `LookupLeewayCoefficients` is
     fine.
   - `TWCe`: if the current came from a GRIB, a fixed estimate (~0.3 kt,
     IAMSAR's default for an unverified current); if manual set & drift, same.
   - `X` (initial position error): needs an input. Simplest first cut — a
     fixed small value (e.g. 1 NM) with a `// TODO` to derive it from how the
     position was entered / a dialog field. Don't build the dialog field in
     this task.
   - With **no** environmental input, drift is zero but `De` is *not* — widen
     it to represent "drift unknown" (e.g. assume up to ~0.5 kt over the
     interval). The uncertainty must grow, not vanish, when data is missing.
   - Because the datum uses pure downwind (no divergence), note in a comment
     that `E` should really also cover the ±15–30° leeway divergence fan
     (`docs/LEEWAY_NEEDS_VERIFICATION.md`); a full divergence model is a later
     task, but a rough widening term here is acceptable.

   Store `E` (NM) on the `Case` next to `aged_lat`/`aged_lon`. Unit-test in
   `test_datum_age.cpp` or a new file: the Appendix Q sub-results are clean
   checks — `DVe` = 0.60 kt and `De` = 18.75 h × 0.60 = 11.25 NM. Keep the
   public API additive. Then draw it as a circle around the datum — this is
   the first custom `RenderOverlay` drawing; use the #3 harness (`test_overlay`
   asserts a ring of set pixels at radius `E` around the projected datum).

5. **Intercept onto a *moving* datum** (Planned direction #4, full form): once
   #1–#4 are in, the course to steer leads the target — solve for the point
   where own-ship and the drifting datum coincide, ETA that updates as it
   drifts. Builds directly on #1 by iterating the datum forward.

6. **Search patterns** (Planned direction #5): expanding square, sector,
   parallel track, emitted as OpenCPN routes centred on the datum.

*Landed:*
- Datum ageing (PR #5) — `ComputeAgedDatum` / `FinalizeDatum` on the `Case`,
  GRIB and manual set & drift as sources, drift optional.
- Leeway coefficient fixed `0.36 → 0.036` and pinned by
  `test/test_reference_iamsar.cpp` against IAMSAR Vol II Appendix Q + the
  Fig N-2 leeway band (PR #9). Input validation + `CombineVectors` guard
  (PR #7).
- The datum *method* and the leeway *magnitude* are now both confirmed
  against IAMSAR Vol II & III. Still unconfirmed by a SAR-literate human: the
  two-bucket craft model, the no-divergence simplification, and whether
  `0.036` (vs a more conservative ~0.07) is the right single default — see
  `docs/LEEWAY_NEEDS_VERIFICATION.md`.
