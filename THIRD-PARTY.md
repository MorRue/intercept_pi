# Third-party components and references

## Bundled code

| Component | Where | License | Notes |
|---|---|---|---|
| OpenCPN plugin template (`Rasbats/shipdriver_pi` and the OpenCPN project template) | `cmake/`, `ci/`, `buildwin/`, `flatpak/`, `build-deps/`, `Plugin.cmake`, `CMakeLists.txt`, `plugin.xml.in`, `scripts/`, `update-templates` | GPL-3.0-or-later | Copyright (c) 2020-2021 Mike Rossiter and the OpenCPN developers. Individual files retain their own copyright headers. |
| `ghc::filesystem` | `libs/std_filesystem/include/ghc/filesystem.hpp` | MIT | Copyright (c) 2018 Steffen Schümann. Header-only `std::filesystem` shim for legacy platforms, from <https://github.com/gulrak/filesystem>. Full MIT text in `libs/std_filesystem/LICENSE`. |

## Git submodule (not bundled — fetched on `git clone --recurse-submodules`)

| Submodule | Path | Upstream |
|---|---|---|
| `opencpn-libs` | `opencpn-libs/` | <https://github.com/opencpn/opencpn-libs> — OpenCPN plugin support libraries (tinyxml, wxJSON, plugin_dc, jsoncpp, wxsvg, …), each under its own license. |

## Test fixtures

| File | Source | Status |
|---|---|---|
| `test/data/gfs_10m_wind_simple_drt5.0.grib2`, `test/data/gfs_pbl_wind_complex_drt5.3.grib2` | NOAA NCEP GFS operational run, retrieved from NOMADS (`nomads.ncep.noaa.gov`). Provenance and exact download commands in `test/data/README.md`. | US Government work — public domain (17 U.S.C. § 105). |

## References (cited, not reproduced)

The drift model in `src/datum_age.cpp` implements the standard search-and-rescue
datum method. Its leeway coefficients are representative values drawn from:

- **Allen, A. A. & Plourde, J. V.** (1999). *Review of Leeway: Field
  Experiments and Implementation.* U.S. Coast Guard R&D Center, Report
  CG-D-08-99 (DTIC ADA366414). — A US Government work in the public domain;
  the primary experimental source for leeway data.
- **Allen, A. A.** (2005). *Leeway Divergence.* U.S. Coast Guard R&D Center,
  Report CG-D-05-05.
- **IAMSAR Manual** (International Aeronautical and Maritime Search and Rescue
  Manual), Vol. II and Vol. III, IMO/ICAO. — Cited for the datum method and
  a published worked example (`docs/LEEWAY_NEEDS_VERIFICATION.md`,
  `test/test_reference_leeway.cpp`). The manual is copyright IMO/ICAO and is
  **not** included in this repository.

This project is **not affiliated with or endorsed by** the IMO, ICAO, the
U.S. Coast Guard, the OpenCPN project, or any search-and-rescue authority.
