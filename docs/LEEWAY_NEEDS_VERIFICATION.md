# Leeway / drift model — NOT VERIFIED, do not deploy

**Status: the datum-ageing drift model in `src/datum_age.cpp` was written
autonomously and has not been checked against an authoritative source. A
person with SAR domain knowledge must validate it against IAMSAR Vol III and
Allen & Plourde before this is used operationally.**

## What the code does now

| thing | value / method in code | where |
|---|---|---|
| leeway speed, rubber-hulled | `kLeewayRubber = 0.36` × wind speed | `src/datum_age.cpp` |
| leeway speed, wooden | `kLeewayWooden = 0.04` × wind speed | `src/datum_age.cpp` |
| leeway offset | `kLeewayConstant = 0.0` | `src/datum_age.cpp` |
| leeway direction | `wind.direction_deg + 180°` (pure downwind) | `src/datum_age.cpp` |
| craft classification | case-insensitive substring match on `craft_type` | `src/datum_age.cpp` |
| integration | 30-min rhumb-line steps, `kMaxSteps = 2000` | `src/datum_age.cpp` |

## What the literature says (needs a human to apply properly)

- **Life-raft leeway is ~2–13 % of the 10 m wind speed** (downwind slope
  ≈ 0.03–0.13), *not* 36 %. It depends strongly on drogue / ballast / canopy —
  a drogued, deep-ballasted raft can be ~1–3 %; an undrogued poorly-ballasted
  one ~10–13 %. So **`kLeewayRubber = 0.36` is almost certainly a
  ~10× error** — plausibly meant to be `0.036`, but really the single
  coefficient should be replaced with the IAMSAR configuration-dependent
  values.
- **"Rubber vs. wooden" is not how leeway is categorised.** Allen & Plourde
  define ~63 object classes (life raft, PIW, skiff, sailboat, …) each with its
  own downwind and crosswind slopes and a divergence angle. A wooden hull and
  a rubber hull of the same object type drift similarly; the object *type* and
  its *configuration* are what matter.
- **Leeway is not pure downwind.** It has a downwind component and a crosswind
  component, and a **leeway divergence angle** (~30–50° off downwind, and
  which side is uncertain). SAR therefore ages the datum to a *divergence
  area*, not a single point. The `wind + 180°` direction is a simplification
  that will place the datum in the wrong location.
- **`kEarthRadiusNm = 3440.065`** is the correct authalic Earth radius in NM —
  that one is fine.

Sources:
- Allen, A.A. & Plourde, J.V., *Review of Leeway: Field Experiments and
  Implementation*, USCG R&D Center, CG-D-08-99 (DTIC ADA366414).
- Allen, A.A., *The Leeway of an Open Boat and Three Life Rafts in Heavy
  Weather*, USCG R&D Center (DTIC ADA338829).
- Allen, A.A. (2005), *Leeway Divergence*, USCG R&D Center CG-D-05-05.
- IAMSAR Manual Vol III, Section 3 — liferaft leeway graph and datum method.

## Until a human has done that

The intercept computation must let the operator **turn drift off**:

- compute the datum both **with** and **without** leeway drift whenever wind /
  current data is available, and
- let the operator toggle which one the displayed intercept route uses (a
  checkbox / switch),

so an operator who doesn't trust the (unverified) leeway model can fall back
to current-only or reported-position-only. See the roadmap note in
`CLAUDE.md`.
