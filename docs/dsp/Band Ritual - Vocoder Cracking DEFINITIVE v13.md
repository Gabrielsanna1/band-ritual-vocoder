---
tags:
  - band-ritual
  - vocoder
  - definitive
date: '2026-03-24'
---
# Band Ritual — Vocoder Cracking DEFINITIVE v13 (2026-03-24)

## Results: 1.8 dB avg RMS, 0.0 dB levels, envelope matched
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

## CRACKED Architecture
1. **Grid**: logspace(20, 18000, 40)
2. **Carrier**: 2 cascaded biquad bandpass (4th order, JUCE makeBandPass)
3. **Analysis**: 3 stages, Qa = (0.85 - 0.55*widthNorm) × Qc
4. **Phase alternation**: every other band (bi%2) inverted before summing
5. **Envelope**: `pow(0.01, 1/(sr*t))` — release = time to -40 dB (VERIFIED from burst test)
6. **cos()^1.29**: frequency-dependent Q correction
7. **Tilt**: alpha = -0.003*norm - 0.090 + LF shelf (3.0*(1-|norm-0.35|*2.5) dB below 250 Hz)

## Parameters
### Q Table (11 entries)
{10,53}, {15,26}, {20,19}, {30,17}, {40,11.4}, {50,12.7}, {70,10.7}, {100,7.9}, {130,7.6}, {170,4.5}, {200,3.5}

### bwGain Table
{0.000,44.5}, {0.105,35.1}, {0.211,34.6}, {0.316,33.8}, {0.474,30.3}, {0.632,28.6}, {0.842,20.9}, {1.000,16.6}

## Envelope Formula (CRACKED)
```cpp
attackCoeff = pow(0.01, 1.0 / (sampleRate * attackSec));
releaseCoeff = pow(0.01, 1.0 / (sampleRate * releaseSec));
```
Verified: Ableton at release=200ms → T_1%=191ms (matches pow(0.01) → T_1%=200ms)

## Session Discoveries
1. Reference bug: ableton_crack/ had low_freq=40 → re-recorded as ableton_crack_v2/
2. Phase alternation: KVR forum tip, reduced RMS from 4.1→1.8 dB
3. Envelope formula: pow(0.01) not exp(-1) — verified from burst noise recordings
4. Pedalboard: reliable within 1.0 dB, enables automated iteration
5. Filter shape: 4th order confirmed, topology irrelevant

## TODO: Crack Ableton's Unvoiced/Noise system
