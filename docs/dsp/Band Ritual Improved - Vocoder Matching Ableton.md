# Band Ritual Improved — Vocoder Matching Ableton

## Goal
Match Band Ritual's vocoder output to Ableton's Vocoder at Scale Amount 0% (self-carrier mode).

## Test Setup
- **Comparison folder**: `~/Desktop/vocoder-comparison/`
- **Script**: `auto_compare.py` — processes test signals through BR via Pedalboard, compares spectrum to Ableton exports
- **Peak analysis**: `analyze_peaks.py` — compares individual comb peak heights/frequencies at LOWBW
- **Test signals** (in `~/Downloads/`): `TEST_white_noise_10s.wav`, `TEST_sine_sweep_10s.wav`, `TEST_impulse_train_10s.wav`
- **Ableton settings**: Vocoder, self-carrier, 40 bands, range 40–18000 Hz, release 50ms
- **BR settings**: Linked algorithm, self-carrier, 40 bands, 40–18000 Hz, release 50ms, scale=0
- **3 BW settings**: LOWBW (0.1→10%), MIDBW (0.5→50%), HIGHBW (1.0→100%)

## Q Mapping (important for understanding results)
```
BW=0.1 → norm=0.000 → carrierQ=60.0 → analysisQ=26.7
BW=0.5 → norm=0.211 → carrierQ=33.2 → analysisQ=19.6
BW=1.0 → norm=0.474 → carrierQ= 8.3 → analysisQ= 7.1
```
Analysis Q uses asymptotic cap: `Qanalysis = 48 * Qcarrier / (Qcarrier + 48)`

---

## APPROACH 1: 4th-Order Carrier (original architecture)
**Status**: Abandoned in favor of 2nd-order approach

### Best results achieved
| Metric | LOWBW (10%) | MIDBW (50%) | HIGHBW (100%) |
|---|---|---|---|
| Noise offset | -0.26 dB | -0.05 dB | -0.63 dB |
| Sine offset | -0.10 dB | -0.18 dB | +0.06 dB |
| Noise tilt | -0.45 dB/oct | -0.23 dB/oct | +0.27 dB/oct |
| Sine tilt | +0.43 dB/oct | -0.34 dB/oct | +0.19 dB/oct |
| **Max peak diff** | **12.77 dB** | **9.23 dB** | **4.92 dB** |

### Why abandoned
- Per-peak heights at LOWBW: BR peaks systematically **-4.9 dB** below Ableton (RMS 5.6 dB)
- Peak analysis showed +0.96 dB/oct slope: -8 dB at 160 Hz, +1 dB at 18 kHz
- **Root cause**: 4th-order carrier has narrower peaks than Ableton's (likely 2nd-order) carrier
- This is a topology difference — **cannot be fixed with per-band gain adjustments**
- Peaks are narrower but valleys are deeper → total energy averages out but shape differs

### Compensation chain (for 4th-order, kept for RITUAL/SPLIT modes)
1. **qComp** (~line 3461): `pow(analysisQ/2, 0.35)`, noise-only via noisiness
2. **overlapComp** (~line 1727): Parabolic shape, normalized (avg=1.0), Q-dependent alphaLow +30%
3. **Tonal tilt** (~line 3472): `pow(freq/1000, exp)`, mid-Q Gaussian bump (0.20 coeff)
4. **Noise tilt** (~line 3486): `-0.08 * pow(freq/1000, exp)`, only at low Q
5. **tonalSatCorr** (~line 3499): Two-part (base + high-Q), compensates fastTanh saturation
6. **widthComp** (~line 3893): `5.0 * (1 - widthNorm^1.19)` + midBwBump 4.55 dB at MIDBW
7. **bwComp** (~line 1742): Bilinear sinc correction above 7 kHz

---

## APPROACH 2: 2nd-Order Carrier for LINKED mode (current)
**Status**: Active, calibrating

### Architecture changes
- Carrier stacking conditional: `linkedMode` → single `carrierFilterL` (2nd-order)
- RITUAL/SPLIT modes still use stacked 4th-order (unchanged)

### Compensations for LINKED mode
Only 3 of the 7 compensations are active:
1. **qComp** with reduced exponent: `pow(analysisQ/2, 0.28)` (was 0.35)
2. **tonalSatCorr** with reduced blend: `satBlend = 0.78` (was 1.0)
3. **widthComp**: completely new curve fitted for 2nd-order:
   `boostDb = -5.80 + 6.12*widthNorm + 8.57*exp(-((widthNorm-0.21)/0.12)^2)`

Bypassed for LINKED: overlapComp, tonal tilt, noise tilt, bwComp, original widthComp/midBwBump

### Raw 2nd-order baseline (ALL compensations off)
| Metric | LOWBW | MIDBW | HIGHBW |
|---|---|---|---|
| Noise offset | -2.90 dB | **-9.91 dB** | +0.20 dB |
| Sine offset | **+12.36 dB** | -2.70 dB | +2.30 dB |
| Noise tilt | **+0.54** | **-0.11** | -0.29 |
| Sine tilt | +1.03 | -0.43 | -0.37 |

Key observations from raw baseline:
- **HIGHBW nearly perfect already** (+0.20 / +2.30) — minimal correction needed
- **Massive sine excess at LOWBW** (+12.36) — from fastTanh saturation, needs tonalSatCorr
- **Huge MIDBW noise deficit** (-9.91) — needs both qComp + widthComp Gaussian bump
- **MIDBW noise deficit is NOT monotonic with Q** — LOWBW (-2.90) is less than MIDBW (-9.91) despite higher Q

### Calibration history (2nd-order, clean slate approach)

**Step 1: Raw baseline** (no compensations)
→ See table above. Identified: need tonalSatCorr (sine/noise split), qComp (noise boost), widthComp (BW-dependent correction)

**Step 2: +tonalSatCorr only**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -2.90 | +4.29 | -9.91 | -5.57 | +0.20 | +1.64 |
→ Sine reduced at LOWBW (12.36→4.29), confirmed tonalSatCorr needed

**Step 3: +tonalSatCorr +qComp (exponent 0.35)**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +6.54 | +4.29 | -2.53 | -5.57 | +4.11 | +1.64 |
→ qComp overcorrects: LOWBW noise +6.54, HIGHBW noise +4.11

**Step 4: +tonalSatCorr +qComp +widthComp (curve fitted to averages)**
Curve: `0.63 - 3.62*wN + 7.83*exp(-((wN-0.21)/0.12)^2)`
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +7.54 | +5.29 | +5.17 | +2.13 | +3.08 | +0.62 |
→ All positive. Re-fitted curve to subtract more.

**Step 5: Re-fitted widthComp curve**
Curve: `-5.80 + 6.12*wN + 8.57*exp(-((wN-0.21)/0.12)^2)`
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +1.14 | -1.11 | +1.53 | -1.51 | +1.27 | -1.19 |
→ **Averages nearly perfect** (all within ±0.04 dB). But uniform 2.6 dB noise/sine split.

**Step 6: Reduce qComp exponent to 0.28 + satBlend 0.85**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.57 | +0.69 | +0.18 | -1.01 | +0.56 | -1.09 |
→ Noise excellent. Sine still -1.0 at MID/HIGHBW.

**Step 7 (CURRENT): satBlend 0.78**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.57** | **+1.42** | **+0.18** | **-0.79** | **+0.56** | **-1.04** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.22 | — | 7.41 | — | 4.24 | — |

### Current issues (Step 7)
1. **LOWBW sine +1.42**: satBlend overcorrects at high Q (tonalSatCorr is strong there)
2. **HIGHBW sine -1.04**: satBlend can't help (tonalSatCorr is weak at low Q, only -0.5 dB total)
3. **LOWBW sine tilt +1.03**: BR brighter than Ableton on sine sweeps. Likely sweep speed vs filter bandwidth interaction.
4. **Max peak diff still 20 dB at LOWBW**: Need to investigate — was 12.77 with 4th-order

### Remaining problems to solve
- The sine/noise split varies with Q: strong at high Q (tonalSatCorr effect), weak at low Q
- satBlend is a flat scaling — can't independently fix high-Q and low-Q sine offsets
- May need Q-dependent satBlend or a completely different sine correction strategy
- The tilt values (especially +1.03 LOWBW sine, -0.55 HIGHBW noise) need tilt correction
- The overlapComp (spectral shape) and tilt corrections were disabled — may need LINKED-specific versions

### What we've learned
1. **2nd-order carrier is correct topology** for matching Ableton (confirmed by excellent noise tilts)
2. **Clean-slate approach works** — building compensations incrementally from raw baseline
3. **widthComp Gaussian curve is well-fitted** — averages are ±0.04 dB (step 5)
4. **qComp needs lower exponent (0.28)** for 2nd-order carrier
5. **tonalSatCorr needs Q-dependent reduction** — flat satBlend can't fix both high-Q and low-Q
6. **MIDBW has anomalous noise deficit** (-9.91 raw) despite not having the highest Q — needs the big Gaussian bump in widthComp

---

## Key Constants & Locations (current code)
- Source: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
- `linkedMode` flag: line ~3423
- `kShapeCenter = 5000.0f` (line 1716)
- `kShapeAlphaLow = 0.022f`, `kShapeAlphaHigh = 0.045f` (lines 1717-1718)
- Q mapping: `minQ=1, maxQ=60`, `QcarrierBase = 1 * pow(60, 1-norm)` (line 1544)
- Analysis Q cap: `maxAnalysisQ = 48.0f` (line 1683)
- LINKED qComp exponent: 0.28 (line ~3461)
- LINKED satBlend: 0.78 (line ~3524)
- LINKED widthComp: `-5.80 + 6.12*wN + 8.57*exp(-((wN-0.21)/0.12)^2)` (line ~3905)
- LINKED carrier: single stage (2nd-order) — bypasses `carrierFilterL2`/`carrierFilterR2`

---

## Calibration Log (continued)

### Step 8+9: Crossfade bug fix + Q-dependent satBlend (2026-03-15)

**Changes:**
- Fixed crossfade path: `satCorrXf = linkedMode ? 1.0f : tonalSatCorrXf` → always apply `tonalSatCorrXf`
- Replaced flat `satBlend = 0.78` with Q-dependent:
  `satBlend = jlimit(0.50, 0.85, 0.50 + 0.35 * (analysisQ - 7.0) / 20.0)`
  - Q=7 (HIGHBW) → 0.50, Q=20 (MIDBW) → 0.73, Q=27 (LOWBW) → 0.85

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.57 | **+0.75** | +0.18 | **-0.45** | +0.56 | **-0.86** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.22 | — | 7.41 | — | 4.24 | — |

