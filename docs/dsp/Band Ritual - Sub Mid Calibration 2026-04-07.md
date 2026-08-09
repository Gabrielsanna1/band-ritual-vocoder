# Band Ritual - Sub/Mid Calibration (2026-04-07)

## Discovery: Sub-rich input reveals hidden problem
With DRYWAV1_WARPED (low sub energy): tests appeared "cracked" within 4 dB.
With DRY-EXPORT 1 (sub-rich, like a kick): BR generates +11 dB sub vs Ableton.

## Root cause
Band Ritual's per-band carrier Q correction `cos(w)^1.29` only reduces Q at HIGH frequencies.
Below 1kHz: cos(w) ≈ 1, so Q stays at maximum.
Result: low-freq bands have HIGH resonance → amplify sub heavily on sub-rich inputs.

Ableton apparently has lower Q or attenuation at low bands.

## Current state (PluginProcessor.cpp ~line 1856)
Tried in Linked mode:
1. **Pure bypass** (tiltCorr = overlapComp = 1.0): sub +11dB, mid -9dB
2. **Mid bell only** (Gaussian @700Hz +5dB): sub +11dB still
3. **Sub cut + mid bell** (-12dB@20Hz + +5dB@700Hz): sub +2.8dB but bass 80-200Hz -9.8dB
4. **Original full corrections**: sub +9.3dB, mid was OK-ish

The dependency is non-trivial — fixing sub creates a hole in bass, fixing mid causes sub spike.

## NEXT STEPS
1. Need to **measure ALL bands individually** with the sub-rich input to find exact per-band gain error
2. The fix should be a **per-band gain table** matching Ableton's actual response
3. Or: investigate if Q at low freqs is actually the root issue and reduce it surgically

## Test files
- `gate_test/BR - EXPORT 1.wav` (current, with sub problem)
- `gate_test/ABLETONVOCODER - EXPORT 1.wav` (target)
- `gate_test/DRY-EXPORT 1.wav` (sub-rich input)
- `gate_test/EXPORT1_comparison.png` (PSD overlay)


## 2026-04-08 — Approach abandoned

The per-band LUT iterative approach was abandoned after the LUT converged to a violently zigzagging pattern (±25 dB swings on adjacent bands, several clamped at the limit). Investigation showed the zigzag is caused by structural issues in the carrier path (phase inversion + SVF regression), not by genuine per-band correction needs. See `Band Ritual - Linked Mode SVF Regression 2026-04-08`.

The DRY-EXPORT 1.wav sub-rich test case is preserved as a regression test for the new structural fix approach.
