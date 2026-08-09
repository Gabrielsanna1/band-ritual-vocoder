# Band Ritual — Linked Mode SVF Regression (2026-04-08)

**Date**: 2026-04-08
**Tags**: #bandritual #vocoder #regression #dsp
**Status**: HYPOTHESIS — being tested 2026-04-08

## Summary

The April per-band correction LUT (`linkedBandCorrectionDb`, 40 entries) was iterated to convergence on a single sub-rich input file (`DRY-EXPORT 1.wav`). It achieves ±1 dB region match against that file, but the LUT itself is **structurally suspect**: values zigzag ±25 dB between adjacent bands and several entries are pinned at the clamp limit. Investigation found three structural defects in the Linked-mode carrier path that, together, create exactly the alternating notches the LUT was learning to compensate. The LUT is therefore an **overfit**, not a fix. This note documents the findings, preserves the LUT for posterity, and points at the structural fix plan.

## Root cause 1: Phase inversion every other band

`Source/PluginProcessor.cpp:3849`:

```cpp
float phaseSign = (bi % 2 == 0) ? 1.0f : -1.0f;
```

Every odd-indexed band's carrier output is multiplied by −1 before summing into `wet`. With finite-Q bandpass filters, adjacent bands always overlap on their skirts. Inverting alternate bands creates **systematic destructive interference at every band boundary**, producing a `+, −, +, −, …` notch pattern across the spectrum. This is precisely the alternating-sign zigzag the per-band LUT learned to chase. The inline comment cites a "KVR tip" that this "smooths overlap" — but at BW=10% with 40 bands the overlap is small and the inversion creates more notches than it fixes.

## Root cause 2: SVF carrier regression vs Master Findings

See `Dev Notes/Ableton Vocoder - Master Findings Log` (March 2026), which documents the cracked architecture as:

- **4-stage biquad cascade** (8th-order carrier)
- maxQ = 37
- analysis Q = 0.60·carrier Q
- RMS error ±0.5 dB across BW 10–170%

`PluginProcessor.cpp:3834-3843` now uses **two cascaded SVF stages = 4th order**, not 8th order, with a `1/Q²` level compensation:

```cpp
auto& svf1 = (ch == 0) ? b->svfCarrierL1 : b->svfCarrierR1;
auto& svf2 = (ch == 0) ? b->svfCarrierL2 : b->svfCarrierR2;
float qComp = 1.0f / juce::jmax (0.5f, b->bandQ * b->bandQ);
```

The architecture was switched (probably to support smooth `setCutoffFrequency` for formant shifting) and the calibration was never redone. The level compensation can fix gain but cannot fix the **wider skirts** of a 4th-order cascade vs the original 8th-order biquad cascade. This is a regression vs the documented "cracked" state.

## Root cause 3: Missing cos(w)^1.29 Q correction on SVF carrier

Line 1754-1759 computes `Qcarrier *= cos(πf/fs)^1.29` for the IIR carrier path, but line 3840 reads `b->bandQ` directly when configuring the SVF path. The painstakingly calibrated high-frequency BW compensation from March is therefore **not applied** to the active SVF carrier. Negligible below 1 kHz, but explains +1 to +3 dB residual at 8–18 kHz that the LUT was also chasing.

## The overfit LUT (preserved for posterity)

```cpp
static constexpr float linkedBandCorrectionDb[40] = {
    +2.38f, +5.51f, +6.40f,+15.00f,-10.47f,-13.41f,-14.20f, +9.49f,
   +11.54f, +6.17f,+10.34f,-22.90f,-22.86f, +7.32f,-25.00f,+13.29f,
   -17.49f,-18.98f,+13.83f,+14.54f,+10.89f,-13.92f, +5.50f, +6.72f,
   +10.05f,-17.64f,-25.00f,-10.55f, +2.67f, -3.70f, +2.15f, +2.11f,
    +2.18f, +2.40f, +2.78f, +3.26f, +3.72f, +3.86f, +3.24f, +2.46f
};
```

Notes:
- Bands 11/12/14/15/17/26 pinned at the ±25 dB clamp.
- Adjacent bands oscillate with ±20 dB swings.
- Tail (bands 30–39) is smoother because the optimizer ran out of headroom.

## Region match achieved (with the LUT, on DRY-EXPORT 1.wav)

| Region | BR | ABL | Δ |
|---|---|---|---|
| Sub 20-80 | -7.0 | -6.6 | -0.3 |
| Bass 80-200 | -18.4 | -18.0 | -0.5 |
| Low 200-500 | -19.3 | -19.7 | +0.4 |
| Mid 500-2k | -20.2 | -19.9 | -0.3 |
| HiMid 2-6k | -44.1 | -43.7 | -0.4 |
| High 6-12k | -51.9 | -52.2 | +0.2 |
| Air 12-18k | -66.2 | -65.3 | -0.9 |

±1 dB across all regions — but only on this single file.

## Why this is overfit, not a crack

- **Single input file**: only `DRY-EXPORT 1.wav` (sub-rich kick/bass) was used for fitting.
- **Single carrier mode**: `Self`.
- **Single bandwidth**: BW = 10%.
- **Single band count**: 40.
- **Zigzag LUT shape**: a real DSP correction is smooth. A ±25 dB alternating pattern is the optimizer chasing a systematic interference artifact (the phase inversion), not real per-band correction.
- **No validation against vocals, drums, sweeps, or sines**: the LUT will almost certainly fail on any input that occupies different bands than the kick/bass loop.

## Plan forward

Full plan: `/Users/ricosan/.claude/plans/encapsulated-snuggling-flamingo.md`

- **Phase A — Diagnostic** (no plugin changes): bypass the LUT and bypass the phase inversion separately, re-render `DRY-EXPORT 1.wav`, and confirm that disabling phase inversion collapses the zigzag. Run a single-band isolation sweep to measure actual Q / gain / skirt slope per band on both BR and Ableton.
- **Phase B — Structural fixes**: remove or gate the phase inversion (line 3849), restore `cos(w)^1.29` Q correction to the SVF path (line 3840), and (if Phase A confirms 8th-order Ableton) restore the 4-stage biquad cascade for Linked mode when formant shift is zero. Reset `linkedBandCorrectionDb` to all zeros.
- **Phase C — Multi-input validation**: build `validation_suite.py` that renders BR + Ableton on vocal, snare, sweep, white noise, pink noise, and three single sines (50 Hz, 1 kHz, 10 kHz). Pass = all 7 regions within ±2 dB on all inputs simultaneously. See `Band Ritual - Validation Suite Spec`.
- **Phase D — Parameter sweep**: once C passes, sweep BW × band count × gate × depth × formant × noise. Pass = max region error ≤3 dB across the matrix.

## Status

**HYPOTHESIS** — being tested 2026-04-08. The structural-fix approach has not yet been built; the diagnostic Phase A measurements are the next step.
