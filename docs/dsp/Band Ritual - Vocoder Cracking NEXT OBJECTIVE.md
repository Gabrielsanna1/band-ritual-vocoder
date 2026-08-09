---
tags:
  - band-ritual
  - vocoder
  - objective
  - next-session
date: '2026-03-26'
---
# Band Ritual — NEXT OBJECTIVE: Crack Transient/Resonant Behavior

## The Gap
Steady-state PSD matches at 1.8 dB (white noise). But on REAL AUDIO:
- Ableton's vocoder resonates at band frequencies when audio stops
- Band Ritual doesn't — sounds flat/lifeless in comparison
- At BW=10% the difference is most obvious — Ableton "rings", ours doesn't

## Root Cause Hypothesis
We LOWERED Q from 62 to 53 at BW=10% to match steady-state PVR.
But the PVR difference might have been caused by something else (gain staging, 
analysis bandwidth, etc.), and Q=62 was the CORRECT value (measured from Ableton's 
isolated band recordings). The Q reduction killed the resonant character.

## What We Need to Verify
1. Test Q=62 (original) + phase alternation + all current cracked params → does PVR still work?
2. Compare filter ring time at Q=62 vs Q=53 vs Ableton
3. The burst noise recordings (envelope_test/) show transient tails — re-analyze for ring time

## NEW Cracking Definition
A vocoder is "cracked" when BOTH are matched:
1. **Steady-state**: PSD on white noise ≤ 2 dB RMS (DONE)
2. **Transient**: Filter ring time, envelope attack/release dynamics, resonant decay ALL match Ableton when measured with burst/speech signals

## Current Q Table (v12, Q lowered)
{10, 53}, {15, 26}, {20, 19}, {30, 17}, {40, 11.4}, {50, 12.7}, {70, 10.7}, {100, 7.9}, {130, 7.6}, {170, 4.5}, {200, 3.5}

## Original Q Table (from Ableton isolated measurements)
{10, 62}, {30, 21}, {50, 12.7}, {70, 6.6}, {100, 6.2}, {130, 4.7}, {200, 3.0}

## Data Available
- White noise PSD: `ableton_crack_v2/crack_bw*.wav` (steady-state, DONE)
- Burst noise: `envelope_test/env_rel*.wav` (transient envelope)
- Isolated bands: `ableton_crack/crack_40band_bw10_ONLY_BAND*.wav` (true Q measurement)
- All comparison scripts in `~/Desktop/vocoder-comparison/`

## What MIGHT Be Missing
- Speech/vocal through Ableton's vocoder at BW=10% — to hear the resonance difference on real audio
- Impulse response of single bands — would directly show ring time
- Measurement of Q from full vocoder output (not just isolated bands)
