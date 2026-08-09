---
tags:
  - band-ritual
  - dsp
  - vocoder
  - calibration
date: '2026-03-14'
---
# Band Ritual — 2nd-Order Carrier Filter Experiment (2026-03-14)

## Hypothesis
Ableton's vocoder uses 2nd-order IIR bandpass (12 dB/oct) for carrier, not 4th-order cascaded (24 dB/oct). Switching to 2nd-order should match the spectral curve shapes, especially the LOWBW comb pattern.

## What Was Changed
1. **Carrier filter**: Cascaded 2×2nd-order (4th-order) → Single 2nd-order
2. **Compensation stack**: 6 systems recalibrated from scratch

## All Iterations Tested

### Baseline (IIR-7, 4th-order)
- Per-band RMS: all within ±1.0 dB ✓
- HIGHBW noise range: 0.50 dB
- LOWBW noise range: 1.64 dB
- 6 compensation systems finely tuned

### 2nd-Order, No Comp Changes
- Everything +5 to +13 dB too loud (wider passband = more signal)
- 4th-order compensations massively overcorrect

### 2nd-Order, All Comps Stripped
- Everything -8 to -26 dB too quiet
- Clear that qComp is essential (LOWBW vs HIGHBW = 16 dB difference)

### 2nd-Order, qComp Only
- HIGHBW: -4.3 dB mean, LOWBW: -12.2 dB
- Strong HF deficit (-8 to -12 dB at highs)
- Shape was OK but level completely wrong

### 2nd-Order, qComp + overlapComp
- Minimal improvement — overlapComp modulated by noisiness, doesn't help sine
- overlapComp parabolic was tuned for 4th-order overlap patterns

### Phase 3: Linear Envelope (no fastTanh)
- envGain = e × depth (instead of fastTanh formula)
- RESULT: Everything -10 to -12 dB (envelope too small without normalization)
- Tilt WORSE (fastTanh was actually helping by compressing HF bands)
- **IMMEDIATELY REVERTED** — fastTanh is essential for normalization

### Best 2nd-Order Result (current)
- depthComp: 5.5 → 2.0 dB
- widthComp: 10.5 → 12.0 dB max, curve changed to (1-wn)^2
- tiltFactor: now applies to ALL signals (not just tonal), exponent 0.12 → 0.14
- Parabolic alphas: reduced ~35% (0.022→0.015, 0.045→0.030)
- bwCompPower: 0.80 → 0.65
- **Results:**
  - HIGHBW noise: 4.28 dB range, mean +0.79 dB
  - LOWBW noise: 4.87 dB range, mean +1.05 dB
  - HIGHBW sine: mean -2.26 dB (highs -4.19)
  - LOWBW sine: mean +1.76 dB (sub-bass +5.47 outlier)

## Key Findings

### 1. Spectral Shapes ARE Better with 2nd-Order
- HIGHBW noise curves nearly overlap from 200 Hz to 10 kHz
- HIGHBW sine curves are nearly parallel
- The overall spectral envelope matches more closely

### 2. Per-Band RMS is WORSE
- IIR-7 (4th-order): all within ±1.0 dB
- Best 2nd-order: ±2-4 dB range
- The compensation stack was meticulously tuned for 4th-order over 7 iterations

### 3. fastTanh is Essential (Don't Remove)
- Provides normalization of raw envelope to useful gain range
- Raw envelope too small (0.01-0.2 range) for direct use
- Its frequency-dependent compression actually helps reduce natural 3 dB/oct tilt
- Without it: -12 dB level, worse tilt, unusable

### 4. LOWBW Comb Pattern — NOT a Filter Order Issue
- The comb peaks are at DIFFERENT POSITIONS in BR vs Ableton
- This is a FREQUENCY MAPPING issue (band center frequencies)
- Changing filter order changes notch depth but NOT position
- Would need to match Ableton's exact frequency mapping to fix peaks

### 5. Remaining Issues at 2nd-Order
- HF deficit for both noise and sine (highs -2 to -4 dB)
- LOWBW mid/upper-mid hump (+2.5 to +3.5 dB noise)
- LOWBW sub-bass sine outlier (+5.5 dB)
- The parabolic shape correction was designed for 4th-order overlap patterns

## Decision Point

**Option A: Continue with 2nd-order, keep tuning**
- Pros: Better spectral shapes, potentially closer to Ableton's actual topology
- Cons: Need to redo all calibration work (IIR-1 through IIR-7), currently 4× worse RMS range
- Risk: May never reach ±1 dB with 2nd-order (different physics)

**Option B: Revert to 4th-order (IIR-7), focus on frequency mapping**
- Pros: Already at ±1 dB, proven calibration
- Cons: Spectral shapes don't match curves exactly (but per-band is excellent)
- Key insight: LOWBW comb mismatch is frequency mapping, not filter order

**Option C: Hybrid — use 2nd-order carrier only above a frequency threshold**
- High bands (>2 kHz): 2nd-order where overlap is less critical
- Low bands (<2 kHz): 4th-order for deeper notches matching Ableton
- Complex but could get best of both

## Recommendation
The LOWBW comb pattern mismatch was the original motivation, and it turned out to be a frequency mapping issue, not filter order. The 2nd-order change improved HIGHBW shapes but degraded per-band precision. **Option B (revert to 4th-order)** is probably the most practical path — the ±1 dB IIR-7 result is already commercially excellent.

If spectral curve matching is truly the priority, Option A needs several more calibration iterations to get the compensation stack right for 2nd-order physics. The physics are fundamentally different (wider passband, more low-frequency overlap, less HF BW compression) and the existing parabolic/bwComp formulas may not capture the right shape.
