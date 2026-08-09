---
tags:
  - band-ritual
  - vocoder
  - session-log
date: '2026-03-23'
---
# Band Ritual — Vocoder Cracking Session (2026-03-23) — FINAL

## CRITICAL BUG FOUND: Reference recordings had wrong low_freq
- `ableton_crack/crack_bw*.wav` were recorded at **low_freq=40**, NOT 20
- This caused 18.3 dB RMS mismatch — peaks at completely wrong positions
- Fix: re-recorded as `ableton_crack_v2/` with low_freq=20
- Result: BW=10% went from 18.3 dB → 5.7 dB instantly

## Current Plugin Config (v5)
- 2 carrier stages, Q from lookup table (53 at BW=10%, scaled down from 62)
- cos(π×f/fs)^1.29 frequency-dependent Q correction
- Tilt: alpha = -0.259×wN + 0.186
- bwGain v5: calibrated against ableton_crack_v2 reference
- Peak detector envelope (attack 1ms, release 50ms)
- logspace(20, 18000, 40), bandDistributionCurve bypassed

## Results (vs Ableton Vocoder, correct reference)
| BW% | Level | Shape RMS | PVR diff |
|-----|-------|-----------|----------|
| 10% | 0.0 dB | 5.7 dB | +2.7 |
| 30% | 0.0 dB | 4.5 dB | +2.3 |
| 50% | 0.0 dB | 2.4 dB | +6.5 |
| 70% | 0.0 dB | 2.4 dB | +6.7 |
| 100% | 0.0 dB | 2.8 dB | +6.3 |
| 130% | 0.0 dB | 4.7 dB | +6.8 |
| 170% | 0.0 dB | 5.6 dB | +5.4 |
| 200% | 0.0 dB | 5.0 dB | +4.0 |
| **Avg** | **0.0** | **4.1** | **+5.1** |

## Remaining issue: PVR too high at wide BWs
- Our bands are slightly sharper than Ableton's
- Lowering Q INCREASES PVR (non-monotonic relationship at low Q)
- This is structural — different filter implementation, not tunable

## Key Discovery: Pedalboard workflow
- Matches Ableton export within 1.0 dB
- Enables automated iteration without user exports
- Set ALL params explicitly (defaults wrong: band_count=16, scale_amount=1.0)

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- CORRECT references: `~/Desktop/vocoder-comparison/ableton_crack_v2/`
- OLD references (WRONG low_freq=40): `~/Desktop/vocoder-comparison/ableton_crack/`
