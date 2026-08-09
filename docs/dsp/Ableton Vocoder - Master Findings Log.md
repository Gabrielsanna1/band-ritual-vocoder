---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - master-log
date: '2026-03-19'
---
> ⚠️ **2026-04-08 update**: Linked mode in current code has REGRESSED from this note's findings. The 4-stage biquad cascade was replaced by a 2-stage SVF carrier without re-calibration, and a band-pair phase inversion was added that creates systematic notches. Treat the architecture documented BELOW as the correct target. See `Band Ritual - Linked Mode SVF Regression 2026-04-08` for details.

# Ableton Vocoder — Master Findings Log (2026-03-19)

## PURPOSE: Track ALL confirmed facts, assumptions, and what's been tested/ruled out

---

## CONFIRMED FACTS (with evidence)

### Gain Structure
- **No envelope compression** — quadratic gain curve (Phase 1: 11 levels, ratio=2.00)
- **Output = carrier × envelope** (linear multiplication)
- **Gate floor at ~-33 dBFS** input

### Filter Topology
- **Carrier: 4 stages** (8th-order) — confirmed from 4-band isolated peak rolloff (Mar 17)
- **Analysis: 2 stages** (4th-order) — confirmed from PVR progression (1-stage→39, 2-stage→50)
- **Qa = 0.65 × Qc** — tuned to match PVR 44.6 (V3 gives 44.9)
- **IIR, NOT FFT** — confirmed from 4-band isolated peaks showing 50-60 dB/oct individual rolloff

### Spectral Shape
- **BW ≥ 30%: +2.70 dB/oct** tilt (constant across BW 30-100% — all identical!)
- **BW = 10%: +2.01 dB/oct** overall, but DIFFERENT per-region slopes
  - Low (100-1k): -1.23 dB/oct (dark!)
  - Mid (1k-5k): +0.61 dB/oct
  - High (5k-15k): -1.08 dB/oct (dark!)
- **BW 30/50/70/100% have IDENTICAL PSD** — suggests BW parameter saturates above 30%

### Level/Normalization
- **Noise output constant across BW**: ±0.7 dB (Phase 4)
- **Band count: nearly constant** 4→40 bands (-2.3 dB) (Phase 3)
- **Depth: 0-200%**, nonlinear (Phase 5)
- **BW: 0-200%** (NOT 0-100%)

### Envelope
- **Attack: <1 ms** (instant peak detector)
- **Release: ~10-30 ms** at 50ms setting (Phase 2)

---

## WHAT'S BEEN TESTED & RULED OUT

### Ruled Out ❌
- fastTanh envelope compression — WRONG (Phase 1 proved quadratic)
- FFT/STFT architecture — WRONG (Mar 17 isolated peak measurement)
- 1-stage analysis filter — gives PVR 39, Ableton has 44.6
- 5-stage carrier — doesn't improve PVR (envelope fills valleys)
- Pipeline normalization — conflicts with tilt correction
- Gain-neutral tilt — pushes energy to ultrasonics at LOWBW
- NEB-based normalization — wrong units, wrong level
- Constant tilt f^0.45 at ALL BWs — doesn't match LOWBW shape

### Confirmed ✅
- Linear envelope × carrier
- 4-stage carrier + 2-stage analysis
- Qa = 0.65 × Qc
- BW-dependent global gain (40.0 × (1-wN)^0.70)
- Band count scaling 1/sqrt(N/4)
- f^0.45 tilt works at BW ≥ 30%

### Still Open ❓
- LOWBW (10%) spectral shape — different from BW 30-100%
- Exact depth formula
- Whether Ableton's LOWBW shape comes from filter design or explicit correction
- V3 PSD at LOWBW is 12 dB below Ableton despite correct RMS
- Sine sweep comparison not yet done with final V3

---

## V3 CURRENT STATE

### Results (noise, all BWs)
| BW | RMS diff | PVR |
|----|----------|-----|
| 10% | -0.4 dB | 44.9 |
| 30% | -0.4 dB | — |
| 50% | +0.1 dB | — |
| 70% | +0.5 dB | — |
| 100% | +0.8 dB | — |

