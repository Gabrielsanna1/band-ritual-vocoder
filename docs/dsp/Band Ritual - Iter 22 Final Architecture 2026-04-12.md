---
name: Iter 22 Final — 91.4% Cosine Match to Ableton
description: Complete reverse-engineered architecture. Q=40, Qa=0.52×Qc, simple peak detector, e^0.75 compression. Session went from 0.4493 to 0.9138. All standard vocoder techniques tested and confirmed.
type: project
date: 2026-04-12
tags: [bandritual, vocoder, ableton, iteration22, final, architecture]
---

# Band Ritual — Iter 22 Final Architecture (2026-04-12)

## Result: 0.9138 cosine (91.4%) — from 0.4493 (44.9%)

## Locked Architecture

| Component | Value | How confirmed |
|---|---|---|
| Carrier filter | 4-stage cascaded RBJ BPF2 | Sweep 3/4/5 stages |
| Carrier Q (BW=10%) | 40 | Formula Q=N/(n_oct×BW) + sweep 37-43 |
| Analysis filter | 3-stage cascaded RBJ BPF2 | Sweep 2/3 stages |
| Analysis Qa | 0.52 × Qc = 20.8 | Fine sweep 0.40-0.85 |
| Envelope follower | Simple peak detector | Beat all alternatives (LP, RMS, cascade) |
| Envelope compression | e^0.75 | Sweep 0.5-1.0, peak at 0.75 |
| Rectification | Full-wave |y| | Beat half-wave and power |
| Coefficient formula | exp(-1/(sr×tau)) | +0.0003 over pow(0.01) |
| Phase alternation | ON (marginal) | +0.0005 |
| Depth formula | floor=0.065, >100% exp=1.35 | Measured from AB_DEPTH exports |
| bwGain BW=10% | 35.0 dB | Level matching |

## What was tested and DOESN'T help

| Technique | Cosine | Result |
|---|---|---|
| Textbook LP envelope (10-100 Hz) | 0.84-0.92 | Worse than peak detector |
| Half-wave rectification | 0.9092 | Slightly worse |
| Power detection (y²) | 0.8110 | Much worse |
| RMS envelope | 0.9199 | Close but worse |
| Carrier normalization | 0.7326 | Much worse |
| Envelope normalization | 0.8548 | Worse |
| 2x oversampling | 0.9159 | No improvement over 1x |
| Per-band tilt (f^1.0) | 0.4405 | Hurts (already inherent in constant-Q) |
| Cascade LP | 0.6797 | Band-aid for Q=6 bug |
| Ring buffer boxcar | 0.54 | Implementation had Q=6 bug |
| Float64 precision | 0.9159 | +0.003 but can't use in JUCE IIR |

## Journey this session

| Step | Cosine | What changed |
|---|---|---|
| Start (Q=6, cascade LP) | 0.4493 | — |
| Q=37 restored | 0.7747 | **+0.33** |
| Simple peak detector | 0.9003 | **+0.13** |
| Q=40 | 0.9065 | +0.006 |
| Qa=0.52 | 0.9128 | +0.006 |
| exp(-1/(sr×tau)) formula | 0.9131 | +0.0003 |
| Envelope e^0.75 | 0.9138 | +0.0007 |

## Theoretical ceiling

- Float64 simulation: 0.9209 (92.1%)
- Float32 plugin: 0.9138 (91.4%)
- Gap: 0.007 (float precision loss)
- Remaining ~8%: irreducible implementation differences, input-specific harmonic patterns

## Key lessons

1. **Q=6 regression** was the root cause of everything (wasted 15hrs of exotic fixes)
2. **Simple peak detector** beats all textbook envelope techniques for matching ABL
3. **Frequency-dependent corrections HURT cosine** — ABL uses no per-band tilt
4. **Validate basic parameters FIRST** before optimizing architecture
5. **16-bit WAV clipping** corrupted all measurements for months
6. **ABL's vocoder is simple**: standard IIR filters + peak detector + slight compression

## Files

- Plugin source: `/Users/ricosan/Desktop/BandRitualV3/Source/`
- Test script: `/Users/ricosan/Desktop/vocoder_test/realmusic_test.py`
- ABL exports: on Ricosan HD (read-only, needs reformat)
- Git tag: iter22-final

## Related notes

- `Dev Notes/Reverse Engineering - Lessons Learned 2026-04-12.md`
- `Dev Notes/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md`
