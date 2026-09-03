# Leeway / drift model — NOT VERIFIED, do not deploy

**Status: the datum-ageing drift model in `src/datum_age.cpp` has been checked
against the published SAR datum method and the Allen & Plourde leeway data
(see Sources) and is structurally sound. The default leeway coefficient was
`0.36` (5–13× too large); it is now `0.036`, pinned by
`test/test_reference_leeway.cpp`. A person with SAR domain knowledge should
still confirm that value and the simplifications (two craft buckets, no
divergence) before operational use.**

This project is not affiliated with or endorsed by the IMO, ICAO, the USCG,
or any search-and-rescue authority. The references below are cited as
sources; consult the authoritative publications directly before any
operational use.

## What the code does now (`src/datum_age.cpp`)

| thing | value / method in code |
|---|---|
| leeway speed, "wooden" craft_type | `0.04 × wind speed` (kt), no offset |
| leeway speed, anything else (default) | `0.036 × wind speed` (kt), no offset |
| leeway direction | `wind FROM + 180°` — pure downwind, no divergence |
| craft classification | `craft_type.Lower().Contains("wooden")` else default |
| drift | vector sum of current + leeway (`CombineVectors`) |
| integration | 30-min rhumb-line steps (`AdvancePosition`), `kMaxSteps = 2000` |
| Earth radius | `kEarthRadiusNm = 3440.065` |

## How this compares to the published method

### The method is standard

The SAR datum method (IAMSAR Manual Vol II §4.4 / App. K, Vol III §3):

- drift = **vector sum of leeway + total water current** — matches `CombineVectors`.
- drift distance = drift speed × time interval — matches the integration loop.
- datum = reported position moved along the drift vector — matches `AdvancePosition`.
- downwind direction = **average surface wind direction ± 180°** — matches
  `wind FROM + 180°`.

So the structure of `ComputeAgedDatum` is the standard datum method. The
problems are all in the leeway numbers.

### Leeway is single-digit percent of wind speed (why `0.36` → `0.036`)

Representative leeway figures from **Allen & Plourde 1999** (USCG CG-D-08-99,
public domain — the primary experimental source; the IAMSAR Manual's leeway
graphs are adapted from it):

| craft / config | leeway ≈ % of wind | divergence angle | probable error |
|---|---|---|---|
| liferaft, no ballast, no drogue | ~7 % (0.07) | ±25° | 0.25 kt |
| liferaft, no ballast, canopy+drogue | ~4 % | ±30° | 0.35 kt |
| liferaft, shallow ballast, no drogue | ~4 % | ±20° | 0.1 kt |
| liferaft, deep ballast | ~3 % | ±15° | 0.2 kt |
| liferaft, shallow ballast + drogue | ~2.5 % | ±20° | 0.1 kt |
| PIW (person in water) | ~1 % or less | ±10–30° | 0.1–0.35 kt |
| **medium displacement fishing vessel** (published worked example: ASW 31.72 kt → leeway 1.3 kt) | **~4 %** (0.041) | **±50°** | 0.3 kt |

So:

- **The default is now `0.036`** — mid raft range, a plausible typo fix for
  `0.36`, defensible as a single-value default. A more conservative choice for
  an unknown liferaft would be the no-drogue value ~`0.07`; a SAR reviewer
  should make that call.
- **`0.04` for "wooden" is about right** — a displacement hull's leeway is
  low, and the Appendix Q fishing vessel comes out at 4.1 %. Keep it, but note
  it's a coincidence of magnitude, not a verified figure for wooden craft
  specifically.
- "Rubber vs wooden" is not how IAMSAR/Allen & Plourde categorise leeway
  (they define ~60+ object classes by type and rig, each with a downwind
  slope, a crosswind slope, and a divergence angle). The two-bucket model is
  a deliberate simplification for a first cut — acceptable if documented, but
  a finer `craft_type` → coefficient table is a later milestone.

### Divergence is dropped — acceptable for the Vol III method, not the Vol II method

Real leeway diverges from downwind by a **divergence angle** (±15–30° for
rafts, larger for hulls), and which side is not known — so IAMSAR Vol II
produces **two datums** (left-of-downwind, right-of-downwind) and searches the
area between them. IAMSAR **Vol III's simplified on-scene method uses pure
downwind** ("leeway direction is downwind", no divergence) — which is what the
code does.

