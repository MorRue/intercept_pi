# GRIB2 test fixtures

Both files come from NOAA NCEP's GFS 0.25° operational run, cycle
`2026090100` (2026-09-01 00Z), forecast hour `f000` (analysis), served by
NOMADS (`nomads.ncep.noaa.gov`). Retrieved 2026-09-02.

Neither `wgrib2` nor `eccodes`/`grib_api` is installed on this sandbox, and
there is no root/`sudo` access to install them (`apt-get install` fails with
"Erlangen der Sperre ... nicht möglich; sind Sie root?"), so the packing of
each fixture was confirmed by hand-parsing the GRIB2 section headers (a
~60-line Python script reading Section 3, 4 and 5 octets directly) rather
than with `wgrib2 -V`. Byte offsets below are section-relative and follow
the GRIB2 spec layout (WMO Manual on Codes, FM 92-XIV GRIB2, Template
3.0/4.0/5.0/5.3). If `wgrib2` is available elsewhere, `wgrib2 -V <file>`
should show the same Grid/Product/Data-Representation templates reported
here.

## (a) `gfs_10m_wind_simple_drt5.0.grib2` — simple-packed, decodable

4,982 bytes. Two messages: 10 m U-wind (`UGRD`) then 10 m V-wind (`VGRD`),
on a 41×41 regular lat/lon grid, 30–40°N / 10–20°E at 0.25° spacing.

Obtained from NOMADS's `filter_gfs_0p25.pl` subregion-extraction service.
The intent was to download an unmodified extract and re-pack it to simple
packing with `wgrib2 -set_grib_type s`, but `wgrib2` is not available here
(see above). It turned out to be unnecessary: NOMADS's filter script
re-packs whatever it subsets to simple packing (Data Representation
Template 5.0) as part of extracting a subregion — confirmed directly from
the bytes, see below — so the file downloaded this way already satisfies
the "simple-packed, DRT 5.0" requirement without any local re-pack step.

Download command actually run:

```sh
curl -s -o gfs_10m_wind_simple_drt5.0.grib2 \
  "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl?dir=%2Fgfs.20260901%2F00%2Fatmos&file=gfs.t00z.pgrb2.0p25.f000&var_UGRD=on&var_VGRD=on&lev_10_m_above_ground=on&subregion=&toplat=40&leftlon=10&rightlon=20&bottomlat=30"
```

Verified by parsing the raw bytes of both messages:

- Section 3 (Grid Definition): template `3.0` (regular lat/lon), ni=41,
  nj=41, la1=30.0, lo1=10.0, la2=40.0, lo2=20.0, di=dj=0.25 — matches the
  `GribReader` grid support (`Grid Definition Template 3.0`).
- Section 4 (Product Definition): template `4.0` (instantaneous field at a
  horizontal level), discipline=0, category=2 (momentum), number=2 (`UGRD`)
  / 3 (`VGRD`), level type=103 (height above ground), level value=10.0 —
  matches every field `GribReader::LookupWind` filters on
  (`src/grib_reader.cpp`, discipline 0 / category 2 / number 2,3 / 10 m
  level).
- Section 5 (Data Representation): template `5.0` (grid point data, simple
  packing) on both messages — the encoding `GribReader` decodes.

This fixture is expected to decode fully and produce a wind sample once a
reader unit test exists.

## (b) `gfs_pbl_wind_complex_drt5.3.grib2` — complex-packed, undecodable

60,478 bytes. Two messages: U-wind and V-wind at the "planetary boundary
layer" level (`UGRD`/`VGRD`, level type 220), on the native 1×1 degree
global GFS grid (360×181 points), taken byte-for-byte — unmodified — out of
the full, un-filtered `gfs.t00z.pgrb2.1p00.f000` file via an HTTP Range
request. No repacking, no NOMADS filter script — this is exactly what NCEP
produced.

10 m wind itself was not usable for this fixture: at usable resolutions the
individual 10 m U/V messages run 77–80 KB each (larger once combined),
over the 64 KB budget, at both 0.25° and 1° native resolution, and the
un-filtered global 0.25° message is ~950 KB. The 1° grid's `UGRD`/`VGRD` at
the planetary-boundary-layer level is the smallest wind field pair under
64 KB combined (each message ~30 KB) — still true wind data, still an
unmodified stock GFS extract, just not the 10 m level.

Commands actually run:

```sh
# 1. Fetch the .idx sidecar to find this run's byte offsets (tiny, text).
curl -s -o gfs.t00z.pgrb2.1p00.f000.idx \
  "https://nomads.ncep.noaa.gov/pub/data/nccf/com/gfs/prod/gfs.20260901/00/atmos/gfs.t00z.pgrb2.1p00.f000.idx"

# 2. Find the UGRD/VGRD "planetary boundary layer" records:
#      11:427488:d=2026090100:UGRD:planetary boundary layer:anl:
#      12:457343:d=2026090100:VGRD:planetary boundary layer:anl:
#      13:487966:d=2026090100:VRATE:planetary boundary layer:anl:
#    i.e. UGRD spans bytes [427488, 457342], VGRD spans [457343, 487965].

# 3. Byte-range fetch of both messages, unmodified, from the full file:
curl -s -r 427488-487965 -o gfs_pbl_wind_complex_drt5.3.grib2 \
  "https://nomads.ncep.noaa.gov/pub/data/nccf/com/gfs/prod/gfs.20260901/00/atmos/gfs.t00z.pgrb2.1p00.f000"
```

Verified by parsing the raw bytes of both messages:

- Section 3: template `3.0`, ni=360, nj=181, la1=90.0, lo1=0.0, di=dj=1.0 —
  a grid `GribReader` supports.
- Section 4: template `4.0`, discipline=0, category=2, number=2/3 (`UGRD`/
  `VGRD`), level type=220 — a level `GribReader::LookupWind` does not
  request (it requires level type 103, 10 m above ground), so this record
  would be skipped even before the packing is examined.
- Section 5: template `5.3` (grid point data, complex packing with spatial
  differencing) on both messages — `GribReader` only implements template
  `5.0` and is documented to skip any other Data Representation Template
  rather than fail the file (`src/grib_reader.h`), so this record is the
  "encoding this reader does not support" case.

This fixture is expected to parse the file (no crash) but yield no decoded
wind sample.

## File sizes

| File | Bytes | DRT |
|---|---|---|
| `gfs_10m_wind_simple_drt5.0.grib2` | 4,982 | 5.0 (simple) |
| `gfs_pbl_wind_complex_drt5.3.grib2` | 60,478 | 5.3 (complex, spatial differencing) |

Both are under the 64 KB fixture budget.
