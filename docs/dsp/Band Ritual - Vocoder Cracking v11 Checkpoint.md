---
tags:
  - band-ritual
  - vocoder
  - checkpoint
date: '2026-03-24'
---
# Band Ritual — Vocoder v11 Checkpoint (SAVE POINT)

## Current Results: 1.7 dB avg RMS
| BW% | Level | RMS | PVR diff |
|-----|-------|-----|----------|
| 10% | 0.0 | 1.4 | -0.7 |
| 30% | 0.0 | 2.1 | -3.8 |
| 50% | 0.0 | 1.3 | +0.3 |
| 70% | 0.0 | 2.2 | +0.1 |
| 100% | 0.0 | 2.2 | -2.4 |
| 130% | 0.0 | 2.3 | -1.9 |
| 170% | +0.5 | 1.1 | -4.2 |
| 200% | +0.5 | 1.4 | -5.6 |

## Current Parameters (v11)
### Q Table (line 1549)
```
{10, 53}, {15, 26}, {20, 19}, {30, 17},
{40, 11.4}, {50, 12.7}, {70, 10.7},
{100, 7.9}, {130, 7.6}, {170, 4.5}, {200, 3.5}
```
Table size: 11

### bwGain Table (line ~3832)
```
{0.000, 42.6}, {0.105, 33.6}, {0.211, 33.5}, {0.316, 33.2},
{0.474, 30.4}, {0.632, 29.5}, {0.842, 23.4}, {1.000, 19.9}
```

### Tilt (line ~1792)
```
tiltAlpha = -0.003*norm - 0.090
```
Plus LF shelf: `3.0 * (1 - |norm - 0.35| * 2.5)` dB below 250 Hz

### Phase Alternation (line ~3627)
Every other band (bi%2) gets polarity inverted before summing to wet.

### cos()^1.29 freq-dep Q — still active

### Analysis Q: Linked mode uses Qa = 0.85 * Qc

## Key Discoveries This Session
1. **Reference was wrong** — ableton_crack/ had low_freq=40, re-recorded as ableton_crack_v2/ with low_freq=20
2. **Phase alternation** — alternating band polarity reduced avg RMS from 4.1→1.7 dB
3. **Pedalboard workflow** — reliable within 1.0 dB of Ableton export, enables automated iteration
4. **Filter type confirmed** — 4th order bandpass, filter topology doesn't matter, only order

## Remaining Issue
BW=100-130% still shows visible peaks while Ableton is nearly flat. Hypothesis: analysis Q ratio or envelope type differs at wide BWs.
