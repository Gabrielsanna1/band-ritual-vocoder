---
tags:
  - band-ritual
  - vocoder
  - cracking
  - results
date: '2026-03-22'
status: in-progress
---
# Band Ritual — Vocoder Cracking Results (2026-03-22)

## CRACKED Parameters (all confirmed from measurements)

### Carrier Filter
- **2 cascaded biquad stages** (4th-order bandpass)
- **Q per stage = 856 / BW%** (for 40 bands)
- IIR makeBandPass coefficients

### Band Frequency Grid
- **logspace(34.5, 15200, 40)** — NOT logspace(20, 18000)
- Ableton offsets band centers inward: low × 1.725, high × 0.844
- Alignment: RMS = 20 cents vs Ableton's peaks (was 1100 cents before fix)

### Tilt Correction
```
tiltAlpha = -0.259 × widthNorm + 0.186
tiltCorr = pow(freq / 1000.0, tiltAlpha)
```
Error: < ±0.33 dB/octave at all BWs.

### Level Correction (bwGain piecewise linear, dB)
| wN | BW% | bwGain |
|-----|------|--------|
| 0.000 | 10% | 47.9 |
| 0.105 | 30% | 39.7 |
| 0.211 | 50% | 36.5 |
| 0.316 | 70% | 35.1 |
| 0.474 | 100% | 34.2 |
| 0.632 | 130% | 33.3 |
| 0.842 | 170% | 31.6 |
| 1.000 | 200% | 29.8 |

Level error: ~±0.5 dB at all BWs.

### Envelope
- Attack: 1 ms (Ableton's minimum)
- Release: 50 ms

## Current PSD Results (avg 4.3 dB)
| BW% | PSD RMS | Level | PVR Abl | PVR Ours | Status |
|-----|---------|-------|---------|----------|--------|
| 10% | 8.8 | +0.3 | 41.5 | 32.1 | PVR gap |
| 30% | 5.6 | +0.3 | 20.1 | 18.0 | Close |
| 50% | 4.1 | +0.3 | 9.4 | 11.1 | ✓ |
| 70% | 3.4 | +0.2 | 3.8 | 6.6 | ✓ |
| 100% | 3.0 | -0.1 | 1.7 | 4.4 | ★ |
| 130% | 2.6 | -0.4 | 1.8 | 3.9 | ★ |
| 170% | 3.0 | -0.7 | 1.8 | 5.2 | ★ |
| 200% | 4.1 | -0.9 | 1.5 | 8.0 | ✓ |

## Remaining Issues
1. **PVR at BW=10%**: 32.1 vs 41.5 (Δ=-9.4) — valleys not deep enough
2. **PVR at BW=70-200%**: too high — valleys too deep (Q too high for wide BWs in context)
3. **bwGain needs recalibration** for the new grid (levels shifted slightly)

## Files Modified (from original V3)
- `PluginProcessor.cpp` line ~1421: frequency grid → effectiveLow/High (×1.725, ×0.844)
- `PluginProcessor.cpp` line ~1557: Q formula → 856/BW%
- `PluginProcessor.cpp` line ~1696: Q clamp → 0.5 to 200
- `PluginProcessor.cpp` line ~1757: tiltCorr → pow(f/1000, -0.259*wN+0.186)
- `PluginProcessor.cpp` line ~3585: carrier stages → 2 (was 4)
- `PluginProcessor.cpp` line ~3800: bwGain → piecewise linear table

## Key Discovery: Frequency Grid Was the #1 Problem
The single biggest improvement came from fixing the band frequency grid. Our bands were at completely wrong frequencies (1100 cents = ~1 octave off). Fixing this dropped BW=10% error from 19.7 to 8.8 dB. The filter shape, Q, and gain corrections were all correct — but applied at wrong frequencies.
