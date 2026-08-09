---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - algorithm
date: '2026-03-19'
---
# Ableton Vocoder — Cracked Algorithm (2026-03-19)

## ALL MEASUREMENTS COMPLETE — 31 recordings analyzed

---

## 1. GAIN CURVE → NO COMPRESSION (quadratic)

**Output ∝ input²** — exactly 12 dB output per 6 dB input (ratio = 2.00).

This means Ableton does simple `output = carrier × envelope` with NO nonlinear shaping:
- No fastTanh
- No sqrt
- No soft-clip
- Just linear multiplication

Gate/floor below ~-33 dBFS input (output drops to zero).

## 2. ENVELOPE SHAPE → Instant attack, fast release

- Attack: **< 1 ms** (essentially instant — peak detector)
- Release: **~10-30 ms** at the measured settings (50ms set, but self-carrier mode makes output decay faster than envelope alone)
- Confirms: **peak detector with exponential attack/release**

## 3. BAND COUNT SCALING → Nearly constant total output

| Bands | RMS | dB vs 4 bands |
|-------|-----|---------------|
| 4 | 0.438 | 0.0 |
| 8 | 0.439 | 0.0 |
| 16 | 0.425 | -0.3 |
| 20 | 0.414 | -0.5 |
| 40 | 0.336 | -2.3 |

**Total output is nearly constant regardless of band count** (only -2.3 dB from 4→40 bands). This means Ableton normalizes per-band output so the sum stays stable. The scaling is between 1/sqrt(N) and 1/N — closer to 1/sqrt(N).

## 4. BW SCALING → Almost perfectly flat for noise

| BW | Noise RMS | Sine RMS |
|----|-----------|----------|
| 10% | -7.2 dBFS | -1.7 dBFS |
| 30% | -6.5 dBFS | — |
| 50% | -6.5 dBFS | — |
| 70% | -6.5 dBFS | — |
| 100% | -6.5 dBFS | +1.2 dBFS |
| 200% | — | -4.4 dBFS |

**Noise output is constant (±0.7 dB) across all BW settings!** This confirms perfect BW normalization. Only BW=10% (very narrow) is slightly quieter.

Sine varies more (-4.4 to +1.2) because sine concentrates in one band regardless of BW.

## 5. DEPTH CURVE → Nonlinear (roughly depth²)

| Depth | RMS | Ratio to D=0 |
|-------|-----|-------------|
| 0% | 0.042 | 1.0× (dry) |
| 50% | 0.112 | 2.7× |
| 100% | 0.336 | 8.0× |
| 150% | 0.528 | 12.5× |
| 200% | 0.849 | 20.1× |

Steps: 0→50%: +8.5 dB, 50→100%: +9.5 dB, 100→150%: +3.9 dB, 150→200%: +4.1 dB.
The first two steps are ~9 dB each, the last two are ~4 dB. This suggests the depth formula is NOT linear — it's approximately quadratic at low depth and compresses at high depth.

At depth=0%: output = 0.042 (dry signal still present).

## RECONSTRUCTED ALGORITHM

```
For each band i:
  1. analysis_filtered = bandpass(input, fc[i], Q[i])     // single biquad
  2. envelope = peak_detect(|analysis_filtered|, attack, release)
  3. carrier_filtered = cascaded_bandpass(input, fc[i], Q[i], 4 stages)
  4. band_gain = 1.0 / sqrt(filter_energy[i])             // BW normalization
  5. band_output = carrier_filtered × envelope × band_gain × depth_factor

Sum all bands → output
No fastTanh. No tonal detection. No tilt correction (normalization handles it).
```

## What V3 Needs to Change

1. **REMOVE fastTanh** — confirmed Ableton doesn't use it
2. **Linear envelope multiplication** — just `carrier × envelope`
3. **Energy normalization** — precomputed per-band so noise output is BW-independent
4. **Band count normalization** — scale by ~1/sqrt(N) so total stays constant
5. **Depth formula** — nonlinear, needs further analysis (possibly depth² or similar)
6. **BW range 0-200%**, Depth range 0-200%

## Files
- All 31 recordings: `~/Desktop/vocoder-comparison/ableton_crack/`
- Test signals: `~/Downloads/VOCODER_TEST_SIGNALS/`
- Analysis: this note + `Ableton Vocoder - Cracked Gain Curve.md`
- Parameter ranges: `Ableton Vocoder - Actual Parameter Ranges.md`
