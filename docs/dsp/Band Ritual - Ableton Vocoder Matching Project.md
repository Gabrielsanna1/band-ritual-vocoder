# Band Ritual — Ableton Vocoder Matching Project

## Goal
Make Band Ritual Improved's vocoder output match Ableton's vocoder as closely as possible, verified by automated WAV comparison scripts.

## Approach: Iterative Calibration with Automated Testing
1. Record Ableton vocoder output for test signals (already done — 12 WAVs)
2. Process same signals through Band Ritual via Pedalboard (Python, headless VST3)
3. Compare per-band RMS/peak levels, spectral shape, envelope transients
4. Adjust DSP parameters in PluginProcessor.cpp
5. Rebuild, re-test, check ALL plots in detail — repeat until curves match

## Test Setup
- **40 bands**, range 40Hz-18kHz, Attack 1ms, Release 50ms, Gate off
- **Ableton**: Carrier=Modulator, BW 10% (low) / 100% (high)
- **Band Ritual**: Carrier=Self, Linked mode, Scale Amt 0%, BW 0.1 (low) / 1.0 (high), ADAPT OFF
- **Test signals**: sine sweep (20Hz→20kHz log), impulse train (5ms bursts every 500ms), white noise — all 10s, 44100 Hz

## Key Files
- **Plugin source**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
- **Comparison scripts**: `~/Desktop/vocoder-comparison/auto_compare.py` (processes via Pedalboard), `deep_analysis.py` (per-band metrics)
- **Ableton WAVs**: `~/Desktop/vocoder-comparison/ableton_{lowbw,highbw}_{noise,sine,impulse}/test.wav`
- **Results**: `~/Desktop/vocoder-comparison/results/` (PNGs + deep_analysis.txt)
- **Install command**: `~/Desktop/install-and-test.txt`

## DSP Architecture (what we're tuning)
- **4th-order stacked carrier filters**: Two cascaded 2nd-order IIR bandpass per band
- **2nd-order analysis filters**: Single IIR bandpass for envelope detection
- **Envelope follower**: 1-pole asymmetric peak tracker (attack/release coefficients)
- **envLevelComp**: Global envelope gain multiplier (currently 15.5 = +23.8 dB)
- **Tilt correction**: dB/oct slope across bands to compensate 4th-order HF rolloff
- **widthGain (maxBoostDb)**: BW-dependent gain on wet bus — steep curve, big boost at narrow BW
- **Soft limiter**: threshold 2.0, tanh compression above
- **bwNormGain**: Currently DISABLED at 1.0f
- **LINKED mode Q**: Analysis Q soft-capped at 48 via `48 * Qcarrier / (Qcarrier + 48)`

## Baseline Measurements (before calibration)
From `deep_analysis.txt` with envLevelComp=10, maxBoostDb=8.5:

### Low BW (0.1) — Band Ritual too quiet
| Signal  | Mean RMS dB | Worst Band         |
|---------|-------------|--------------------|
| Noise   | -12.84      | Highs (-14.84)     |
| Sine    | -7.07       | Sub-bass (-12.45)  |
| Impulse | -8.32       | Highs (-19.77)     |

### High BW (1.0) — Band Ritual slightly loud
| Signal  | Mean RMS dB | Worst Band         |
|---------|-------------|--------------------|
| Noise   | +1.56       | Sub-bass (+5.88)   |
| Sine    | +2.81       | Upper-mid (+4.58)  |
| Impulse | +7.36       | Sub-bass (+23.48)  |

## Calibration Steps (3-step plan)

### Step 1: Recalibrate widthGain + envLevelComp
- envLevelComp: 10.0 → 15.5 (+23.8 dB)
- maxBoostDb: 8.5 → 23.0, exponent 1.44 → 6.0 (steep curve)
- Formula: `boostDb = 23.0 * pow(1.0 - widthNorm, 6.0)`
- Expected: fixes -12.84 dB low BW deficit

### Step 2: BW-dependent tilt correction
- Old: fixed 0.6 dB/oct
- New: `tiltDbPerOct = 0.3 + 0.5 * bwNorm01` (0.3 narrow, 0.8 wide)
- Expected: flatter spectral difference across BW settings

### Step 3: Sub-bass shelf correction at high BW
- Up to -6 dB for bottom 15% of bands when BW > 0.3 norm
- Expected: sub-bass at high BW drops from +5.88 to ~±1 dB

## Build & Test Protocol
```bash
# 1. Force recompile
rm -f "...build/CMakeFiles/BandRitual.dir/Source/PluginProcessor.cpp.o"
touch "...Source/PluginProcessor.cpp"

# 2. Build
cd "...build" && cmake --build . --config Release -j8

# 3. Run comparison (auto_compare loads from BUILD dir, not /Library)
cd ~/Desktop/vocoder-comparison && python3 auto_compare.py && python3 deep_analysis.py

# 4. Check results
cat results/deep_analysis.txt
# + visually inspect ALL PNGs in results/
```

