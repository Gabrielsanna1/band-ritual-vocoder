# Band Ritual — STFT Linked Mode Integration (2026-04-10)

**Status:** Wired in, NOT calibrated for BW=10%. IIR iter 19l (cosine 0.928) remains the active path.

## What was done

1. **STFTVocoder integrated into BandRitualV3 processBlock** — `useSTFTLinked` flag in PluginProcessor.h switches between IIR and STFT carrier paths
2. **Band info feeding** from updateBandFilters to STFTVocoder.updateBands()
3. **Audio routing** through stftVocoder.process() with goto bypass of IIR carrier loop
4. **Latency:** STFT adds 2048 samples (~46 ms at 44.1 kHz)

## Why it doesn't work yet at BW=10%

The STFT's contiguous bin assignment creates bands that are **7× wider** than the IIR's Q=37 bandpass:
- At 10 kHz, IIR BW = 270 Hz (~12 FFT bins)
- At 10 kHz, STFT contiguous bins span ~1820 Hz (~85 bins)

This causes +20-30 dB HF excess on real music because the STFT captures all carrier energy in the wide band, while the IIR only passes the narrow center.

### Attempts to fix
- **1/sqrt(binCount) normalization**: mathematically correct for noise, but not sufficient for tonal content
- **Gaussian per-bin weighting** (sigma = fc/74): reduced HF by ~5 dB but still +17-22 dB excess
- The fundamental issue: STFT contiguous bins ≠ IIR bandpass at narrow BW

### Why it worked at HIGHBW (Obsidian: STFT Vocoder Calibration, attempt 22)
At high bandwidth, IIR Q is low (Q ≈ 4-8), so the IIR BW is close to the STFT contiguous bin range. The calibration constants (kNoiseEnvComp=1.18, kTonalEnvComp=0.042, tilt sqrt(f/4000)) compensated for the small residual difference. At BW=10% (Q=37), the gap is too large for constant-based correction.

## What's needed for STFT to work at BW=10%

1. **BW-dependent bin selection**: instead of using all contiguous bins, weight bins by a window (Gaussian, Hann, or raised-cosine) that narrows with BW. Width = fc/Q.
2. **BW-dependent calibration constants**: kNoiseEnvComp, kTonalEnvComp, and tilt all need to be functions of BW, not constants.
3. **Investigate Ableton's BW mapping**: does Ableton narrow its STFT bins when BW decreases? Or does it always use contiguous bins and rely on the overlap between adjacent bands?

## Important debug discovery

**Pedalboard loads from `/Library/` not `~/Library/`!** cmake installs to ~/Library but pedalboard finds the system-wide /Library/ copy first. ALL STFT changes had zero effect until we installed to /Library/ with sudo. The IIR measurements were correct because they happened to load from ~/Library/ (pedalboard behavior might vary).

**ALWAYS install to /Library/ when testing with pedalboard.**

## Files modified
- `PluginProcessor.h`: added `#include "STFTVocoder.h"`, `STFTVocoder stftVocoder`, `bool useSTFTLinked`
- `PluginProcessor.cpp`: prepare(), updateBandFilters(), processBlock() STFT routing
- `STFTVocoder.cpp`: experimental changes (Gaussian weighting, binNorm) — should be reverted to original attempt 22 state before next calibration attempt

## Code location
- Flag: `useSTFTLinked = false` in PluginProcessor.h (line ~379)
- Routing: processBlock around line 4069, `if (useSTFTLinked && algoMode == 1)`
- Band feeding: updateBandFilters around line 2078
