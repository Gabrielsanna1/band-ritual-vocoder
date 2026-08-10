# UNCERTAINTY — what these notes are *not* sure about

> **Why this document exists.** The rest of `docs/` reads as a record of things established.
> Much of it is. But the match to Ableton stopped at a cosine of about 0.93, not 1.0, and the
> notes were written over three months while the picture kept changing. This file lists the
> places where the record is thin, self-contradictory, or extrapolated beyond what was measured —
> ranked by how much each one matters if you are rebuilding.
>
> Read this **after** `CONTEXT.md` and `FILTERS.md`, before treating any constant as a fact.

---

## The short version

| # | What is uncertain | Matters for the rebuild |
|---|---|---|
| 1 | The scale-locked mode was never validated against anything | **Critical** |
| 2 | Cosine figures come from two measurement environments and are not labelled | Low — resolved below, needs one line of text |
| 3 | Four of the ten "locked constants" sit on a flat optimum, so the fit does not identify them | High |
| 4 | Four questions listed as open were closed by sweeping, not by identifying | High |
| 5 | The band-count Q law is measured at 4–20 bands and used up to 100 | Medium |
| 6 | The depth curve is a fit whose own note doubts its form | Medium |
| 7 | The unvoiced/noise system was mapped but not reproduced | Medium |
| 8 | Everything measured before 11 April was measured on clipped audio | Low (mostly re-done) |

---

## 1. The scale-locked mode has no reference — and it is the part being rebuilt

Everything in these notes that carries a number was measured **against Ableton's stock Vocoder**,
in the mode where Band Ritual imitates it. Ableton has **no scale-locked mode**. There is nothing
to compare a scale-locked filterbank against, and no measurement in this repository speaks to it.

So the situation is the inverse of what the volume of notes suggests:

- the mode that is **well characterised** (LINKED / logarithmic band placement) is the one that
  merely reproduces an existing device
- the mode that is **the reason the plugin exists** (bands placed on scale notes) rests on ear
  judgement alone

Nothing here is wrong about it. There is simply no evidence about it either way.

**What would settle it:** a validation target that does not depend on Ableton. Intelligibility on
speech is measurable; "sounds musical" is not, unless a procedure is defined in advance. See
`process/Band Ritual - Validation Suite Spec.md` for the harness that exists, and note that it was
built to compare against Ableton, so it does not cover this case.

---

## 2. Cosine figures come from two environments, and the tables do not say which

**This one is resolved — it needs a line of text, not investigation.** It is documented here because
a reader will otherwise reach the wrong conclusion, and the wrong conclusion is alarming.

`CONTEXT.md` reports the final architecture at **0.9138**, while its "what does not work" table lists
`RMS envelope 0.9199` and `2x oversampling 0.9159` — both apparently *higher* than the architecture
that was chosen.

They are not higher. There are two measurement environments, and the table reports the other one.
From `PROTOTYPE_GIT_HISTORY.md`, for the final change of the session:

```
Simulation (float64):  0.9159 -> 0.9209  (+0.005)
Plugin (float32):      0.9131 -> 0.9138  (+0.0007)
```

So the reference points are:

| environment | baseline | final |
|---|---|---|
| float64 simulation | 0.9159 | **0.9209** |
| float32 plugin | 0.9131 | **0.9138** |

Read against the correct baseline, every row is consistent:

| Row in the table | Value | Compare against | Reading |
|---|---|---|---|
| 2x oversampling | 0.9159 | float64 baseline 0.9159 | identical, hence "no improvement" |
| Float64 precision | 0.9159 | it *is* the float64 baseline | same number for the same reason |
| RMS envelope | 0.9199 | float64 final 0.9209 | worse by 0.001, hence "close, still worse" |

Nothing rejected outperformed what was chosen. The only defect is that the environment is unlabelled,
and that 0.9159 appears twice in one table for two different reasons, which is what makes it look
like a contradiction.

**Fix:** add one sentence above that table in `CONTEXT.md` — that its figures are from the float64
simulation, whose baseline was 0.9159 and whose final was 0.9209, whereas the 0.9138 headline is the
float32 plugin. Then state each figure's environment wherever cosine is quoted.

---

## 3. Four of the ten locked constants sit on a flat optimum

From the iteration history in `dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md`:

| Change | Cosine gain |
|---|---|
| Q = 37 restored (fixing a regression) | **+0.33** |
| Simple peak detector instead of a lowpass | **+0.13** |
| Q = 40 | +0.006 |
| Analysis Q ratio 0.52 | +0.006 |
| `exp(-1/(sr·tau))` coefficient form | +0.0003 |
| Envelope compression `e^0.75` | +0.0007 |

These gains are **real**: the tests are deterministic, so a difference of 0.006 is reproducible and
not measurement noise. The uncertainty is of a different kind.

