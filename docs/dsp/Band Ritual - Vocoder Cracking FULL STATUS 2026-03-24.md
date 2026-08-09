---
tags:
  - band-ritual
  - vocoder
  - status
date: '2026-03-24'
---
# Band Ritual — Vocoder Cracking FULL STATUS (2026-03-24)

## What's CRACKED ✓
1. **Band grid**: logspace(20, 18000, 40) — confirmed from isolated recordings
2. **Filter type**: 4th order bandpass (2 cascaded biquads) — confirmed from filter shape analysis
3. **Phase alternation**: every other band inverted — from KVR forum, reduced RMS 4.1→1.8 dB
4. **Envelope formula**: `pow(0.01, 1/(sr*t))` — release = time to -40 dB, verified from burst test
5. **Q table**: calibrated per BW (11 entries), with cos()^1.29 freq-dep correction
6. **Tilt**: alpha = -0.003*norm - 0.090 + LF shelf below 250 Hz
7. **bwGain**: calibrated per BW (8 entries)
8. **Analysis Qa ratio**: BW-dependent (0.85 at BW=10% → 0.30 at BW=200%)
9. **Gate range**: -inf(-60) to +12 dB, matching Ableton's range, displays "-inf" at minimum
10. **Gate threshold**: knob_dB - 17 dB offset (accounts for per-band energy split)

## Results: 1.8 dB avg RMS spectral match
| BW% | Level | RMS | PVR diff |
|-----|-------|-----|----------|
| 10% | 0.0 | 1.6 | -0.7 |
| 30% | 0.0 | 2.1 | -3.5 |
| 50% | 0.0 | 1.4 | +0.6 |
| 70% | 0.0 | 2.3 | +0.2 |
| 100% | 0.0 | 2.3 | -2.7 |
| 130% | 0.0 | 2.3 | -2.1 |
| 170% | 0.0 | 1.0 | -3.0 |
| 200% | 0.0 | 1.3 | -3.8 |

## What STILL needs work

### 1. Gate fine-tuning (close but not identical)
- Gate cutoff timing is close but our gate cuts slightly later than Ableton
- Ableton's gate has a smoother fade-out, ours is more abrupt (hard gate vs soft gate)
- The -17 dB offset might need per-BW adjustment
- **Next step**: Try a softer gate (restore gateRange instead of hard 0), compare gate fade shape

### 2. BW=100-130% spectral shape
- Our bands still slightly more pronounced than Ableton's at mid BWs
- PVR diff: -2.1 to -2.7 (our valleys slightly shallower)
- LF region (below 200 Hz) still ~3 dB quieter than Ableton
- **Next step**: This might improve with correct gate behavior (gate affects low bands first)

### 3. Noise/Unvoiced system
- Not yet cracked — need to match Ableton's unvoiced detection and noise injection
- Our system exists but hasn't been calibrated against Ableton
- **Next step**: Export recordings with Unvoiced knob at different levels, analyze detection

### 4. Gate smoothing
- Ableton's gate fades smoothly (visible in the fade test — gradual rolloff)
- Our gate has a more sudden cutoff
- **Next step**: Adjust gate attack/release smoothing coefficients (currently 2ms/30ms)

## Key Technical Insights
- `pow(0.01, 1/(sr*t))` NOT `exp(-1/(sr*t))` for envelope coefficients
- Phase alternation on every other band is essential for smooth overlap at wide BWs
- Pedalboard (Spotify) matches Ableton export within 1.0 dB — reliable for iteration
- Reference recordings MUST use correct low_freq (old recordings had low_freq=40, wasted hours)
- Gate operates on per-band envelope which is ~17 dB below broadband input level

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- Correct references: `~/Desktop/vocoder-comparison/ableton_crack_v2/`
- Gate test: `~/Desktop/vocoder-comparison/gate_test/`
- Envelope test: `~/Desktop/vocoder-comparison/envelope_test/`
