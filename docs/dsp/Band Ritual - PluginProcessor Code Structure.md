# Band Ritual — PluginProcessor.cpp Code Map

**File**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
**Lines**: ~4642
**Class**: `ColorVocoderAudioProcessor`

---

## File Structure Overview

The file follows this top-level layout:
1. InputFilter class implementation (HP/LP with variable slope)
2. Constructor / Destructor
3. Parameter layout (all APVTS params)
4. prepareToPlay (filter init, buffer allocation)
5. Envelope coefficient calculation
6. Scale/chord frequency building + snapping
7. Band rebuild + filter coefficient update
8. GUI snapshot helpers
9. **processBlock** (the main DSP pipeline)
10. State save/load (XML serialization)
11. Editor factory

---

## DSP Processing Pipeline (processBlock)

All line numbers refer to `processBlock` starting at **line 2701**.

### 1. Pre-processing (L2701-2934)
| Line | What |
|------|------|
| 2704 | `ScopedNoDenormals` |
| 2708 | Deferred band distribution rebuild (thread-safe dirty flag) |
| 2717-2806 | MIDI Chord Trigger — consume octave 2/3 notes, set chord degree/variation |
| 2809-2829 | MIDI Learn — capture chord from MIDI input |
| 2831-2874 | MIDI Follow — track held notes, build `midiNoteFreqs` |
| 2883-2888 | License check — mute if trial expired |
| 2894-2898 | Safety: bail if `prepareToPlay` not called |
| 2901-2922 | **Input Gain** + input RMS metering |
| 2926-2927 | Bypass check |
| 2930-2932 | Formant smoothing (80ms) |
| 2934 | `updateBandsIfNeeded()` — rebuilds bands or updates filters |

### 2. Input Filter Setup (L2936-2981)
| Line | What |
|------|------|
| 2937-2960 | Read HP/LP freq, slope, Q, Q-width params |
| 2966-2981 | Configure `inputFilter` and `dryFilter` with same settings |

### 3. Parameter Loading (L2983-3094)
| Line | What |
|------|------|
| 2997-3000 | mix, modLevel, complementMode |
| 3002-3055 | **Complement mode EQ memory swap** (swaps Bell params on toggle) |
| 3057-3066 | depth, noiseColor, hpBlend, carrier params, noise level/mode |
| 3069-3070 | Gate amount + range |
| 3074-3083 | Spectral tilt/intensity (smoothed) |
| 3089-3094 | Pre-calculate gate threshold (linear, avoids per-sample log) |

### 4. Complement Buffer Copy (L3105-3116)
Copy raw input BEFORE Bell EQ for complement sub/high extraction.

### 5. Input Bell EQ (L3118-3191)
| Line | What |
|------|------|
| 3128-3158 | Bell 1 — `makePeakFilter`, process per-sample L/R |
| 3160-3191 | Bell 2 — same pattern |
Applied BEFORE wet/dry split — shapes what vocoder "hears" AND dry signal.

### 6. Dry Copy + Carrier Setup (L3193-3307)
| Line | What |
|------|------|
| 3195-3197 | Copy input (post-Bell EQ) to `dry` buffer |
| 3224-3232 | Create modulator buffer with `modLevel` gain |
| 3238-3303 | Carrier generation (Self/External/Noise/Saw/Square/PitchTrack) |
| 3271-3300 | External sidechain routing |

### 7. Noise Generation (L3309-3392)
| Line | What |
|------|------|
| 3317-3333 | Pink/white noise blend via `noiseColor` param |
| 3336-3392 | **Unvoiced detection**: HP/LP ratio (LPF@1kHz vs HPF@3kHz envelopes) |

