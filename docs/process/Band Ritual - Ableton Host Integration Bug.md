---
tags:
  - band-ritual
  - bug
  - ableton
  - hosting
date: '2026-03-26'
---
# Band Ritual — Ableton Host Integration Bug (2026-03-26)

## Problem
Same plugin, same settings, same input audio — but Ableton produces DIFFERENT output than pedalboard.
- RMS ratio: 0.707 (-3 dB) — exactly 1/sqrt(2)
- Correlation: 0.675 (not 1.0 — different signal, not just level)
- Massive LF deficit in Ableton: -36 dB at 40-80 Hz, -21 dB at 80-320 Hz
- Pedalboard output matches Ableton's built-in vocoder at 4.2 dB RMS

## Root Cause Theory
The 1/sqrt(2) ratio is the classic mono/stereo summing coefficient. Ableton might be:
1. Sending stereo input but our plugin treats one channel differently
2. Converting stereo to mono with -3 dB before the plugin
3. Bus routing mismatch — sidechain bus interfering with main input

## Verified
- Settings are correct (debug log confirms all parameters match during playback)
- Plugin processes correctly via pedalboard (DSP code is right)
- Input filter ON/OFF makes no difference
- Fresh plugin instance doesn't help

## Next Steps
1. Check VST3 bus configuration — does our sidechain input cause Ableton to route differently?
2. Try disabling the sidechain bus entirely and test
3. Check if Ableton sends stereo or mono to the plugin
4. Compare raw input buffer in Ableton vs pedalboard (already done: bufCh=2 in both)

## Key Data
- Debug: numCh=2, bufCh=2, inputRMS=-11 dBFS — Ableton sends stereo at correct level
- But output is -3 dB and LF-deficient compared to pedalboard
- The processing chain AFTER the input buffer is the problem area
