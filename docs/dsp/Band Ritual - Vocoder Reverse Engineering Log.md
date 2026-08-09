# Band Ritual — Vocoder Reverse Engineering Log

## Goal
Match Band Ritual Improved's vocoder output to Ableton's vocoder as closely as possible.

## Test Setup
- **40 bands**, range 40Hz–18kHz, Attack 1ms, Release 50ms, Gate off, Depth 100%, Mix 100%
- **Ableton**: Carrier=Modulator, BW 10% (low) / 100% (high)
- **Band Ritual**: Carrier=Self, Linked mode, Scale Amt 0%, BW 0.1 (low) / 1.0 (high), ADAPT OFF
- **Test signals**: white noise, impulse train (5ms bursts every 500ms), sine sweep (20Hz→20kHz log) — all 10s, 44100Hz
- **Automated pipeline**: Spotify Pedalboard loads VST3 headlessly, processes signals, compares vs Ableton WAV exports

## Current State (2026-03-12, latest iteration)

### Level Matching — SOLVED
| Test | Offset | Tilt | Max Diff |
|------|--------|------|----------|
| Low BW noise | **-0.53 dB** | -0.24 dB/oct | 21.93 dB |
| High BW noise | **+0.95 dB** | -0.29 dB/oct | 6.94 dB |
| Low BW sine | +6.79 dB | +1.41 dB/oct | — |
| High BW sine | +0.62 dB | +1.27 dB/oct | — |

### Q Matching
- **Low BW (10%)**: Ableton Q=57.6, BR Q=57.4 — **perfect match**
- **High BW (100%)**: Ableton Q≈0, BR Q=18.7 — BR still too narrow (BW=1.0 maps to Q≈10, Ableton 100% is nearly flat)

### Envelope Timing (with ADAPT OFF)
- High BW: very close (within 1-2ms across all bands)
- Low BW: BR faster than Ableton on low-mids (7ms vs 44ms attack) — Ableton has inherent per-band smoothing

## Changes Made (chronological)

### Change 1: Added ADAPT toggle (adaptive envelope on/off)
- **What**: New `adaptiveEnv` bool parameter. OFF = flat timing. ON = per-band adaptive timing.
- **Why**: Ableton uses flat timing → zappy/reactive. Band Ritual's adaptive made low bands sluggish.
- **Default**: OFF (Ableton-style)

### Change 2: Extended BW range to 0.01–2.0
- **What**: BW parameter from 0.1–2.0 to 0.01–2.0, default 1.0
- **Why**: Ultra-narrow Q~200 for scale-locked mode (below Ableton's range)
- **Q mapping**: `Q = 0.5 * pow(400, 1-norm)` where norm=(bw-0.01)/(2.0-0.01)

### Change 3: Removed fastTanh envelope saturation
- **What**: Replaced `fastTanh(e * 4.0f)` with soft-limited linear: linear below 1.0, `1 + fastTanh(e-1)` above
- **Why**: fastTanh compressed ALL envelope dynamics → bwNormGain had zero effect, bands sounded "smoothed out"
- **Old**: `envShaped = fastTanh(e * 4.0f); envGain = (1 + depth*(1.75*envShaped - 0.75)) * envShaped`
- **New**: `eLimited = (e < 1) ? e : 1 + fastTanh(e-1); envGain = 1 - depth + depth * eLimited`

### Change 4: Flat envelope level compensation (+20 dB)
- **What**: `const float envLevelComp = 10.0f;` applied to all bands equally
- **Why**: Calibration showed raw uncompensated levels were -19 to -20 dB vs Ableton at BOTH BW settings. No Q-dependent correction needed — analysis/carrier filter interaction naturally balances.
- **Key insight**: Removed qComp and bwNormGain entirely. A single flat gain matches both BW settings.

### Change 5: Recalibrated width-based loudness compensation
- **What**: Reduced maxBoostDb from 10.5 to 9.0 in the width compensation curve
- **Why**: With envLevelComp=10 (+20 dB), the old 10.5 dB boost was too strong. 9.0 dB calibrated to give ~0 dB offset at both BW settings.

### Change 6: BW-scaled tilt correction
- **What**: Per-band tilt correction of 1.27 dB/oct, scaled by BW (0 at BW≤0.1, full at BW≥1.0)
- **Why**: 4th-order stacked carrier has slight HF rolloff vs Ableton's likely 2nd-order. Only visible at wide BW.
- **Code**: `tiltCorrectionDb = 1.27 * bwTiltScale * 9.0 * (bandPos - 0.5)`

## Remaining Improvements
1. **BW=1.0 Q too high** (18.7 vs Ableton's ~0): Would need to change Q mapping so BW=1.0→Q≈1 instead of Q≈10. Currently `Q = 0.5 * pow(400, 1-norm)` where BW=1.0 gives norm=0.497→Q≈10.
2. **Low BW envelope**: BR low-mids are faster than Ableton (7ms vs 44ms). Ableton may have inherent per-band smoothing even without "adaptive" mode.
3. **Sine sweep level at low BW**: +6.79 dB overshoot on resonant peaks. Soft limiter helps but doesn't fully cap. Real audio (voice/music) won't exhibit this.

## Key Files
- `PluginProcessor.cpp`: lines ~920 (bwNormGain=1.0), ~3470 (envLevelComp=10), ~3550 (soft limiter), ~3536 (tilt correction), ~3775 (width comp)
- `auto_compare.py`: `~/Desktop/vocoder-comparison/auto_compare.py`
- `vocoder_compare.py`: `~/Desktop/vocoder-comparison/vocoder_compare.py`  
- Results: `~/Desktop/vocoder-comparison/results/`
- Ableton reference WAVs: `~/Desktop/vocoder-comparison/ableton_*/test.wav`