### 8. Band Loop — Core Vocoder (L3394-3583) ★
| Line | What |
|------|------|
| 3427-3431 | BW-dependent tilt correction (0.6-1.0 dB/oct) |
| 3433 | `for (int bi = 0; bi < nBands; ++bi)` |
| 3440-3441 | Skip merged secondary bands (unless additive mode) |
| 3454-3458 | Skip note-excluded bands |
| 3484-3533 | **Per-sample envelope**: analysis filter -> abs -> attack/release -> level comp -> gate -> chord gate |
| 3541-3547 | Per-band multipliers: spectral gain, tilt correction, band gain, merge norm |
| 3549-3579 | **Carrier filtering**: stacked 2x bandpass (4th order) per channel, multiply by envelope * depth |
| 3581-3582 | Accumulate into `wet` buffer |

### 9. Spectral Centroid (L3585-3617)
Combines HP/LP ratio with band-energy centroid for robust unvoiced detection.

### 10. Crossfade (L3619-3744)
Equal-power crossfade between old and new bands (~50ms) when params change.

### 11. Post-Band Gain Compensation (L3746-3787)
| Line | What |
|------|------|
| 3747-3751 | Safety scaling: `1/sqrt(nBands/8)` |
| 3757-3766 | Depth compensation: +6dB at 100% depth |
| 3773-3787 | **Width compensation**: up to +23dB at narrow BW, steep `(1-norm)^6` curve |

### 12. Noise Mixing (L3789-3884)
| Line | What |
|------|------|
| 3801-3839 | Noise mode ratios: Vocal (35/65), Synth (70/30), Auto (carrier-aware) |
| 3844-3883 | Per-sample: `(baseNoise + reactiveNoise) * energyGate` added to wet |

### 13. HP Blend (L3886-3946)
High-passed modulator passthrough for sibilant clarity (Vocodex "clarity trick"). Gated by same gate params.

### 14. Post-Vocoder EQ / Input Filter (L3948-4006)
| Line | What |
|------|------|
| 3967 | `inputFilter.process(wet)` — HP/LP on wet |
| 3968 | `dryFilter.process(dry)` — same HP/LP on dry (master EQ) |
| 3973-3989 | Complement LP filter (isolate sub from original) |
| 3995-4001 | High complement HP filter (isolate highs from original) |

### 15. Complement Bells — Wet/Dry Ratio EQ (L4044-4180)
| Line | What |
|------|------|
| 4077-4130 | CompBell1: boost wet + inverse cut dry (or vice versa) |
| 4132-4180 | CompBell2: same pattern |
Inverse gain clamped to +/-12dB to prevent phase distortion.

### 16. Final Mix (L4182-4263)
| Line | What |
|------|------|
| 4192-4207 | `out = dry*(1-mix) + wet*mix` (with or without compBell EQ) |
| 4212-4263 | Sub complement (fixed level, ignores mix) + High complement (follows mix) |

### 17. Trial Nag + Output (L4274-4340)
| Line | What |
|------|------|
| 4277-4303 | Trial nag: 0.7s silence every 7s |
| 4306-4316 | Spectrum FFT (DISABLED for CPU) |
| 4318-4339 | **Output Gain** + output RMS metering |

---

## Key Support Functions

| Function | Line | Purpose |
|----------|------|---------|
| `updateEnvelopeCoeffs()` | 880 | Per-band attack/release, adaptive envelope, gate smoothing |
| `buildScaleNoteFrequencies()` | 979 | Generate scale note Hz array for snapping |
| `rebuildBands()` | 1425 | Allocate Band objects, set base/scale freqs, init filters, crossfade |
| `updateBandFilters()` | 1587 | Set carrier/analysis Q, formant shift, chord snap, merge detection |
| `updateBandsIfNeeded()` | 2579 | Check param changes, call rebuild or update |
| `calculateSpectralGain()` | (header) | X/Y pad spectral tilt multiplier |

---

## Key Constants & Calibrated Values