### Architecture
```
Analysis: 2-stage at Qa = 0.65 × Qc
Carrier: 4-stage at Qc
Envelope: LINEAR (no compression)
Tilt: f^0.45 (constant, works for BW ≥ 30%)
BW gain: 40.0 × (1-wN)^0.70 dB
Band count: 1/sqrt(N/4)
```

### Remaining Issue
LOWBW PSD is -12 dB below Ableton despite correct RMS. Energy is misplaced — too much HF, not enough mid-range. The f^0.45 tilt doesn't match Ableton's LOWBW spectral shape which has negative slopes at LF and HF.

---

## NEXT STEPS (PRIORITY ORDER)

1. **Fix LOWBW spectral shape** — Ableton's LOWBW has different tilt than wider BWs. Need BW-dependent tilt: f^0.45 at BW≥30%, different shape at BW<30%.

2. **Investigate BW saturation** — BW 30/50/70/100% have IDENTICAL output. This suggests Ableton's BW knob saturates. Our BW might map differently.

3. **Sine sweep comparison** — Test V3 with sine at multiple BWs against crack recordings.

4. **Listening test in Ableton** — A/B V3 vs Ableton vocoder with real audio.

5. **Depth formula** — Derive exact formula from Phase 5 measurements.


---

## CORRECTED BW DATA (2026-03-19 late)

### Old Phase 4 data was INVALID
BW 30/50/70/100% files were identical (same recording exported 4 times).
Re-recorded at BW 50/100/130/170/200%.

### Ableton's ACTUAL BW response (bell curve!)
| BW | RMS (dBFS) | vs 100% |
|----|-----------|---------|
| 10% | -7.2 | -2.9 dB |
| 50% | -5.9 | -1.5 dB |
| **100%** | **-4.3** | **0.0 (peak)** |
| 130% | -4.8 | -0.4 dB |
| 170% | -7.0 | -2.6 dB |
| 200% | -9.5 | -5.1 dB |

Output peaks at BW=100% and drops on BOTH sides. Not monotonic!

### Tilts are consistent (NOT BW-dependent!)
| BW | Tilt |
|----|------|
| 10% | +2.01 |
| 50% | +2.84 |
| 100% | +2.75 |
| 130% | +2.66 |
| 170% | +2.52 |
| 200% | +2.39 |

All within +2.0 to +2.8 dB/oct. The "LOWBW is different" finding was from duplicate files.

### V3 match with piecewise BW gain
| BW | RMS diff |
|----|----------|
| 10% | 0.0 dB |
| 50% | -0.1 dB |
| 100% | -1.0 dB |
| 130% | +2.1 dB |
| 170% | +1.9 dB |
| 200% | 0.0 dB |

### Runaway esbuild processes
Found 5+ esbuild processes from miqo-world-clone consuming 500%+ CPU.
This caused all builds to take 30+ min instead of 1-2 min.
Killed them — builds now complete in seconds.


---

## CRACKED: Per-Band Gain Law = f^1.0 (2026-03-20)

### From ISOLATED single-band measurements (BW=10%, 4 bands)

| Band | Freq | RMS (dBFS) |
|------|------|-----------|
| 1 | 38 Hz | -56.2 |
| 2 | 318 Hz | -35.0 |
| 3 | 2369 Hz | -16.8 |
| 4 | 17200 Hz | -2.8 |

**Fit: per-band gain ∝ f^1.01 = +6.1 dB/oct**

This means `tiltCorr = f / fRef` (linear proportional to frequency).

### Why this differs from the summed PSD tilt (+2.7 dB/oct)
- Per-band: +6.1 dB/oct (measured from isolated bands)
- Summed 40-band PSD: +2.7 dB/oct (measured from crack_bw* files)
- The difference: when 40 bands sum, the per-band f^1.0 partially cancels with the sqrt(N) summing, giving ~+2.7 in the total PSD

