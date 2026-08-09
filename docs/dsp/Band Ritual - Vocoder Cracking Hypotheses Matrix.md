---
tags:
  - band-ritual
  - vocoder
  - cracking
  - hypotheses
  - iteration
date: '2026-03-22'
status: active
---
# Band Ritual — Vocoder Cracking: Hypotheses Matrix

## Purpose
Master list of every plausible correction to match Ableton's vocoder.
Status: ✅ VALIDATED, ❌ INVALIDATED, 🔶 PARTIALLY VALIDATED, ⬜ UNTESTED.

---

## 1. FILTER TYPE
- **1A. IIR biquad**: ✅ VALIDATED
- **1B. SVF**: ❌ INVALIDATED
- **1C. Different biquad formula**: ❌ INVALIDATED — all formulas identical shape

## 2. CARRIER FILTER STAGES
- **2 stages (4th order)**: ✅ CRACKED — fit error 0.66, confirmed at 6 BW settings
- 1 stage: ❌ too wide
- 3+ stages: ❌ too narrow

## 3. ANALYSIS FILTER STAGES
- **3 stages (current)**: 🔶 — simulation showed minimal PSD impact, but may affect PVR
- **→ NEXT: investigate if analysis stages affect PVR mismatch**

## 4. Q MAPPING
- **Q = 856 / BW% (for 40 bands)**: ✅ CRACKED from isolated Band 20 recordings
- **General: Q = (46.74/BW% - 0.063) × ((N-1)/3)^1.014**: ✅ verified at N=4 and N=40
- Old maxQ=37 log mapping: ❌ INVALIDATED

## 5. Qa RATIO (analysis Q / carrier Q)
- **0.60 (current)**: 🔶 — not independently validated with new Q range
- **→ May need adjustment for PVR tuning**

## 6. FREQUENCY RANGE
- **20 Hz – 18,000 Hz**: ✅ VALIDATED and implemented

## 7. BAND FREQUENCY GRID
- **Log-spaced**: 🔶 — works for wide BWs, may cause PSD error at narrow BWs due to slight misalignment

## 8. ENVELOPE FOLLOWER
- **Attack 1ms, Release 50ms**: ✅ matched to Ableton's settings
- Envelope dynamics effect on PVR: ⬜ UNTESTED

## 9. GAIN CORRECTIONS
- **Tilt correction**: ✅ CRACKED — `alpha = -0.259*wN + 0.186`, error < ±0.33 dB/oct
- **bwGain (level)**: ✅ CRACKED — piecewise linear table, 0.0 dB at all BWs
- **bandCountGain**: ✅ `1/sqrt(N/4)` — unchanged, works

## 10. PVR (Peak-to-Valley Ratio)
- **BW=30%**: ✅ 19.5 vs 20.1 — nearly perfect
- **BW=10%**: ❌ 32.8 vs 41.5 — valleys not deep enough (Δ=-8.7)
- **BW=70-200%**: ❌ PVR too high — valleys too deep
- **→ ROOT CAUSE UNKNOWN — top priority for next session**

## NEXT SESSION PRIORITIES
1. Investigate PVR mismatch root cause (band grid? analysis filter? envelope?)
2. Try adjusting Qa ratio for new Q range
3. Try 2 analysis stages (match carrier)
4. Check if band frequencies exactly match Ableton at 40 bands