| Constant | Value | Line | Purpose |
|----------|-------|------|---------|
| `envLevelComp` | 15.5 (+23.8 dB) | 3472 | Envelope level to match Ableton |
| `maxBoostDb` (width comp) | 23.0 dB | 3782 | Width compensation ceiling |
| Width comp curve | `(1-norm)^6` | 3783 | Steep rolloff — big boost at narrow BW only |
| `tiltDbPerOct` | 0.6 + 0.4*bwNorm | 3431 | BW-dependent HF tilt correction |
| Safety scaling | `1/sqrt(nBands/8)` | 3749 | Prevents clipping with many bands |
| Depth comp | +6dB at 100% | 3760 | Compensates vocoder amplitude loss |
| Crossfade time | 50ms | 1445 | Band transition smoothing |
| Gate attack | 2ms (fixed) | 965 | Standard gate attack |
| Gate release | 30ms (fixed) | 966 | Prevents gate crackle |
| Formant smoothing | 80ms | 660 | Prevents IIR crackling |
| Param smoothing | 10ms | 654 | Spectral tilt/intensity |
| HP blend cutoff | 4000 Hz (fixed) | 639 | Sibilant passthrough frequency |
| Unvoiced LPF | 1000 Hz | 700 | Voiced content detection |
| Unvoiced HPF | 3000 Hz | 701 | Sibilant content detection |
| Pink noise cutoff | 1000 Hz | 647 | 1-pole LP for pink approximation |
| Q range | 0.5 - 200.0 | 1611-1612 | BandWidth -> Q mapping |
| Ritual mode analysis Q | 2.0 (fixed) | 1746 | Original Band Ritual behavior |
| Linked mode max Q | 48.0 (asymptotic) | 1751 | Caps analysis Q in linked mode |
| Merge threshold | 0.5 semitones | 1823 | Band deduplication distance |
| Chord gate threshold | 1 semitone | 1705 | Start fading |
| Chord gate falloff | 2 semitones | 1706 | Fully muted at 3 semitones |
| Noise: Vocal ratios | 35% base / 65% reactive | 3803-3804 | Speech intelligibility |
| Noise: Synth ratios | 70% base / 30% reactive | 3808-3809 | Constant grit texture |
| Inverse gain clamp | 0.25 - 4.0 (+-12dB) | 4093 | CompBell phase safety |
| Trial nag | 0.7s / 7s | 4276 | Silence burst interval |

---

## Quick Reference — Most Commonly Tuned Parameters

### Volume/Level Calibration
- **`envLevelComp`** (L3472): Master envelope scaling. Currently 15.5. Change this if overall vocoder is too quiet/loud vs reference.
- **`maxBoostDb`** width comp (L3782): Currently 23.0. If narrow-BW presets are too loud/quiet.
- **Width comp exponent** (L3783): Currently `^6`. Steeper = less compensation at mid-widths.
- **`tiltDbPerOct`** (L3431): HF tilt correction. Currently 0.6-1.0. If HF is too bright/dark.
- **Safety scaling** (L3749): `1/sqrt(nBands/8)`. If high band counts clip or are too quiet.
- **Depth comp** (L3760): +6dB at 100%. If vocoder at full depth is too quiet.

### Envelope / Dynamics
- **Gate attack/release** (L965-966): 2ms/30ms fixed. If gate clicks or is sluggish.
- **Formant smoothing** (L660): 80ms. If formant automation crackles, increase. If sluggish, decrease.
- **Envelope soft limit** (L3563): tanh above 2.0, peaks at ~3.0. If clipping on transients.

### Noise / Intelligibility
- **Vocal/Synth ratios** (L3803-3809): Base vs reactive noise blend per mode.
- **HP blend cutoff** (L639): 4kHz fixed. If sibilants need different frequency.
- **Unvoiced detection** LP/HP (L700-701): 1kHz/3kHz. If detection misses consonants.

### Band Behavior
- **Q range** (L1611-1612): 0.5-200. If extreme BW settings misbehave.
- **Merge threshold** (L1823): 0.5 semitones. If bands stack when they shouldn't (or vice versa).
- **Chord gate** (L1705-1706): 1st threshold + 2st falloff. If chord mode is too aggressive/loose.
- **Ritual analysis Q** (L1746): Fixed at 2.0. The "secret sauce" of Ritual mode.
- **Linked max Q** (L1751): 48.0 asymptotic cap. If linked mode is too resonant at high Q.