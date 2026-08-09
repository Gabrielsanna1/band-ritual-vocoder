---
tags:
  - band-ritual
  - vocoder
  - ableton
  - parameters
date: '2026-03-19'
---
# Ableton Vocoder — Actual Parameter Ranges (2026-03-19)

## CRITICAL: Ranges are WIDER than assumed

| Parameter | Assumed | Actual |
|-----------|---------|--------|
| **BW** | 0-100% | **0-200%** |
| **Depth** | 0-100% | **0-200%** |

This means:
- Our "HIGHBW" tests at BW=100% are only MID BW in Ableton's range!
- Our depth=100% tests are only half of Ableton's max!
- ALL previous comparisons were done at HALF the actual max BW/depth

## Impact on V2-Improved calibration
The ±0.41 dB results were measured against Ableton at BW=10% (LOWBW), BW=50% (MIDBW), BW=100% (HIGHBW). But 100% is actually Ableton's MIDPOINT, not maximum. We never tested against BW=200%.

## Impact on V3 normalization
The pipeline normalization and tilt correction were calibrated for BW up to 100%. At 200%, the Q would be ~3.5 (very wide) — the filter bank becomes nearly flat. Need to test at 200% too.

## What the user recorded (Phase 4 sine sweep corrections)
- crack_bw10_sine.wav → BW=10% (correct)
- crack_bw50_sine.wav → actually recorded at BW=**100%** (mid), NOT 50%
- crack_bw100_sine.wav → actually recorded at BW=**200%** (max), NOT 100%

## What the user recorded (Phase 5 depth corrections)
- crack_depth0_noise.wav → Depth=0% (correct)
- crack_depth25_noise.wav → actually Depth=**50%**
- crack_depth50_noise.wav → actually Depth=**100%**
- crack_depth75_noise.wav → actually Depth=**150%**
- crack_depth100_noise.wav → actually Depth=**200%**

## Phase 3 filename note
User named files "band" not "bands" (crack_band4_highbw.wav etc.)
