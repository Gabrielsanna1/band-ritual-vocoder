---
tags:
  - band-ritual
  - vocoder
  - cracking
  - final
date: '2026-03-22'
status: near-complete
---
> [!warning] CORRECTED 2026-06-15 — see `Band Ritual - Grid VERIFIED Pure-Log 2026-06-15`
> The **"34.5–15200 Hz actual centers"** grid and the **"20 cents RMS / grid artifact"** framing
> below are **WRONG** — an artifact of a peak-search window clipped to 25 Hz–17 kHz that corrupted
> the band-1 and band-40 measurements. **Truth:** Ableton's grid is **pure log 20 Hz → 18 kHz,
> `i/(N-1)`, 302 c/band**, and **BR already matches it to 7.0 c RMS** (the measurement floor).
> Do NOT re-introduce any endpoint inset or bake a measured-center table. The narrow-BW "floor"
> is a measurement artifact, not an audio difference. Everything else in this note (filter order,
> Q tables, envelope) still stands.

# Band Ritual — Vocoder Cracking: Final Status (2026-03-22)

## FULLY CRACKED PARAMETERS

### Carrier Filter
- **2 cascaded IIR biquad bandpass stages** (4th-order)
- Q per stage: exact lookup table from isolated Band 20 recordings

### Q Lookup Table (40 bands, measured from Ableton)
| BW% | Q per stage |
|-----|-------------|
| 10 | 85.57 |
| 15 | 42.44 |
| 20 | 31.62 |
| 30 | 29.01 |
| 40 | 15.83 |
| 50 | 17.46 |
| 70 | 9.08 |
| 100 | 8.49 |
| 130 | 6.52 |
| 200 | 4.18 |

**Note**: Q curve is NOT a simple 1/BW formula. Has a sharp knee between BW=10-30%. Non-monotonic at BW=40-50% (possibly measurement artifact — needs verification).

### Band Frequency Grid
- `logspace(34.5, 15200, 40)` or equivalently KVR formula `center[i] = 34.5 * (15200/34.5)^(i/39)`
- Standard vocoder formula uses `pow(H/L, 1/N)` not `1/(N-1)` — but with adjusted endpoints both give same result
- Alignment: **20 cents RMS** vs Ableton (measurement resolution limit)
- Ableton's user-facing range (20-18000 Hz) maps to actual band centers (~34.5-15200 Hz)

### Envelope Follower
- **One-pole lowpass of |x|** (not peak detector with attack/release)
- Time constant from Attack parameter (~1ms)
- This change improved wide-BW PVR from 4.4→2.5 dB (target 1.7)

### Tilt Correction
```
tiltAlpha = -0.259 × widthNorm + 0.186
tiltCorr = pow(freq / 1000.0, tiltAlpha)
```

### Level Correction (bwGain v3, for one-pole envelope)
| wN | BW% | bwGain (dB) |
|-----|------|-------------|
| 0.000 | 10% | 51.7 |
| 0.105 | 30% | 44.0 |
| 0.211 | 50% | 41.2 |
| 0.316 | 70% | 40.1 |
| 0.474 | 100% | 39.6 |
| 0.632 | 130% | 39.0 |
| 0.842 | 170% | 37.7 |
| 1.000 | 200% | 36.1 |

### Other Settings
- Analysis: 3 stages, Qa = 0.6 × Qc (analysis doesn't affect PVR)
- Attack: 1ms (Ableton's minimum)
- Release: 50ms
- Frequency range: 20-18000 Hz (user-facing), 34.5-15200 Hz (actual centers)

## FINAL PSD RESULTS (avg 4.1 dB)
| BW% | PSD RMS | Level | PVR Δ | Status |
|-----|---------|-------|-------|--------|
| 10% | 8.9 | 0.0 | -7.6 (grid artifact) | Grid artifact |
| 30% | 6.0 | -0.1 | -1.0 | Close |
| 50% | 4.8 | -0.2 | +1.1 | ✓ |
| 70% | 2.7 | +1.7 | +1.0 | ★ |
| 100% | 3.3 | 0.0 | +1.8 | ✓ |
| 130% | 2.5 | 0.0 | +1.0 | ★ |
| 170% | 2.1 | 0.0 | +1.2 | ★ |
| 200% | 2.8 | 0.0 | +1.3 | ★ |

## WHAT THE REMAINING ERROR IS
- **BW=10% (8.9 dB)**: Proven to be 20-cent frequency grid misalignment artifact. At Q=85, 20 cents causes 14 dB apparent PVR loss. True filter PVR = 44.9 dB (matches Ableton's 41.5).
- **BW=30% (6.0 dB)**: Same grid artifact at lower magnitude (Q=29).
- **BW=50-200% (2.1-4.8 dB)**: Mix of grid artifact and ~1 dB real PVR difference.
- **20 cents is the measurement resolution limit** from PSD peak detection.

## HOW TO IMPROVE FURTHER
1. Record isolated single bands at more band positions (Band 10, 15, 25, 30) to build exact 40-point frequency table
2. This would reduce grid error from 20 cents to <5 cents
3. Would primarily improve BW=10% and BW=30% scores

## RESEARCH FINDINGS (web search)
- Standard vocoder formula: `center[i] = L × (H/L)^(i/N)` (KVR Audio forums)
- Ableton uses log-spaced centers (confirmed by all sources)
- "Precise" mode = constant BW, "Retro" mode = constant-Q
- No source publishes exact Ableton vocoder parameters
- Someone reverse-engineered Ableton Live 12 internals via DLL injection (KVR) but didn't publish vocoder DSP details

## FILES MODIFIED (from original V3)
- `PluginProcessor.cpp` ~line 1421: frequency grid → effectiveLow/High
- `PluginProcessor.cpp` ~line 1561: Q → 10-point lookup table
- `PluginProcessor.cpp` ~line 1696: Q clamp → 0.5 to 200
- `PluginProcessor.cpp` ~line 1715: Qa ratio = 0.6 × Qc
- `PluginProcessor.cpp` ~line 1757: tiltCorr → pow(f/1000, alpha)
- `PluginProcessor.cpp` ~line 3513: envelope → one-pole lowpass of |x|
- `PluginProcessor.cpp` ~line 3585: carrier stages → 2
- `PluginProcessor.cpp` ~line 3822: bwGain → v3 piecewise linear table
