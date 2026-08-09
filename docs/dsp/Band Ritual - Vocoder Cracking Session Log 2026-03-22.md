---
tags:
  - band-ritual
  - vocoder
  - session-log
date: '2026-03-22'
---
# Band Ritual — Session Log (2026-03-22) — FINAL

## ROOT CAUSE FOUND: Cached Plugin State
The band center misalignment was caused by **cached plugin state** — NOT a code bug.
- Default `scaleAmount` = 1.0 (100%), default `bandCount` = 16
- When user loaded old session, plugin retained 16 bands + Scale Amount 100%
- Even after changing GUI to 40 bands / Scale 0%, the old state persisted
- **FIX: DELETE the old plugin instance and add a fresh one**

## CONFIRMED: Band Centers Align Perfectly
With a fresh plugin instance:
- Debug log shows: nBands=40, scaleAmt=0, formant=1.0, offset=0
- fAnalysis = fCarrier for ALL bands
- logspace(20, 18000, 40) matches Ableton within 4 cents

## CURRENT PLUGIN STATE (stripped for testing)
- 2 carrier stages ✓
- Q from lookup table (85.6 at BW=10%) — NO frequency-dependent correction
- logspace(20, 18000, 40) — NO hardcoded table needed
- Peak detector envelope (attack/release)
- NO tilt correction (tiltCorr = 1.0)
- NO bwGain table (constant 40 dB)
- bandDistributionCurve BYPASSED

## WHAT NEEDS TO BE ADDED BACK (one at a time)
1. **Tilt correction** — alpha = -0.259×wN + 0.186
2. **bwGain table** — recalibrate for current Q
3. **Frequency-dependent Q** — cos(π×f/fs)^1.29 (if Q=61 measurements are correct)
4. **BW-dependent Q table** — recalibrate

## KEY LEARNING
- ALWAYS delete old plugin instance before testing — cached state corrupts everything
- The `bandDistributionCurve` can persist from saved presets
- Default scaleAmount=1.0 snaps carrier frequencies to musical notes
- The Python simulation (`minimal_vocoder.py`) is the ground truth

## Q DISCREPANCY — STILL UNRESOLVED
- Q=85.6 (original isolated Band 20, first session)
- Q=61.0 (re-recorded all 40 bands, later session)
- Possibly different Ableton settings between sessions
- Need controlled re-measurement

## FILES
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- Simulation: `~/Desktop/vocoder-comparison/minimal_vocoder.py`
- Debug log: `~/Desktop/band_ritual_debug_freqs.txt`
- All recordings: `~/Desktop/vocoder-comparison/`
