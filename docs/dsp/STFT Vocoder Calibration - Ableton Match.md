# STFT Vocoder Calibration - Matching Ableton's Vocoder

## Status: Attempt 22 (2026-03-13) - Best overall result (geometric RMS blend)

## STFT Architecture (confirmed working)
- **FFT**: 2048-point, hop 512 (75% overlap), sqrt-Hann window
- **COLA**: sqrt-Hann x sqrt-Hann = Hann, 4 overlapping sum to 1.5, normalize by 1/1.5
- **fftNorm**: 1/sqrt(N/4) = 1/sqrt(512)
- **Contiguous bin assignment**: Each FFT bin assigned to nearest band via log-space midpoints
- **Geometric dual RMS envelope** (Attempt 22 key insight):
  - **Noise**: per-bin RMS = sqrt(bandEnergy / binCount) * fftNorm (energy spread across bins)
  - **Tonal**: total RMS = sqrt(bandEnergy) * fftNorm (energy concentrated in 1-2 bins)
  - Per-bin RMS dilutes tonal energy by 1/sqrt(N) — causes N-dependent level error
  - **Linear blend** has N-dependent excess at moderate noisiness: +1.68 dB at mid (N=12), +0.47 at upper-mid (N=40). 2.20 dB range.
  - **Geometric blend** fixes this: bandRMS = noiseRMS^η × tonalRMS^(1-η). Reduces range to 0.59 dB.
  - Sine sweep analysis: ALL detectable bands have noisiness ~0.17-0.24 (sweep moves through bins during 46ms window)

## What We Confirmed About Ableton's Vocoder

### Matches perfectly (within +/-1.3 dB)
- **HIGHBW noise**: -0.26 dB mean, range -1.28 to +0.89 dB
- **HIGHBW sine (detected bands >1kHz)**: mid -0.14, upper-mid +0.01, highs +0.45 (0.59 dB range!)
- This confirms Ableton uses a similar FFT/STFT approach for wide-bandwidth vocoding
- **3 dB/octave tilt correction** (sqrt(f/4000)) is needed and correct for noise
- **Noise envLevelComp = 1.05**, **Tonal envLevelComp = 0.042** (geometric blend scale)

### Key mathematical insight: Participation Ratio normalization
- PR = (sum|X|^2)^2 / (N * sum|X|^4)
- For noise (exponentially distributed |X_k|^2): **E[PR] = (N+1)/(2N) ~ 0.5, NOT 1.0**
- For pure sine in 1 bin: PR = 1/N
- Correct normalization: noisiness = (PR - 1/N) / ((N+1)/(2N) - 1/N)
- Only reliable for bands with >= 8 FFT bins (small bands have too much PR variance)

