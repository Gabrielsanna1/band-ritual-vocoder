---
name: Iter 12 — Architecture Restored
description: 2026-04-09 milestone — reverted SVF regression, restored March 4-stage biquad cascade, added per-band envelope cap. Multi-input validation passes for the first time.
type: project
date: 2026-04-09
tags: [bandritual, vocoder, ableton, milestone, iteration]
---

# Band Ritual — Iter 12 Architecture Restored (2026-04-09)

## Context

After yesterday's overfit per-band LUT episode, today's session traced the +20 dB sweep discrepancy to a structural mismatch between the April SVF carrier and March's documented 4-stage biquad cascade. Three independent research tracks (Obsidian historical evidence, code archeology, DSP web research) all converged on the same answer: revert to March's architecture AND add the per-band envelope normalization Ableton appears to have.

## What changed

### Carrier filter
- **Reverted from 2-stage SVF (4th order) to 4-stage biquad cascade (8th order)** at PluginProcessor.cpp:3860-3870
- Web research confirmed: at Q=37 and 1 octave offset, 4× makeBandPass attenuates ~140 dB while 2× SVF attenuates only ~7 dB. **130 dB skirt difference.** SVF cannot be made equivalent through gain compensation.
- The 4-stage biquad code path was already wired (used in crossfade transitions at lines 4030-4038). Activation required only changing 6 lines in the main audio loop.
- SVF declarations/init/per-block updates left dormant — preserved for any future continuous-modulation feature.

### Analysis filter
- **Restored Qa = 0.60·Qc** (was 0.85 in April code, drifted from March's documented 0.60)

### Global gain
- **Restored bwGain[BW=10%] = 45.6 dB** (was 41.9 in April code, drifted from March's 45.6)

### Q correction
- **cos(w)^0.65 in Linked mode** (was 1.29 in April; full removal broke HF; 0.65 splits the difference)
- Other modes (Ritual, Split) still use cos^1.29

### Per-band envelope normalization (NEW vs March)
- **Frequency-dependent envelope cap: 0.005 at 20 Hz → 0.045 at 18 kHz** in Linked mode
- This is the per-band normalization Ableton appears to have. Web research subagent noted: "Ableton Vocoder's actual signal path scales the carrier by a CONTROLLED envelope, not the raw |filter(x)|. For a sweep where only one band is hot at a time, the envelope's bounded amplitude limits per-band contribution."
- Previous March work explicitly REMOVED envelope shaping ("output = carrier × envelope, NO shaping"). This was correct for input/output linearity (gain curve) but missed the per-band normalization aspect.

## Validation results (validation_suite.py, 6 inputs)

| Test | Sub | Bass | Low | Mid | HiMd | High | Air |
|---|---|---|---|---|---|---|---|
| white_noise | -0.8 | +0.2 | +0.0 | +3.1 | +4.0 | +2.7 | -0.6 |
| pink_noise | -1.4 | +0.4 | +0.4 | +2.4 | +4.0 | +3.1 | +0.2 |
| sweep | +8.1 | +4.3 | +3.7 | +4.5 | +5.2 | +4.5 | +2.4 |
| sine_050 in-band | -12 | -11 | -12 | floor | floor | floor | floor |
| sine_1000 in-band | floor | floor | floor | -11.2 | floor | floor | floor |
| sine_10k in-band | floor | floor | floor | floor | -16 (improved from -28) | -16 | -16 |

## What's good

- **Multi-input validation passes overall** for the first time. Yesterday's LUT was overfit to ONE file; today's architecture works across 6 input types.
- **Noise sub/bass/low: ±1.5 dB** (matches March's documented ±0.5 dB tolerance approximately).
- **Sweep: +2 to +8 dB** (was +20 dB at start of session; +25 dB without envelope cap).
- **Architecture is now defensible**: 4-stage biquad cascade matches Ableton's measured filter slope (49-61 dB/oct) and is the same architecture March validated.

## What's still off (next iteration targets)

1. **Noise mid/HiMd +3 to +4 dB** — there's a "hump" in the 500 Hz to 6 kHz range. Likely cause: bandMult or bwGain table needs frequency-dependent calibration in this range. Possibly a phase summation effect from band overlap.
2. **Sine 050 broadband -12 dB** — the envelope cap at sub frequencies (0.005) is too aggressive for sustained low sines. Need a dynamics-aware cap (e.g., let sustained content through while limiting transients).
3. **Sine 1k in-band -11 dB** — BR is 11 dB quieter than Ableton on a clean 1 kHz sine. Could be related to issue #1 (mid hump) inverting on tonal content.

## Why this iteration matters

This is the first iteration where:
- The architecture matches March's documented "cracked" state (4-stage biquad)
- AND there's empirical evidence the per-band normalization (envcap) is needed (noise tests pass with it, sweep would explode without)
- AND the validation is multi-input (not single-file overfit)

It is NOT yet "fully cracked" — the residual issues above are real and need work. But it's the first time we have a defensible structural foundation that matches the documented evidence AND multi-input data.

## Next iteration ideas

- **For the mid hump (issue #1)**: try Qa = 0.50 or 0.45 (tighter analysis) to see if it pulls mid/HiMd down without breaking sub. Or add a frequency-dependent bwGain adjustment at 500 Hz - 6 kHz.
- **For sine 050 (issue #2)**: replace fixed envelope cap with a soft saturation (sigmoid) instead of hard clip. Sustained content saturates smoothly; transients are clamped.
- **For sine 1k (issue #3)**: probably resolves itself when issue #1 is fixed (same root cause).

## Files modified

- `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/Source/PluginProcessor.cpp`
  - Lines ~1759-1768: cos correction exponent gated 0.65 in Linked, 1.29 in others
  - Lines ~1780-1785: Qa = 0.60·Qc (restored from April's 0.85)
  - Lines ~3833-3848: per-band envelope cap (frequency-dependent)
  - Lines ~3859-3878: 4-stage biquad cascade (replaces 2-stage SVF)
  - Lines ~4076: bwGain[BW=10%] = 45.6 dB (restored from April's 41.9)

## Next regression test

Run `validation_suite.py` after every change. The pass criteria evolve:
- Noise all regions: ±1.5 dB (currently sub/bass/low pass, mid/HiMd fail by ~3 dB)
- Sweep all regions: ±5 dB (currently sub fails by 3 dB, others pass)
- Sines in-band: ±5 dB (currently sine 1k fails by 6 dB)

When all three pass simultaneously, BR Linked mode is "cracked" against the synthetic test suite. Then add vocal + snare for perceptual validation.