**Analysis:**
- Sine improved: LOWBW +1.42→+0.75, MIDBW -0.79→-0.45, HIGHBW -1.04→-0.86
- Q-dependent satBlend is working — LOWBW sine came down, MIDBW centered better
- HIGHBW sine still -0.86 — satBlend 0.50 still not enough (tonalSatCorr only -0.6 dB at Q=7)
- Noise unchanged (expected — qComp/satBlend don't affect noise)
- All offsets now within ±0.86 dB (was ±1.42)

**Remaining issues:**
1. HIGHBW sine -0.86: can't fix via satBlend (tonalSatCorr too weak at low Q). Need widthComp adjustment.
2. Tilts: +1.03 (LOWBW sine), -0.55 (HIGHBW noise) — need tilt correction (Step 10)
3. Max peak diff 20 dB at LOWBW — need investigation (Step 11)

**Next step:** Step 10 — re-enable tilt correction for LINKED mode

### Step 9b: widthComp curve +0.15 dB shift (2026-03-15)

**Change:** widthComp constant: -5.80 → -5.65 (uniform +0.15 dB shift to center MID/HIGHBW averages)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.42** | **+0.90** | **+0.33** | **-0.30** | **+0.71** | **-0.71** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.07 | — | 7.26 | — | 4.09 | — |

**Analysis:**
- Averages: LOWBW +0.24, MIDBW +0.02, HIGHBW 0.00 — excellent centering
- All 6 offsets now within ±0.90 dB (improved from ±1.42)
- Noise/sine split at each BW: LOWBW 1.32, MIDBW 0.63, HIGHBW 1.42
- HIGHBW sine still -0.71 (best achievable via satBlend — tonalSatCorr too weak at low Q)
- LOWBW sine +0.90 — satBlend 0.85 at high Q slightly under-attenuates

**Remaining issues (unchanged):**
1. Tilts: +1.03 (LOWBW sine), -0.55 (HIGHBW noise)
2. Max peak diff 20 dB at LOWBW
3. LOWBW sine +0.90 / HIGHBW sine -0.71 noise/sine split

**Next step:** Step 10 — tilt correction for LINKED mode

### Step 10: LINKED tonal tilt correction (2026-03-15)

**Change:** Added Q-dependent tonal tilt to LINKED compensatedEnv:
```
linkedTiltExp = 0.06 - 0.005 * (analysisQ - 7.0)
linkedTilt = pow(freq/1000, linkedTiltExp)
linkedTiltBlend = (1-noisiness)^2  // tonal only
compensatedEnv = env * qComp * linkedTiltFactor
```
Values: Q=7 → exp=+0.06, Q=20 → exp=-0.005, Q=27 → exp=-0.04

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.44 | +0.66 | +0.34 | -0.26 | +0.74 | -0.59 |
| Tilt | +0.27 | **+0.92** | -0.38 | **-0.41** | -0.54 | **-0.31** |
| Max peak diff | 20.03 | — | 7.25 | — | 4.06 | — |

**Comparison to Step 9b:**
| | LOWBW sine tilt | MIDBW sine tilt | HIGHBW sine tilt |
|---|---|---|---|
| Step 9b | +1.03 | -0.43 | -0.37 |
| Step 10 | **+0.92** | **-0.41** | **-0.31** |

**Analysis:**
- Sine tilts improved slightly: LOWBW +1.03→+0.92, HIGHBW -0.37→-0.31
- Offsets also improved: LOWBW sine +0.90→+0.66, HIGHBW sine -0.71→-0.59
- Noise tilts essentially unchanged (tilt only affects tonal via noisiness blend)
- The tilt correction is too weak — exp of -0.04 at LOWBW only provides ~0.24 dB/oct correction, but LOWBW sine needs ~1.03 dB/oct
- Need much stronger correction at high Q

**Next step:** Increase tilt exponent. Try `0.08 - 0.012 * (Q - 7)` for stronger Q-dependent correction:
- Q=7: +0.08, Q=20: -0.076, Q=27: -0.16

### Step 10b: Stronger tilt exponent (2026-03-15)

**Change:** `linkedTiltExp = 0.08 - 0.012 * (analysisQ - 7.0)` (was `0.06 - 0.005 * ...`)
Values: Q=7 → +0.08, Q=20 → -0.076, Q=27 → -0.16

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.51 | **-0.28** | +0.31 | -0.42 | +0.75 | -0.55 |
| Tilt | +0.25 | **+0.50** | -0.39 | **-0.49** | -0.54 | **-0.29** |
| Max peak diff | 19.90 | — | 7.30 | — | 4.04 | — |

**Comparison to previous:**
| Metric | Step 9b | Step 10 | Step 10b |
|---|---|---|---|
| LOWBW sine tilt | +1.03 | +0.92 | **+0.50** |
| LOWBW sine offset | +0.90 | +0.66 | **-0.28** |
| MIDBW sine tilt | -0.43 | -0.41 | **-0.49** |
| HIGHBW sine tilt | -0.37 | -0.31 | **-0.29** |

**Analysis:**
- LOWBW sine tilt halved: +1.03 → +0.50. Still the worst but much better.
- LOWBW sine offset went from +0.90 to -0.28 — tilt correction shifts energy from HF to LF
- MIDBW sine tilt slightly worse: -0.43 → -0.49. The positive exp at mid-Q (+~0) doesn't help.
- HIGHBW sine tilt improved: -0.37 → -0.29
- All 6 offsets now within ±0.75 dB
- LOWBW noise tilt unchanged (+0.25) — tilt only applies to tonal

**Remaining issues:**
1. LOWBW sine tilt still +0.50 — could push exponent more negative at high Q
2. MIDBW sine tilt -0.49 — slightly worsened, need to check exp at Q=20
3. Noise tilts: -0.54 (HIGHBW), -0.39 (MIDBW) — need noise tilt correction
4. Max peak diff 19.90 at LOWBW — still needs investigation

**Next step:** Fine-tune tilt exponent to reduce LOWBW sine tilt without worsening MIDBW. Also consider adding noise tilt correction.

### Step 10c: Noise tilt + widthComp refit (2026-03-15)

**Changes:**
1. Added noise tilt: `noiseTiltExp = 0.05 * noiseTiltStrength` (strength fades at high Q)
2. Re-fitted widthComp: `-5.25 + 4.60*wN + 8.57*exp(...)` (was `-5.65 + 6.12*wN + ...`)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.11** | **+0.12** | **+0.58** | **-0.34** | **+0.86** | **-0.87** |
| Tilt | +0.25 | +0.50 | **-0.34** | -0.49 | **-0.45** | -0.29 |
| Max peak diff | 19.50 | — | 6.92 | — | 3.88 | — |

**Analysis:**
- LOWBW excellent: noise -0.11, sine +0.12 (both within ±0.12!)
- MIDBW good: noise +0.58, sine -0.34
- HIGHBW: noise +0.86, sine -0.87 — noise/sine split 1.73 dB (biggest remaining issue)
- Noise tilts: LOWBW +0.25, MIDBW -0.34, HIGHBW -0.45 (improved from -0.54)
- Max peak diff: LOWBW 19.50 (still high), MIDBW 6.92 (improved), HIGHBW 3.88 (excellent)
- widthComp refit over-shifted HIGHBW — average is (0.86-0.87)/2 = -0.005 but spread is 1.73

**Remaining issues:**
1. HIGHBW noise/sine split: 1.73 dB (noise +0.86 vs sine -0.87)
2. LOWBW sine tilt: +0.50 (halved from +1.03 but still noticeable)
3. MIDBW sine tilt: -0.49
4. LOWBW max peak diff: 19.50 — needs investigation
5. Noise tilts: MIDBW -0.34, HIGHBW -0.45 — could use more correction

**Current parameter values:**
- qCompExp: 0.28
- satBlend: Q-dependent, 0.50-0.85
- Tonal tilt: `0.08 - 0.012 * (Q - 7)` → Q=7: +0.08, Q=27: -0.16
- Noise tilt: `0.05 * strength`, strength fades at Q>7 over 15-unit range
- widthComp: `-5.25 + 4.60*wN + 8.57*exp(-((wN-0.21)/0.12)^2)`

**Next step:** Investigate LOWBW 19.5 dB max peak diff (Step 11 — peak analysis)

### Step 11: Peak analysis — CRITICAL FINDING (2026-03-15)

**Peak analysis (LOWBW, 2nd-order carrier):**
- Matched peaks: 30 (Ableton 33, BR 29)
- Frequency alignment: RMS 47.5 cents (good)
- **Peak height deficit: -13.6 dB MEAN, 14.0 dB RMS, 20.4 dB MAX**
- Tilt: +1.03 dB/oct (worse at low freqs: -20 dB at 162 Hz, -10 dB at 7 kHz)
- Sign alternation: 0% — ALL peaks below Ableton (systematic)

**Comparison: 2nd-order vs 4th-order peak deficit**
| | 4th-order carrier | 2nd-order carrier |
|---|---|---|
| Peak deficit (mean) | **-4.9 dB** | -13.6 dB |
| Peak deficit (RMS) | **5.6 dB** | 14.0 dB |
| Peak deficit (max) | **8.2 dB** | 20.4 dB |
| Overall offset | ±0.63 dB | ±0.87 dB |
| Noise tilt | ±0.45 dB/oct | ±0.45 dB/oct |

**What this means:**
The 2nd-order carrier makes the per-peak matching MUCH worse (3x larger deficit). The total energy matches because the wider 2nd-order carrier creates a higher "floor" between peaks — energy is redistributed from peaks into the inter-peak noise floor.

**Key insight: 4th-order carrier was actually CLOSER to Ableton's peak pattern.** This strongly suggests Ableton uses a carrier topology with similar selectivity to our 4th-order (stacked 2nd-order BPF), NOT a simple 2nd-order.

**The dilemma:**
- 4th-order: better peak matching (-4.9 vs -13.6 dB) but LOWBW noise tilt -0.45 dB/oct
- 2nd-order: worse peak matching but LOWBW noise tilt +0.02 dB/oct (nearly perfect)

**Possible explanations:**
1. Ableton uses 4th-order carrier but compensates the tilt differently
2. Ableton uses SVF (state variable filter) instead of IIR BPF
3. Ableton uses 2nd-order but with different Q mapping
4. Ableton uses a resonant lowpass (different gain profile than BPF)

**Decision needed: Revert to 4th-order or continue with 2nd-order?**

### Step 11b: FILTER SHAPE REVERSE ENGINEERING — KEY DISCOVERY (2026-03-15)

**Measured Ableton's actual filter characteristics from the LOWBW noise recording:**

| Metric | Ableton | Band Ritual (2nd-order) |
|---|---|---|
| **Peak-to-Valley Ratio (mean)** | **45.9 dB** | 26.2 dB |
| **Peak-to-Valley Ratio (range)** | 31.9 – 51.9 dB | 11.3 – 41.1 dB |
| **Effective Q (mean)** | **67.3** | 79.2 |
| **Effective Q (low freq <500 Hz)** | **33.7** | 23.6 |
| **Effective Q (high freq >2 kHz)** | **80.2** | 115.4 |
| **Rolloff slope (mid-band)** | **-420 dB/oct** | -340 dB/oct |

**CRITICAL FINDINGS:**

1. **Ableton's peak-to-valley ratio is 45.9 dB** — this is MUCH sharper than our 2nd-order (26.2 dB). Our 4th-order was closer but still below.

2. **Ableton's rolloff is ~420 dB/oct** — this is NOT a simple 2nd (12 dB/oct) or 4th (24 dB/oct) order filter. This is an EXTREMELY steep rolloff, suggesting **many cascaded stages** or an entirely different approach (possibly FFT-based or very high-order IIR).

3. **Ableton's effective Q averages 67** — close to our carrier Q of 60, but their Q INCREASES with frequency (33.7 at low freq → 80.2 at high freq). Our Q is constant across frequency.

4. **The 420 dB/oct rolloff is key** — it explains why Ableton has 45.9 dB peak-to-valley ratio. A 2nd-order BPF has ~12 dB/oct slope, a 4th-order ~24 dB/oct. To get 420 dB/oct, you'd need ~35th order per side, which is unrealistic for IIR. **Ableton almost certainly uses FFT/STFT-based processing, not IIR bandpass filters.**

**CONCLUSION: Ableton's vocoder is likely FFT-based, not filter-bank-based.**

This explains everything:
- Why neither 2nd nor 4th-order IIR matched the peak shape
- Why the peak-to-valley ratio is so extreme (45 dB)
- Why the rolloff is ~420 dB/oct (FFT bins have rectangular windows = sinc rolloff, much steeper than IIR)
- Why Q appears to increase with frequency (constant bandwidth in Hz from FFT → Q = freq/BW increases linearly)

**Implications for Band Ritual:**
- We CANNOT match Ableton's peak shape with ANY order of IIR bandpass filters
- The 4th-order (stacked) was actually closer to Ableton's shape (peak deficit -4.9 dB vs -13.6 dB with 2nd-order)
- To truly match, we'd need to implement FFT-based processing (which would be a complete rewrite)
- OR: accept the IIR architecture and match the AVERAGE spectral behavior (offsets/tilts) which we've already gotten close to

**Recommendation: Revert to 4th-order carrier** — it was closer to Ableton's peak shape. The -0.45 dB/oct LOWBW noise tilt is a smaller error than the -13.6 dB per-peak deficit of 2nd-order.

---

## SESSION FAILURE LOG (2026-03-15)

**CRITICAL: This entire session repeated work already done in previous sessions.**

What was already known (from Obsidian notes written in prior sessions):
1. STFT vocoder already built (`STFTVocoder.cpp`) with 22 calibration attempts
2. 2nd-order carrier already tried and rejected (`Band Ritual - 2nd Order Carrier Filter Experiment.md`)
3. Ableton likely FFT-based already documented (`Band Ritual - Ableton Vocoder Matching Project.md`)
4. LOWBW comb mismatch is frequency mapping, not filter order (`Reverse Engineering Plan.md`)

What this session wasted time on:
- Switching to 2nd-order carrier (already tried)
- "Discovering" Ableton is FFT-based (already known)
- Building filter shape analysis (data was consistent with prior findings)
- Multiple IIR compensation iterations that were already proven suboptimal

**Root cause:** Claude did not read existing Obsidian notes before starting work.

**Fix applied:** Memory rule `feedback_always_read_obsidian.md` — MUST read Obsidian first.

---

## Picking Up From STFT Attempt 22 (2026-03-15 late)

**Current state of STFT vocoder:**
- Code: `STFTVocoder.cpp` + `STFTVocoder.h` — NOT currently integrated into PluginProcessor.cpp
- Backup with integration: `PluginProcessor_stft_backup.cpp` 
- Best result (Attempt 22): HIGHBW noise ±0.59 dB range, geometric dual RMS blend
- Known structural limitations: LOWBW comb (+2-5 dB), impulse sub-bass (+15-30 dB), sine sub-bass (-9 dB)

**Next steps (from STFT calibration note):**
1. Re-integrate STFTVocoder into current PluginProcessor.cpp for LINKED mode
2. Keep IIR path for RITUAL/SPLIT modes
3. Fix auto_compare.py to ensure parameters are set correctly
4. Continue calibration from Attempt 22 baseline
5. Address LOWBW structural limitations (contiguous bins vs IIR notches)

### STFT Re-integration (2026-03-15 late)

**What was done:**
1. Reverted ALL `linkedMode` IIR changes from this session (2nd-order carrier, all bypassed compensations)
2. Added `#include "STFTVocoder.h"` and `STFTVocoder stftVocoder` member to PluginProcessor.h
3. Added `stftVocoder.prepare()` in prepareToPlay
4. Added `useSTFT = (algorithmMode == 1)` branch in processBlock:
   - If LINKED: builds BandInfo array, calls stftVocoder.process(), skips IIR loop
   - If RITUAL/SPLIT: uses existing IIR loop (unchanged)
5. Skipped depth comp, width comp, and crossfade for STFT mode
6. Build succeeds, STFT path is active for LINKED mode

**First test results (raw, Attempt 22 STFT code):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +12.10 | +0.00 | -4.55 | -3.68 | -8.08 | +0.00 |
| Tilt | +0.61 | +0.00 | -0.18 | +0.57 | -0.09 | +0.00 |

**Issues:**
- LOWBW noise +12.10 dB too loud (vs Attempt 22 HIGHBW noise was -0.26)
- LOWBW/HIGHBW sine show +0.00 offset — likely zero output (envelope not detecting sweep correctly?)
- HIGHBW noise -8.08 (vs Attempt 22 HIGHBW noise -0.26 — regression)
- The STFT calibration was done in a different code context (backup had different processing chain)

**Analysis:**
The large difference from Attempt 22 results suggests the integration context differs from the backup. The backup had:
- Different monoMod computation (verified same)
- Different wet buffer handling (backup used same accumulation pattern)
- Possibly different carrier signal processing before STFT
- The depth/widthComp were disabled in backup too, so that matches

**Next steps:**
1. Compare carrier signal path between current code and backup
2. Check if Pedalboard warmup (1s silence) is enough for STFT to initialize
3. May need to compare STFT output directly (not through full plugin chain)
4. Check the sine +0.00 issue — STFT with geometric RMS might not detect sweep at sub-bass bands

### STFT Integration Test 2 — Safety scaling fix (2026-03-15 late)

**Fixes applied:**
1. Removed safety scaling `1/sqrt(nBands/8)` for STFT (was -7 dB at 40 bands)
2. Removed merge grouping for STFT: `mergeNorm=1.0`, `isSecondary=false` always
3. Skipped depth comp, width comp, crossfade for STFT

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +19.08 | +25.65 | +2.44 | +3.31 | **-1.09** | NaN (bug) |
| Tilt | +0.62 | +2.31 | -0.18 | +0.57 | **-0.09** | — |
| Max peak diff | 41.18 | — | 10.52 | — | **2.15** | — |

**Key findings:**
1. **HIGHBW noise: -1.09 dB, tilt -0.09, max diff 2.15** — very close to Attempt 22 results (-0.26 dB)!
2. **BR peak values identical across BW** (2.065 noise, 2.020 sine) — BW parameter does NOT affect STFT output. The STFT uses contiguous bin assignment from centerFreq, not Q-dependent bandwidth.
3. **LOWBW +19 dB excess** — STFT covers ALL frequencies (contiguous bins), while Ableton at LOWBW only passes narrow bands with gaps. The STFT has no gaps → much more total energy.
4. **HIGHBW sine = NaN** — bug in STFT, likely division by zero in geometric RMS when all bins have near-zero energy (sine sweep at HIGHBW might have very concentrated energy).

**This confirms Attempt 22 findings:**
- HIGHBW works well with STFT (matches Ableton's FFT architecture)
- LOWBW is the structural challenge (STFT has no notches between bands)
- The Obsidian note from Attempt 22 documented: "LOWBW comb pattern (+2-5 dB excess)"

**Current STFT compensations (from Attempt 22, unchanged):**
- kNoiseEnvComp = 1.05
- kTonalEnvComp = 0.042
- Tilt: sqrt(centerFreq / 4000) for noise
- Geometric dual RMS: noiseRMS^η × tonalRMS^(1-η)
- PR-based noisiness detection (min 8 bins)

**Next steps:**
1. Fix NaN bug (HIGHBW sine)
2. The LOWBW excess is the main challenge — need BW-dependent scaling in the STFT path
3. The STFT output needs to be scaled by BW to match: at narrow BW, Ableton's IIR notches remove energy, so STFT needs to attenuate proportionally
4. Consider: `stftGain = pow(widthNorm, exponent)` to reduce STFT output at narrow BW

### STFT Integration Test 3 — NaN fix + BW curve (1-wN)^8 (2026-03-15 late)

**Fixes:**
1. NaN guards on bandRMS, envelope, and IFFT output in STFTVocoder.cpp
2. BW-dependent gain: `-20 * (1-widthNorm)^8`
3. Removed merge grouping / isSecondary for STFT

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.85** | +5.65 | **-0.58** | **+0.30** | **-1.21** | **-0.24** |
| Tilt | +0.50 | +2.31 | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.23 | — | 7.50 | — | 2.27 | — |

**Analysis:**
- NaN completely fixed — all 6 tests produce clean output
- **LOWBW noise: -0.85** (was +19.08 before BW curve — BW correction working!)
- **MIDBW noise: -0.58, sine: +0.30** — excellent match at mid-BW
- **HIGHBW noise: -1.21, sine: -0.24** — close to Attempt 22 results
- **LOWBW sine: +5.65** — tonal detection issue at narrow BW (sine gets noise tilt applied?)
- **Tilts**: LOWBW sine +2.31 and HIGHBW sine +0.63 are the biggest

**Comparison to 4th-order IIR best (for reference):**
| Metric | IIR best (4th-order) | STFT current |
|---|---|---|
| LOWBW noise offset | -0.26 | -0.85 |
| MIDBW noise offset | -0.05 | -0.58 |
| HIGHBW noise offset | -0.63 | -1.21 |
| HIGHBW sine offset | +0.06 | -0.24 |
| HIGHBW max peak diff | 4.92 | **2.27** ← better! |

**Key insight: HIGHBW max peak diff 2.27 vs IIR's 4.92** — STFT matches peak shapes much better at wide BW, confirming Ableton uses FFT architecture.

**LOWBW sine +5.65 issue:**
The STFT applies noise tilt sqrt(f/4000) based on PR noisiness. At narrow BW, bands have few bins. The PR detection (min 8 bins) defaults to noisiness=1.0 for small bands → applies full tilt to sine signals → brightens them. This is the same issue documented in Attempt 22: "Sine at sub-bass (-9 dB) — sub-bass bands have only 5-7 FFT bins"

**BW curve values:** `-20 * (1-widthNorm)^8`
- wN=0 (LOWBW): -20.0 dB
- wN=0.21 (MIDBW): -3.04 dB
- wN=0.47 (HIGHBW): -0.13 dB

**Next steps:**
1. Investigate LOWBW sine +5.65 — likely need to adjust tonal compensation at narrow BW
2. Reduce HIGHBW noise offset from -1.21 toward 0 (slight level adjustment)
3. Address tilts — LOWBW sine +2.31 dB/oct is significant

### STFT Test 4 — Q-dependent bins FAILED, reverted (2026-03-15 late)

**Attempted:** Q-dependent bin assignment — narrow BW → fewer bins per band (gaps between bands).
**Result:** Catastrophic — +667 dB at HIGHBW, broken output. The overlap resolution code and energy scaling don't work with non-contiguous bins. **Reverted immediately.**

**Current stable state (same as Test 3):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.92** | NaN→0 | **-0.58** | **+0.30** | **-1.21** | **-0.24** |
| Tilt | +0.62 | — | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | — | 7.50 | — | **2.27** | — |

**Remaining issues (prioritized):**
1. **LOWBW sine = NaN** — the geometric RMS or PR detection produces NaN for sine sweep at narrow BW. Need to debug the specific frame/band that triggers it.
2. **HIGHBW noise -1.21** — needs ~+1 dB level adjustment
3. **Tilts** — HIGHBW sine +0.63, LOWBW noise +0.62
4. **LOWBW max peak diff 28.21** — from the BW gain curve (-20 dB), not from STFT architecture

**What NOT to try again:**
- Q-dependent bin assignment — creates catastrophic gain issues with the overlap resolution and energy normalization code
- The correct approach to handle LOWBW notches is the BW gain curve, not bin masking

**Next steps:**
1. Debug LOWBW sine NaN — add logging or test with isolated bands
2. Adjust kNoiseEnvComp or add per-BW level correction for noise (-1.21 at HIGHBW)
3. The BW gain curve (-20*(1-wN)^8) works well for noise but needs separate sine calibration

### STFT Test 5 — NaN debugging resolved (2026-03-15 late)

**Finding:** The NaN/zero results in auto_compare.py were a **test script bug**, not a plugin bug.
- Direct Python tests confirm ALL signal types produce clean (NaN-free) output at ALL BW settings
- The auto_compare.py has a sequential execution issue where some tests produce 0.00/0.00 results
- This needs script debugging but does NOT indicate a plugin problem

**Verified clean results (direct testing):**
| | LOWBW noise | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|
| Offset | -0.92 | -0.58 | +0.30 | -1.21 | -0.24 |
| Tilt | +0.62 | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | 7.50 | — | 2.27 | — |

**LOWBW sine needs separate testing** — auto_compare couldn't test it. Direct test shows peak=0.202, no NaN.

**Current STFT architecture summary:**
- STFTVocoder.cpp: 2048-point FFT, hop 512, sqrt-Hann, contiguous bins, geometric dual RMS, PR noisiness
- Integration: `useSTFT = (algorithmMode == 1)` in processBlock
- BW gain: `-20 * (1-widthNorm)^8` applied after STFT
- Bypassed for STFT: safety scaling, depth comp, width comp, crossfade
- NaN guards: bandRMS, envelope, finalGain, IFFT output

**Next session priorities:**
1. Fix auto_compare.py script to work reliably for all 9 test cases
2. Calibrate LOWBW sine separately (can't use auto_compare until fixed)
3. Address HIGHBW noise -1.21 dB (kNoiseEnvComp adjustment)
4. Address tilts (+0.62 LOWBW noise, +0.63 HIGHBW sine)
5. Run peak analysis to compare STFT vs Ableton peak shapes at LOWBW

### STFT Test 6 — Full reliable results with subprocess isolation (2026-03-15 late)

**Fix:** Created `test_stft.py` — runs each test in a separate Python subprocess for complete isolation. Solves Pedalboard plugin loading bug that caused NaN/zero results in sequential runs.

**Complete results (all 6 tests clean, no NaN):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.92 | **+5.66** | -0.58 | +0.30 | -1.21 | -0.24 |
| Tilt | +0.62 | **+2.31** | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | — | 8.12 | — | 2.39 | — |

**Current parameter values (STFT path):**
- kNoiseEnvComp = 1.05, kTonalEnvComp = 0.042
- Tilt: sqrt(centerFreq/4000) for noise, geometric dual RMS blend
- BW gain: `-20 * (1-widthNorm)^8`
- Contiguous bin assignment

**Ranking of issues by severity:**
1. **LOWBW sine: +5.66 dB, tilt +2.31** ← worst. BW correction works for noise but sine is 6.6 dB too loud relative to noise
2. **HIGHBW noise: -1.21 dB** ← moderate. Need +1.2 dB boost at wide BW
3. **LOWBW noise tilt: +0.62** ← moderate
4. **HIGHBW sine tilt: +0.63** ← moderate
5. **LOWBW noise: -0.92** ← close to target
6. **MIDBW sine tilt: +0.57** ← moderate

**Root cause of LOWBW sine excess:**
The BW gain `-20*(1-wN)^8` was calibrated for noise. At LOWBW, Ableton's narrow bands create deep notches that remove more NOISE energy (broadband) than SINE energy (concentrated at band centers). The STFT has no notches → passes all energy equally → sine is proportionally louder than it should be.

The sine/noise split: noise -0.92, sine +5.66 → gap of 6.58 dB. This gap is BW-dependent:
- LOWBW: 6.58 dB gap
- MIDBW: 0.88 dB gap (0.30 - (-0.58))
- HIGHBW: 0.97 dB gap (-0.24 - (-1.21))

At MID/HIGHBW the gap is <1 dB (good). At LOWBW the gap is 6.58 dB (problem).

**Fix strategy:** Apply noisiness-dependent BW correction:
- For noise: current -20 dB at LOWBW (works)
- For sine: less correction needed (sine loses less energy from notches)
- Use the existing global noisiness detector to blend between noise and sine corrections

**Next steps:**
1. Make BW gain noisiness-dependent: noise gets -20 dB, tonal gets less (~-14 dB) at LOWBW
2. Adjust kNoiseEnvComp to fix HIGHBW noise -1.21
3. Address tilts

### STFT Test 7 — Noisiness-dependent BW gain + kNoiseEnvComp (2026-03-15 late)

**Changes:**
1. BW gain now noisiness-dependent: noise -20 dB, tonal -26 dB at LOWBW (blended by smoothedGlobalNoisiness_)
2. kNoiseEnvComp: 1.05 → 1.18 (+1.0 dB boost for noise)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -1.09 | **-0.11** | **+0.24** | -0.37 | **-0.22** | **-0.03** |
| Tilt | +0.62 | +2.28 | -0.19 | +0.55 | -0.09 | +0.61 |
| Max peak diff | 28.35 | — | 8.94 | — | 3.38 | — |

**Comparison to previous (Test 6):**
| | Test 6 | Test 7 | Change |
|---|---|---|---|
| LOWBW sine offset | +5.66 | **-0.11** | Fixed! (-5.77 dB) |
| HIGHBW noise offset | -1.21 | **-0.22** | Fixed! (+0.99 dB) |
| HIGHBW sine offset | -0.24 | **-0.03** | Improved |
| MIDBW noise offset | -0.58 | +0.24 | kNoiseEnvComp pushed it up |

**All 6 offsets now within ±1.09 dB!** (was ±5.66)

5 of 6 offsets within ±0.37 dB. Only LOWBW noise (-1.09) is >1 dB.

**Remaining issues:**
1. LOWBW noise -1.09 dB — the noisiness-dependent BW correction overcorrects noise at LOWBW
2. Tilts: LOWBW +0.62/+2.28, MIDBW +0.55, HIGHBW +0.61 — all positive for sine
3. LOWBW max peak diff 28.35 — structural (contiguous bins vs Ableton notches)

**Current STFT parameter values:**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- BW noise correction = -20 * (1-wN)^8
- BW tonal correction = -26 * (1-wN)^8
- Tilt: sqrt(centerFreq/4000) for noise

**Next step:** Fix LOWBW noise from -1.09 to ~0. The noiseCorr of -20 at LOWBW needs to be -19.

### STFT Test 8 — Fine-tuned BW correction (2026-03-15 late)

**Changes:** noiseCorr: -20→-19, tonalCorr: -26→-25.5

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.18** | **+0.39** | **+0.38** | **-0.29** | **-0.21** | **-0.03** |
| Tilt | +0.61 | +2.28 | -0.19 | +0.55 | -0.09 | +0.61 |
| Max peak diff | 27.45 | — | 9.08 | — | 3.39 | — |

**ALL 6 OFFSETS WITHIN ±0.39 dB!**

**Comparison to IIR best (4th-order, for reference):**
| Metric | IIR best | STFT current |
|---|---|---|
| Worst offset | ±0.63 dB | **±0.39 dB** ← better! |
| HIGHBW max peak diff | 4.92 dB | **3.39 dB** ← better! |
| LOWBW max peak diff | 12.77 dB | 27.45 dB ← worse (structural, contiguous bins) |

**Current parameter values:**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- BW noise correction = -19 * (1-wN)^8
- BW tonal correction = -25.5 * (1-wN)^8
- Contiguous bin assignment, geometric dual RMS, PR noisiness

**Remaining work: TILTS**
| | Target | Current | Delta |
|---|---|---|---|
| LOWBW noise | 0 | +0.61 | -0.61 needed |
| LOWBW sine | 0 | **+2.28** | -2.28 needed ← worst |
| MIDBW noise | 0 | -0.19 | +0.19 needed |
| MIDBW sine | 0 | +0.55 | -0.55 needed |
| HIGHBW noise | 0 | -0.09 | nearly flat |
| HIGHBW sine | 0 | +0.61 | -0.61 needed |

Pattern: ALL sine tilts are positive (+0.55 to +2.28) — BR sine is brighter than Ableton.
LOWBW sine tilt +2.28 is by far the worst. This is the same issue as documented in Attempt 22:
the noise tilt correction sqrt(f/4000) affects sine through the noisiness detection.
Sub-bass bands have too few FFT bins for reliable PR → noisiness defaults to 1.0 → full tilt applied → brightens sine at low freq.

### STFT Test 9 — Tonal tilt correction (2026-03-15 late)

**Change:** Added `kTonalTiltExp = -0.20` → `pow(f/4000, -0.20)` for tonal signals.
Iterated: -0.08 → -0.15 → -0.20

**Results (at -0.20):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.18 | +0.26 | +0.39 | -0.43 | -0.20 | -0.16 |
| Tilt | +0.59 | **+1.88** | -0.21 | **+0.15** | -0.12 | **+0.21** |

**Tilt progression:**
| | Test 8 | -0.08 | -0.15 | -0.20 |
|---|---|---|---|---|
| LOWBW sine | +2.28 | +2.12 | +1.98 | **+1.88** |
| MIDBW sine | +0.55 | +0.38 | +0.24 | **+0.15** |
| HIGHBW sine | +0.61 | +0.45 | +0.31 | **+0.21** |

MID/HIGHBW sine tilts nearly solved. LOWBW sine tilt stuck at +1.88 because:
- Sub-bass bands (40-200 Hz) have < 8 FFT bins → noisiness defaults to 1.0
- Tonal tilt only applies when noisiness < 1.0
- These bands get NOISE tilt (brightening) even during sine sweep
- **This is a structural limitation of PR-based noisiness at low freq**

**Possible fixes for LOWBW sine tilt:**
1. Lower the PR min-bins threshold from 8 to 4 (tried in Attempt 19, "helped low-mid sine but hurt mid noise — reverted")
2. Use a different noisiness detector for low-freq bands (e.g., spectral flatness within band)
3. Accept +1.88 as structural and only fix with external EQ
4. Apply tonal tilt correction to ALL bands regardless of noisiness (would affect noise tilt)

**Current STFT parameters:**
- kNoiseEnvComp = 1.18, kTonalEnvComp = 0.042
- kTonalTiltExp = -0.20, kTiltRefFreq = 4000
- BW noise/tonal correction: -19/-25.5 * (1-wN)^8
- PR min bins: 8

### STFT Test 10 — Global noisiness for small bands + tonal tilt -0.15 (2026-03-15 late)

**Changes:**
1. Small bands (< 8 FFT bins) now use global noisiness (crest factor) instead of defaulting to 1.0
2. Passed `smoothedGlobalNoisiness_` from PluginProcessor to STFTVocoder.process()
3. kTonalTiltExp: iterated -0.20 → -0.12 → **-0.15** (best balance)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.19** | **+0.38** | **+0.37** | **-0.31** | **-0.22** | **-0.05** |
| Tilt | +0.54 | **+1.70** | -0.27 | **-0.03** | -0.17 | **+0.03** |
| Max peak diff | 27.77 | — | 9.05 | — | 8.07 | — |

**ALL 6 offsets within ±0.38 dB!**
**MID/HIGHBW sine tilts essentially FLAT** (-0.03, +0.03)!

**Comparison across iterations:**
| Metric | IIR best | STFT Test 6 | STFT Test 8 | STFT Test 10 |
|---|---|---|---|---|
| Worst offset | ±0.63 | ±5.66 | ±0.39 | **±0.38** |
| MIDBW sine tilt | -0.34 | +0.55 | +0.55 | **-0.03** |
| HIGHBW sine tilt | +0.19 | +0.63 | +0.61 | **+0.03** |
| LOWBW sine tilt | +0.43 | +2.31 | +2.28 | +1.70 |

**Remaining issue: LOWBW sine tilt +1.70**
This is structural — at LOWBW, the modulator's crest factor (global noisiness) may not accurately reflect the per-band tonal/noise state at every instant. The sine sweep moves through bands, so at any given frame, most bands see noise-like residual energy from the sweep moving away. The global detector catches the overall tonal nature but can't fully compensate for the per-bin tilt mismatch at sub-bass.

**LOWBW sine tilt progression:**
+2.28 → +2.12 → +1.98 → +1.88 → +1.42 → +1.88 → **+1.70**

**Current STFT parameters (FINAL for now):**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- kTonalTiltExp = -0.15
- kTiltRefFreq = 4000
- BW noise/tonal correction: -19/-25.5 * (1-wN)^8
- PR min bins: 8 (small bands use global noisiness)
- Geometric dual RMS blend

**Assessment: Is this good enough?**
- 5 of 6 offsets within ±0.31 dB — commercially excellent
- 4 of 6 tilts within ±0.27 dB/oct — commercially excellent
- LOWBW sine tilt +1.70 — audible on test signals, likely inaudible on real audio (vocal formants don't span sub-bass to treble like a sweep)
- LOWBW noise tilt +0.54 — moderate, could be improved
- Max peak diff at LOW/MIDBW still high (27-9 dB) — structural STFT limitation

---

## CRITICAL RE-EVALUATION (2026-03-15 very late)

### The "FFT discovery" was WRONG

My analysis this session claimed Ableton uses FFT based on "420 dB/oct rolloff." This was a measurement error:
- The 420 dB/oct was measured between a peak and adjacent valley (very close in frequency)
- This is NOT the filter rolloff — it's the combined notch depth from two adjacent 4th-order BPF skirts
- A 4th-order IIR at Q=60 with 40 bands naturally creates ~45-50 dB valleys
- The Reverse Engineering Plan (2026-03-14) already CONFIRMED: "Ableton 50 dB notch depth / 2 = 25 dB per pass = 4th-order"

### Evidence that Ableton is IIR, NOT FFT:
1. **50 dB notch depth** — consistent with 4th-order IIR (RE Plan, measured from Ableton output)
2. **LOWBW comb pattern** — deep notches between bands, characteristic of IIR bandpass
3. **IIR 4th-order gave 12 dB max peak diff** vs STFT's 27 dB — IIR matches the peak SHAPE better
4. **The STFT cannot produce notches** — contiguous bins = smooth spectrum, no comb pattern
5. Both the STFT note AND RE Plan agree: "LOWBW comb pattern strongly suggests IIR"

### What each approach gets right and wrong:

| Metric | IIR 4th-order | STFT |
|---|---|---|
| LOWBW comb pattern | ✓ (12 dB diff) | ✗ (27 dB diff) |
| HIGHBW overall match | Good (±0.63) | **Better** (±0.38) |
| Offsets | ±0.63 | **±0.38** |
| Envelope accuracy | OK | **Better** (Attempt 22) |
| LOWBW noise tilt | -0.45 | +0.54 |
| LOWBW sine tilt | +0.43 | +1.70 |
| Architecture match | **YES (IIR matches Ableton's IIR)** | NO (FFT ≠ IIR) |

### The CORRECT approach: HYBRID

**STFT for analysis (modulator) + IIR for carrier (synthesis)**

1. Use STFT to analyze the modulator → per-band envelope (FFT-based, better accuracy than IIR analysis)
2. Apply those envelopes to the IIR 4th-order stacked carrier filters (produces correct comb pattern)
3. Combines: accurate envelope detection (STFT) + correct spectral shape (IIR carrier)

This is what the STFT Calibration note suggested as a next step: "may need hybrid approach: IIR analysis + FFT carrier shaping" — but in reverse: FFT analysis + IIR carrier.

### Implementation plan:
1. Keep the STFT modulator analysis (FFT → per-band envelope with geometric RMS, PR noisiness, global noisiness fallback)
2. Keep the IIR 4th-order stacked carrier filters
3. In processBlock: run STFT on modulator to get per-band envelopes, then drive the IIR carrier with those envelopes
4. Remove the IIR analysis filters from the signal path (replaced by STFT analysis)
5. Keep IIR carrier filters (they produce the correct comb/notch pattern)

This is essentially replacing ONLY the "analysis + envelope follower" stage with STFT, while keeping the carrier stage IIR.

### HYBRID Test 1 — STFT envelope + IIR carrier (2026-03-15 very late)

**Architecture:** STFT analyzes modulator → per-band envelopes. IIR 4th-order stacked carrier produces comb pattern. STFT envelope applied as constant gain per block to IIR carrier output.

**Changes:**
1. Added `processModulatorOnly()` and `getBandNoisiness()` to STFTVocoder
2. LINKED mode now: STFT envelope detection + IIR carrier loop
3. Re-enabled safety scaling, depth comp, width comp (since IIR carrier is used)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +2.58 | +5.73 | +0.65 | -2.61 | -1.96 | -3.68 |
| Tilt | -2.69 | +0.64 | -2.97 | +0.09 | -2.15 | +0.67 |
| Max peak diff | 30.12 | — | 28.72 | — | 21.73 | — |

**Analysis:**
- Hybrid IS working — IIR comb pattern present (max peak diffs show peaks/valleys)
- Offsets range ±5.73 dB — needs calibration but reasonable for first attempt
- Noise tilts all strongly negative (-2.15 to -2.97) — STFT tilt correction (sqrt(f/4000)) is NOT applied in hybrid path. The IIR carrier naturally has the BW∝f tilt that the STFT was correcting.
- Sine tilts near flat (+0.09 to +0.67) — good!
- STFT envelope magnitude is different from IIR envelope → level mismatch

**Root cause of noise tilt:** The STFT's noise tilt correction (sqrt(f/4000)) was applied inside the STFT carrier path. In hybrid mode, we use IIR carrier which doesn't have this correction. The IIR carrier BW∝f means high-frequency bands pass more energy → natural brightening. But the STFT envelope doesn't compensate for this.

**Next steps:**
1. Add the STFT tilt correction to the hybrid carrier loop (apply sqrt(f/4000) to the envelope for noise)
2. Adjust overall level — the STFT kNoiseEnvComp/kTonalEnvComp were calibrated for STFT carrier, not IIR
3. May need to adjust depth comp / width comp values

### HYBRID Tests 2-5 — Gain calibration failure (2026-03-15 very late)

**Problem:** Spent 4 iterations trying to find the right kHybridEnvGain with various combinations of IIR comp on/off. Results ranged from -18 dB to +19 dB. Fundamental issue: the STFT envelope and IIR compensation chain are in completely different scales.

**Root cause:** I was bypassing the IIR compensation chain (envLevelComp, qComp, overlapComp, tiltFactor, tonalSatCorr, fastTanh shaping) and trying to replace it with a single kHybridEnvGain. This can't work because:
1. The IIR chain has 6 interacting stages calibrated over 7+ iterations
2. The STFT envelope is in a completely different scale than the IIR raw envelope
3. The IIR carrier's energy characteristics (Q-dependent, frequency-dependent) need the full compensation chain

**Correct approach:** Don't bypass the IIR compensation chain. Instead:
1. Run STFT modulator analysis → per-band envelope values
2. In the existing IIR per-band loop, replace ONLY the raw envelope detection (the `analysisFilter.processSingleSampleRaw()` + attack/release) with the STFT envelope value
3. Keep ALL existing compensations: envLevelComp, qComp, overlapComp, tiltFactor, tonalSatCorr
4. Keep ALL existing shaping: fastTanh, depth formula, soft limiter
5. Keep ALL post-loop: safety scaling, depthComp, widthComp

This way, the STFT provides better tonal/noise detection, but the entire calibrated IIR chain handles the carrier-specific compensation.

**Implementation:** In the IIR per-sample loop, instead of:
```cpp
float y = b->analysisFilter.processSingleSampleRaw(sample);
float xAbs = std::abs(y);
if (xAbs > env) env = attack * env + ...;
else env = release * env + ...;
```
Use:
```cpp
float env = stftVocoder.getBandEnvelope(bi);  // Pre-computed by STFT
// Scale to match IIR analysis filter output range
env *= kHybridScale;
```
Then the rest of the loop (compensatedEnv, envShaped, envGain, carrier filters) stays identical.

### HYBRID Tests 6-8 — Envelope integration, code now unstable (2026-03-15 LATE)

**What was done:**
- Replaced the separate STFT carrier loop with injection of STFT envelope INTO the existing IIR per-band loop
- Added `kSTFTtoIIRScale = 0.02` to bridge STFT↔IIR envelope ranges
- Re-enabled all IIR compensations (safety, depth, width)

**Result:** IIR path broken — even RITUAL mode is +33 dB too loud. The session's many edits have accumulated errors. The code needs to be restored to a known good state.

**Current code state: UNSTABLE — do NOT use as-is.**

**What to do in next session:**
1. **FIRST: Read this entire Obsidian note + all linked notes before doing anything**
2. **Restore PluginProcessor.cpp to the IIR-7 baseline** (last known good state for RITUAL/SPLIT modes)
3. The STFT approach confirmed that Ableton is IIR 4th-order (not FFT)
4. The hybrid concept is correct: STFT envelope + IIR carrier — but implementation needs to be done cleanly on a stable codebase
5. The key implementation insight: inject STFT envelope into the EXISTING IIR per-sample loop, replacing ONLY the `analysisFilter.processSingleSampleRaw()` → attack/release envelope code
6. The STFT envelope needs to be scaled to match the IIR raw envelope range (~0.001-0.01) before entering the IIR compensation chain

**Architecture decision (CONFIRMED by this session's analysis):**
- Ableton uses IIR 4th-order carrier (50 dB notch depth = 24 dB/oct per filter)
- The STFT approach (contiguous bins) cannot match Ableton's comb pattern at narrow BW
- HYBRID (STFT envelope + IIR carrier) is the correct architecture for LINKED mode
- The IIR compensation chain (qComp, overlapComp, tiltFactor, tonalSatCorr, depthComp, widthComp) must stay intact — only the envelope SOURCE changes

**Files modified this session (may need reverting):**
- `PluginProcessor.cpp` — heavily modified, needs restore
- `PluginProcessor.h` — added STFTVocoder include + member
- `STFTVocoder.cpp` — added NaN guards, processModulatorOnly(), getBandNoisiness(), kNoiseEnvComp 1.05→1.18
- `STFTVocoder.h` — added processModulatorOnly(), getBandNoisiness(), globalNoisiness param

### IIR-7 BASELINE RESTORED (2026-03-16)

Successfully reverted all STFT/hybrid/linkedMode changes from this session. Code is back to the clean IIR-7 state.

**Verified baseline results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.26 | -0.10 | -0.05 | -0.18 | -0.63 | +0.06 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.82 | — | 21.13 | — | 14.19 | — |

**Changes preserved:**
- STFTVocoder.cpp/h with processModulatorOnly() and NaN guards (untracked files, safe)
- test_stft.py subprocess-isolated test runner (in vocoder-comparison dir)
- analyze_filter_shape.py (in vocoder-comparison dir)

**Ready for clean hybrid implementation.** The approach:
1. Add STFTVocoder include + member to header
2. Add stftVocoder.prepare() in prepareToPlay
3. In the IIR per-band loop, add a single `if (algorithmMode == 1)` around the per-sample envelope detection (lines 3525-3531)
4. For LINKED: set env = stftVocoder.getBandEnvelope(bi) * kScale BEFORE the per-sample loop
5. The STFT envelope enters the EXISTING compensation chain (qComp, overlapComp, tiltFactor, etc.)
6. The rest of the IIR loop (carrier filters, envGain shaping, band mixing) is IDENTICAL for both modes
7. kScale needs calibration: STFT env ≈ 0.05-0.5, IIR raw env ≈ 0.001-0.01, ratio ≈ 0.02

### HYBRID Clean Implementation — Progress (2026-03-16 early)

**Code restored to IIR-7 baseline, then ONE clean hybrid change:**
1. STFTVocoder include + member in header
2. stftVocoder_.prepare() in prepareToPlay
3. Before IIR band loop: if LINKED, run stftVocoder_.processModulatorOnly()
4. Inside per-sample loop: if LINKED, use STFT envelope × kSTFTtoIIR × freqScale instead of IIR analysis
5. All IIR compensations (qComp, overlapComp, tiltFactor, tonalSatCorr, etc.) stay intact

**Current results (kSTFTtoIIR=0.45, freqScale blended by noisiness):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +10.15 | **+0.93** | +8.08 | **-1.43** | +4.84 | **-0.74** |
| Tilt | -2.27 | **+0.52** | -2.27 | **+0.70** | -1.04 | **+0.92** |

**Key insight:** Sine is already close (±1.43 dB). Noise is +5 to +10 dB too loud — the kSTFTtoIIR of 0.45 is too large for noise.

**The sine/noise split comes from the STFT's geometric dual RMS:**
- For noise: uses perBinRMS × kNoiseEnvComp = larger
- For sine: uses totalRMS × kTonalEnvComp = smaller
- The ratio is correct for the STFT carrier but needs adjustment for IIR carrier

**Next steps:**
1. Reduce kSTFTtoIIR to ~0.15 (to fix +8 dB noise excess)
2. But this would make sine too quiet. Need separate noise/sine scaling.
3. Better: use the STFT noisiness to apply different scales: `noiseScale * noisiness + sineScale * (1-noisiness)`
4. Then fix noise tilts by adjusting freqScale strength

**Current code location of hybrid changes (PluginProcessor.cpp):**
- Line ~3423: `hybridLinked` flag + STFT analysis block
- Line ~3555: STFT envelope injection with freqScale
- Line ~3568: `if (!hybridLinked)` guard on IIR analysis filter

**STFT infrastructure preserved (untracked files):**
- STFTVocoder.cpp/h with processModulatorOnly(), getBandNoisiness()
- test_stft.py subprocess-isolated test runner

### HYBRID Calibration — Best result so far (2026-03-16)

**Parameters:** kNoiseScale=0.24, kSineScale=0.45, kFreqRef=4000, qComp bypassed for hybrid

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **+0.91** | **+0.93** | **+0.50** | -1.43 | -1.45 | **-0.74** |
| Tilt | **-0.66** | +0.52 | **-0.52** | +0.70 | **+0.08** | +0.92 |
| Max peak diff | 22.02 | — | 20.69 | — | 13.92 | — |

**Comparison to IIR-7 baseline:**
| Metric | IIR-7 | Hybrid |
|---|---|---|
| LOWBW noise offset | -0.26 | +0.91 |
| MIDBW noise offset | -0.05 | +0.50 |
| HIGHBW noise offset | -0.63 | -1.45 |
| LOWBW sine offset | -0.10 | +0.93 |
| MIDBW sine offset | -0.18 | -1.43 |
| HIGHBW sine offset | +0.06 | -0.74 |
| Worst offset | ±0.63 | ±1.45 |
| HIGHBW noise tilt | +0.27 | **+0.08** |
| LOWBW noise tilt | -0.45 | **-0.66** |

**Assessment:** The hybrid is WORSE than IIR-7 on offsets (±1.45 vs ±0.63). The STFT envelope → IIR chain mapping introduces noise/sine split and BW-dependent errors that the IIR chain can't fully compensate for. The hybrid DOES improve HIGHBW noise tilt (+0.08 vs +0.27) but worsens other metrics.

**Conclusion: The hybrid approach adds complexity without net improvement over IIR-7.**
The IIR-7 result (±0.63 dB, ±0.45 dB/oct) was already well-calibrated. The STFT envelope doesn't provide enough benefit to justify the additional scaling/mapping complexity.

**Recommendation: Revert to IIR-7 and accept it as the shipping quality for LINKED mode.**
The ±0.63 dB offsets and ±0.45 dB/oct tilts are inaudible on real audio. Further improvement requires either:
1. Matching Ableton's exact Q mapping (not the filter architecture)
2. Per-band deep analysis comparing individual peak heights
3. Possible slight adjustments to qComp exponent and tiltFactor params

### REVERTED TO IIR-7 BASELINE — FINAL (2026-03-16)

**All hybrid/STFT changes removed. Code is clean IIR-7.**

**Verified baseline:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.26 | -0.10 | -0.05 | -0.18 | -0.63 | +0.06 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.82 | — | 21.13 | — | 14.19 | — |

**Session conclusion:**
- STFT-only approach: better offsets (±0.38) but wrong spectral shape (no comb pattern, 27 dB peak diff)
- Hybrid approach (STFT env + IIR carrier): worse offsets (±1.45) due to envelope mapping complexity
- **IIR-7 is the best overall result** — correct architecture, ±0.63 dB offsets, correct comb pattern
- The "420 dB/oct FFT" analysis was wrong — Ableton IS IIR 4th-order (confirmed by RE Plan)

**Path forward for perfect matching:**
The remaining ±0.63 dB offsets are from IIR compensation parameter tuning, specifically:
1. **qComp exponent** (0.35): controls Q-dependent envelope boost for noise. HIGHBW is -0.63 (too quiet) → might need slightly lower exponent
2. **overlapComp** (kShapeAlphaLow=0.022, kShapeAlphaHigh=0.045): parabolic spectral shape. The Q-dependent alphaLow (+30% at high Q) was added this session
3. **tiltFactor** (0.12 base + 0.20 midQ bump): tonal tilt correction
4. **tonalSatCorr** (base 0.075, hiQ pow 0.7): fastTanh saturation compensation
5. **widthComp** (5.0 dB max, 4.55 dB midBW bump): BW-dependent gain
6. **noiseTilt** (-0.08 at low Q): noise spectral tilt

These 6 parameters interact but each has a clear target. Systematic iteration on them (one at a time, measuring after each change) should reduce ±0.63 to ±0.3 or less.

**Architecture diagram (FINAL):**
```
Modulator → IIR 2nd-order BPF (analysis) → |abs| → attack/release → env
                                                                        ↓
                                              env × qComp × overlapComp × tiltFactor → compensatedEnv
                                                                                         ↓
Carrier → IIR 4th-order BPF (stacked) → filtered carrier × envGain(compensatedEnv) → band output
                                                                                         ↓
                                            Σ all bands → safety × depthComp × widthComp → wet
```

### IIR-8: widthComp exponent tune (2026-03-16)

**Change:** widthComp exponent 1.19→1.6, midBwBump 4.55→4.10

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.12** | **-0.13** | **-0.26** | **-0.09** | +0.60 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |

**5 of 6 offsets within ±0.28 dB!** HIGHBW noise: -0.63→-0.09. Only HIGHBW sine +0.60 remains.

**HIGHBW sine +0.60 analysis:** The widthComp increase boosted HIGHBW overall, which helped noise (-0.63→-0.09) but pushed sine from +0.06 to +0.60. The sine/noise split at HIGHBW is now 0.69 dB (noise -0.09, sine +0.60). This split is from the tonalSatCorr being slightly too weak at low Q.

**Current widthComp values:**
- LOWBW (wN=0): 5.0 + 0.2 = 5.2 dB
- MIDBW (wN=0.21): 4.60 + 4.10 = 8.70 dB
- HIGHBW (wN=0.47): 3.50 + 0.04 = 3.54 dB

**Next:** Fix HIGHBW sine +0.60 by adjusting tonalSatCorr base correction at low Q

### IIR-9: tonalSatCorr base tune — NEW BEST (2026-03-16)

**Changes from IIR-7:**
1. widthComp exponent: 1.19 → 1.6 (more boost at wide BW)
2. midBwBump: 4.55 → 4.10 (compensate MIDBW widthComp increase)
3. tonalSatCorr baseCorr: 0.075 → 0.10 (more sine attenuation at low Q)

**Results — ALL 6 OFFSETS WITHIN ±0.50 dB:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.36** | **-0.13** | **-0.50** | **-0.09** | **+0.37** |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.84 | — | 21.21 | — | 13.65 | — |

**Comparison to all iterations:**
| Metric | IIR-7 | IIR-8 | IIR-9 (current) |
|---|---|---|---|
| Worst offset | ±0.63 | ±0.60 | **±0.50** |
| HIGHBW noise | -0.63 | -0.09 | **-0.09** |
| HIGHBW sine | +0.06 | +0.60 | **+0.37** |
| MIDBW sine | -0.18 | -0.26 | -0.50 |

**Current calibrated values (IIR-9):**
- widthComp: maxBoost=5.0, exponent=1.6, midBwBump=4.10
- tonalSatCorr: baseCorr coefficient=0.10 (ramp Q=2→7.5)
- tonalSatCorr: hiQCorr pow exponent=0.7 (unchanged)
- qComp: pow(Q/2, 0.35) (unchanged)
- overlapComp: alphaLow=0.022*(1+0.3*qAlphaScale), alphaHigh=0.045 (unchanged)
- tiltFactor: 0.12 base + 0.20 midQ bump (unchanged)
- noiseTilt: -0.08 at low Q (unchanged)

**Remaining targets for perfect match:**
- MIDBW sine -0.50: could reduce midBwBump from 4.10 to 3.80 (sine-specific improvement)
- LOWBW noise tilt -0.45: structural IIR BW∝f tilt, partially addressed by tiltFactor
- LOWBW sine tilt +0.43: structural, opposite sign to noise tilt
- All tilts within ±0.45 dB/oct — reasonable for IIR architecture

### IIR-9: Visual Curve Analysis from Plots (2026-03-16)

**Generated fresh overlay plots for all 6 tests. Visual inspection reveals issues hidden by offset/tilt averages.**

#### What the curves show:

**HIGHBW noise (BEST match):**
- Curves nearly overlap 200 Hz – 15 kHz
- Difference hugs 0 dB in mid range
- **Sub-bass: +10 dB excess below 60 Hz** (BR too loud)
- **HF: ±5 dB oscillation above 10 kHz** (comb pattern mismatch at extreme HF)

**HIGHBW sine:**
- Good tracking overall
- Sub-bass excess visible

**MIDBW noise/sine:**
- Comb pattern visible, peaks roughly align in frequency
- Difference oscillates ±5 dB around zero in mid range
- **Sub-bass: +10 dB excess** (same as HIGHBW)

**LOWBW noise/sine:**
- Comb pattern very pronounced (narrow bands)
- **Peak POSITIONS match** (frequencies correct — confirms frequency grid is right)
- **Peak HEIGHTS differ by ±10 dB** — the comb teeth are at right positions but wrong levels
- This is the per-peak deficit documented earlier (-4.9 dB mean in IIR-7)

#### Three remaining issues (from visual analysis):

1. **Sub-bass excess (+10 dB below 60 Hz)** — consistent across ALL 6 tests. BR is always too loud at sub-bass. This is likely from the IIR carrier filter having wider effective bandwidth at very low frequencies (bilinear transform warping, or the analysis filter capturing too much sub-bass energy).

2. **LOWBW per-peak height mismatch (±10 dB)** — comb peaks at correct frequencies but wrong amplitudes. This is the hardest remaining issue — requires matching Ableton's exact per-band Q and gain normalization.

3. **HF oscillation at HIGHBW (±5 dB above 10 kHz)** — likely from the bilinear transform BW compression at high frequencies. The bwComp correction (sinc-based) may need adjustment.

#### Priority order:
1. Sub-bass excess — most consistent, affects all tests, likely one fix
2. HF oscillation — affects HIGHBW, may improve with bwComp tuning
3. LOWBW per-peak heights — hardest, may require fundamental Q/gain changes

#### Plot files:
- `results/compare_highbw_noise.png` — best overall match reference
- `results/compare_lowbw_noise.png` — shows per-peak height mismatch
- All 6 at `results/compare_{bw}_{signal}.png`
- Generated by `plot_comparison.py` (subprocess-isolated)

### IIR-10: Sub-bass correction + visual curve analysis (2026-03-16)

**Changes from IIR-9:**
1. Added sub-bass per-band correction for bands < 120 Hz at low Q (< 15):
   - `subBassCorr = 1 - 0.5 * qFade * (1 - freqFade²)` — up to -6 dB at lowest freq/Q
   - Only active at wide BW (low Q) where filter skirt leakage causes excess
   - No effect at narrow BW (high Q) — preserves LOWBW results

**HIGHBW sub-bass improvement:**
| Region | IIR-9 | IIR-10 |
|---|---|---|
| Sub-bass (30-80 Hz) | +3.87 dB | **-0.17 dB** |
| Bass (80-250 Hz) | +1.05 dB | +0.73 dB |

**Full results (IIR-10):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.28 | -0.36 | -0.13 | -0.50 | -0.09 | +0.37 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.28 | +0.19 |

**Per-band analysis (HIGHBW noise):**
| Band | IIR-9 mean | IIR-10 mean |
|---|---|---|
| Sub-bass | +3.87 | **-0.17** |
| Bass | +1.05 | +0.73 |
| Low-mid | -0.16 | +0.49 |
| Mid | -0.82 | similar |
| Upper-mid | -0.42 | similar |
| Highs | +0.30 | similar |

**Visual curve assessment from plots:**
- HIGHBW noise: curves nearly overlap 80 Hz – 10 kHz, sub-bass fixed, HF ±5 dB oscillation remains
- HIGHBW sine: good overlap
- MIDBW: comb pattern visible, peaks at correct positions, ±5 dB oscillation
- LOWBW: comb peaks at correct positions but ±10 dB height mismatch (structural — per-peak Q/gain)

**Current calibrated values (IIR-10):**
- widthComp: exponent=1.6, midBwBump=4.10
- tonalSatCorr base: 0.10 (was 0.075)
- subBassCorr: 0.5 attenuation, freq < 120 Hz, Q < 15, squared fade
- noiseTilt: -0.08 (unchanged from IIR-9)
- All other params unchanged from IIR-7

**Remaining visual issues:**
1. LOWBW per-peak height mismatch (±10 dB comb oscillation) — structural, needs per-band Q/gain tuning
2. HIGHBW HF oscillation (±5 dB above 10 kHz) — bilinear transform / bwComp
3. All offsets within ±0.50 dB, tilts within ±0.45 dB/oct

### IIR-10: Per-Peak Analysis — LOWBW (2026-03-16)

**Peak analysis results (30 matched peaks):**
- Frequency alignment: RMS 49 cents — peaks at correct positions ✓
- Peak height deficit: **-4.9 dB mean, +0.96 dB/oct slope**
- Low-freq (<500 Hz): -6.8 dB RMS
- High-freq (>2 kHz): -4.0 dB RMS
- Peaks at 13-18 kHz: match within ±1 dB ✓

**Per-peak height pattern (BR relative to Ableton):**
| Frequency | dB diff | Notes |
|---|---|---|
| 163 Hz | -8.2 | Worst |
| 421 Hz | -7.8 | |
| 919 Hz | -6.6 | |
| 2008 Hz | -5.4 | |
| 4382 Hz | -4.8 | |
| 7029 Hz | -2.9 | Improving |
| 13154 Hz | +0.2 | Matches! |
| 17987 Hz | +1.0 | Slightly loud |

**Root cause:** The IIR carrier's 4th-order BPF has a per-band gain that depends on frequency. At low frequencies, the carrier filter passes less energy per peak than Ableton's. At high frequencies, it matches. The overlapComp parabolic correction partially addresses this but is too weak at low frequencies.

**Fix strategy:** Increase the overlapComp low-frequency correction. The `kShapeAlphaLow` with Q-dependent scaling needs to be stronger:
- Current: `alphaLow = 0.022 * (1 + 0.3 * qAlphaScale)` → at LOWBW Q=27: alphaLow = 0.0286
- Need ~3x more low-frequency boost to close the -6.8 dB gap
- Try: `alphaLow = 0.022 * (1 + 1.5 * qAlphaScale)` → at LOWBW: 0.055

**Important: overlapComp is NORMALIZED (avg=1.0)**
This means increasing low-freq boost automatically decreases high-freq boost. It redistributes per-band gains without changing overall level. This is exactly what we need — boost lows, reduce highs, keep average the same.

### SESSION END STATE (2026-03-16)

**CRITICAL: Test scripts were loading STALE binary!**
The "Band Ritual Improved.vst3" in the build directory was not being updated by some builds due to cmake target name issues. The test_stft.py subprocess isolation may also cache the plugin. Direct python load confirmed DIFFERENT results (+13.57 dB vs test_stft.py's -0.29 dB).

**Current code state has MULTIPLE incremental changes from IIR-7:**
1. widthComp exponent: 1.19 → 1.6
2. midBwBump: 4.55 → 4.10
3. tonalSatCorr base: 0.075 → 0.10
4. overlapComp qAlphaScale: threshold Q>10→Q>22, factor 0.3→2.5
5. subBassCorr: freq<120, Q<15, 0.5 attenuation
6. narrowBwBoost: 2.6 dB * (1-wN)^10

**IMPORTANT: Some of these changes may NOT have been tested on the actual current binary.**
The test script was loading a stale VST from Mar 13 for many iterations. The "IIR-9" and "IIR-10" results may not reflect the code I thought I was testing.

**Before next session:**
1. Verify which binary the test loads (check file timestamp vs test results)
2. Consider reverting to the IIR-7 baseline (git stash state) and re-applying changes ONE AT A TIME with verified builds
3. The cmake reconfigure created TWO targets: "Band Ritual.vst3" (from committed CMakeLists) and "Band Ritual Improved.vst3" (from working tree changes) — this is confusing

**The IIR-7 results ARE valid** — they were tested before any of these changes. The question is which subsequent changes are actually in the binary that produced those results.

**Reliable results (verified):**
- IIR-7 baseline: ±0.63 dB offsets, ±0.45 dB/oct tilts ← CONFIRMED via git stash restore
- All subsequent "improvements" may be untested due to stale binary issue

### IIR-9 RESTORED AND VERIFIED (2026-03-16)

**Reverted overlapComp 1.5x, narrowBwBoost, and Q>22 threshold — all made things worse.**
**Confirmed recompilation: "Building CXX object" in build output + fresh install timestamp.**

**Verified IIR-9 results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.36** | **-0.13** | **-0.50** | **-0.09** | **+0.37** |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.28 | +0.19 |

**ALL 6 OFFSETS WITHIN ±0.50 dB — CONFIRMED REAL.**

**Changes from IIR-7 (committed baseline) that ARE in the current code:**
1. widthComp exponent: 1.19 → 1.6 ✓
2. midBwBump: 4.55 → 4.10 ✓
3. tonalSatCorr base: 0.075 → 0.10 ✓
4. subBassCorr: freq<120, Q<15, 0.5 attenuation ✓ (verified in code)
5. overlapComp qAlphaScale: 0.3 (UNCHANGED from IIR-7) ✓

**Build verification protocol (MUST follow every time):**
1. Delete .o: `rm -f .../CMakeFiles/BandRitual.dir/Source/PluginProcessor.cpp.o`
2. Verify deletion: `ls` should show "No such file"
3. Build: `cmake --build . --config Release --target BandRitual_VST3`
4. Verify "Building CXX object" appears in output (confirms recompilation)
5. Verify "Installing: .../MacOS/Band Ritual Improved" appears (confirms install)
6. ONLY THEN run test_stft.py

**Plugin binary location:** `/Volumes/Ricosan HD/.../build/BandRitual_artefacts/Release/VST3/Band Ritual Improved.vst3`
**Test script plugin path must match this EXACTLY**

### PLAN: Fix LOWBW per-peak deficit (2026-03-16)

**Problem:** At LOWBW (Q=60), individual comb peaks are -4.9 dB mean below Ableton, with +0.96 dB/oct slope:
- 163 Hz: -8.2 dB
- 919 Hz: -6.6 dB
- 4382 Hz: -4.8 dB
- 13154 Hz: +0.2 dB (matches!)

The peaks are at the right FREQUENCIES but wrong HEIGHTS. Low-freq peaks are too quiet.

**Why the overlapComp approach failed:**
The overlapComp is normalized (avg=1.0). Boosting low-freq bands reduces high-freq bands. The OVERALL level drops, requiring widthComp increase, which shifts everything. The normalization fights the per-peak correction.

**Root cause analysis:**
The IIR carrier output energy per band ∝ carrier_bandwidth = f/Q. At 163 Hz: BW = 163/60 = 2.7 Hz. At 13154 Hz: BW = 13154/60 = 219 Hz. The high-freq band passes 80× more total energy.

The envelope detection captures: analysis_BW ∝ f/Q too. The envelope compensations (qComp, overlapComp) correct for the analysis part. But the CARRIER bandwidth also contributes — and this isn't separately compensated.

Ableton likely normalizes this internally: each band's carrier output is divided by its bandwidth, making peak heights constant across frequency. Band Ritual doesn't do this.

**Proposed fix: Per-band carrier bandwidth normalization**

Add an UNNORMALIZED correction that scales each band's output by `1/sqrt(carrier_bandwidth)` = `1/sqrt(f/Q)` = `sqrt(Q/f)`. This equalizes peak heights across frequency.

At LOWBW (Q=60):
- 163 Hz: sqrt(60/163) = 0.607 → -4.3 dB
- 13154 Hz: sqrt(60/13154) = 0.068 → -23.4 dB

That's too aggressive — it would massively reduce all peaks. The overall level would change.

**Better approach:** Apply the correction RELATIVE to a reference frequency, not absolute:
`bwNormCorr = sqrt(refFreq / freq)` where refFreq is chosen so correction = 1.0 at the midpoint.

Using refFreq = 5000 Hz:
- 163 Hz: sqrt(5000/163) = 5.54 = +14.9 dB
- 13154 Hz: sqrt(5000/13154) = 0.617 = -4.2 dB
Range: 19.1 dB — way too much. The actual deficit range is only 8.4 dB.

**Use a fractional power instead of sqrt:**
`bwNormCorr = pow(refFreq/freq, 0.10)`:
- 163 Hz: pow(5000/163, 0.10) = pow(30.7, 0.10) = 1.41 = +3.0 dB
- 13154 Hz: pow(5000/13154, 0.10) = pow(0.38, 0.10) = 0.91 = -0.8 dB
Range: 3.8 dB — about half needed.

Try 0.16:
- 163 Hz: pow(30.7, 0.16) = 1.72 = +4.7 dB
- 919 Hz: pow(5.44, 0.16) = 1.31 = +2.3 dB
- 4382 Hz: pow(1.14, 0.16) = 1.02 = +0.2 dB
- 13154 Hz: pow(0.38, 0.16) = 0.85 = -1.4 dB
Range: 6.1 dB — close to 8.4 needed.

Try 0.20:
- 163 Hz: pow(30.7, 0.20) = 1.98 = +5.9 dB
- 919 Hz: pow(5.44, 0.20) = 1.39 = +2.9 dB
- 4382 Hz: pow(1.14, 0.20) = 1.03 = +0.2 dB
- 13154 Hz: pow(0.38, 0.20) = 0.82 = -1.7 dB
Range: 7.6 dB — good.

**Implementation plan:**
1. Add `peakHeightCorr = pow(5000/freq, 0.20)` to compensatedEnv
2. Make it Q-dependent: full at Q>20 (LOWBW), zero at Q<10 (HIGHBW)
3. Make it noise-only: blend by noisiness (peaks are measured on noise, sine already matches)
4. Apply AFTER qComp but BEFORE gate
5. This is UNNORMALIZED — it will shift the overall level at LOWBW. Need to adjust widthComp to compensate.

**Expected impact:**
- LOWBW noise: peaks boosted 0-6 dB at low freq, cut 0-2 dB at high freq
- Overall LOWBW level shifts up by ~3 dB (average of the correction)
- widthComp at LOWBW needs -3 dB to compensate
- MIDBW: partial correction (Q≈20, partial blend)
- HIGHBW: no correction (Q<10)

**Risk:** The correction changes the spectral TILT as well (boosts lows, cuts highs). The tiltFactor may need adjustment. Iterate after first test.

**Files to modify:** PluginProcessor.cpp line ~3536 (compensatedEnv calculation)
**After implementing:** Must verify with build protocol (delete .o, check "Building CXX", check "Installing MacOS")

### IIR-11: Per-peak height correction (2026-03-16)

**Change:** Added unnormalized `peakCorr = pow(5000/freq, 0.12)` to compensatedEnv.
- Q-dependent: full at Q>20, zero at Q<10
- Noise-only: blended by noisiness²
- Boosts low-freq peaks, cuts high-freq peaks (unnormalized)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +0.04 | -0.36 | +0.10 | -0.50 | -0.09 | +0.37 |
| Tilt | **-1.19** | +0.43 | **-0.71** | -0.34 | +0.28 | +0.19 |

**Per-peak improvement:**
| Metric | IIR-9 | IIR-11 |
|---|---|---|
| Peak deficit mean | -4.9 dB | **-3.8 dB** |
| Peak deficit RMS | 5.6 dB | **4.2 dB** |
| Peak tilt | +0.96 dB/oct | **+0.26 dB/oct** |
| Low-freq RMS | 6.8 dB | **4.1 dB** |
| High-freq RMS | 4.0 dB | 3.8 dB |

**Trade-off:** Per-peak matching improved (deficit -4.9→-3.8, tilt +0.96→+0.26) but noise TILT worsened (-0.45→-1.19). The peak correction boosts low-freq envelope for noise, which darkens the overall spectrum.

**The fundamental tension:** Improving per-peak heights and improving overall tilt are CONFLICTING goals with a single correction. The peak heights need low-freq boost, but the overall spectrum needs to stay flat.

**Why:** The per-peak correction boosts ALL frequencies below 5 kHz (peaks AND valleys). This darkens the overall spectrum. The peak heights improve because the boost is stronger at low freq. But the valleys between peaks also get boosted, tilting the overall spectrum.

**Options:**
1. Accept the trade-off: better peaks (-3.8 dB) with worse tilt (-1.19)
2. Reduce the exponent further to find a balance between peak improvement and tilt
3. Try a different approach: instead of envelope correction, adjust the IIR carrier filter Q per band so that low-freq bands have slightly higher Q → taller peaks without affecting valleys
4. Accept IIR-9 (±0.50 offsets, ±0.45 tilts) as the practical limit of IIR per-band compensation

### IIR-12: Frequency-dependent carrier Q — FAILED (2026-03-16)

**Tried:** `Qcarrier *= pow(freq/5000, 0.15)` in LINKED mode. Lowers Q at low freq for wider peaks.
**Result:** LOWBW tilt -0.45→-1.55, MIDBW -0.23→-1.24. Wider carrier at low freq captures more total noise energy → darkens overall spectrum. Same fundamental issue as envelope-based correction.

**Also tried and failed this session:**
- Envelope-based peakCorr (pow(5000/freq, 0.12)): improves peaks (-4.9→-3.8) but worsens tilt (-0.45→-1.19)
- overlapComp alphaLow increase (0.3→1.5): normalization fights the correction, overall level drops
- Q-dependent carrier (this attempt): same dark tilt issue

**Fundamental insight: The per-peak deficit CANNOT be fixed independently of the overall tilt.**
Any correction that boosts low-frequency bands (wider Q, higher envelope, more overlapComp) also boosts the TOTAL low-frequency energy, creating a dark tilt. The deficit is specifically in the PEAK heights (vs valleys), but all available corrections affect both peaks and valleys equally.

**To fix peaks WITHOUT affecting valleys, you'd need to:**
1. Change the carrier filter's peak-to-total energy ratio (i.e., the filter order or shape)
2. Apply a frequency-dependent gain ONLY at the peak frequencies (requires knowing which frequencies are peaks — circular)
3. Accept the per-peak deficit as a structural IIR characteristic

**CONCLUSION:** The -4.9 dB per-peak deficit at LOWBW is a structural characteristic of our IIR 4th-order carrier implementation. It represents the difference between our filter's peak/valley shape and Ableton's. To fix it would require changing the filter topology (e.g., 6th-order at low freq, or SVF instead of biquad), which is a major architectural change.

**Recommended final state: IIR-9** (±0.50 dB offsets, ±0.45 dB/oct tilts, -4.9 dB per-peak deficit at LOWBW). This is the practical limit of IIR parameter tuning.

---

## SESSION 2026-03-18: Butterworth Q Experiment + Compensation Chain Analysis

### Context
Previous session (2026-03-17) hit API 500 errors while testing Butterworth Q correction factors. Resumed today.

### Butterworth Mixed-Q Experiment — DEAD END

**Goal:** Match true 4th-order Butterworth bandwidth AND rolloff using 4 cascaded biquad stages with different Q values per stage.

**Tested configurations (fc=1000 Hz, Q_target=60):**

| Config | BW (Hz) | Roll@2xBW | Roll@3xBW |
|--------|---------|-----------|-----------|
| TRUE BUTTERWORTH 4th-order | 16.3 | -47.6 dB | -61.4 dB |
| Stacked identical (Q=60) | 6.6 | -48.7 dB | -61.9 dB |
| 2tight+2wide (0.8Q, 0.3Q) | 12.1 | -28.3 dB | -39.4 dB |
| 2tight+2wide (0.7Q, 0.25Q) | 13.5 | -24.5 dB | -34.9 dB |
| BW poles (0.54Q, 1.31Q) | 7.3 | -43.6 dB | -56.3 dB |
| Progressive (Q, 0.7Q, 0.5Q, 0.35Q) | 10.7 | -33.0 dB | -44.9 dB |
| Tight3+Wide1 | 8.0 | -38.6 dB | -50.1 dB |
| Tight2+Med+Wide | 9.4 | -36.3 dB | -48.3 dB |
| All corrected (0.435Q) | 16.3 | -23.8 dB | -34.9 dB |

**Conclusion:** No mixed-Q configuration can match both Butterworth bandwidth AND rolloff with peak filters. Bandwidth and rolloff are coupled in cascaded peak biquads. This approach is fundamentally limited.

### True Butterworth SOS — Not Directly Usable in JUCE

Tested scipy's `butter(4, [fl, fh], btype='bandpass')` → SOS. The 4 sections are LP→BP transformed pairs with mixed numerator types (one section has near-zero b coefficients). These are NOT standard bandpass biquads and can't be directly used with JUCE's `makeBandPass`.

### KEY FINDING: Filter Type Does NOT Cause the 13 dB Sine Gap

**Tested sine (1000 Hz) through all filter types at Q=60:**
- Stacked peak x4: gain = 0.0 dB
- True Butterworth 4th BP: gain = 0.0 dB  
- JUCE makeBandPass x4: gain = 0.0 dB

**All filter types pass sine identically.** The only difference is noise bandwidth (2.6 dB). The 13 dB sine gap is NOT a filter shape problem.

### Root Cause: Compensation Chain Analysis

Simulated the full envelope compensation math for LOWBW (Q=27):

```
                        SINE        NOISE
Raw envelope:          0.7000      0.0205
qComp factor:          1.000       2.487
After qComp:           0.7000      0.0510
fastTanh(e*4):         0.9926      0.2010
tonalSatCorr:          1.393       1.000
envGain:               2.7485      0.1210
envGain ratio:         +27.1 dB
widthGain (global):    5.97x (15.5 dB)
```

**The problem is fastTanh saturation:**
- Sine envelope: input 2.80 → output 0.993 → **-9.0 dB compression**
- Noise envelope: input 0.20 → output 0.201 → ~0 dB compression (linear region)
- fastTanh crushes sine but barely touches noise

**Also: tonalSatCorr is BOOSTING sine (+1.39x = +2.9 dB) at Q=27** instead of cutting it. The hiQCorr formula with negative exponent creates > 1.0 correction at high Q. This is counterproductive.

### Next Steps
1. Fix tonalSatCorr: should CUT sine at high Q, not boost
2. Consider replacing fastTanh with a linear or piecewise-linear envelope shaper that doesn't crush tonal signals
3. Or: add a tonal-specific widthGain reduction (sine doesn't need 15 dB of BW compensation)
4. Run auto_compare.py after fix to verify improvement


### Fresh Measurement (2026-03-18) — Current Build State

Ran auto_compare with current plugin binary (built 2026-03-17 14:29).
**The sweep_results.csv was STALE** — from an older patched state, not the current code.

| Test | Offset (dB) | Tilt (dB/oct) | RMS diff (dB) |
|------|-------------|---------------|---------------|
| LOWBW noise | **+0.91** | **-2.19** | +8.85 |
| LOWBW sine | **-0.58** | -0.60 | +12.13 |
| HIGHBW noise | **-0.08** | +0.25 | +3.81 |
| HIGHBW sine | **+0.76** | +0.09 | +2.10 |

**Sine/noise gap:** LOWBW -1.49 dB, HIGHBW +0.84 dB (both excellent!)

**Key finding:** The 13 dB sine gap from sweep_results.csv was from a stale plugin state. Current code has all spectral offsets within ±1 dB. The Butterworth Q experiment and compensation chain deep-dive were solving a problem that no longer exists in the current build.

**Remaining issue:** LOWBW noise tilt = -2.19 dB/oct (too dark — highs attenuated). This is the primary thing to fix.

**RMS differences are large** (+8.85 to +12.13 dB for LOWBW) — BR is louder overall due to widthGain. But spectral SHAPE (offset/tilt) is what matters for matching, and RMS can be normalized.


### Hybrid Tilt Correction — Current Best (2026-03-18)

**Approach:** Two-part Q-dependent tilt correction:
1. **Carrier tiltCorr8** (affects both sine+noise): slope = 0.85 + 0.65*qFade (0.85→1.50 dB/oct)
2. **Noise envelope noiseTiltExpHiQ** (affects only noise): 0.30*hiQFade

**Code changes:**
- `PluginProcessor.cpp` tiltCorr8: Q-dependent slope (0.85 at HIGHBW, 1.50 at LOWBW)
- `PluginProcessor.cpp` noise tilt: added hiQFade ramp 0→1 over Q=10→27, boost exponent 0.30
- Both main and crossfade paths updated
- `maxBoostDb`: 15.0 → 13.0 (compensate carrier tilt net gain)

**Results (current best this session):**

| Test | Offset | Tilt | vs Baseline offset | vs Baseline tilt |
|------|--------|------|--------------------|------------------|
| LOWBW noise | +2.32 | **-0.68** | +1.41 worse | **+1.51 better** |
| LOWBW sine | -1.20 | **+0.04** | -0.62 worse | **+0.64 better** |
| MIDBW noise | +0.31 | **-0.44** | new | new |
| MIDBW sine | +1.18 | **-0.16** | new | new |
| HIGHBW noise | -0.35 | +0.26 | -0.27 | +0.01 |
| HIGHBW sine | +0.49 | +0.10 | -0.27 better | +0.01 |

**Key achievements:**
- LOWBW noise tilt: **-2.19 → -0.68** (1.51 dB/oct improvement)
- LOWBW sine tilt: **-0.60 → +0.04** (nearly perfect!)
- MIDBW and HIGHBW all excellent

**Remaining issue:** LOWBW noise/sine offset gap of 3.52 dB (+2.32 vs -1.20). The tilt correction adds net gain to noise. Needs either:
1. Signal-adaptive widthGain (reduce for noise, keep for sine) — but widthGain is global
2. Increased tonalSatCorr at high Q to boost sine more (current correction = 1.39x, need ~2.1x)
3. Accept the offset gap and tune widthGain to split the difference (noise +0.56, sine -0.56)

**Fundamental insight (this session):** 
- Butterworth Q experiments are a dead end — filter type doesn't affect sine gain
- The -2.19 dB/oct LOWBW noise tilt has TWO components: carrier tilt + analysis envelope tilt
- Carrier tilt needs carrier correction (affects both sine+noise)
- Analysis tilt needs noise-only envelope correction
- Hybrid approach successfully separates the two corrections


### FINAL STATE — Session 2026-03-18

**Parameters changed (vs pre-session baseline):**
1. Carrier tiltCorr8: Q-dependent slope `0.85 + 0.65*qFade` (0.85 at HIGHBW → 1.50 at LOWBW)
2. Noise envelope tilt: added `hiQFade` ramp Q=10→27, boost exponent `0.30`
3. tonalSatCorr hiQExp: `-0.35` → `-0.65` (stronger sine boost at high Q)
4. maxBoostDb (widthGain): `15.0` → `11.5`

**Final results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.82 | **-0.68** |
| LOWBW sine | +0.47 | **+0.04** |
| MIDBW noise | -0.43 | -0.44 |
| MIDBW sine | +1.38 | -0.16 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | +0.27 | +0.10 |

**Summary metrics:**
- Max |offset|: 1.38 dB (MIDBW sine)
- Max |tilt|: 0.68 dB/oct (LOWBW noise)
- Mean |offset|: 0.65 dB
- Mean |tilt|: 0.28 dB/oct

**Key improvements vs baseline:**
- LOWBW noise tilt: -2.19 → -0.68 (+1.51 improvement)
- LOWBW sine tilt: -0.60 → +0.04 (nearly perfect)
- LOWBW noise/sine gap: 1.49 → 0.35 dB (nearly eliminated)

**Remaining optimization targets (diminishing returns):**
- MIDBW sine offset +1.38 — caused by midBwBump (11 dB gaussian at BW=0.5), hard to reduce without breaking MIDBW noise
- LOWBW noise tilt -0.68 — could improve with stronger noise envelope correction, but risk offset regression
- These are within perceptual thresholds for vocoder use


### REFINED — Squared hiQ ramp + stronger baseCorr (2026-03-18)

**Additional changes:**
- tonalSatCorr baseCorr: `0.10` → `0.15` (main), `0.075` → `0.11` (crossfade) — stronger base cut for tonal
- hiQExp ramp: `linear` → `squared` (`ramp * ramp`) — gentle at mid-Q, full at high-Q
  - Prevents MIDBW sine overshoot while keeping LOWBW correction

**Final results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.82 | -0.68 |
| LOWBW sine | -0.31 | +0.04 |
| MIDBW noise | -0.43 | -0.44 |
| MIDBW sine | **-0.23** | -0.16 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | -0.20 | +0.10 |

**Summary:**
- Max |offset|: **0.82 dB** (LOWBW noise)
- Max |tilt|: **0.68 dB/oct** (LOWBW noise)
- Mean |offset|: **0.43 dB**
- Mean |tilt|: **0.28 dB/oct**
- All 6 tests within ±0.82 dB offset and ±0.68 dB/oct tilt

**Full parameter summary (all changes this session):**
```
tiltCorr8 slope:       0.85 + 0.65*qFade (Q-dependent, 0.85→1.50)
noiseTiltExpHiQ:       0.30 * hiQFade (Q=10→27)
tonalSatCorr baseCorr: 0.15 (main), 0.11 (crossfade)
tonalSatCorr hiQExp:   -0.65 * ramp² (squared ramp, gentler at mid-Q)
maxBoostDb:            11.5 (was 15.0)
```

**Remaining optimization target:** LOWBW noise (offset +0.82, tilt -0.68) is the weakest point. This is structural: the carrier filter's dark tilt at narrow BW and the noise envelope correction fighting the tanh compression. Further improvement would require architectural changes (e.g., bypassing tanh for noise, or using SVF filters).


### BEST RESULT — qComp hi-Q reduction + steeper carrier tilt (2026-03-18)

**Additional changes from previous iteration:**
- qComp: added hi-Q reduction factor `1 - 0.09 * hiQFade` (compensates net gain from tilt correction)
- Carrier tilt slope: `0.85 + 0.65*qFade` → `0.85 + 0.80*qFade` (1.65 at LOWBW)

**Results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | **+0.51** | **-0.27** |
| LOWBW sine | **+0.01** | +0.19 |
| MIDBW noise | -0.59 | -0.31 |
| MIDBW sine | -0.07 | -0.09 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | -0.20 | +0.10 |

**Summary:**
- **Max |offset|: 0.59 dB** (MIDBW noise)
- **Max |tilt|: 0.31 dB/oct** (MIDBW noise)
- **Mean |offset|: 0.32 dB**
- **Mean |tilt|: 0.20 dB/oct**

**Improvement from session start:**
- LOWBW noise tilt: -2.19 → **-0.27** (+1.92 improvement!)
- LOWBW sine offset: -0.58 → **+0.01** (essentially zero!)
- Max |offset|: 0.91 → **0.59** (35% reduction)
- Max |tilt|: 2.19 → **0.31** (86% reduction!)

**Complete parameter set (this session):**
```cpp
// Carrier tilt: Q-dependent
tiltSlope = 0.85 + 0.80*qFade;  // 0.85→1.65 dB/oct

// Noise envelope hi-Q tilt
noiseTiltExpHiQ = 0.30 * hiQFade;  // Q=10→27

// qComp hi-Q reduction
qCompHiQReduce = 1.0 - 0.09 * hiQFade;  // Q=10→27

// tonalSatCorr
baseCorr = 1.0 - 0.15*t (main), 1.0 - 0.11*t (xfade)
hiQExp = -0.65 * ramp² (squared ramp)

// widthGain
maxBoostDb = 11.5
```

This is now the best IIR result achieved, surpassing IIR-9 (which had ±0.50 offset, ±0.45 tilt). The STFT approach (±0.59 dB overall range at HIGHBW) is comparable but this IIR result matches across ALL bandwidth settings.


### CORRECTION: Ableton Uses IIR, NOT FFT (2026-03-18)

**The Step 11 "FFT" conclusion was WRONG.** The 420 dB/oct measurement was taken from a 40-band LOWBW output where inter-band notch slopes appear extremely steep due to 40 overlapping bands. This does NOT represent a single filter's rolloff.

**Definitive evidence (from Mar 17 "Filter Order Correction" note):**
A 4-band LOWBW Ableton recording with isolated peaks showed:
- Peak 1: 49.6 dB/oct ≈ 8th order
- Peak 2: 61.2 dB/oct ≈ 10th order
- Peak 3: 53.8 dB/oct ≈ 9th order

**Ableton uses IIR with 4-5 cascaded biquad stages (8th-10th order).** Our 4-stage (8th-order) architecture is correct.

**This means:** The ±0.59 dB result is NOT a structural IIR-vs-FFT limit. It's calibration precision within the same architecture. Further improvement is possible through parameter tuning.


### NEW BEST — Flat widthGain offset + tuned qComp (2026-03-18)

**Changes from previous best:**
- widthGain: added `+0.5f` flat offset (lifts HIGHBW without affecting LOWBW much)
- qCompHiQReduce: `0.09` → `0.15` (trims LOWBW noise by ~0.5 dB)
- Carrier tilt slope: `0.80*qFade` (kept from previous iteration)

**Results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | **+0.50** | **-0.10** |
| LOWBW sine | +0.51 | +0.19 |
| MIDBW noise | -0.31 | -0.27 |
| MIDBW sine | +0.43 | -0.09 |
| HIGHBW noise | **-0.06** | +0.26 |
| HIGHBW sine | +0.30 | +0.10 |

**Summary:**
- **Max |offset|: 0.51 dB** (improved from 0.59)
- **Max |tilt|: 0.27 dB/oct** (improved from 0.31)
- **Mean |offset|: 0.35 dB**
- **Mean |tilt|: 0.17 dB/oct**

**Session total improvement:**
- LOWBW noise tilt: **-2.19 → -0.10** (95% reduction!)
- Max |offset|: 0.91 → **0.51** (44% reduction)
- Max |tilt|: 2.19 → **0.27** (88% reduction)

**Complete final parameter set:**
```cpp
// Carrier tilt (Q-dependent)
tiltSlope = 0.85 + 0.80*qFade;  // 0.85→1.65 dB/oct

// Noise envelope hi-Q tilt  
noiseTiltExpHiQ = 0.30 * hiQFade;  // Q=10→27

// qComp hi-Q reduction
qCompHiQReduce = 1.0 - 0.15 * hiQFade;  // Q=10→27

// tonalSatCorr
baseCorr = 1.0 - 0.15*t (main), 1.0 - 0.11*t (xfade)
hiQExp = -0.65 * ramp² (squared ramp)

// widthGain
maxBoostDb = 11.5, curve = (1-wN)^3 + 0.5  // flat offset for HIGHBW
midBwBump = 11.0 * gaussian(wN=0.21, σ=0.12)
```


### LATEST BEST — baseCorr 0.18 (2026-03-18)

**Change:** baseCorr `0.15` → `0.18` (main), `0.11` → `0.13` (xfade) — stronger tonal cut

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.50 | -0.10 |
| LOWBW sine | +0.19 | +0.19 |
| MIDBW noise | -0.31 | -0.27 |
| MIDBW sine | +0.11 | -0.09 |
| HIGHBW noise | -0.06 | +0.26 |
| HIGHBW sine | **+0.00** | +0.10 |

- **Max |offset|: 0.50 dB, Mean: 0.20 dB**
- **Max |tilt|: 0.27 dB/oct, Mean: 0.17 dB/oct**
- HIGHBW sine = +0.00 dB (perfect match)


### FINAL SESSION RESULT — Flat offset 0.4 (2026-03-18)

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.40 | -0.10 |
| LOWBW sine | +0.09 | +0.19 |
| MIDBW noise | -0.41 | -0.27 |
| MIDBW sine | +0.01 | -0.09 |
| HIGHBW noise | -0.16 | +0.26 |
| HIGHBW sine | -0.10 | +0.10 |

- **Max |offset|: 0.41 dB** (session start: 0.91)
- **Max |tilt|: 0.27 dB/oct** (session start: 2.19)
- **Mean |offset|: 0.20 dB**
- **Mean |tilt|: 0.17 dB/oct**

**Complete final parameter set:**
```cpp
tiltSlope = 0.85 + 0.80*qFade;         // carrier tilt 0.85→1.65
noiseTiltExpHiQ = 0.30 * hiQFade;       // noise envelope hi-Q
qCompHiQReduce = 1.0 - 0.15 * hiQFade; // qComp trim at high Q
baseCorr = 1.0 - 0.18*t;               // tonal base cut (xfade: 0.13)
hiQExp = -0.65 * ramp²;                // tonal hi-Q boost (squared)
maxBoostDb = 11.5;                      // widthGain base
widthGain offset = +0.4;               // flat boost all BWs
midBwBump = 11.0 * gaussian(0.21, 0.12);
```


---

## ⚠️ V2-IMPROVED STATUS: VALID — NEEDS LISTENING TEST

**Date:** 2026-03-18
**Results:** ±0.41 dB offset, ±0.27 dB/oct tilt across 6 tests
**Method:** 8+ compensation layers (NOT Ableton's actual algorithm)
**Next step:** Load in Ableton, A/B against Ableton's vocoder with real audio (voice, music)
**Decision after test:** If it sounds good → ship as-is. If not → V3 clean rewrite is planned.

**V3 plan:** [[Band Ritual V3 - Clean Ableton Architecture]] — energy-normalized filter bank, no compensation layers, replicate Ableton's actual processing.
