---
name: Band Ritual - Grid VERIFIED Pure-Log 2026-06-15
description: >-
  Ableton vocoder grid = pure log 20-18kHz, i/(N-1), 302c/band. BR already
  matches to 7c RMS (measurement floor). The 34.5-15200 inset + low-band
  compression were an artifact of a clipped peak-search window. Nothing to fix.
type: reference
date: '2026-06-15T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - measurement
  - filter
  - grid
  - correction
---
# Band Ritual — Grid VERIFIED Pure-Log (2026-06-15)

> **SUPERSEDES** the "34.5–15200 inset" + "Ableton compresses the low bands" claims in
> `Band Ritual - Vocoder Cracking Final Status` (2026-03-22) and the framing in
> `Band Ritual - Ableton Filter Measurements 2026-04-09`. Those were a **measurement artifact**.

## TL;DR

Ableton's vocoder band grid is a **pure geometric (log) progression from 20 Hz to 18 kHz**,
40 bands, **endpoints inclusive** (`i/(N-1)` convention), **302 cents/band**.
**BR already replicates this exactly** — verified band-by-band against the real isolated-band
recordings: **7.0 cents RMS, max 12.5 cents, residuals RANDOM (no structure)**. 7 cents = the
FFT peak-detection resolution floor. The grids are indistinguishable. **Nothing to fix.**

## What was wrong before, and why

- March's "actual centers = 34.5–15200 Hz (inset from the 20–18000 user range)" and the later
  "Ableton compresses the low bands / systematic curvature" finding were **both derived from
  bad edge measurements**.
- Root cause: the peak-finder search window was clipped to **25 Hz – 17 kHz**. Band 1 (~20 Hz)
  fell *below* 25 Hz and band 40 (~18 kHz) *above* 17 kHz → the detector grabbed garbage for the
  two edge bands. Tell-tale: band 2 measured *lower* than band 1 (impossible for an ascending
  grid), and band 1 showed a +462-cent residual.
- Those two bogus edge points dragged the straight-line log fit, manufacturing a fake
  systematic curvature (low bands "compressed", high bands "spread") with RMS ~34 c.

## The corrected measurement (the proof)

Re-ran with a **wide search window (5 Hz – 22 kHz)** on the 40 isolated-band noise exports
(`ableton_crack/crack_40band_bw10_ONLY_BAND{1..40}_noise.wav`, on Ricosan HD):

- Grid now **monotonic**: band 1 = **19.86 Hz** (≈20), band 40 = **17986 Hz** (≈18 kHz).
- Pure-log fit residual: **7.0 c RMS, max 12.5 c, RANDOM sign** (no trend, no curvature).
- Implied endpoints from the fit: band1 = 20.00 Hz, band40 = 18004 Hz → exactly the user range.
- Direct compare to BR's actual formula `center[i] = 20·(18000/20)^(i/39)`:
  **7.0 c RMS, 12.5 c max** — identical within measurement noise.

## Consequence for BR

- BR's grid (`PluginProcessor.cpp` ~line 1567–1572): pure log, `i/(N-1)`, raw `lowFreq`/`highFreq`
  params, defaults **20 / 18000** (lines 411–417). This **IS** Ableton's grid. No remap, no inset.
- The old 34.5–15200 effective-endpoint remap is **already gone** from the current source — good.
- **Do NOT re-introduce any endpoint inset.** Do NOT bake a "measured center table" — the pure-log
  formula already matches to the measurement floor. Baking would only add noise.

## The narrow-BW "floor" — final word

The stubborn ~5–6 dB residual at BW 10–20% is **100% a measurement artifact**, not an audio
difference and not "filter topology DNA":
- At BW10 the filters are razor-sharp (Q≈85). A ~7-cent grid offset (the measurement floor) on a
  Q=85 filter produces a large *apparent* dB difference on the steep skirt — but the filter
  **shapes match** (true PVR 44.9 dB vs Ableton 41.5).
- No topology change (Bessel/Butter) and no scalar SHAPE value can close it, because there is
  nothing real to close. At the BW actually used (35–65%) the match is excellent.

## Method / reusable

`/tmp` python (python3.13 + scipy.io.wavfile). Per isolated-band file: normalize, long Hann FFT,
argmax in a **generous** frequency window (NOT clipped near the range edges), parabolic
interpolation on log-magnitude for sub-bin center. **Lesson: never clip the peak-search window
inside the band range you're measuring — it silently corrupts the edge bands.**

## Related

- `Dev Notes/Band Ritual - Vocoder Cracking Final Status` (now corrected at top)
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09` (Q-vs-band-count still valid)
- `Dev Notes/Band Ritual - Vocoder Cracking DEFINITIVE`
