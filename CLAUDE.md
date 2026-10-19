# Intercept — OpenCPN plugin

## What this is

An OpenCPN plugin that computes the course to steer to reach a reported
position, including the case where that position is itself drifting with
wind and current. The bearing to the last known position is not generally
the course to steer, because the target moves between the report and
arrival. The drift model follows the standard SAR datum method — leeway
plus total water current, summed as vectors.

The naming is deliberately generic: it describes the mathematics, not a
particular use case, and the repository is named `intercept_pi` for the
same reason. Not affiliated with or endorsed by any search-and-rescue
authority; **not verified for operational use** — see
`docs/LEEWAY_NEEDS_VERIFICATION.md`.

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
- **Linux CI builds in `debian:bookworm`, `debian:trixie` and
  `ubuntu:26.04` containers.** The tarball is labelled with the system it
  was built on, and OpenCPN's plugin manager only accepts labels it maps to
  the host: Debian 12 builds on Debian 12 and Ubuntu 23.04–24.04 (Mint 22),
  Debian 13 builds only on Debian 13 (`is_debian_plugin_compatible` in
  OpenCPN's `plugin_handler.cpp`, the same from 5.8.4 to 5.14.2; only
  OpenCPN master maps Debian 13 to Ubuntu 26.04). Any other Ubuntu needs a
  build labelled with its exact version — confirmed on a real Ubuntu 26.04
  with OpenCPN 5.14.2, which refused both Debian builds. A Flathub-installed OpenCPN takes only
  a `flatpak-x86_64` build made against its own runtime (the `flatpak`
  job). Never ship a locally built `.so` — release artifacts come from CI.
- **GPLv3+**, inherited from the template.
- **No custom chart drawing.** Everything the plugin shows on the chart is
  an OpenCPN object — marks, routes, tracks, and waypoint range rings —
  never `RenderOverlay` / `RenderGLOverlay`. Custom drawing has to be written
  twice (OpenCPN's OpenGL mode, the default, and its plain-DC mode), shares
  drawing state with OpenCPN and other plugins, and can only be checked in a
  running OpenCPN. A circle is a range ring on a mark; any other shape (an
  ellipse, the leeway divergence fan) is a closed `PlugIn_Track` whose
  points the plugin computes. The headless overlay-harness PR was closed for
  this reason; don't rebuild it.

## State

Loads in OpenCPN 5.8.4. The toolbar button toggles `InterceptPanel` — a
non-modal window carrying the case inputs and the computed course to steer;
[Recalculate] ages the datum, computes bearing/distance/ETA, and draws a
"Datum" mark plus an activatable "Course to steer" route on the chart.

`src/`: `intercept_pi.{h,cpp}` (plugin entry points, `Case`),
`position_parse.{h,cpp}` (typed lat/lon → degrees; standalone, unit-tested
by `test_position_parse`), `intercept_panel.{h,cpp}` (the UI), `datum_age.{h,cpp}` +
`grib_reader.{h,cpp}` (datum ageing), `intercept.{h,cpp}` (course to steer),
`format.{h,cpp}` (output formatting), `route_helper.{h,cpp}`, `portability.h`,
`plug_utils` (icons). ShipDriver's AIS maker and simulator GUI were deleted.

## Planned direction (IAMSAR Vol. III mechanics)

1. Case intake: position (DD/DDM/DMS), time of report, craft type, POB — done
2. Datum: age the position forward using surface current + leeway
   (Allen & Plourde coefficients). With no environmental data at all,
   datum = reported position. Method and leeway magnitude are checked
   against the standard SAR datum method and the Allen & Plourde data, and
   pinned by `test/test_reference_leeway.cpp`; the remaining simplifications
   (two craft buckets, no divergence, single default coefficient) still want
   a SAR-literate review —
   `docs/LEEWAY_NEEDS_VERIFICATION.md`.
3. Uncertainty radius: position error + drift error — done (range ring)
4. Intercept: course to steer + range + ETA. The **static form** (steer to the
   datum, no lead angle — the datum already = reported position with no drift
   data) ships first as the usable v0.1; the **moving form** leads the drifting
   datum. **Leeway drift is optional in this calculation.** When wind/current data is
   available, compute the datum both *with* and *without* leeway drift, and let
   the operator toggle (a checkbox) which one the displayed intercept route
   uses — so an operator who doesn't trust the unverified leeway model can fall
   back to current-only, and with no environmental data at all the route is
   just bearing-to-reported-position. Wind and drift are both optional inputs.
   — **done:** the panel's "Leeway: include" checkbox (`Case::include_leeway`,
   `ComputeAgedDatum(…, include_leeway)`); unticked, the leeway counts as
   unknown in the uncertainty (`AgedDatum::leeway_applied`) and the ring is
   widened to contain the leeway estimate's ring (`leeway_off_nm`) — so the
   ring never shrinks when leeway is switched off.
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