Using pure downwind is therefore consistent with the Vol III method and fine
for a first cut, **provided the uncertainty radius (roadmap #3) is widened to
cover the divergence area** — i.e. the datum is a best estimate, and the search
region must still contain the true position. This must not be forgotten when
#3 is built.

## Reference case for a regression test

A published SAR datum worked example ("F/V SAMPLE", a medium displacement
fishing vessel; IAMSAR Manual Vol II Appendix Q). The figures below are
reproduced as factual reference data to serve as an external-truth test
oracle (see `test/test_reference_leeway.cpp`):

```
Start (EIP):     37°10.0'N, 065°45.0'W  at 25 2145Z JAN 2000
Commence search: 26 1630Z JAN 2000   (drift interval 18.75 h)
Avg surface wind (ASW):   194°T, 31.72 kt      (direction FROM)
Total water current (TWC): 057°T,  1.86 kt      (set = toward)
Leeway speed:              1.3 kt               (fishing vessel)
Leeway divergence angle:   ±50°                 → downwind 014°T, so 324°T / 064°T

Published result:
  drift, left of downwind:  021°T, 2.21 kt → 41.49 NM → datum 37°48.7'N, 065°26.3'W
  drift, right of downwind: 060°T, 3.15 kt → 59.14 NM → datum 37°39.6'N, 064°40.5'W
  divergence distance (DD): 37.5 NM
```

The current code has no divergence, so feed it the **pure-downwind** variant:
leeway 1.3 kt toward 014°T (the downwind of ASW 194°T), no ±50°. Vector sum
with the TWC (1.86 kt toward 057°T):

```
LW  @ 014°T, 1.3  kt →  x=+0.314  y=+1.261   (x = East, y = North)
TWC @ 057°T, 1.86 kt →  x=+1.560  y=+1.013
sum                  →  x=+1.874  y=+2.274
drift = 2.95 kt @ 039.5°T   over 18.75 h  = 55.3 NM
datum ≈ 37°52.6'N, 065°00.7'W
```

(This is **not** the midpoint of the two IAMSAR divergence datums — averaging
two equal vectors ±50° off a centre bearing gives `L·cos50° ≈ 0.64 L` along
that bearing, not `L`, so a pure-downwind sum lands *outside* the L/R pair, to
the north. The earlier draft of this file said "≈ 37°44'N, 065°03'W, between
the two points" — that was wrong.)

Test it as: `ComputeAgedDatum` must land within **~5 NM** of
37°52.6'N 065°00.7'W. 5 NM, not 2: the code's own leeway at the fixed 0.036
coefficient is 0.036 × 31.72 ≈ 1.14 kt, ~0.16 kt short of IAMSAR's textbook
1.3 kt for this craft, which by itself shifts the datum ~3 NM. The check still
fails by ~190 NM if the coefficient regresses to 0.36.

Also assert the leeway sub-calc directly (the tight, unambiguous coefficient
check): `LookupLeewayCoefficients(<default>)` × wind must

- be **≈ 1.3 kt (±0.4)** at 31.72 kt (IAMSAR Appendix Q) — passes at 0.036
  (1.14 kt), fails hard at 0.36 (11.4 kt); and
- land inside the IAMSAR liferaft-leeway band (~2.5–7.5 % of wind, Fig N-2 /
  Vol III p.3-18) at 10, 20, 30 and 40 kt — confirms the *slope*, not just one
  mid-range point.

## Sources

- IAMSAR Manual **Vol II** (Mission Co-ordination), 7th ed. 2017 — §4.4,
  Appendix K (Determining datum), Appendix N Figure N-2/N-3 (leeway graphs),
  Appendix Q (worked example).
- IAMSAR Manual **Vol III** (Mobile Facilities), 9th ed. 2016 — §3, p.3-16
  to 3-18 (datum method + liferaft leeway graph).
- Allen, A.A. & Plourde, J.V., *Review of Leeway: Field Experiments and
  Implementation*, USCG R&D Center CG-D-08-99, 1999 (DTIC ADA366414) — the
  primary source Figure N-2 is adapted from.
- Allen, A.A. (2005), *Leeway Divergence*, USCG R&D Center CG-D-05-05.

## Until a SAR reviewer has confirmed the model

The intercept computation must let the operator **turn drift off** — compute
the datum both with and without leeway whenever wind/current is available, and
let the operator choose which the displayed route uses. See `CLAUDE.md`.
