---
tags:
  - band-ritual
  - vocoder
  - cracking
  - definitive
date: '2026-03-23'
---
# Band Ritual — Vocoder Cracking: DEFINITIVE Analysis (2026-03-23)

## CRITICAL REVISION: Devil's Advocate Review

### What IS confirmed (high confidence)
1. **4th order carrier (2 cascaded biquad stages)** — BW_20dB/BW_3dB ratio = 3.2 matches 4th order. Confirmed from isolated band fitting (error 0.914 vs 14.6 for 4 stages).
2. **Combined output Q ≈ 61** — measured from definitive recording with verified screenshot
3. **Band grid = logspace(20, 18000, 40)** — confirmed from 40 isolated recordings + debug log
4. **Peak detector envelope** — Ableton has Attack/Release controls

### What was WRONG (revised)
1. **Q=61 is NOT the per-stage Q.** It's the combined Q of 2 cascaded stages. Per-stage Q = 61/1.554 ≈ **39**. This matches March 20's Q=37 within measurement error!
2. **cos()^1.29 frequency-dependent Q is likely a MEASUREMENT ARTIFACT.** The 40-band verification data (verify_all_40_Q.txt) showed Q INCREASING at HF (opposite of cos^1.29). The 5-point fit was from unreliable HF measurements.
3. **Ableton "Precise" mode = constant Q (constant fractional bandwidth).** Data shows constant BW/fc ratio. NOT constant absolute Hz bandwidth.

### The CORRECT architecture (best estimate)
```
Carrier: 2 stages (4th order) at Q_stage ≈ 39 (CONSTANT across all bands)
Analysis: uncertain (2 or 3 stages, Qa ratio uncertain)
Envelope: peak detector (attack/release)
Grid: logspace(20, 18000, N)
Precise mode: constant Q, no frequency-dependent correction
```

### Why March 20 and March 22 both had pieces of truth
- March 20: Q=37 per stage, 4 stages → WRONG stage count, RIGHT Q per stage
- March 22: Q=61 combined, 2 stages → RIGHT stage count, Q was combined not per-stage
- Both give similar effective filter shape; 2-stage Q=39 is the correct decomposition

### What to test in next session
1. **Use 2 stages with constant Q=39 per stage** (no cos correction, no freq-dep Q)
2. **Remove tilt correction** (March 20 showed it wasn't needed with correct Q)
3. **Recalibrate bwGain** from scratch
4. This should give similar results to March 20 (±0.5 dB RMS) but with correct architecture

### Remaining unknowns
- Analysis stage count (2 or 3)
- Analysis Qa ratio (0.6 or different)
- Whether the per-stage Q is exactly 39 or needs fine-tuning
- Envelope attack/release exact behavior

### Sources consulted
- Ableton Live Manual (vocoder section)
- Sound on Sound: Introduction to Vocoders
- Stone et al. 2008 (PMC) — envelope bandwidth effects
- CCRMA Stanford — audio filter banks
- KVR Audio — vocoder design forum
- Cycling '74 — vocoder tutorial

### Key insight from research
"For noise input, AM sidebands do NOT broaden the output spectrum in a measurable way" — the long-term PSD of noise × envelope reflects the carrier filter shape. BUT the BW ratio (20dB/3dB) is more robust than absolute Q measurement because it's a relative measure that cancels envelope effects.