**User-guide screenshots are generated**, not taken by hand:
`scripts/guide-screenshots/make-screenshots.sh` (Linux only — the sandbox;
needs `opencpn xvfb xdotool imagemagick`) installs the built plugin, runs
OpenCPN on a virtual display with a fake GPS, types a demo case into the
panel and writes the 13 pictures `docs/USER_GUIDE.md` uses to
`build/guide-screenshots/`. Copy them to `docs/images/guide/`. Rerun it after
any panel change; its clicks and crops are panel-relative pixel positions, so
a layout change means adjusting them (full-screen shots of each step are in
`build/guide-screenshots/steps/`).

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

- **CI** (`build.yml`): Linux builds in `debian:bookworm`, `debian:trixie`
  and `ubuntu:26.04` containers (wx 3.2 in apt) on every push and PR. The
  **Flatpak** build (flatpak-builder against Flathub's `org.opencpn.OpenCPN`,
  Freedesktop SDK `FLATPAK_SDK` — must match the runtime Flathub's OpenCPN
  uses, and the `sdk:` line in `flatpak/*.yaml`) runs only for `v*` tags and
  `workflow_dispatch`, since it downloads ~1 GB per run. Windows (MSVC
  Win32, prebuilt wxWidgets 3.2.6, gettext pinned by URL + SHA-256 from
  mlocati/gettext-iconv-windows) also runs on every push/PR for now — it's
  ~2 min with
  the wx binaries cached, but it counts 2× against the private-repo Actions
  budget, so consider narrowing it to tags + `workflow_dispatch` if minutes
  get tight. **macOS** (universal arm64 + x86_64, macOS 11+, OpenCPN's
  prebuilt wxWidgets pinned by URL + SHA-256) runs only for `v*` tags and
  `workflow_dispatch`; the release job waits for it. It takes under a minute
  on GitHub's Apple Silicon runners, ~10 billed minutes at the 10× macOS
  rate, so adding it to PRs would be affordable.
- **This will be developed on a sandbox machine** that reaches GitHub through
  a per-repository *deploy key*. Deploy keys authenticate git over SSH but not
  the GitHub API, so `gh` cannot work there at all — no pull requests, no
  issues. Anything needing the API happens on the Mac. See the `homelab` repo,
  ADR-0009.
- The `OCPN_*_REPO` values in `Plugin.cmake` name Cloudsmith repositories
  under `MorRue/` that do not exist yet. Harmless: the upload step only runs
  when CI has Cloudsmith credentials.
