# Band Ritual — Filter Order Correction (2026-03-16)

## The RE Plan's "CONFIRMED: 4th-order" was WRONG

Flawed math: "50 dB notch / 2 = 25 dB per pass = 4th-order"

## Measured data (filter shape analysis):
| Metric | Ableton | Band Ritual (2 stages) |
|---|---|---|
| Peak-to-valley ratio | **45.9 dB** | 26.2 dB |
| P/V low freq | 32-43 dB | 11-17 dB |
| P/V high freq | 48-52 dB | 34-41 dB |

2 stages → 26 dB. Ableton 46 dB → likely **3-4 stages** (6th-8th order).

## Fix: Add 3rd carrier stage (6th-order)
- Add carrierFilterL3/R3 + one more processSingleSampleRaw
- Expected P/V: ~39 dB (closer to Ableton's 46)
- Will need compensation recalibration

## Test Results (2026-03-16)

| Stages | Order | P/V Ratio | vs Ableton (45.9) |
|---|---|---|---|
| 2 | 4th | 26.2 dB | -19.7 gap |
| 3 | 6th | 31.5 dB | -14.4 gap |
| 4 | 8th | **40.8 dB** | **-5.1 gap** |

4 stages = best match. Now need to recalibrate compensation chain for 8th-order carrier energy.

## Full Analysis: Is 8th-order the right direction? (2026-03-16)

### What 8th-order FIXES:
1. **Valley depth**: 26→41 dB P/V (Ableton=46) — the BIGGEST visual difference in the curves
2. **Band isolation**: sharper peaks = cleaner vocoding, better articulation
3. **Low-freq P/V consistency**: 4th-order has 11-17 dB at low freq, 8th-order should be 25-35 dB

### What 8th-order DOESN'T fix:
1. **Offsets** — will need full recalibration of qComp, widthComp, depthComp, overlapComp
2. **Tilts** — compensation chain needs retuning
3. **Sub-bass excess** — sub-bass correction already in place, may need adjustment
4. **Per-peak height deficit** — may improve (deeper valleys mean peak energy is more concentrated) or worsen (narrower carrier passes less total energy)

### What 8th-order might WORSEN:
1. **CPU cost**: 4 biquads × 2 channels × 40 bands = 320 filter ops/sample (was 160)
2. **Transient response**: higher order = longer ring-up time = slower attack on fast transients
3. **Compensation difficulty**: the existing chain was tuned through 9+ iterations for 4th-order

### Alternative directions considered:
A. **Stay at 4th-order** (IIR-9: ±0.50 dB offsets, ±0.45 tilts) — good numbers but curves look different
B. **Go to 8th-order + recalibrate** — matches the SHAPE, then tune the LEVEL
C. **Different filter topology** (SVF, Butterworth) — risky, unproven
D. **Mixed order per band** — complex, hard to calibrate

### RECOMMENDATION: B (8th-order + recalibrate)

The valley depth is the single most visible difference between our curves and Ableton's. With 8th-order, the SHAPE matches. The LEVEL calibration (offsets, tilts) is iterative work we've done before and can do again.

The IIR-9 result (±0.50 dB) had good offsets but the curves LOOKED wrong because of shallow valleys. The user noticed this immediately. Matching the SHAPE first, then tuning the LEVEL, is the right order.

### Recalibration plan for 8th-order:
1. Measure raw 8th-order offsets at all BW (already done: -13 to -26 dB too quiet)
2. Increase widthComp maxBoostDb to compensate for less carrier energy
3. Adjust qComp exponent (0.35 was for 4th-order)
4. Re-tune overlapComp, tiltFactor, tonalSatCorr
5. Iterate with test_stft.py until ±0.50 dB
6. Generate comparison plots and filter shape analysis to verify curves match

## 8th-Order Sine Deficit — Physics Constraint (2026-03-16)

8th-order carrier at Q=60: effective BW ≈ 8 Hz at 1 kHz.
Sine sweep passes through in ~12 ms. Ring-up time: ~76 ms. Filter never reaches full amplitude.
→ -13 dB sine deficit at LOWBW. NOT fixable with compensation.

**For real audio (voice/music), this is less of a problem** — vowel formants are relatively stable, giving the filter time to ring up. But the sine sweep test penalizes high-order filters.

**Order comparison for practical use:**
| Order | P/V ratio | Sine sweep deficit | Best for |
|---|---|---|---|
| 4th (2 stages) | 26 dB | 0 dB | Fast-changing signals |
| 6th (3 stages) | 31.5 dB | ~-5 dB (est) | Balance |
| 8th (4 stages) | 40.8 dB | -13 dB | Static/slow signals |
| Ableton | 45.9 dB | ? (need to test) | Reference |

**Key question: Does Ableton also have a sine deficit at high order?**
If Ableton uses 8th-order and ALSO has long ring-up, our sine sweep comparison is BOTH penalized equally → the deficit would cancel out. Need to check Ableton's sine sweep response at LOWBW.

**Decision: Test with 6th-order (3 stages) as compromise.** Better P/V than 4th (31.5 vs 26), less sine deficit than 8th.

## Ableton sine/noise ratio proves fast ring-up (2026-03-16)

Ableton LOWBW: sine peak 4.31, noise peak 2.07 → sine is 2x LOUDER.
If Ableton had ring-up issues, sine would be quieter. Ableton's filters ring up fine.

**New theory: Ableton uses 3 stages (6th-order) with HIGHER Q, not 4 stages with Q=60.**
- Higher Q = deeper valleys per stage (without needing more stages)
- Fewer stages = faster ring-up = sine sweep works
- 3 stages at Q=90 might give P/V ≈ 45 dB (matching Ableton)

Testing: revert to 3 stages, increase maxQ from 60 to 90.

## Sweep Round 1 Results (2026-03-16)

| Config | Stages | Q | WidthBoost | qComp | Depth | Worst Offset | Worst Tilt | PVR | Score |
|---|---|---|---|---|---|---|---|---|---|
| **IIR-9 baseline** | 2 | 60 | 5.0 | 0.35 | 10.45 | **0.37** | **0.45** | 30.8 | **2.3** |
| 6th moderate | 3 | 60 | 8.0 | 0.35 | 11.5 | 3.77 | 0.45 | 30.8 | 5.7 |
| 6th more comp | 3 | 60 | 10.0 | 0.40 | 11.5 | 7.22 | 0.54 | 30.8 | 9.3 |
| 6th higher Q | 3 | 70 | 8.0 | 0.35 | 11.5 | 3.40 | 0.67 | 32.7 | 5.4 |
| 6th lower Q | 3 | 50 | 7.0 | 0.35 | 11.0 | 6.52 | 0.60 | 28.0 | 8.9 |

**Finding:** IIR-9 (4th-order) still wins. 6th-order configs ALL overcorrected.

**Key insight:** 6th-order needs LESS compensation than expected (the 3rd stage doesn't reduce energy as much as going from 2nd to 4th order). Need Round 2 with widthBoost=3-5 for 6th-order.

**NEXT: Get 4-band Ableton recording** — isolates individual filter shape, eliminates guessing about filter order/Q. This is the single most impactful data we can get.

## DEFINITIVE: Ableton uses 8th-10th order (4-5 stages) — MEASURED (2026-03-17)

**From 4-band LOWBW Ableton recording (isolated peaks):**
| Peak | Freq | Rolloff | Order | P/V | Q |
|---|---|---|---|---|---|
| 1 | 41.7 Hz | 49.6 dB/oct | ~8th | 43.0 dB | 4.4 |
| 2 | 317.6 Hz | 61.2 dB/oct | ~10th | 46.6 dB | 6.7 |
| 3 | 2304 Hz | 53.8 dB/oct | ~9th | 48.8 dB | ~120* |

*Q measurement at 2304 Hz unreliable (FFT resolution limit)

**Conclusion:** Ableton uses 4-5 stacked biquad stages per carrier band.
Our 4th-order (2 stages) gives 24 dB/oct rolloff — HALF of Ableton's.
Our 8th-order test (4 stages) gave P/V=40.8 — close to Ableton's 43-49.

**The 8th-order sine deficit (-25 dB) was from wrong COMPENSATION, not filter ring-up.**
Ableton also has ring-up issues at high order — the comparison is relative, so ring-up cancels out.

**NEXT: Go back to 8th-order (4 stages) and recalibrate properly.**
Key: keep compensation params CLOSE to 4th-order values. The 8th-order carrier passes less energy but the relative noise/sine behavior should be similar. Only widthComp needs significant increase.

## Sweep Rounds 2-5 Summary (2026-03-17)

**8th-order (4 stages) confirmed working.** PVR=41.9 dB (vs Ableton 45.9).

**Best config so far:** wB=16, (1-wN)^3 curve, qComp=0.35, depthComp=12.0
- LOWBW noise: -0.77 (close!)
- LOWBW sine: **-13.33** (main problem — tonalSatCorr overcorrects)
- HIGHBW noise: -2.34
- HIGHBW sine: -1.50
- Tilts: -3.04 noise, -1.46 sine (all dark — need tilt correction)

**The blocking issue: 13 dB sine/noise gap at LOWBW.**
This is from tonalSatCorr (hiQCorr) which was calibrated for 4th-order. With 8th-order carrier, sine is ALREADY attenuated by the narrower filters, so the additional tonalSatCorr attenuation is excessive.

**Next: Disable or drastically reduce tonalSatCorr for 8th-order, then re-run sweep.**

## hiQExp=-0.45 BREAKTHROUGH (2026-03-17)

**LOWBW noise/sine gap: 12.6 → 0.41 dB!** Negative hiQExp BOOSTS sine at high Q to compensate for 8th-order ring-up deficit.

| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -1.77 | **-2.18** | -9.59 | -7.46 | -2.49 | -1.65 |
| Tilt | -3.04 | -1.46 | -2.06 | -1.32 | -0.60 | -0.76 |

LOWBW: gap=0.41 (was 12.6!) ✓
HIGHBW: close (-2.49/-1.65) — needs +2 dB overall
MIDBW: needs more widthComp at mid-BW
Tilts: all negative — need tilt correction for 8th-order

**Current 8th-order params:** stages=4, Q=60, widthBoost=16, (1-wN)^3, qComp=0.35, depthComp=12.0, hiQExp=-0.45, baseCorr=0.10

**Next:** Increase widthComp and depthComp to bring all offsets toward 0. Then fix tilts.

## 8th-Order Calibration Session Results (2026-03-17 continued)

### Final 8th-order parameters:
- **Stages:** 4 (8th-order carrier)
- **maxQ:** 60
- **widthBoost:** 15 (was maxBoostDb in widthComp)
- **widthComp curve:** `(1-wN)^3` (changed from `1-wN^1.6`)
- **depthComp (maxBoostDbAt100):** 12.5
- **midBwBump:** 11.0 (gaussian at widthNorm=0.21, sigma=0.12)
- **qCompExp:** 0.35
- **hiQExp:** -0.35 (NEGATIVE = boost sine at high Q, compensates 8th-order ring-up)
- **baseCorr:** 0.10
- **tiltCorr8:** 0.85 dB/octave centered at 850 Hz

### Results:
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +0.91 | -0.58 | -0.34 | +1.48 | -0.08 | +0.76 |
| Tilt | -2.19 | -0.60 | -1.21 | -0.47 | +0.25 | +0.09 |

**PVR: 41.9 dB** (Ableton: 45.9 dB) — was 30.8 with 4th-order

### What was fixed in this session:
1. **Crossfade path hiQExp** was still +0.7 (old value) → fixed to -0.35
2. **Added tilt correction** — 8th-order carrier is darker, centered boost at 850 Hz
3. **Negative hiQExp discovery** — BOOSTS sine at high Q to compensate ring-up deficit

### Remaining limitations (diminishing returns):
- **MIDBW sine +1.48** — sine/noise split inherent to single compensation chain
- **LOWBW noise tilt -2.19** — 8th-order filter shape mismatch (narrower passband = less noise from tails, can't fix with per-band gain)
- **PVR gap (41.9 vs 45.9)** — would need 10th-order (5 stages) to match exactly, but CPU cost increases

### Key lesson: hiQExp sign
- **Positive hiQExp** = cut sine at high Q (designed for 4th-order where sine was too loud)
- **Negative hiQExp** = boost sine at high Q (needed for 8th-order where ring-up is slower)
- The sign flip was the breakthrough that closed the 13 dB sine/noise gap at LOWBW
