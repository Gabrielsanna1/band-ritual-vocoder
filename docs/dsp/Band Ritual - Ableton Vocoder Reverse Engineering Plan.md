---
tags:
  - band-ritual
  - dsp
  - vocoder
  - reverse-engineering
date: '2026-03-15'
---

# Band Ritual — Ableton Vocoder Reverse Engineering Plan

## Goal
Perfectly match Ableton's vocoder by reverse-engineering its exact band frequency grid, Q values, filter order, and depth curve.

## Current State (IIR-7, 2026-03-15)
- HIGHBW noise: +0.07 dB offset, +0.21 dB/oct tilt — **NEARLY PERFECT**
- HIGHBW sine: -0.87 dB offset — good
- LOWBW noise: +5.47 dB offset — TOO LOUD (Q compensation issue)
- LOWBW sine: +10.81 dB offset — TOO LOUD
- Architecture: confirmed IIR, not FFT

## Key Discovery (2026-03-14)
The LOWBW comb pattern mismatch is NOT filter order — it's **frequency mapping**. The comb peaks land at different positions because our band center frequencies don't match Ableton's. Changing filter order (2nd vs 4th) changes notch depth but NOT peak positions.

## Frequency Grid Analysis (2026-03-15) — RESOLVED

### Finding: Band Ritual's frequency grid ALREADY matches Ableton's
- Both use edge-to-edge formula: `f_k = Range_Low * (Range_High/Range_Low)^(k/(N-1))` for k=0..N-1
- Band Ritual code (PluginProcessor.cpp:1425): `linearPos = i / (nBands - 1)` — same formula
- The earlier claim that BR used `(k+0.5)/N` was wrong — that was in analysis scripts, not the actual plugin code
- 8-band verification: **24 cents RMS** — all bands within ±40 cents of Ableton's peaks
- 4-band verification: **86 cents RMS** — good match
- **Frequency grid is NOT the source of spectral mismatch**

### Filter Order — CONFIRMED MATCHING
- Ableton: 50 dB notch depth in self-carrier mode / 2 = 25 dB per pass = 4th-order (24 dB/oct)
- Band Ritual: 4th-order cascaded (2×2nd-order) — same

### Remaining Issue: Q Compensation at Narrow BW
- `qComp = sqrt(analysisQ / 2)` is too aggressive at LOWBW (Q~27)
- At Q=27: qComp = sqrt(27/2) = 3.67x boost — too much
- Ableton likely uses less aggressive compensation for narrow filters
- HIGHBW match proves the architecture is correct
- Only the Q-dependent gain needs adjustment

### Analysis Scripts Created
All at `~/Desktop/vocoder-comparison/`:
- `extract_band_centers.py` — initial peak extraction (unreliable at extremes)
- `plot_band_spectra.py` — visual spectral plots
- `analyze_grid_v2.py` — optimization-based fitting (poor results due to bad peaks)
- `analyze_grid_v3.py` — spectrum scoring approach
- `analyze_grid_final.py` — log-frequency resampling (good peak detection, derived R=1040)
- `verify_formula.py` — **DEFINITIVE** — proved edge-to-edge formula matches at 24 cents RMS

## What We Need From Ableton

### Captures Required (minimum)
All use `TEST_white_noise_10s.wav` as input, self-carrier mode.

| File | Bands | BW | Purpose |
|------|-------|----|---------|
| `ableton_4bands_lowbw/test.wav` | 4 | minimum | Frequency grid (easy to read) |
| `ableton_8bands_lowbw/test.wav` | 8 | minimum | Frequency grid (more points) |
| `ableton_4bands_highbw/test.wav` | 4 | maximum | Q verification |
| `ableton_8bands_highbw/test.wav` | 8 | maximum | Q verification |
| `ableton_vocoder_screenshot.png` | — | — | UI parameter ranges |

### Optional (if needed later)
- 16/20/40 bands at LOWBW/HIGHBW
- Depth 25%/50%/75% at 40 bands HIGHBW

### Ableton Vocoder Settings (constant across all captures)
- Carrier: Modulator (self-carrier)
- Range: 40 Hz – 18000 Hz
- Depth: 100%
- Attack: 1 ms
- Release: 50 ms
- Dry/Wet: 100%
- Gate: OFF
- Formant: 0

## Analysis Plan (after captures)

### Step 1: Extract Band Centers
- FFT the 4-band LOWBW output → 4 clear spectral peaks
- Each peak position = one band's center frequency
- Repeat for 8-band → 8 peaks

### Step 2: Derive Frequency Formula
- From 4+8 band centers, determine the mapping formula
- Likely logarithmic: `f_k = f_low * (f_high/f_low)^(k/N)` or similar
- Compare against our current formula

### Step 3: Measure Q/BW
- Compare LOWBW vs HIGHBW peak widths at same band count
- Peak width at -3 dB = bandwidth → Q = center/bandwidth

### Step 4: Determine Filter Order
- Notch depth between peaks: -12 dB = 2nd-order, -24 dB = 4th-order
- Measure from 4-band LOWBW (clearest notches)

### Step 5: Implement & Iterate
- Update Band Ritual's frequency mapping to match Ableton's
- Re-run comparison → comb peaks should align
- Fine-tune Q and compensations

## Files Location
- Test signals: `~/Downloads/TEST_*.wav`
- Ableton captures: `~/Desktop/vocoder-comparison/ableton_*/test.wav`
- Analysis scripts: `~/Desktop/vocoder-comparison/`
- Capture guide: `~/Desktop/ABLETON_CAPTURE_GUIDE.txt`

## Related Notes
- [[Band Ritual - 2nd Order Carrier Filter Experiment]]
- [[Band Ritual - Plugin Silence Debugging Checklist]]