## Known Gotchas
- **CMake external HD timestamp issue**: Must delete .o AND touch source before rebuild
- **Pedalboard loads from build dir**: PLUGIN_PATH in auto_compare.py points to build output, not /Library
- **updateEnvelopeCoeffs caching bug**: Per-band coefficients must be set inline in processBlock
- **Welch PSD can be misleading**: Use per-band RMS (deep_analysis.py) as ground truth, not spectral density

## Current Calibrated Values (2026-03-12)
- **envLevelComp**: 15.5 (+23.8 dB)
- **envLevelCompXf**: 15.5 (crossfade, must match)
- **maxBoostDb**: 23.0, formula: `23.0 * pow(1 - widthNorm, 6.0)` (steep BW-dependent wet bus gain)
- **Tilt**: BW-dependent: `(0.6 + 0.4 * bwNorm01) * 9 * (bandPos - 0.5)` dB
- **bwNormGain**: DISABLED at 1.0
- **Sub-bass shelf**: REMOVED (doesn't help — excess comes from filter leakage, not direct band energy)

## Results After Calibration

### Noise RMS (gold standard)
| Setting | Baseline | Calibrated | Improvement |
|---------|----------|------------|-------------|
| Low BW mean | -12.84 dB | **+0.02 dB** | +12.86 dB |
| High BW mean | +1.56 dB | **+0.25 dB** | +1.31 dB |

### Per-Band Noise RMS
| Band | Low BW (was→now) | High BW (was→now) |
|------|-----------------|-------------------|
| Sub-bass | -12.11→**+0.73** | +5.88→**+4.70** |
| Low-mid | -14.22→**-1.39** | +1.46→**+0.21** |
| Mid | -11.60→**+1.26** | +0.87→**-0.34** |
| Upper-mid | -11.43→**+1.44** | +0.72→**-0.71** |
| Highs | -14.84→**-1.95** | -1.13→**-2.60** |

### Structural Limitations (IIR filter bank vs Ableton's likely FFT-based vocoder)

**Root cause**: Ableton's vocoder probably uses FFT/STFT processing, not IIR filters. This gives fundamentally different transient/impulse behavior — no filter ringing, no bandwidth leakage.

**Tested and ruled out:**
- **2nd-order carriers**: Made impulse LOW-MID +29 dB (WORSE than 4th-order +17 dB). 2nd-order has wider effective bandwidth → captures more broadband impulse energy. Reverted.
- **Sub-bass shelf**: Doesn't help because excess comes from higher bands' filter skirts leaking down, not the bottom bands themselves.
- **Aggressive BW-dependent tilt**: Worsens sub-bass via the same leakage mechanism.

**What we CAN'T match with IIR architecture:**
- Impulse transient peaks at low/mid frequencies (+7 to +17 dB excess)
- Sub-bass excess at high BW (+4.70 dB from filter leakage)
- HF deficit at high BW (-2.60 dB from 4th-order rolloff)

**What we DID match:**
- Noise RMS within ±2 dB across all bands at low BW (mean +0.02 dB)
- Noise RMS within ±5 dB at high BW (mean +0.25 dB)
- These are the steady-state level curves — the "tone" of the vocoder on real audio

### Key Insight: Build System Gotcha
- External HD timestamps unreliable — `touch` alone may not force recompile
- **MUST delete .o file AND verify deletion AND use future timestamp**:
  ```bash
  rm -f "...PluginProcessor.cpp.o" && ls "...PluginProcessor.cpp.o" 2>&1 || echo "DELETED"
  touch -t YYYYMMDD0000 "...PluginProcessor.cpp"
  ```
- Look for "Building CXX object" in build output to CONFIRM recompile happened

## Progress Log
- 2026-03-12: Baseline measurements complete. Started 3-step calibration.
- 2026-03-12: Step 1 (envLevelComp + widthGain): Low BW noise -12.84 → -0.01 dB. Massive improvement.
- 2026-03-12: Discovered build wasn't recompiling — external HD timestamp issue. Wasted 2 iterations.
- 2026-03-12: BW-dependent tilt (0.6+0.4*bw): Mild improvement at HF. Aggressive tilt (0.65+1.5*bw) worsens sub-bass via filter leakage.
- 2026-03-12: Sub-bass shelf tried and removed — doesn't help (leakage from higher bands, not direct).
- 2026-03-12: Final calibration: envLevelComp=15.5, tilt=0.6+0.4*bw, maxBoost=23 pow(1-w,6). All noise bands within ±2 dB (low BW) and ±5 dB (high BW).