The first two changes are load-bearing — each would be audible on its own, and together they account
for 0.46 of the 0.47 total improvement. The last four total **+0.0127**, which means the surface
being optimised is almost flat around them: a neighbouring value scores nearly the same. A flat
optimum tells you that the chosen value **fits** the measurement, and tells you almost nothing about
whether it is the value the reference device actually uses. A rebuild that chose an analysis ratio of
0.60 or a compression exponent of 0.8 would measure within a few thousandths of these.

Treat them as sound starting values rather than as recovered constants. The two that are genuinely
recovered — because the penalty for getting them wrong is enormous — are the magnitude of Q and the
asymmetric peak detector.

---

## 4. Questions recorded as open, then closed by sweeping

`dsp/Band Ritual - Vocoder Cracking DEFINITIVE.md` lists, under **Remaining unknowns**:

- analysis stage count — 2 or 3
- analysis Q ratio — 0.6 or something else
- whether the per-stage Q is exactly 39 or needs fine tuning
- the exact attack/release behaviour of the envelope

All four were subsequently *settled* by parameter sweep. None was *identified* from a measurement of
Ableton. The distinction matters: a sweep finds the value that best fits the output you can measure,
which is not the same as discovering the value the reference device uses — especially when several
values fit almost equally well, which point 3 shows they do.

The one that would repay real work is the **envelope attack and release**. It has the largest audible
consequence of the four (the detector change alone was worth +0.13), and it was never measured
directly, only fitted.

**What would settle it:** the approach that worked for the filters — an isolated stimulus.
`dsp/Band Ritual - Filter Order Correction.md` already names the trick and calls it "the single most
impactful" next step: feed the reference device a signal designed to isolate one behaviour, rather
than trying to infer everything from music.

---

## 5. The band-count Q law is used well outside where it was measured

The law `Q ≈ N_bands / (n_octaves × BW)` comes from
`dsp/Band Ritual - Ableton Filter Measurements 2026-04-09.md`, measured at band counts
**4, 8, 12, 16 and 20**, and anchored to a calibration at **40** bands.

The plugin allows up to **100** bands. Between 20 and 100 the law is extrapolation. The same note
raises this itself and lists as action item 2: *"re-run validation suite at band counts
{16, 32, 40, 64, 100} to confirm the new formula tracks Ableton across the full band-count range."*
`process/Band Ritual - Validation Suite Spec.md` includes those counts in its plan. **No results note
for that run exists in this repository.** Treat it as not done.

The same applies to the frequency range: the measurements used **20 Hz to 18 kHz**, and the plugin
allows 20 Hz to 20 kHz. Behaviour at the edges is outside the measured domain.

---

## 6. The depth curve is a fit, and its own note doubts the form

The depth constants — floor 0.065, exponent 1.35 above 100% — are recorded as *"measured from Ableton
depth exports"*. But `dsp/Ableton Vocoder - Complete Cracked Algorithm.md` observes of the same
exports that the steps are about 9 dB, 9 dB, 4 dB, 4 dB and concludes the relation is *"NOT linear —
approximately quadratic at low depth"*.

So the fitted form and the observation about the shape were not reconciled. The numbers reproduce the
measured points; whether they generalise between and beyond them is unknown.

---

## 7. The unvoiced/noise system was mapped, not reproduced

`dsp/Band Ritual - Noise Cracking Session 2026-04-04.md` identifies the reference device's three
controls (level, sensitivity, speed) and a gain law of roughly `unvoicedParam^6`. Band Ritual's noise
path is its own design, not a reproduction of that system. The mapping is a starting point; the
matching was never carried through.

---

## 8. The clipping bug — mostly cleaned up, worth one check

`dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md` establishes that every cosine
measurement from March onward was taken on clipped 16-bit audio and is invalid. Anything dated before
11 April 2026 should be read with that in mind.

Most of the important constants were re-established afterwards — in particular the Q magnitude was
re-derived independently on 9 April and again in Iter 22. But **the "what does not work" table has not
obviously been re-run**, and it is the table the README recommends most strongly. At least one of its
rows says so explicitly: *"ring-buffer boxcar 0.54 — measured under the Q=6 bug, invalid"*. Others may
be in the same position without saying so.

**Before relying on that table to rule an approach out, check whether the row predates 11 April.**

---

## How to read the rest of these notes

Three rules that follow from the above:

1. **Later beats earlier.** The notes contradict each other because understanding changed. Dates are
   in the filenames for this reason.
2. **A cosine figure without its environment is not comparable.** Some tables report the float64
   simulation, some the float32 plugin, and the gap between them is about 0.007 — larger than most of
   the individual improvements being discussed. See point 2.
3. **"Confirmed by sweep" is weaker than "measured".** The constants table in `CONTEXT.md` mixes both
   and does not distinguish them. Point 3 above says which are which.
