---
tags:
  - band-ritual
  - vocoder
  - v3
  - final
  - plan
date: '2026-03-20'
---
# Band Ritual V3 — Final 1 (testable) + Final 2 (SVF in progress)

## Status: PLANNING (2026-03-20)

## FINAL 1 — Current V3 State (ready for listening test)

### Location
`/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

### Parameters
```
Product name: "Band Ritual Final 1"
maxQ = 37
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages (8th-order)
tiltCorr = 1.0 (none)
bandNormGain = 1.0 (none)
Envelope: LINEAR (no fastTanh)
envGain = envelope × depth
bwGain (≤100%): 45.6 × (1-wN)^0.44 dB
bwGain (>100%): lerp(34.5, 26.0, (bw-1)/1) dB
bandCountGain: 1/sqrt(N/4)
PVR: 44.9 (Ableton: 44.6)
```

### Results
| BW | RMS vs Ableton |
|----|----------------|
| 10% | 0.0 dB |
| 50% | +0.5 dB |
| 100% | +0.5 dB |
| 130% | +2.7 dB |
| 170% | +1.0 dB |
| 200% | 0.0 dB |

### What matches Ableton
- ✅ RMS: ±1.0 dB at 5/6 BWs
- ✅ PVR: 44.9 vs 44.6
- ✅ Quadratic gain curve (linear envelope)
- ✅ BW=100% curves overlap visually
- ✅ No tilt correction needed

### What doesn't fully match
- PSD tilt at LOWBW: -2.4 dB/oct (V3 slightly darker)
- PSD offset at MIDBW: -7.0 dB
- Sub-bass at BW>130%: V3 too quiet
- Root cause: JUCE makeBandPass skirts narrower than Ableton's filters

---

## FINAL 2 — SVF Filter Upgrade (next step)

### Goal
Replace JUCE makeBandPass with SVF (State Variable Filter) or custom Butterworth bandpass that has wider skirts matching Ableton's measured peak shape.

### Evidence for SVF
From isolated band analysis (Band 3, ~2350 Hz, 40 bands BW=10%):
- Ableton -3dB BW: wider peaks with ~25 dB/oct rolloff on both sides
- V3 (JUCE makeBandPass): narrower peaks with steeper rolloff
- JUCE makeBandPass is a direct-form biquad with specific skirt shape
- SVF has different pole placement → potentially wider/different skirts

### Approach
1. Copy V3 to BandRitualV3-Final2
2. Rename plugin to "Band Ritual Final 2"
3. Implement SVF bandpass (or try different biquad coefficient formulas)
4. Measure peak width and compare to Ableton
5. If SVF matches better → re-tune Q, Qa ratio, BW gain
6. Iterate until curves match at all BWs

### SVF Implementation
JUCE has `juce::dsp::StateVariableTPTFilter` which implements:
- Low-pass, high-pass, band-pass modes
- Different internal topology from IIR biquad
- May have wider skirts at the same Q

Alternatively: use Robert Bristow-Johnson's cookbook bandpass formulas with different normalization (constant-skirt vs constant-peak) which changes the skirt width.

### Measurement targets
- Peak width at Band 20 (918 Hz): match Ableton's measured shape
- PVR: maintain 44.6
- RMS: ±1.0 dB across all BWs
- PSD shape: match at ALL BWs (not just BW=100%)

### Files to modify
- CMakeLists.txt (rename, new plugin code)
- Source/PluginProcessor.h (add SVF filter members)
- Source/PluginProcessor.cpp (replace filter processing + re-tune parameters)

### Test infrastructure
- Reuse ~/Desktop/vocoder-comparison/ scripts
- Compare against crack_bw*_noise.wav files
- Same methodology: build → test → measure → iterate
