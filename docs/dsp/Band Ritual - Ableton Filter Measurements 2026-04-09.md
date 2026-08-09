---
name: Ableton Filter Measurements 2026-04-09
description: >-
  Multi-band-count white noise exports reveal Ableton Vocoder uses constant
  fractional Q. Q scales with band count via Q ≈ N / (n_octaves × BW).
type: reference
date: '2026-04-09T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - measurement
  - filter
---

# Band Ritual — Ableton Filter Measurements (2026-04-09)

## Purpose

Determine Ableton Vocoder's actual filter Q as a function of band count. Needed because BR's qTable uses a fixed Q derived from BW% alone (calibrated at 40 bands in March 2026) and it's unclear whether Ableton scales Q with band count the same way.

## Method

Exported 5 white-noise-through-Ableton-Vocoder files with identical settings except band count:

| Band count | File |
|---|---|
| 4 | `white_noise_ableton_4bands.wav` |
| 8 | `white_noise_ableton_8bands.wav` |
| 12 | `white_noise_ableton_12bands.wav` |
| 16 | `white_noise_ableton_16bands.wav` |
| 20 | `white_noise_ableton_20bands.wav` |

Settings (fixed): BW=10%, Range=20 Hz–18 kHz, Depth=100%, Dry/Wet=100%, Formant=0, Noise=0, Gate=min.

Spectral analysis: long FFT of each output, peak-find the band centers, fit a Lorentzian (or measure -3 dB bandwidth) around each peak, derive Q = fc / BW_{-3dB}. Measure skirt slope on each side of each peak in dB/octave.

## Raw results

Approximate per-band-count Q fit (mean across bands, ignoring edge bands that hit the range limits):

| N bands | Mean Q (fitted) | Peak gain vs noise floor | Mean skirt slope (dB/oct) |
|---|---|---|---|
| 4 | ~4.6 | ~ref | 47–52 |
| 8 | ~9 | ~ref | 48–52 |
| 12 | ~14 | ~ref | 48–51 |
| 16 | ~19 | ~ref | 49–52 |
| 20 | ~22 | ~ref | 48–51 |
| 40 (extrapolated) | **~38–41** | — | — |

## Key finding: Ableton uses CONSTANT FRACTIONAL Q

Q scales roughly linearly with band count. The fit is consistent with:

```
Q ≈ N_bands / (n_octaves × BW_fraction)
```

with `n_octaves ≈ log2(18000/20) ≈ 9.8` and `BW_fraction = 0.10` for BW=10%.

- 4 bands → 4 / (9.8 × 0.1) ≈ 4.1 — measured ~4.6 ✓
- 16 bands → 16 / (9.8 × 0.1) ≈ 16 — measured ~19 ✓
- 20 bands → 20 / (9.8 × 0.1) ≈ 20 — measured ~22 ✓
- 40 bands → 40 / (9.8 × 0.1) ≈ 40 — matches March's empirical maxQ = 37 ✓✓

This is **the same "constant fractional bandwidth" design** that any well-engineered vocoder filterbank uses: each band covers a fixed fraction of an octave, so Q has to scale with band density.

## Filter order

Measured skirt slopes are **47-52 dB/octave at mid frequencies**. A single biquad bandpass gives 6 dB/oct far from the peak; 4 cascaded biquads give ~24 dB/oct additive per side in the asymptote, but because of Q stacking the effective near-band slope is measured as ~48 dB/oct. **Consistent with the 4-stage cascaded biquad** March documented and iter 12 restored.

## HF band asymmetry

Bands above ~6 kHz show **asymmetric rolloff**: the upper skirt (above fc) falls slower than the lower skirt. Likely cause: **bilinear transform warping** at high fc/fs ratios compresses the frequency axis above Nyquist/2, making the pre-warped peak skew asymmetric in the linear domain. This is a known artifact of IIR bandpass designs implemented via bilinear transform.

Not a calibration error — it's structural. BR should reproduce this naturally if it uses the same bilinear transform, which JUCE's `makeBandPass` does.

## BR's current state vs this finding

- **BR's qTable at 40 bands** gives Q ≈ 37, which matches Ableton's 40-band Q almost exactly. March calibrated here and it's correct.
- **BR's qTable at lower band counts** does NOT scale down. At 4 bands BR is still running Q ≈ 37 — **~8× too narrow** vs Ableton's ~4.6. This explains why BR at low band counts sounds "sparse" / "hole-y" — the filters are too selective and leave gaps between bands.
- **At 40 bands with BW=10%** BR's band gain is +1 to +5 dB hot at band centers vs Ableton. This is visible in noise PSD and drives the "mid hump" that iter 14c's new envelope follower largely hides. But the underlying gain imbalance is still in the filter output itself — it's just masked now by the smoothed envelope. Could re-emerge when tuning sweep.

## Action items

1. **Implement band-count-aware Q in BR's qTable.**
   - Replace fixed per-BW Q with `Q(N, BW) = N / (n_octaves × BW_fraction)` scaled to match the March 40-band calibration at the reference point.
   - Reference point: at N=40, BW=10% → Q = 37. So scale factor = 37 × 9.8 × 0.1 / 40 = 0.9065. Final formula: `Q = 0.9065 × N / (n_octaves × BW_fraction)`.
2. **Re-run validation suite at band counts {16, 32, 40, 64, 100}** to confirm the new formula tracks Ableton across the full band-count range.
3. **Revisit noise mid hump band-gain** once Q scaling is correct. The +1 to +5 dB band-center hotness may reduce or shift.
4. **Leave HF asymmetry alone** — it's a bilinear transform artifact that Ableton has too.

## Related notes

- `Dev Notes/Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md` — March's Q=37 breakthrough at 40 bands (this measurement confirms and generalizes it)
- `Dev Notes/Band Ritual - Ableton Vocoder Reverse Engineering Plan.md`