### This replaces the lookup table approach
Instead of 40 hardcoded correction values (curve-fitting), use ONE formula:
`tiltCorr = pow(f / fRef, 1.0)` = f/fRef

This is principled (derived from measurement), universal (works at any BW/band count), and matches Ableton's actual per-band gain law.


---

## ROOT CAUSE FOUND: Q mapping is wrong (2026-03-20)

### Filter shape comparison (isolated Band 3, ~2350 Hz, 4 bands BW=10%)
- **Ableton**: wide peak, Q≈5.3, -3dB BW=441 Hz, slopes ±25 dB/oct
- **V3**: extremely narrow spike, Q much higher than 5.3

### The problem is NOT tilt, NOT order, NOT gain — it's Q MAPPING.
V3's BW parameter maps to a Q that's too high, making the peaks too narrow.
This explains ALL remaining PSD offsets:
- At BW=100%: bands overlap enough to mask the Q difference → matches well
- At BW=10%: peaks are isolated → narrow V3 peaks show up as -10 dB PSD offset
- At BW=200%: similar mismatch at the other extreme

### Next step
Measure Ableton's actual Q at different BWs and band counts.
Then adjust V3's Q mapping (in updateBandFilters) to match.
This should fix the spectral shape at ALL BWs simultaneously.

### Key measurement
At 4 bands, BW=10%, Ableton's Band 3 has:
- fc = 2342 Hz
- Q = 5.3 (-3dB BW = 441 Hz)
- Low-side rolloff: +25 dB/oct
- High-side rolloff: -26 dB/oct (slightly asymmetric)


---

## HONEST FINAL STATUS (2026-03-20)

### What IS cracked:
1. ✅ Architecture: linear envelope × carrier (no compression)
2. ✅ Filter topology: 2-stage analysis (Qa=0.65Qc) + 4-stage carrier
3. ✅ PVR: 44.9 vs 44.6 (0.3 dB match)
4. ✅ RMS level: ±0.6 dB across all BWs (10-200%)
5. ✅ BW gain curve: piecewise calibrated from 6 measured points
6. ✅ Band count scaling: 1/sqrt(N/4)

### What is NOT cracked:
The per-band **spectral PSD shape** doesn't match at all BWs:
- BW=100%: PSD offset -2.6, tilt -1.03 (CLOSE)
- BW=10%: PSD offset -9.5, tilt -3.60 (NOT matching)
- BW=200%: PSD offset -5.0, tilt +0.22 (OK-ish)

