---
tags:
  - band-ritual
  - vocoder
  - final
date: '2026-03-24'
---
# Band Ritual — Vocoder Cracking FINAL v12 (2026-03-24)

## Results: 1.8 dB avg RMS, 0.0 dB levels
| BW% | Level | RMS | PVR diff |
|-----|-------|-----|----------|
| 10% | 0.0 | 1.5 | +1.0 |
| 30% | 0.0 | 2.2 | -3.2 |
| 50% | 0.0 | 1.3 | +0.9 |
| 70% | 0.0 | 2.2 | +0.4 |
| 100% | 0.0 | 2.2 | -2.5 |
| 130% | 0.0 | 2.3 | -2.3 |
| 170% | 0.0 | 1.1 | -4.4 |
| 200% | 0.0 | 1.3 | -5.7 |

## Architecture (CRACKED)
- **Grid**: logspace(20, 18000, 40)
- **Carrier**: 2 cascaded biquad bandpass (4th order)
- **Analysis**: 3 stages, Qa = (0.85 - 0.55*widthNorm) × Qc (BW-dependent)
- **Phase alternation**: every other band inverted before summing
- **Envelope**: peak detector (attack 1ms, release 50ms)
- **cos()^1.29**: frequency-dependent Q correction

## Parameters
### Q Table (11 entries)
```
{10, 53}, {15, 26}, {20, 19}, {30, 17}, {40, 11.4},
{50, 12.7}, {70, 10.7}, {100, 7.9}, {130, 7.6}, {170, 4.5}, {200, 3.5}
```

### bwGain Table
```
{0.000, 44.3}, {0.105, 34.9}, {0.211, 34.4}, {0.316, 33.7},
{0.474, 30.3}, {0.632, 28.7}, {0.842, 21.1}, {1.000, 16.8}
```

### Tilt
```
tiltAlpha = -0.003*norm - 0.090
LF shelf: 3.0 * (1 - |norm - 0.35| * 2.5) dB below 250 Hz
```

### Analysis Q ratio
```
qaRatio = 0.85 - 0.55 * norm  (0.85 at BW=10%, 0.30 at BW=200%)
```

## Key Discoveries (entire cracking journey)
1. **Reference bug**: original Ableton recordings had low_freq=40 not 20 → 18 dB error
2. **Phase alternation**: alternating band polarity reduced avg RMS from 4.1→1.7 dB (KVR forum tip)
3. **Filter type**: 4th order bandpass confirmed, topology doesn't matter, only order
4. **BW-dependent Qa**: analysis Q ratio drops at wide BWs for smoother envelopes
5. **Pedalboard**: Spotify's Python lib loads VST3 and matches Ableton within 1.0 dB

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- References: `~/Desktop/vocoder-comparison/ableton_crack_v2/`
- Comparison plots: `~/Desktop/vocoder-comparison/FINAL_v12.png`