### Adaptive tilt/level correction (geometric dual RMS)
- **Noise** (noisiness ~ 1.0): perBinRMS × 1.05, full tilt sqrt(f/4000)
- **Tonal** (noisiness ~ 0.0): totalRMS × 0.042, no tilt (1.0)
- Tilt blended linearly by noisiness
- RMS blended **geometrically**: bandRMS = noiseRMS^η × tonalRMS^(1-η)
- Geometric blend eliminates N-dependent excess at moderate noisiness (key A22 insight)
- Bands with < 8 bins: default to noisiness = 1.0 (can't reliably detect tonal)
- Lowering threshold to 4 bins was tried (A19) — helped low-mid sine but hurt mid noise. Reverted.

## Structural STFT Limitations (can't match Ableton here)

### 1. LOWBW comb pattern (+2-5 dB excess)
- IIR bandpass has soft rolloff between narrow bands -> deep notches at low Q
- FFT contiguous bins cover ALL frequencies -> no notches
- This is the fundamental FFT-vs-IIR difference at narrow bandwidth
- Would need non-contiguous bins to match, but that creates brick-wall artifacts

### 2. Impulse sub-bass (+15-30 dB)
- STFT window captures full impulse energy in one frame
- All FFT bins get energy from impulse -> sub-bass bands accumulate energy
- IIR filters have inherent time-domain spreading that reduces peak impact

### 3. Sine at sub-bass (-9 dB)
- Sub-bass bands have only 5-7 FFT bins
- Can't distinguish tonal from noise with so few bins
- Default noisiness=1.0 applies full tilt correction (17 dB attenuation at 80 Hz)
- For sine, tilt should be 0 dB, but we can't detect it
- This is an edge case (sine sweep at sub-bass) unlikely in real vocoder use

## Current Code Constants (Attempt 22)
```cpp
constexpr float kNoiseEnvComp = 1.05f;   // matches Ableton noise level (per-bin RMS path)
constexpr float kTonalEnvComp = 0.042f;  // matches Ableton tonal level (geometric blend)
constexpr float kTiltRefFreq = 4000.0f;  // sqrt(f/4000) tilt correction reference
// PR threshold: binCount >= 8 for reliable tonal/noise detection
// Geometric dual RMS: bandRMS = (perBinRMS*1.05)^noisiness * (totalRMS*0.042)^(1-noisiness)
// Falls back to linear blend if either term is near zero
```

## All Calibration Attempts Summary
| # | Config | HIGHBW Noise RMS | HIGHBW Sine RMS | Notes |
|---|--------|-----------------|-----------------|-------|
| 1 | sqrt(IIR_BW/FFT_BW) | Sub-bass +23 dB | - | Massive tilt |
| 2 | Fixed tilt sqrt(f/4000) | -0.49 mean | +0.59 mean, 18dB tilt | Good noise, bad sine tilt |
| 3 | Tilt + envLevelComp=1.05 | -0.49 mean | +0.59 mean | Best noise at the time |
| 7 | Global spectral flatness | +0.02 mean | +7.06 mean | Perfect noise, sine too loud |
| 8 | Adaptive level (kTonal=0.45) | +0.02 | +3.99 | Half the reduction needed |
| 10 | Aggressive kTonal=0.10 | -0.01 | -5.16 | Overcorrected |
| 12 | Per-band PR (correct norm) | +0.45 | +2.00 | Sub-bass +3.28 |
| 13 | PR min 8 bins | -0.18 | -0.46 mean, sub -8.93 | Best noise ever |
| 17 | PR min 8 + kTonal=0.36 | **-0.19** | **-0.87** mean, sub -8.93 | Best balanced (single RMS) |
| 18 | Dual RMS linear + kTonal=0.0265 | **-0.22** | mid +1.68, umid +0.47, hi -0.52 | 2.20 dB range |
| 19 | Min bins 4 (reverted) | -0.15 | low-mid -2.02, mid +3.00 | Helped low-mid, hurt mid |
| 20 | BW correction (reverted) | -2.74 | - | Overcorrected, Q-dependent |
| 21 | Squared noisiness (reverted) | - | mid -2.12, umid -4.00 | Too aggressive |
| 22 | **Geometric RMS + kTonal=0.042** | **-0.26** | **mid -0.14, umid +0.01, hi +0.45** | **0.59 dB range! CURRENT BEST** |

## Test Infrastructure (verified working)
- Pedalboard correctly sets LINKED mode + 40 bands + correct BW
- Debug output showed Q=2.0/16 bands only from initial plugin load defaults
- After parameter setting + warmup, all params propagate correctly
- Calibration numbers are valid

## Next Steps
1. Fix auto_compare.py parameter names to ensure LINKED mode + 40 bands are actually set
2. Consider if Ableton actually uses IIR (LOWBW comb pattern strongly suggests IIR)
3. If so, may need hybrid approach: IIR analysis + FFT carrier shaping
4. Investigate Ableton's sub-bass handling (does it roll off below ~50 Hz?)
5. Port remaining features: formant shift, noise injection, chord
6. **Remember to revert DEV OVERRIDE in LicenseManager.h before shipping**

## Files
- Calibration log: `~/Desktop/vocoder-comparison/calibration_log.md`
- Test scripts: `~/Desktop/vocoder-comparison/auto_compare.py`, `deep_analysis.py`
- STFT code: `/Volumes/Ricosan HD/.../BandRitualV2-Improved/Source/STFTVocoder.cpp`
- Results: `~/Desktop/vocoder-comparison/results/deep_analysis.txt`