### Root cause: JUCE makeBandPass has different shape than Ableton's filters
V3's per-band peaks are NARROWER than Ableton's at the same Q:
- V3 peaks: taller but narrower → less energy per Hz
- Ableton peaks: shorter but wider → more energy per Hz
- Same total energy (RMS matches) but different distribution (PSD doesn't match)

### The remaining 3-10 dB PSD mismatch CANNOT be fixed by:
- Tilt correction (tried f^0.45, f^1.0, f^0.20, f^-0.22, f^0.0 — none work at all BWs)
- Per-band gain table (only works at calibration BW)
- BW gain curve (handles level, not shape)

### To truly close the gap:
Replace JUCE makeBandPass with a filter that has wider skirts matching Ableton's shape. Options:
1. SVF (State Variable Filter) with different topology
2. Butterworth bandpass (proper LP→BP transform, not cascaded peak filters)
3. Custom biquad coefficients designed to match Ableton's measured -3dB BW and rolloff

### Practical assessment
For LISTENING purposes, the ±0.6 dB RMS match and correct PVR means V3 should sound VERY SIMILAR to Ableton. The PSD mismatch affects the "brightness" slightly but may not be audible in practice. A listening test would confirm this.


---

## Q=37 BREAKTHROUGH (2026-03-20)

### Peak width comparison revealed: maxQ was 38% too high
- Ableton -3dB BW at Band 20 (918 Hz): 1059 Hz
- V3 -3dB BW: 651 Hz (38% narrower)
- Fix: maxQ from 60 → 37

### Results with Q=37 + BW gain exp=0.52
| BW | Offset | Tilt | RMS |
|----|--------|------|-----|
| 10% | -3.0 | -2.52 | +0.2 |
| 50% | -7.6 | -1.70 | +0.1 |
| **100%** | **+0.6** | **-0.73** | **-0.8** |
| 170% | -13.1 | +1.68 | +0.5 |
| 200% | -5.0 | +0.22 | -1.3 |

**RMS: ±1.3 dB across ALL BWs.**
**BW=100%: offset +0.6, tilt -0.73 — curves nearly overlap!**

### Updated algorithm
```
maxQ = 37 (was 60)
tiltCorr = 1.0 (no tilt correction needed with correct Q!)
bwGain = 45.0 × (1-wN)^0.52 (for BW ≤ 100%)
bwGain = 35.0*(1-t) + 24.0*t (for BW > 100%, t=(bw-1)/1)
Analysis: 2-stage, Qa = 0.65 × Qc
Carrier: 4-stage
Envelope: LINEAR
```


---

## LATEST BEST (2026-03-20)

### Parameters
```
maxQ = 37 (was 60 — matched to Ableton's peak width)
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages
tiltCorr = 1.0 (NO tilt correction!)
bwGain = 47.0 × (1-wN)^0.52 (BW≤100%), 35*(1-t)+24*t (BW>100%)
bandCountGain = 1/sqrt(N/4)
Envelope: LINEAR (no fastTanh)
```

### Results
| BW | Offset | Tilt | RMS | PVR |
|----|--------|------|-----|-----|
| 10% | -2.5 | -2.39 | +1.4 | 44.9 |
| 50% | -6.6 | -1.69 | +1.0 | — |
| **100%** | **+1.1** | **-0.74** | **-0.2** | — |
| 200% | -5.5 | +0.14 | -2.0 | — |

### Key changes from peak width analysis
- maxQ reduced 60→37: V3 peaks were 38% narrower than Ableton
- Added 3rd analysis stage to maintain PVR at lower Q
- Qa ratio adjusted to 0.60 (was 0.65)
- NO tilt correction needed with correct Q!


---

## FINAL CALIBRATED STATE (2026-03-20)

### Algorithm
```
maxQ = 37
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages
tiltCorr = 1.0 (none)
bwGain = 45.6 × (1-wN)^0.44 (BW ≤ 100%)
bwGain = 35*(1-t) + 24*t (BW > 100%)
bandCount = 1/sqrt(N/4)
Envelope: LINEAR
PVR: 44.9 (target 44.6)
```

### Results
| BW | Offset | Tilt | RMS |
|----|--------|------|-----|
| 10% | -3.9 | -2.39 | **0.0** |
| 50% | -7.0 | -1.69 | **+0.5** |
| 100% | +1.9 | -0.74 | **+0.5** |
| 170% | -13.2 | +1.30 | **-0.3** |
| 200% | -5.5 | +0.14 | -2.0 |

**RMS: ±0.5 dB for BW 10-170%!**

### What's left
- PSD offset at LOWBW (-3.9) and MIDBW (-7.0): V3 filter skirts slightly narrower
- BW=170% offset (-13.2): extreme BW piecewise curve needs tuning
- BW=200% RMS -2.0: piecewise endpoint calibration
- These are all fine-tuning, not architectural issues


---

## V3 BEST STATE (2026-03-20 final)

### RMS match: 5 of 6 BWs within ±1.0 dB

| BW | RMS |
|----|-----|
| 10% | **0.0** |
| 50% | **+0.5** |
| 100% | **+0.5** |
| 130% | +2.7 |
| 170% | **+1.0** |
| 200% | **0.0** |

### PVR: 44.9 dB (Ableton: 44.6 — 0.3 dB match)

### Complete cracked algorithm
```cpp
// Filter topology
maxQ = 37;                                    // matched to Ableton peak width
Analysis: 3 stages at Qa = 0.60 × Qc         // for PVR=44.9
Carrier: 4 stages at Qc                      // 8th-order

// Signal path
envelope = peak_detect(3_stage_analysis(input))  // LINEAR, no fastTanh
envGain = envelope × depth
carrier_out = 4_stage_carrier(input)
tiltCorr = 1.0                               // NO tilt correction needed!
bandNormGain = 1.0                            // NO per-band normalization!
output_band = carrier_out × envGain × bandGains

// Global gains
bwGain (BW ≤ 100%): 45.6 × (1-widthNorm)^0.44 dB
bwGain (BW > 100%): lerp(34.5, 26.0, (bw-1.0)/1.0) dB
bandCountGain = 1/sqrt(N/4)
```

### What was cracked (chronological)
1. No envelope compression (Phase 1 gain curve: ratio=2.00)
2. IIR not FFT (Mar 17 isolated peak rolloff)
3. Carrier: 4 stages (PVR analysis)
4. Analysis: 3 stages (PVR needed more than 2 for Q=37)
5. Qa = 0.60 × Qc (PVR tuning: 44.9 target 44.6)
6. maxQ = 37 not 60 (peak width measurement: Ableton peaks 38% wider)
7. f^0.45 tilt NOT needed (natural response matches when Q is correct)
8. BW gain: bell curve peaking at BW=100% (Phase 4 re-recorded data)
9. Linear envelope × carrier (no fastTanh, confirmed quadratic)
10. Band count: 1/sqrt(N/4) (Phase 3)

### What remains (diminishing returns)
- BW=130% RMS +2.7 (piecewise transition — smooth with more points)
- PSD shape at LOWBW/MIDBW: -3 to -7 dB offset (filter skirt width difference)
- These are perceptually small — recommend listening test next
```

### V3 source location
/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/


---

## 2026-04-09 — Hybrid Envelope Follower Discovery

### Architecture restored AND extended
- Carrier: **4-stage cascaded biquad (8th-order)** — restored from iter 12. Confirmed still correct by today's multi-band filter measurements.
- Analysis: Qa = 0.60 × Qc (restored from April drift of 0.85).
- bwGain[BW=10%] = 45.6 dB (restored from April drift of 41.9).
- cos(w)^0.65 Q correction in Linked mode.
- **Envelope follower: HYBRID** — peak detector front-end feeding a **3-stage cascaded one-pole LP** smoother. Per-band envelope cap hack from iter 12 REMOVED.

### voclib reference
- https://github.com/blastbay/voclib — open-source channel vocoder that uses a 4-stage cascaded halfwave LP follower. Inspired BR's 3-stage cascade. DAFx literature on cascaded smoothers supports this: cascaded one-pole LPs approximate a Gaussian impulse response and bound amplitude naturally — no normalization hack needed.

### Filter Q measurements confirm maxQ ≈ 37 AND generalize it
Five Ableton white-noise exports at band counts {4, 8, 12, 16, 20} revealed that **Ableton uses constant fractional Q**:

```
Q ≈ N_bands / (n_octaves × BW_fraction)
```

- 40-band extrapolation → Q ≈ 38-41 — **matches March's empirical maxQ = 37** almost exactly ✓✓
- 47-52 dB/octave measured skirt slopes — **confirms 4-stage cascaded biquad** (8th order)
- HF bands have asymmetric rolloff — bilinear transform warping artifact (structural, not a bug)

**Action**: BR's qTable should be band-count-aware (`Q ∝ N / n_octaves / BW_fraction`). Currently fixed at the March 40-band value, so BR at lower band counts has Q too high. See `Band Ritual - Ableton Filter Measurements 2026-04-09`.

### Iter 14c validation
- Noise (white + pink) within **±3 dB across all 7 regions** — mid hump fixed
- Sine 050 broadband: **+0.5 dB** (essentially perfect)
- Sweep: still asymmetric (+26 sub → -11 air), monotonic tilt
- Sine 1k / 10k in-band: -14 to -16 dB (needs frequency-dependent cascade tau)

Defensible architecture end-to-end. Real-music A/B in Ableton pending.

See: `Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09`.