- **Translations:** the plugin has none yet (`po/` holds only
  `POTFILES.in`). `_()` is redefined in `src/i18n.h` to look only in the
  plugin's own catalog, `opencpn-intercept_pi` (loaded in `Init()` with
  `AddLocaleCatalog`). wxWidgets' own `_()` searches every loaded catalog,
  OpenCPN's included, so labels that match an OpenCPN string came out in
  OpenCPN's translation ("Own ship" → "Eigenes Schiff") in an otherwise
  English panel. **Every source file that uses `_()` must `#include
  "i18n.h"` after its wx headers.**

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
   - **(a) The panel** (`src/intercept_panel.{h,cpp}`, `InterceptPanel`) — a
     **non-modal `wxFrame`** (float-on-parent, tool window) toggled by the
     toolbar button. OpenCPN stays fully interactive while it's open; the
     `[x]` hides it (state kept), `DeInit` destroys it. Inputs are in
     **`wxStaticBoxSizer` group boxes** (titles set a notch smaller via
     `GetFont().Smaller()`, applied after every box + child exists so only
     the labels shrink), top to bottom: **Own ship** (position, speed),
     **Report** (reported position, time of report), **Target drift** — one
     box with two italic sub-headers, "Explicit drift (ignored when a GRIB
     file is set)" (set, rate → `ComputeAgedDatum`'s `ManualSetAndDrift`)
     and "Environment drift (from a GRIB file…)" (GRIB row **Browse…** /
     **Clear**, craft type default "Unknown", optional POB). Every input has
     a checkbox to its right — checked ⇒ field disabled: the report fields'
     is "lock" (start unlocked); own-ship position's is **"use GPS
     position"** and speed's **"use GPS speed"** (both start ticked, live
     `SetPositionFix` used). A read-only field after "use GPS speed" shows
     the live GPS SOG (`ShowLiveFix()`, called from `SetPositionFix`; "--"
     when OpenCPN has no speed, which it reports as NaN). `UpdateGribLock()`: while a GRIB
     path is set the two explicit-drift fields are disabled (file
     wind+current win).
     Then a **"Result" box** with two italic sub-headers. **Target** —
     estimated position (now), drift source, "moved from report", elapsed.
     **Course to steer** — intercept point, bearing, distance to run, and an
     ETA row that also carries the **[Recalculate]** button (bottom-right,
     same row, no row of its own). Intercept point / bearing / distance / ETA
     come from `Case::intercept_*` when the moving-target solve converged.
     Without an intercept the row says why (speed unknown, or own-ship can't
     catch up), bearing/distance are to the estimated position *now* and say
     so, and ETA is "-- (no intercept)" — the target won't be there. Below the box,
     three **display checkboxes** ("Show reported position", "Show estimated
     position", "Show routes", all on) hide/show chart objects. Every input
     control (and the lock checkboxes, Browse and Recalculate) carries a
     `SetToolTip()`; text position fields also use `SetHint()` placeholders.
     **[Recalculate]** re-runs `FinalizeDatum()` + `SolveIntercept()`, updates
     the output rows in place, and calls
     `intercept_pi::ApplyCase(c, own, show_target, show_estimated, show_lines)`
     to refresh the chart; toggling a display checkbox re-applies the stored
     last case. **Auto-recalculate** (last row of the Result box: checkbox +
     1-60 min spin + "Last calculated" time) drives the same
     `RunRecalculate()` path from a `wxTimer`; it keeps running while the
     panel is hidden. Input errors are returned by `ParsePos()` rather than
     shown by it: a manual run shows a dialog, an automatic run **never**
     does — it skips the tick, keeps the last result, and puts the reason in
     red in the note line (`SetNote`) under the auto row. An automatic tick
     also skips while a position field has focus and unsaved edits
     (`IsModified()`), so a half-typed position is never used.
     `m_recalc_running` skips a tick that lands while a manual error dialog is
     open. `DeInit()` calls `InterceptPanel::Detach()` (stops the timer, nulls
     `m_plugin`) before `Destroy()`, because `Destroy()` only schedules the
     deletion.
     **Never replace an object OpenCPN is navigating with:**
     `UpdateChartObjects` checks `GetActiveRouteGUID()` /
     `GetActiveWaypointGUID()` and leaves the active route (with its
     Intercept mark) or an active mark untouched; `ApplyCase` returns whether
     it did, and the panel says so. Every route-update call in the plugin API
     is delete + add, OpenCPN deactivates a route it deletes, and re-activating
     one (`ActivateRoutePI`) needs API 1.21 — we target 1.18.
     The frame's contents are a `wxScrolledWindow` (`m_content`):
     `FitToScreen()` sizes the window to its contents but caps it at the
     display's client area (they scroll instead) and pulls it back on screen
     — used at construction and after a manual Recalculate
     (`RefreshLayout(false)`). Automatic runs and display toggles only
     `FitInside()`, never resize, so a size the operator chose survives. Don't
     go back to `SetSizerAndFit()` on the frame: it makes the full content
     size the minimum window size. Before the first `Show()`, `FitToScreen()`
     centres the panel on OpenCPN so it opens on the monitor the cap was
     computed for. `OnFieldWheel` sends the mouse wheel over the number
     fields and the craft dropdown to the scrolled area unless that field has
     focus — on GTK the wheel otherwise changes their value while scrolling.
     Position but no speed → bearing + distance, no ETA;
     no position at all → datum only. Number formatting is pure
     (`format.{h,cpp}`) and unit-tested. **Positions are shown with
     OpenCPN's own `toSDMM_PlugIn()`** (`FormatPosition()` in the panel), so
     they follow the lat/lon format the user picked in OpenCPN; that only
     works inside OpenCPN, so it has no unit test. `ParseCoordinate()` reads
     every format it prints (° ' " are treated as separators).
   - **(b) On the chart** *(done in the v0.1 PR)*, all on fixed GUIDs,
     delete-before-add, removed on `DeInit`:
     - **"Reported position" mark** at the reported position (left where reported;
       toggle "Show reported position");
     - **"Estimated position" mark** (circle) at the aged datum, where the
       target is estimated to be now (toggle "Show estimated position");
     - **"Intercept" mark** (diamond) at the far end of the course line —
       the moving-target meeting point from `Case::SolveIntercept` (where
       own-ship's straight course meets the still-drifting target). Visibility
       tied to "Show routes". Only drawn when there *is* an intercept;
       without one the course line still points at the estimated position,
       renamed "Toward estimated position (no intercept)", with an unlabelled
       end point;
     - **"Target drift" track** (`AddPlugInTrack`) reported → datum;
     - **"Course to steer" route** (`AddPlugInRoute`, activatable) own-ship
       → intercept.
     The track, route and intercept mark (everything under "routes") are
     suppressed together when "Show routes" is off — `UpdateChartObjects`
     calls `UpdateCourseRoute(c, show_lines ? own : nullopt)` and the nullopt
     path tears down both the route and the mark. GUIDs …a001 estimated,
     …a002 route, …a003 target, …a004 drift track, …a005 intercept.
     Route and track render in OpenCPN's different route/track styles, which
     is how the two lines come out visually distinct without a custom
     overlay. **MSVC gotcha:** `Plugin_WaypointList`'s node methods are
     `WX_DECLARE_LIST`'d in `ocpn_plugin.h` but the api-18 `opencpn.lib`
     doesn't export them (LNK2001 `wxPlugin_WaypointListNode::DeleteData`) —
     `intercept_pi.cpp` provides them locally under `#if defined(_MSC_VER)`
     with `WX_DEFINE_LIST`; Linux gets them from OpenCPN's shared lib so the
     guard must stay MSVC-only or Linux gets a duplicate-symbol error.
   No custom `RenderOverlay` drawing — see "No custom chart drawing" above.
   **This is the usable v0.1.**

3. **Search patterns** (Planned direction #5): expanding square, sector,
   parallel track, emitted as OpenCPN routes centred on the datum.

*Landed:*
- Uncertainty circle (Planned direction #3) — `ComputeDatumUncertainty`
  (`datum_age.{h,cpp}`) fills `Case::uncertainty` in `FinalizeDatum()`:
  IAMSAR `E = √(X² + De²)`, `DVe = √(ASWDVe² + TWCe² + LWe²)` (the
  ASWDVe term was missing from the original plan), plus a leeway-divergence
  allowance `2·L·sin(θ/2)` because the datum is pure downwind, with L the
  *net* leeway displacement (`AgedDatum::leeway_nm`), not its path. Current
  (TWCe) and leeway (LWe) get 0.3 kt when the GRIB covers them at every step
  and 0.5 kt when it doesn't (`AgedDatum::source` / `wind_every_step` /
  `current_every_step`); the wind term (ASWDVe, 0.3 kt) only adds when there
  is wind data. A hand-entered set & drift is not data — both components
  stay at 0.5 kt, so it never comes out tighter than a GRIB. Missing data
  widens the circle. While the "Estimated position" mark is OpenCPN's active
  waypoint it is moved and resized in place (`UpdateSingleWaypointEx`) rather
  than kept, so its ring never goes stale. `X` is a fixed 1 NM for now
  (`kAssumedInitialPositionErrorNm`) — a panel input is the natural next
  step. Drawn as a range ring on the "Estimated position" mark
  (`AddSingleWaypointEx`, name kept visible explicitly), spacing converted
  to the user's ring unit by `RangeRingStepForUnits` from OpenCPN's
  `/Settings/WaypointRangeRingsStepUnits`. Pinned against the IAMSAR worked
  example (DVe 0.60 kt, E 31.67 NM, both divergence datums 20.6 NM away) in
  `test_reference_leeway`; the default error values are listed for SAR
  review in `docs/LEEWAY_NEEDS_VERIFICATION.md`.
- Datum ageing (PR #5) — `ComputeAgedDatum` / `FinalizeDatum` on the `Case`,
  GRIB and manual set & drift as sources, drift optional.
- Moving-target intercept — `SolveMovingIntercept` (`src/intercept_solve.{h,cpp}`,
  `test_intercept_solve`): the earliest `t` where the shortfall
  `range(own, target_at(t)) − sog·t` reaches 0, found by marching forward in
  steps of `shortfall / (sog + 20 kt)` (can't overshoot the first zero while
  targets stay under the 20 kt drift cap; min step 1 min) then bisecting.
  Unlike the fixed-point iteration it replaced, this also finds intercepts
  with a *faster* target heading toward or across own-ship. `target_at`
  extrapolates the same drift model past "now". `Case::SolveIntercept(own)` wires it up (one `GribReader`
  reused across iterations) and fills `Case::intercept_*`; the panel and the
  course route/mark use it, falling back to the present estimated position
  when it doesn't converge (sog ≤ 0, or own-ship can never reach it). ETA
  is now the transit time to the meeting point, not range-to-datum-now / sog.
- Leeway coefficient fixed `0.36 → 0.036` and pinned by
  `test/test_reference_leeway.cpp` against a published SAR worked example + the
  Allen & Plourde leeway band (PR #9). Input validation + `CombineVectors` guard
  (PR #7).
- The datum *method* and the leeway *magnitude* are now both checked
  against the standard SAR method and Allen & Plourde. Still unconfirmed by
  a SAR-literate human: the
  two-bucket craft model, the no-divergence simplification, and whether
  `0.036` (vs a more conservative ~0.07) is the right single default — see
  `docs/LEEWAY_NEEDS_VERIFICATION.md`.
