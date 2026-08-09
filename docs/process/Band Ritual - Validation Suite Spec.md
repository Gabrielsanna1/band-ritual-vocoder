# Band Ritual — Validation Suite Spec

**Tags**: #bandritual #vocoder #testing

## Purpose

Prevent single-file overfit (see `Band Ritual - Linked Mode SVF Regression 2026-04-08`). Validate Linked-mode Ableton matching across diverse inputs and parameter combinations so a calibration that fits one file but breaks others can never be accepted again.

## Required Ableton exports (user must produce)

For each item below: render the **dry source** AND the **Ableton-vocoded version** of that source through Ableton Live's stock Vocoder, with the settings listed in the next section. Same project, same buffer size, same sample rate (48 kHz preferred). Save as 24-bit WAV.

1. **Vocal a cappella** — 8–15 s, dry monophonic vocal (preferably with sustained vowels and a few consonants for transient testing). Dry + vocoded.
2. **Snare loop** — 8 s of repeated snare hits. Dry + vocoded.
3. **Sine sweep** — 20 Hz → 20 kHz logarithmic, 10 s. Dry + vocoded.
4. **White noise** — 10 s, full-band, −12 dBFS RMS. Dry + vocoded.
5. **Pink noise** — 10 s, −12 dBFS RMS. Dry + vocoded.
6. **Single sine 50 Hz** — 5 s, −12 dBFS. Dry + vocoded.
7. **Single sine 1 kHz** — 5 s, −12 dBFS. Dry + vocoded.
8. **Single sine 10 kHz** — 5 s, −12 dBFS. Dry + vocoded.

## Ableton Vocoder settings (apply to ALL exports)

| Param | Value |
|---|---|
| Bands | 40 |
| BW | 10 % |
| Range | 20 Hz – 18 kHz |
| Gate | min |
| Dry/Wet | 100 % |
| Depth | 100 % |
| Formant | 0 |
| Noise | 0 |
| Carrier | Modulator |
| Attack | 1 ms |
| Release | 50 ms |

## Pass criterion (Phase C)

**All 7 frequency regions within ±2 dB on ALL inputs simultaneously.** No single input may be optimized at the expense of another.

## Region definitions

| Region | Range |
|---|---|
| Sub | 20 – 80 Hz |
| Bass | 80 – 200 Hz |
| Low | 200 – 500 Hz |
| Mid | 500 Hz – 2 kHz |
| HiMid | 2 – 6 kHz |
| High | 6 – 12 kHz |
| Air | 12 – 18 kHz |

## Test rig

Existing `vocoder-comparison/gate_test/perband_iterate.py` is the template. It will be replaced by `vocoder-comparison/validation_suite.py` which:

- Loads all 8 dry/vocoded pairs.
- Renders Band Ritual via the Pedalboard host on each dry source.
- Computes per-region dB error vs the Ableton reference.
- Outputs a pass/fail table per input × region.
- Fails the run if any cell exceeds ±2 dB.

## Phase D — Parameter sweep coverage

Once Phase C passes for the default parameter set, sweep the full matrix:

| Parameter | Values |
|---|---|
| BW | 10, 30, 50, 100, 170, 200 % |
| Band count | 16, 32, 40, 64, 100 |
| Gate | −inf, −60, −40, −20, 0, +12 dB |
| Depth | 0, 0.5, 1.0, 1.5, 2.0 |
| Formant | −24, −12, 0, +12, +24 semis |
| Noise | 0, 25, 50, 100 % |

**Pass criterion (Phase D)**: max region error ≤ **3 dB** across the entire matrix on a diagnostic input set (white noise + vocal). This is the actual "cracked" criterion — it means the BR Linked mode tracks Ableton's vocoder across the full parameter space, not just at a single point.

## Related notes

- `Dev Notes/Band Ritual - Linked Mode SVF Regression 2026-04-08`
- `Dev Notes/Ableton Vocoder - Master Findings Log`
- `Dev Notes/Band Ritual - Sub Mid Calibration 2026-04-07`


---

## Current baseline: Iter 14c (2026-04-09)

The validation suite baseline is now **iter 14c** (hybrid envelope follower). See `Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09` for the full results.

### Noise regions (pass ±3 dB)

| Test | Sub | Bass | Low | Mid | HiMid | High | Air |
|---|---|---|---|---|---|---|---|
| white_noise | -1.8 | -0.7 | -2.0 | -0.2 | +0.9 | -0.1 | -3.2 |
| pink_noise  | -1.8 | -0.6 | -1.4 | -0.7 | +0.7 | +0.2 | -2.5 |

### Sines (mixed)
- sine_050 broadband: **+0.5 dB** (pass)
- sine_1000 in-band: -13.7 dB (fail)
- sine_10k in-band: -16.0 dB (fail)

### Sweep (fail — monotonic asymmetry)
Sub +26 · Bass +22 · Low +18 · Mid +12 · HiMid +3 · High -4 · Air -11

Regressions against these numbers should block merge.

---

## Regression suite addition: Multi-band filter shape verification

Five Ableton white-noise exports at band counts **{4, 8, 12, 16, 20}** (BW=10%, Range=20 Hz–18 kHz, all other settings per the spec above) are now part of the regression suite. These are used to verify:

1. BR's per-band Q scales with band count per `Q ≈ N / (n_octaves × BW_fraction)` (currently does not — action item).
2. Skirt slopes stay in the 47-52 dB/octave range (4-stage biquad cascade).
3. HF asymmetric rolloff reproduces Ableton's bilinear transform warping.

Analysis tool: same `validation_suite.py`, extended to load these 5 reference files and compare per-band peak width / peak gain / skirt slope.

See `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09` for the raw measurement findings and the derived Q(N, BW) formula.
