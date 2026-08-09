---
tags:
  - band-ritual
  - vocoder
  - next-session
date: '2026-03-23'
---
# Band Ritual — NEXT SESSION START HERE

## DEFINITIVE MEASUREMENT (2026-03-23, verified screenshot)
- **2 carrier stages, Q_stage = 61.02** — fit error 0.914
- 4 stages REJECTED (error 14.6)
- Q=85.6 was from a different session with WRONG settings
- Settings verified: 40 bands, BW=10%, Range=20-18kHz, Carrier=Modulator

## Current Build State (Q=62 + cos() + tilt)
- 2 carrier stages ✓
- Q_base=62 with cos(π×f/fs)^1.29 ✓
- Tilt: alpha = -0.259×wN + 0.186 ✓
- logspace(20, 18000, 40), bandDistributionCurve bypassed ✓
- Peak detector envelope ✓
- bwGain: OLD formula (WRONG — levels 4-10 dB off)

## ONLY THING LEFT TO DO
1. **Recalibrate bwGain** for Q=62 + cos() + tilt config
2. Use the BR_final_bw{X}_noise.wav recordings already made
3. Compute level corrections, update bwGain table
4. Build, install (fresh instance!), record, verify

## Level Corrections Needed (from latest recordings)
| BW% | Correction |
|-----|-----------|
| 10% | -4.5 dB |
| 30% | -10.3 dB |
| 50% | -10.6 dB |
| 70% | -9.3 dB |
| 100% | -3.5 dB |
| 130% | +1.5 dB |
| 170% | +2.4 dB |
| 200% | -1.8 dB |

## CRITICAL RULES
- ALWAYS use fresh plugin instance (new Ableton project)
- NEVER trust old cached state
- Change ONE thing at a time
- The Python simulation (minimal_vocoder.py) is ground truth

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- Definitive recording: `ableton_crack/compare.wav`
- Latest BR recordings: `final1_crack/BR_final_bw{X}_noise.wav`


## HONEST ASSESSMENT: Peaks ARE shifted at HF
Visual inspection of Q=62 linear-scale plots (q62_linear_check.png) shows:
- 500-1200 Hz: peaks roughly aligned
- 3000-7000 Hz: Band Ritual peaks shifted LEFT
- 7000-15000 Hz: Band Ritual peaks clearly shifted LEFT

This is with verified correct logspace frequencies (debug confirmed fA=fC=logspace values).
The OUTPUT peaks appear at different positions than the FILTER center frequencies.

**Unexplained.** Needs root cause investigation before any more parameter tuning.
Possible causes:
- cos() Q correction creating asymmetric filter shapes at HF
- Tilt correction interacting with peak positions
- Something in the carrier processing path shifting spectral content
- The PSD of noise × envelope creates peaks at different positions than carrier-only peaks


## HF PEAK SHIFT INVESTIGATION (2026-03-23)

### Findings
1. Our Q matches Ableton's isolated-band Q within 3-7% — cos()^1.29 is correct
2. Optimal exponent is 1.357 (marginal improvement over 1.29)
3. Ableton's FULL vocoder peaks shift at HF (up to ±125 cents) due to inter-band overlap
4. Our peaks DON'T shift — meaning our inter-band overlap behavior differs

### Root Cause
The Q per band is correct (verified from isolated recordings). But in the FULL vocoder, Ableton's wide HF peaks (Q=12-50) overlap and merge, shifting apparent peak centers. Our peaks overlap less, so they don't shift.

This could be from:
- Different envelope behavior creating different spectral spreading
- Different analysis Q/stages creating different envelope bandwidth
- Or simply that the Q is 3-7% too high at HF (ratio=0.93-0.97), just enough to reduce overlap

### Action for Next Session
1. Fix bwGain (levels 4-10 dB off — the ONE known remaining issue)
2. Try exponent 1.357 instead of 1.29 (marginal Q improvement)
3. Then compare — with correct levels AND correct Q, the HF peaks might shift properly
4. The 3-7% Q excess at bands 32-37 might be enough to explain the missing overlap
