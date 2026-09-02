# Leeway / drift model — NOT VERIFIED, do not deploy

**Status: the datum-ageing drift model in `src/datum_age.cpp` was written
autonomously. The method has now been checked against IAMSAR Manual Vol II & III
(see below) and is structurally sound, but the leeway *coefficient*
`0.36` is wrong by 5–13× and must be fixed. A person with SAR domain
knowledge should confirm the replacement value and the simplifications before
operational use.**

## What the code does now (`src/datum_age.cpp`, on `main`)

| thing | value / method in code |
|---|---|
| leeway speed, "wooden" craft_type | `0.04 × wind speed` (kt), no offset |
| leeway speed, anything else (default) | `0.36 × wind speed` (kt), no offset |
| leeway direction | `wind FROM + 180°` — pure downwind, no divergence |
| craft classification | `craft_type.Lower().Contains("wooden")` else default |
| drift | vector sum of current + leeway (`CombineVectors`) |
| integration | 30-min rhumb-line steps (`AdvancePosition`), `kMaxSteps = 2000` |
| Earth radius | `kEarthRadiusNm = 3440.065` |

## What IAMSAR says

### The method is correct

IAMSAR Vol III §3 (printed p.3-16/17) and Vol II §4.4 / Appendix K:

- drift = **vector sum of leeway + total water current** — matches `CombineVectors`.
- drift distance = drift speed × time interval — matches the integration loop.
- datum = reported position moved along the drift vector — matches `AdvancePosition`.
- downwind direction = **average surface wind direction ± 180°** — matches
  `wind FROM + 180°` (Vol II Leeway worksheet line 2, Wind current worksheet line 2).

So the structure of `ComputeAgedDatum` is the IAMSAR datum method. The problems
are all in the leeway numbers.

### `0.36` is wrong — leeway is single-digit percent of wind speed

IAMSAR Vol II **Figure N-2** ("Leeway of liferafts, survival craft and PIWs",
*adapted directly from Allen & Plourde 1999, USCG CG-D-08-99* — the same
reference the code comments cite) and Vol III's "Liferaft leeway" graph
(p.3-18):

| craft / config | leeway ≈ % of wind | divergence angle | probable error |
|---|---|---|---|
| liferaft, no ballast, no drogue | ~7 % (0.07) | ±25° | 0.25 kt |
| liferaft, no ballast, canopy+drogue | ~4 % | ±30° | 0.35 kt |
| liferaft, shallow ballast, no drogue | ~4 % | ±20° | 0.1 kt |
| liferaft, deep ballast | ~3 % | ±15° | 0.2 kt |
| liferaft, shallow ballast + drogue | ~2.5 % | ±20° | 0.1 kt |
| PIW (person in water) | ~1 % or less | ±10–30° | 0.1–0.35 kt |
| **medium displacement fishing vessel** (Vol II App. Q worked example: ASW 31.72 kt → leeway 1.3 kt) | **~4 %** (0.041) | **±50°** | 0.3 kt |

So:

- **The default `0.36` should be roughly `0.03`–`0.07`.** A conservative
  choice for an unknown liferaft is the no-drogue value, ~`0.07`. `0.036`
  (a plausible typo for `0.36`) sits in the middle of the raft range and is
  defensible as a single-value default.
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

## Reference case for a regression test — IAMSAR Vol II, Appendix Q

The manual's fully worked example ("F/V SAMPLE", Vol II Appendix Q). Use it to
build an external-truth test (see `CLAUDE.md` → Next):

```
Start (EIP):     37°10.0'N, 065°45.0'W  at 25 2145Z JAN 2000
Commence search: 26 1630Z JAN 2000   (drift interval 18.75 h)
Avg surface wind (ASW):   194°T, 31.72 kt      (direction FROM)
Total water current (TWC): 057°T,  1.86 kt      (set = toward)
Leeway speed:              1.3 kt               (fishing vessel, from fig N-3)
Leeway divergence angle:   ±50°                 → downwind 014°T, so 324°T / 064°T

IAMSAR result:
  drift, left of downwind:  021°T, 2.21 kt → 41.49 NM → datum 37°48.7'N, 065°26.3'W
  drift, right of downwind: 060°T, 3.15 kt → 59.14 NM → datum 37°39.6'N, 064°40.5'W
  divergence distance (DD): 37.5 NM
```

The current code has no divergence, so feed it the **pure-downwind** variant
(leeway 1.3 kt toward 014°T, no ±50°) and expect a datum between the two
IAMSAR points — roughly **37°44'N, 065°03'W**, drift ≈ 035°T / ≈ 2.6 kt over
18.75 h ≈ 49 NM. Tolerance ~2 NM (IAMSAR uses manoeuvring-board plotting).
Also assert the leeway sub-calc: `LookupLeewayCoefficients` × 31.72 kt must be
≈ 1.3 kt (±0.3), **not** 11.4 kt.

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

## Until the coefficient is fixed and confirmed

The intercept computation must let the operator **turn drift off** — compute
the datum both with and without leeway whenever wind/current is available, and
let the operator choose which the displayed route uses. See `CLAUDE.md`.
