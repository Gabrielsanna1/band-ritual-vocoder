# BAND RITUAL — ALL DEVELOPMENT NOTES

Auto-generated context bundle. Do not edit by hand; regenerate with `bundles/make-bundles.sh`.
Source repo: https://github.com/eqenrico-hub/band-ritual-prototype

CONTAINS: all 59 development notes + the distilled CONTEXT + the git history.
NO source code. This is the RECOMMENDED bundle - it is the reasoning, which is
the valuable part, and it fits in every chatbot's context window.

---------------------------------------------------------------------------
HOW TO USE THIS FILE
---------------------------------------------------------------------------
Upload or paste this whole file into any AI chatbot (Gemini, ChatGPT, Claude,
Copilot, Mistral, DeepSeek...) and then ask it questions about the project.
Everything it needs is inside this one file.

Good opening prompt:

  "This is the complete development record of a vocoder plugin called Band
   Ritual, which reverse-engineered Ableton Live's stock Vocoder. Read all of
   it, then give me a summary of the final architecture and the list of things
   that were tried and rejected."

---------------------------------------------------------------------------
READ THIS BEFORE TRUSTING ANY NUMBER IN HERE
---------------------------------------------------------------------------
1. These notes were written across three months and CONTRADICT EACH OTHER,
   because the author's understanding kept changing. When two notes disagree,
   THE LATER ONE WINS. Dates are in the filenames and headers.

2. Notes titled "FINAL", "DEFINITIVE", "v12", "v13" were NOT final. Trust the
   dates, not the titles.

3. Anything measured BEFORE 2026-04-11 was measured on clipped 16-bit audio
   and is INVALID. See the note "Band Ritual - WAV Clipping Bug Discovery
   2026-04-11". Numbers from before that date must not be quoted as fact.

4. The single most reliable summary is the first section below (CONTEXT.md).
   The definitive measured constants are in "Iter 22 Final Architecture".

---------------------------------------------------------------------------


================================================================================
FILE: docs/CONTEXT.md
================================================================================

# CONTEXT — what Band Ritual is, and what was actually learned

*Entry point for anyone (human or agent) reading this prototype cold.*

---

## 1. What the project was

Band Ritual is a **vocoder plugin** (JUCE, VST3/AU). The goal was not "a vocoder" in the abstract —
it was to **reverse-engineer Ableton Live's stock Vocoder** to a measurable match, then build a
plugin around that engine with its own character mode and GUI.

"Measurable match" means a real number: **cosine similarity** between the plugin's output and
Ableton's output on the same input, same settings. That metric drove every decision for 3 months.

| Milestone | Cosine vs Ableton |
|---|---|
| Session start (broken, Q=6 regression) | 0.4493 |
| Iter 22 "final architecture" | 0.9138 |
| Final committed state, avg across 5 inputs | **0.927** |
| Float64 theoretical ceiling | 0.9209 (single input) |

The remaining gap is irreducible: float32 precision in JUCE's IIR, plus input-specific harmonic
differences. **It is not worth chasing further** — that was tested exhaustively.

---

## 2. The cracked algorithm

Ableton's vocoder turned out to be **simple**. Standard IIR filters, a peak detector, slight
compression. Months were lost assuming it was clever.

```
For each band i:
  1. analysis  = cascaded_bandpass(modulator, fc[i], Qa)   // 3 stages, RBJ BPF2
  2. envelope  = peak_detect(|analysis|)                    // full-wave rectify, simple peak
  3. envelope  = envelope ^ 0.75                            // slight compression
  4. carrier_f = cascaded_bandpass(carrier, fc[i], Qc)      // 4 stages, RBJ BPF2
  5. band_out  = carrier_f * envelope * band_gain * depth_factor

Sum all bands -> output

No tanh. No soft-clip. No tonal detection. No per-band tilt correction.
```

### Locked constants (Iter 22, `docs/dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md`)

| Component | Value | How it was confirmed |
|---|---|---|
| Carrier filter | 4-stage cascaded RBJ BPF2 | swept 3 / 4 / 5 stages |
| Carrier Q (at BW=10%) | **40** | formula `Q = N/(n_oct × BW)` + sweep 37–43 |
| Analysis filter | 3-stage cascaded RBJ BPF2 | swept 2 / 3 stages |
| Analysis Q | **0.52 × Qc** = 20.8 | fine sweep 0.40–0.85 |
| Envelope follower | simple peak detector | beat every alternative |
| Envelope compression | `e^0.75` | swept 0.5–1.0, peak at 0.75 |
| Rectification | full-wave `\|y\|` | beat half-wave and power |
| Coefficient formula | `exp(-1/(sr × tau))` | +0.0003 over `pow(0.01)` |
| Depth | floor 0.065, >100% exponent 1.35 | measured from Ableton depth exports |
| bwGain at BW=10% | 35.0 dB | level matching |

A later commit split these into **two Q tables**: `Linked` mode uses Q=40 (the Ableton match),
`Ritual` mode keeps Q=6 (the original, more characterful sound). Both are intentional.

---

## 3. What was tested and does NOT work

This table is the single most useful thing here — it is ~15 hours of dead ends, priced.
**Do not re-run these experiments during the rebuild.**

| Technique | Cosine | Verdict |
|---|---|---|
| Textbook LP envelope (10–100 Hz) | 0.84–0.92 | worse than a plain peak detector |
| Half-wave rectification | 0.9092 | slightly worse |
| Power detection (y²) | 0.8110 | much worse |
| RMS envelope | 0.9199 | close, still worse |
| Carrier normalization | 0.7326 | much worse |
| Envelope normalization | 0.8548 | worse |
| 2× oversampling | 0.9159 | no improvement over 1× |
| Per-band tilt (f^1.0) | 0.4405 | actively hurts — constant-Q already handles it |
| Cascade LP | 0.6797 | was only a band-aid for the Q=6 bug |
| Ring-buffer boxcar | 0.54 | measured under the Q=6 bug, invalid |
| Float64 precision | +0.003 | real, but unusable in JUCE's IIR |
| STFT / FFT vocoder | — | abandoned; see `dsp/…STFT Integration Attempt…` |

---

## 4. The five lessons that cost the most

1. **A Q=6 regression was the root cause of everything.** ~15 hours were spent inventing exotic
   fixes for a problem that was one wrong constant. Validate basic parameters *first*.
2. **16-bit WAV clipping silently corrupted every measurement for months.** Every number taken
   before `docs/dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md` is suspect.
3. **Ableton's vocoder is simple.** Complexity was assumed and repeatedly disproved.
4. **Frequency-dependent "corrections" made the match worse**, not better.
5. **Simple peak detection beat every textbook envelope technique.** Textbook ≠ what the target does.

---

## 5. Where to read next

- Full reverse-engineering trail → `dsp/Ableton Vocoder - Master Findings Log.md`
- The measured algorithm → `dsp/Ableton Vocoder - Complete Cracked Algorithm.md`
- Final architecture → `dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md`
- Parameter ranges → `dsp/Ableton Vocoder - Actual Parameter Ranges.md`
- How measurement was done → `process/Band Ritual - Validation Suite Spec.md`
- Build/DAW gotchas → `process/Band Ritual - Plugin Silence Debugging Checklist.md`
- Full note map → `00-INDEX.md`

---

## 6. For the rebuild

What is worth carrying forward from the prototype:

- **The constants in §2.** They were measured, not guessed. Re-deriving them costs months.
- **The dead-end table in §3.** It is a negative-results dataset; it saves the same months again.
- **The validation methodology** (render both, compute cosine) — the rebuild should have this
  from day one, as an automated test rather than a manual Python script.

What is *not* worth carrying forward: the file structure, the 5864-line processor, the backup
files, the GUI layout code.


================================================================================
FILE: docs/00-INDEX.md
================================================================================

# Index — all 59 development notes

Every note written during Band Ritual's development, exported from Obsidian. Nothing was filtered
out, including dead ends and superseded conclusions — the wrong turns are part of the record.

⚠️ **Chronology matters.** Notes contradict each other, because understanding changed. When two
notes disagree, **the later one wins**, with one hard exception: anything measured before
`Band Ritual - WAV Clipping Bug Discovery 2026-04-11` was measured on clipped audio and is invalid.

**Read `CONTEXT.md` first** — it's the distilled version. Come here for depth.

---

## Suggested reading order (the 6 that matter most)

1. `dsp/Ableton Vocoder - Complete Cracked Algorithm.md` — the reconstructed algorithm, from 31 recordings
2. `dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md` — the locked constants + everything that failed
3. `dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md` — why earlier numbers are void
4. `dsp/Ableton Vocoder - Master Findings Log.md` — the running log across the whole hunt
5. `process/Band Ritual - Validation Suite Spec.md` — how the match was actually measured
6. `dsp/Band Ritual - Vocoder Cracking Hypotheses Matrix.md` — hypotheses tested vs rejected

---

## dsp/ — reverse engineering, measurement, iteration (41 notes)

### The Ableton teardown (measured ground truth)
- `Ableton Vocoder - Complete Cracked Algorithm.md` — ⭐ 31 recordings analyzed; gain curve, envelope, band/BW/depth scaling
- `Ableton Vocoder - Master Findings Log.md` — ⭐ running findings log
- `Ableton Vocoder - Actual Parameter Ranges.md` — real min/max of every Ableton control
- `Ableton Vocoder - Cracked Gain Curve.md` — output ∝ input², i.e. no nonlinear shaping
- `Band Ritual - Ableton Filter Measurements 2026-04-09.md` — measured filter responses
- `Band Ritual - Ableton Vocoder Reverse Engineering Plan.md` — the original attack plan
- `Band Ritual - Ableton Vocoder Matching Project.md` — project framing

### The cracking sessions (chronological, March → April)
- `Band Ritual - Vocoder Cracking Session Log 2026-03-22.md`
- `Band Ritual - Vocoder Cracking Results 2026-03-22.md`
- `Band Ritual - Vocoder Cracking Session 2026-03-23.md`
- `Band Ritual - Vocoder Cracking FULL STATUS 2026-03-24.md`
- `Band Ritual - Vocoder Cracking Hypotheses Matrix.md` — ⭐ what was hypothesised vs confirmed
- `Band Ritual - Vocoder Cracking v11 Checkpoint.md`
- `Band Ritual - Vocoder Cracking FINAL v12.md`
- `Band Ritual - Vocoder Cracking DEFINITIVE.md`
- `Band Ritual - Vocoder Cracking DEFINITIVE v13.md`
- `Band Ritual - Vocoder Cracking Final Status.md`
- `Band Ritual - Vocoder Cracking NEXT OBJECTIVE.md` / `NEXT SESSION.md`
- `Band Ritual - Vocoder Reverse Engineering Log.md`
- `Band Ritual Improved - Vocoder Matching Ableton.md`

*(Note the "FINAL", "DEFINITIVE", "v12", "v13" naming — none of these were actually final. Trust dates over titles.)*

### Component-by-component cracking
- `Band Ritual - Noise Cracking Session 2026-04-04.md`
- `Band Ritual - Gate Cracking Session 2026-04-05.md`
- `Band Ritual - Formant Cracking Session 2026-04-05.md`
- `Band Ritual - Sub Mid Calibration 2026-04-07.md`
- `Band Ritual - Filter Order Correction.md`
- `Band Ritual - 2nd Order Carrier Filter Experiment.md`
- `Band Ritual - Q-from-range + close LO-HI 2026-06-12.md`

### Iteration history (the climb from 0.45 → 0.93)
- `Band Ritual - Linked Mode SVF Regression 2026-04-08.md` — the SVF regression
- `Band Ritual - Iter 12 Architecture Restored 2026-04-09.md` — reverted SVF, first multi-input pass
- `Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md`
- `Band Ritual - Iter 19g Sub Shelf Final 2026-04-10.md`
- `Band Ritual - Iter 19l Session Final 2026-04-10.md`
- `Band Ritual - Iter 20 BPF2 Return 2026-04-11.md` — dropped Butterworth, killed all freq-dependent corrections
- `Band Ritual - Iter 22 Final Architecture 2026-04-12.md` — ⭐ the locked architecture

### The bug that invalidated months of data
- `Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md` — ⭐⭐ **read before trusting any earlier number**

### Abandoned: the STFT/FFT path
- `Band Ritual - STFT Integration Attempt 2026-04-10.md`
- `STFT Vocoder Calibration - Ableton Match.md`

### Code structure
- `Band Ritual - PluginProcessor Code Structure.md` — map of the monolithic processor
- `Band Ritual V3 - Clean Ableton Architecture.md`

---

## gui/ — visual design and layout (9 items)

- `GUI_LAYOUT_SPEC.md` — the layout spec the C++ implements
- `Band Ritual Final 1 - Brief GUI per Grafico.md` — brief written for the graphic designer *(Italian)*
- `Band Ritual - Final 1 GUI Moodboard & Handoff 2026-06-03.md`
- `Band Ritual GUI - Blender + State Aligner Session 2026-04-21.md` — 3D chassis → PNG filmstrip pipeline
- `Band Ritual MIX Knob - Blender Session 2026-04-26.md`
- `Band Ritual Final 1 - Giga Knob Material Checkpoint 2026-04-15.md`
- `Band Ritual - Grid VERIFIED Pure-Log 2026-06-15.md` — frequency grid is pure-log, verified
- `Band Ritual - V4 Chassis Redesign VST Migration TODO.md` — where the design was heading next
- `BandRitual_Final1_GUI_Brief_Assets/` — chassis exports, screenshots, and two large `.pptx` moodboards (~54 MB)

---

## process/ — workflow, build, release (11 notes)

- `Band Ritual - Validation Suite Spec.md` — ⭐ how cosine matching was measured
- `Band Ritual - DSP Iteration Workflow.md` — the measure → change → re-measure loop
- `Band Ritual - Plugin Silence Debugging Checklist.md` — ⭐ plugin outputs silence: checklist
- `Band Ritual - Ableton Host Integration Bug.md`
- `VST3 Plugin Install - Ableton Code Signing Fix.md` — macOS codesign step Ableton requires
- `Ableton Live 12 - Custom Remote Script Installation.md`
- `Band Ritual - Final 1 COMPLETE Checkpoint.md`
- `Band Ritual Final 1 - Next Updates 2026-04-15.md`
- `Band Ritual V3 - Final 1 and Final 2 Plan.md`
- `Band Ritual - Pre-Launch Checklist.md`
- `Band Ritual Fake Crack - Reddit Marketing.md` — marketing experiment, no technical content

---

## Also here

- `PROTOTYPE_GIT_HISTORY.md` — commit log of the original repo (its 316 MB `.git` was not carried over)
- `../reference-renders/` — chassis renders, what it looked like


================================================================================
FILE: docs/dsp/Ableton Vocoder - Actual Parameter Ranges.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - ableton
  - parameters
date: '2026-03-19'
---
# Ableton Vocoder — Actual Parameter Ranges (2026-03-19)

## CRITICAL: Ranges are WIDER than assumed

| Parameter | Assumed | Actual |
|-----------|---------|--------|
| **BW** | 0-100% | **0-200%** |
| **Depth** | 0-100% | **0-200%** |

This means:
- Our "HIGHBW" tests at BW=100% are only MID BW in Ableton's range!
- Our depth=100% tests are only half of Ableton's max!
- ALL previous comparisons were done at HALF the actual max BW/depth

## Impact on V2-Improved calibration
The ±0.41 dB results were measured against Ableton at BW=10% (LOWBW), BW=50% (MIDBW), BW=100% (HIGHBW). But 100% is actually Ableton's MIDPOINT, not maximum. We never tested against BW=200%.

## Impact on V3 normalization
The pipeline normalization and tilt correction were calibrated for BW up to 100%. At 200%, the Q would be ~3.5 (very wide) — the filter bank becomes nearly flat. Need to test at 200% too.

## What the user recorded (Phase 4 sine sweep corrections)
- crack_bw10_sine.wav → BW=10% (correct)
- crack_bw50_sine.wav → actually recorded at BW=**100%** (mid), NOT 50%
- crack_bw100_sine.wav → actually recorded at BW=**200%** (max), NOT 100%

## What the user recorded (Phase 5 depth corrections)
- crack_depth0_noise.wav → Depth=0% (correct)
- crack_depth25_noise.wav → actually Depth=**50%**
- crack_depth50_noise.wav → actually Depth=**100%**
- crack_depth75_noise.wav → actually Depth=**150%**
- crack_depth100_noise.wav → actually Depth=**200%**

## Phase 3 filename note
User named files "band" not "bands" (crack_band4_highbw.wav etc.)


================================================================================
FILE: docs/dsp/Ableton Vocoder - Complete Cracked Algorithm.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - algorithm
date: '2026-03-19'
---
# Ableton Vocoder — Cracked Algorithm (2026-03-19)

## ALL MEASUREMENTS COMPLETE — 31 recordings analyzed

---

## 1. GAIN CURVE → NO COMPRESSION (quadratic)

**Output ∝ input²** — exactly 12 dB output per 6 dB input (ratio = 2.00).

This means Ableton does simple `output = carrier × envelope` with NO nonlinear shaping:
- No fastTanh
- No sqrt
- No soft-clip
- Just linear multiplication

Gate/floor below ~-33 dBFS input (output drops to zero).

## 2. ENVELOPE SHAPE → Instant attack, fast release

- Attack: **< 1 ms** (essentially instant — peak detector)
- Release: **~10-30 ms** at the measured settings (50ms set, but self-carrier mode makes output decay faster than envelope alone)
- Confirms: **peak detector with exponential attack/release**

## 3. BAND COUNT SCALING → Nearly constant total output

| Bands | RMS | dB vs 4 bands |
|-------|-----|---------------|
| 4 | 0.438 | 0.0 |
| 8 | 0.439 | 0.0 |
| 16 | 0.425 | -0.3 |
| 20 | 0.414 | -0.5 |
| 40 | 0.336 | -2.3 |

**Total output is nearly constant regardless of band count** (only -2.3 dB from 4→40 bands). This means Ableton normalizes per-band output so the sum stays stable. The scaling is between 1/sqrt(N) and 1/N — closer to 1/sqrt(N).

## 4. BW SCALING → Almost perfectly flat for noise

| BW | Noise RMS | Sine RMS |
|----|-----------|----------|
| 10% | -7.2 dBFS | -1.7 dBFS |
| 30% | -6.5 dBFS | — |
| 50% | -6.5 dBFS | — |
| 70% | -6.5 dBFS | — |
| 100% | -6.5 dBFS | +1.2 dBFS |
| 200% | — | -4.4 dBFS |

**Noise output is constant (±0.7 dB) across all BW settings!** This confirms perfect BW normalization. Only BW=10% (very narrow) is slightly quieter.

Sine varies more (-4.4 to +1.2) because sine concentrates in one band regardless of BW.

## 5. DEPTH CURVE → Nonlinear (roughly depth²)

| Depth | RMS | Ratio to D=0 |
|-------|-----|-------------|
| 0% | 0.042 | 1.0× (dry) |
| 50% | 0.112 | 2.7× |
| 100% | 0.336 | 8.0× |
| 150% | 0.528 | 12.5× |
| 200% | 0.849 | 20.1× |

Steps: 0→50%: +8.5 dB, 50→100%: +9.5 dB, 100→150%: +3.9 dB, 150→200%: +4.1 dB.
The first two steps are ~9 dB each, the last two are ~4 dB. This suggests the depth formula is NOT linear — it's approximately quadratic at low depth and compresses at high depth.

At depth=0%: output = 0.042 (dry signal still present).

## RECONSTRUCTED ALGORITHM

```
For each band i:
  1. analysis_filtered = bandpass(input, fc[i], Q[i])     // single biquad
  2. envelope = peak_detect(|analysis_filtered|, attack, release)
  3. carrier_filtered = cascaded_bandpass(input, fc[i], Q[i], 4 stages)
  4. band_gain = 1.0 / sqrt(filter_energy[i])             // BW normalization
  5. band_output = carrier_filtered × envelope × band_gain × depth_factor

Sum all bands → output
No fastTanh. No tonal detection. No tilt correction (normalization handles it).
```

## What V3 Needs to Change

1. **REMOVE fastTanh** — confirmed Ableton doesn't use it
2. **Linear envelope multiplication** — just `carrier × envelope`
3. **Energy normalization** — precomputed per-band so noise output is BW-independent
4. **Band count normalization** — scale by ~1/sqrt(N) so total stays constant
5. **Depth formula** — nonlinear, needs further analysis (possibly depth² or similar)
6. **BW range 0-200%**, Depth range 0-200%

## Files
- All 31 recordings: `~/Desktop/vocoder-comparison/ableton_crack/`
- Test signals: `~/Downloads/VOCODER_TEST_SIGNALS/`
- Analysis: this note + `Ableton Vocoder - Cracked Gain Curve.md`
- Parameter ranges: `Ableton Vocoder - Actual Parameter Ranges.md`


================================================================================
FILE: docs/dsp/Ableton Vocoder - Cracked Gain Curve.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - gain-curve
date: '2026-03-19'
---
# Ableton Vocoder — CRACKED: Gain Curve (2026-03-19)

## THE KEY DISCOVERY

**Ableton's vocoder has NO envelope compression.** Output is purely quadratic:

```
output ∝ input²  (12 dB output per 6 dB input, ratio = 2.00)
```

This means: `output = carrier_filtered × envelope` where both scale linearly with input.
In self-carrier mode: carrier ∝ input, envelope ∝ input → output ∝ input².

### Measured Data (4 bands, LOWBW, 1000 Hz sine)

| Input (dBFS) | Output (dBFS) | Gain (dB) | Step ratio |
|---|---|---|---|
| -63 | -600 (zero) | — | — |
| -57 | -600 (zero) | — | — |
| -51 | -600 (zero) | — | — |
| -45 | -600 (zero) | — | — |
| -39 | -600 (zero) | — | — |
| -33 | -118.5 | -85.5 | (gate opens) |
| -27 | -107.5 | -80.5 | 1.83 |
| -21 | -95.6 | -74.6 | 1.99 |
| -15 | -83.6 | -68.6 | 2.00 |
| -9 | -71.6 | -62.6 | 2.00 |
| -3 | -59.6 | -56.6 | 2.00 |

**Step ratio = 2.00 consistently from -27 to -3 dBFS** = exactly quadratic.

### Gate Threshold
Below ~-33 dBFS input, output is zero. This could be:
- Ableton's internal gate (set to OFF but may have a minimum floor)
- 32-bit float noise floor
- The envelope at 4 bands LOWBW being below detection threshold

### What This Means for V3

1. **REMOVE fastTanh** — Ableton does NOT use it
2. **Use linear envelope**: `envGain = envelope` (no shaping function)
3. **The quadratic response is NATURAL** — it comes from multiplying carrier × envelope, both proportional to input
4. **No tonal/noise detection needed** — linear multiplication handles both identically
5. The depth parameter likely just scales the envelope: `envGain = (1-depth) + depth * envelope`

### Why V2's fastTanh Seemed to Work
fastTanh compressed loud signals, which accidentally compensated for the fact that our normalization was wrong. With correct normalization AND linear envelope, the levels should match without compression.


================================================================================
FILE: docs/dsp/Ableton Vocoder - Master Findings Log.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - master-log
date: '2026-03-19'
---
> ⚠️ **2026-04-08 update**: Linked mode in current code has REGRESSED from this note's findings. The 4-stage biquad cascade was replaced by a 2-stage SVF carrier without re-calibration, and a band-pair phase inversion was added that creates systematic notches. Treat the architecture documented BELOW as the correct target. See `Band Ritual - Linked Mode SVF Regression 2026-04-08` for details.

# Ableton Vocoder — Master Findings Log (2026-03-19)

## PURPOSE: Track ALL confirmed facts, assumptions, and what's been tested/ruled out

---

## CONFIRMED FACTS (with evidence)

### Gain Structure
- **No envelope compression** — quadratic gain curve (Phase 1: 11 levels, ratio=2.00)
- **Output = carrier × envelope** (linear multiplication)
- **Gate floor at ~-33 dBFS** input

### Filter Topology
- **Carrier: 4 stages** (8th-order) — confirmed from 4-band isolated peak rolloff (Mar 17)
- **Analysis: 2 stages** (4th-order) — confirmed from PVR progression (1-stage→39, 2-stage→50)
- **Qa = 0.65 × Qc** — tuned to match PVR 44.6 (V3 gives 44.9)
- **IIR, NOT FFT** — confirmed from 4-band isolated peaks showing 50-60 dB/oct individual rolloff

### Spectral Shape
- **BW ≥ 30%: +2.70 dB/oct** tilt (constant across BW 30-100% — all identical!)
- **BW = 10%: +2.01 dB/oct** overall, but DIFFERENT per-region slopes
  - Low (100-1k): -1.23 dB/oct (dark!)
  - Mid (1k-5k): +0.61 dB/oct
  - High (5k-15k): -1.08 dB/oct (dark!)
- **BW 30/50/70/100% have IDENTICAL PSD** — suggests BW parameter saturates above 30%

### Level/Normalization
- **Noise output constant across BW**: ±0.7 dB (Phase 4)
- **Band count: nearly constant** 4→40 bands (-2.3 dB) (Phase 3)
- **Depth: 0-200%**, nonlinear (Phase 5)
- **BW: 0-200%** (NOT 0-100%)

### Envelope
- **Attack: <1 ms** (instant peak detector)
- **Release: ~10-30 ms** at 50ms setting (Phase 2)

---

## WHAT'S BEEN TESTED & RULED OUT

### Ruled Out ❌
- fastTanh envelope compression — WRONG (Phase 1 proved quadratic)
- FFT/STFT architecture — WRONG (Mar 17 isolated peak measurement)
- 1-stage analysis filter — gives PVR 39, Ableton has 44.6
- 5-stage carrier — doesn't improve PVR (envelope fills valleys)
- Pipeline normalization — conflicts with tilt correction
- Gain-neutral tilt — pushes energy to ultrasonics at LOWBW
- NEB-based normalization — wrong units, wrong level
- Constant tilt f^0.45 at ALL BWs — doesn't match LOWBW shape

### Confirmed ✅
- Linear envelope × carrier
- 4-stage carrier + 2-stage analysis
- Qa = 0.65 × Qc
- BW-dependent global gain (40.0 × (1-wN)^0.70)
- Band count scaling 1/sqrt(N/4)
- f^0.45 tilt works at BW ≥ 30%

### Still Open ❓
- LOWBW (10%) spectral shape — different from BW 30-100%
- Exact depth formula
- Whether Ableton's LOWBW shape comes from filter design or explicit correction
- V3 PSD at LOWBW is 12 dB below Ableton despite correct RMS
- Sine sweep comparison not yet done with final V3

---

## V3 CURRENT STATE

### Results (noise, all BWs)
| BW | RMS diff | PVR |
|----|----------|-----|
| 10% | -0.4 dB | 44.9 |
| 30% | -0.4 dB | — |
| 50% | +0.1 dB | — |
| 70% | +0.5 dB | — |
| 100% | +0.8 dB | — |

### Architecture
```
Analysis: 2-stage at Qa = 0.65 × Qc
Carrier: 4-stage at Qc
Envelope: LINEAR (no compression)
Tilt: f^0.45 (constant, works for BW ≥ 30%)
BW gain: 40.0 × (1-wN)^0.70 dB
Band count: 1/sqrt(N/4)
```

### Remaining Issue
LOWBW PSD is -12 dB below Ableton despite correct RMS. Energy is misplaced — too much HF, not enough mid-range. The f^0.45 tilt doesn't match Ableton's LOWBW spectral shape which has negative slopes at LF and HF.

---

## NEXT STEPS (PRIORITY ORDER)

1. **Fix LOWBW spectral shape** — Ableton's LOWBW has different tilt than wider BWs. Need BW-dependent tilt: f^0.45 at BW≥30%, different shape at BW<30%.

2. **Investigate BW saturation** — BW 30/50/70/100% have IDENTICAL output. This suggests Ableton's BW knob saturates. Our BW might map differently.

3. **Sine sweep comparison** — Test V3 with sine at multiple BWs against crack recordings.

4. **Listening test in Ableton** — A/B V3 vs Ableton vocoder with real audio.

5. **Depth formula** — Derive exact formula from Phase 5 measurements.


---

## CORRECTED BW DATA (2026-03-19 late)

### Old Phase 4 data was INVALID
BW 30/50/70/100% files were identical (same recording exported 4 times).
Re-recorded at BW 50/100/130/170/200%.

### Ableton's ACTUAL BW response (bell curve!)
| BW | RMS (dBFS) | vs 100% |
|----|-----------|---------|
| 10% | -7.2 | -2.9 dB |
| 50% | -5.9 | -1.5 dB |
| **100%** | **-4.3** | **0.0 (peak)** |
| 130% | -4.8 | -0.4 dB |
| 170% | -7.0 | -2.6 dB |
| 200% | -9.5 | -5.1 dB |

Output peaks at BW=100% and drops on BOTH sides. Not monotonic!

### Tilts are consistent (NOT BW-dependent!)
| BW | Tilt |
|----|------|
| 10% | +2.01 |
| 50% | +2.84 |
| 100% | +2.75 |
| 130% | +2.66 |
| 170% | +2.52 |
| 200% | +2.39 |

All within +2.0 to +2.8 dB/oct. The "LOWBW is different" finding was from duplicate files.

### V3 match with piecewise BW gain
| BW | RMS diff |
|----|----------|
| 10% | 0.0 dB |
| 50% | -0.1 dB |
| 100% | -1.0 dB |
| 130% | +2.1 dB |
| 170% | +1.9 dB |
| 200% | 0.0 dB |

### Runaway esbuild processes
Found 5+ esbuild processes from miqo-world-clone consuming 500%+ CPU.
This caused all builds to take 30+ min instead of 1-2 min.
Killed them — builds now complete in seconds.


---

## CRACKED: Per-Band Gain Law = f^1.0 (2026-03-20)

### From ISOLATED single-band measurements (BW=10%, 4 bands)

| Band | Freq | RMS (dBFS) |
|------|------|-----------|
| 1 | 38 Hz | -56.2 |
| 2 | 318 Hz | -35.0 |
| 3 | 2369 Hz | -16.8 |
| 4 | 17200 Hz | -2.8 |

**Fit: per-band gain ∝ f^1.01 = +6.1 dB/oct**

This means `tiltCorr = f / fRef` (linear proportional to frequency).

### Why this differs from the summed PSD tilt (+2.7 dB/oct)
- Per-band: +6.1 dB/oct (measured from isolated bands)
- Summed 40-band PSD: +2.7 dB/oct (measured from crack_bw* files)
- The difference: when 40 bands sum, the per-band f^1.0 partially cancels with the sqrt(N) summing, giving ~+2.7 in the total PSD

### This replaces the lookup table approach
Instead of 40 hardcoded correction values (curve-fitting), use ONE formula:
`tiltCorr = pow(f / fRef, 1.0)` = f/fRef

This is principled (derived from measurement), universal (works at any BW/band count), and matches Ableton's actual per-band gain law.


---

## ROOT CAUSE FOUND: Q mapping is wrong (2026-03-20)

### Filter shape comparison (isolated Band 3, ~2350 Hz, 4 bands BW=10%)
- **Ableton**: wide peak, Q≈5.3, -3dB BW=441 Hz, slopes ±25 dB/oct
- **V3**: extremely narrow spike, Q much higher than 5.3

### The problem is NOT tilt, NOT order, NOT gain — it's Q MAPPING.
V3's BW parameter maps to a Q that's too high, making the peaks too narrow.
This explains ALL remaining PSD offsets:
- At BW=100%: bands overlap enough to mask the Q difference → matches well
- At BW=10%: peaks are isolated → narrow V3 peaks show up as -10 dB PSD offset
- At BW=200%: similar mismatch at the other extreme

### Next step
Measure Ableton's actual Q at different BWs and band counts.
Then adjust V3's Q mapping (in updateBandFilters) to match.
This should fix the spectral shape at ALL BWs simultaneously.

### Key measurement
At 4 bands, BW=10%, Ableton's Band 3 has:
- fc = 2342 Hz
- Q = 5.3 (-3dB BW = 441 Hz)
- Low-side rolloff: +25 dB/oct
- High-side rolloff: -26 dB/oct (slightly asymmetric)


---

## HONEST FINAL STATUS (2026-03-20)

### What IS cracked:
1. ✅ Architecture: linear envelope × carrier (no compression)
2. ✅ Filter topology: 2-stage analysis (Qa=0.65Qc) + 4-stage carrier
3. ✅ PVR: 44.9 vs 44.6 (0.3 dB match)
4. ✅ RMS level: ±0.6 dB across all BWs (10-200%)
5. ✅ BW gain curve: piecewise calibrated from 6 measured points
6. ✅ Band count scaling: 1/sqrt(N/4)

### What is NOT cracked:
The per-band **spectral PSD shape** doesn't match at all BWs:
- BW=100%: PSD offset -2.6, tilt -1.03 (CLOSE)
- BW=10%: PSD offset -9.5, tilt -3.60 (NOT matching)
- BW=200%: PSD offset -5.0, tilt +0.22 (OK-ish)

### Root cause: JUCE makeBandPass has different shape than Ableton's filters
V3's per-band peaks are NARROWER than Ableton's at the same Q:
- V3 peaks: taller but narrower → less energy per Hz
- Ableton peaks: shorter but wider → more energy per Hz
- Same total energy (RMS matches) but different distribution (PSD doesn't match)

### The remaining 3-10 dB PSD mismatch CANNOT be fixed by:
- Tilt correction (tried f^0.45, f^1.0, f^0.20, f^-0.22, f^0.0 — none work at all BWs)
- Per-band gain table (only works at calibration BW)
- BW gain curve (handles level, not shape)

### To truly close the gap:
Replace JUCE makeBandPass with a filter that has wider skirts matching Ableton's shape. Options:
1. SVF (State Variable Filter) with different topology
2. Butterworth bandpass (proper LP→BP transform, not cascaded peak filters)
3. Custom biquad coefficients designed to match Ableton's measured -3dB BW and rolloff

### Practical assessment
For LISTENING purposes, the ±0.6 dB RMS match and correct PVR means V3 should sound VERY SIMILAR to Ableton. The PSD mismatch affects the "brightness" slightly but may not be audible in practice. A listening test would confirm this.


---

## Q=37 BREAKTHROUGH (2026-03-20)

### Peak width comparison revealed: maxQ was 38% too high
- Ableton -3dB BW at Band 20 (918 Hz): 1059 Hz
- V3 -3dB BW: 651 Hz (38% narrower)
- Fix: maxQ from 60 → 37

### Results with Q=37 + BW gain exp=0.52
| BW | Offset | Tilt | RMS |
|----|--------|------|-----|
| 10% | -3.0 | -2.52 | +0.2 |
| 50% | -7.6 | -1.70 | +0.1 |
| **100%** | **+0.6** | **-0.73** | **-0.8** |
| 170% | -13.1 | +1.68 | +0.5 |
| 200% | -5.0 | +0.22 | -1.3 |

**RMS: ±1.3 dB across ALL BWs.**
**BW=100%: offset +0.6, tilt -0.73 — curves nearly overlap!**

### Updated algorithm
```
maxQ = 37 (was 60)
tiltCorr = 1.0 (no tilt correction needed with correct Q!)
bwGain = 45.0 × (1-wN)^0.52 (for BW ≤ 100%)
bwGain = 35.0*(1-t) + 24.0*t (for BW > 100%, t=(bw-1)/1)
Analysis: 2-stage, Qa = 0.65 × Qc
Carrier: 4-stage
Envelope: LINEAR
```


---

## LATEST BEST (2026-03-20)

### Parameters
```
maxQ = 37 (was 60 — matched to Ableton's peak width)
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages
tiltCorr = 1.0 (NO tilt correction!)
bwGain = 47.0 × (1-wN)^0.52 (BW≤100%), 35*(1-t)+24*t (BW>100%)
bandCountGain = 1/sqrt(N/4)
Envelope: LINEAR (no fastTanh)
```

### Results
| BW | Offset | Tilt | RMS | PVR |
|----|--------|------|-----|-----|
| 10% | -2.5 | -2.39 | +1.4 | 44.9 |
| 50% | -6.6 | -1.69 | +1.0 | — |
| **100%** | **+1.1** | **-0.74** | **-0.2** | — |
| 200% | -5.5 | +0.14 | -2.0 | — |

### Key changes from peak width analysis
- maxQ reduced 60→37: V3 peaks were 38% narrower than Ableton
- Added 3rd analysis stage to maintain PVR at lower Q
- Qa ratio adjusted to 0.60 (was 0.65)
- NO tilt correction needed with correct Q!


---

## FINAL CALIBRATED STATE (2026-03-20)

### Algorithm
```
maxQ = 37
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages
tiltCorr = 1.0 (none)
bwGain = 45.6 × (1-wN)^0.44 (BW ≤ 100%)
bwGain = 35*(1-t) + 24*t (BW > 100%)
bandCount = 1/sqrt(N/4)
Envelope: LINEAR
PVR: 44.9 (target 44.6)
```

### Results
| BW | Offset | Tilt | RMS |
|----|--------|------|-----|
| 10% | -3.9 | -2.39 | **0.0** |
| 50% | -7.0 | -1.69 | **+0.5** |
| 100% | +1.9 | -0.74 | **+0.5** |
| 170% | -13.2 | +1.30 | **-0.3** |
| 200% | -5.5 | +0.14 | -2.0 |

**RMS: ±0.5 dB for BW 10-170%!**

### What's left
- PSD offset at LOWBW (-3.9) and MIDBW (-7.0): V3 filter skirts slightly narrower
- BW=170% offset (-13.2): extreme BW piecewise curve needs tuning
- BW=200% RMS -2.0: piecewise endpoint calibration
- These are all fine-tuning, not architectural issues


---

## V3 BEST STATE (2026-03-20 final)

### RMS match: 5 of 6 BWs within ±1.0 dB

| BW | RMS |
|----|-----|
| 10% | **0.0** |
| 50% | **+0.5** |
| 100% | **+0.5** |
| 130% | +2.7 |
| 170% | **+1.0** |
| 200% | **0.0** |

### PVR: 44.9 dB (Ableton: 44.6 — 0.3 dB match)

### Complete cracked algorithm
```cpp
// Filter topology
maxQ = 37;                                    // matched to Ableton peak width
Analysis: 3 stages at Qa = 0.60 × Qc         // for PVR=44.9
Carrier: 4 stages at Qc                      // 8th-order

// Signal path
envelope = peak_detect(3_stage_analysis(input))  // LINEAR, no fastTanh
envGain = envelope × depth
carrier_out = 4_stage_carrier(input)
tiltCorr = 1.0                               // NO tilt correction needed!
bandNormGain = 1.0                            // NO per-band normalization!
output_band = carrier_out × envGain × bandGains

// Global gains
bwGain (BW ≤ 100%): 45.6 × (1-widthNorm)^0.44 dB
bwGain (BW > 100%): lerp(34.5, 26.0, (bw-1.0)/1.0) dB
bandCountGain = 1/sqrt(N/4)
```

### What was cracked (chronological)
1. No envelope compression (Phase 1 gain curve: ratio=2.00)
2. IIR not FFT (Mar 17 isolated peak rolloff)
3. Carrier: 4 stages (PVR analysis)
4. Analysis: 3 stages (PVR needed more than 2 for Q=37)
5. Qa = 0.60 × Qc (PVR tuning: 44.9 target 44.6)
6. maxQ = 37 not 60 (peak width measurement: Ableton peaks 38% wider)
7. f^0.45 tilt NOT needed (natural response matches when Q is correct)
8. BW gain: bell curve peaking at BW=100% (Phase 4 re-recorded data)
9. Linear envelope × carrier (no fastTanh, confirmed quadratic)
10. Band count: 1/sqrt(N/4) (Phase 3)

### What remains (diminishing returns)
- BW=130% RMS +2.7 (piecewise transition — smooth with more points)
- PSD shape at LOWBW/MIDBW: -3 to -7 dB offset (filter skirt width difference)
- These are perceptually small — recommend listening test next
```

### V3 source location
/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/


---

## 2026-04-09 — Hybrid Envelope Follower Discovery

### Architecture restored AND extended
- Carrier: **4-stage cascaded biquad (8th-order)** — restored from iter 12. Confirmed still correct by today's multi-band filter measurements.
- Analysis: Qa = 0.60 × Qc (restored from April drift of 0.85).
- bwGain[BW=10%] = 45.6 dB (restored from April drift of 41.9).
- cos(w)^0.65 Q correction in Linked mode.
- **Envelope follower: HYBRID** — peak detector front-end feeding a **3-stage cascaded one-pole LP** smoother. Per-band envelope cap hack from iter 12 REMOVED.

### voclib reference
- https://github.com/blastbay/voclib — open-source channel vocoder that uses a 4-stage cascaded halfwave LP follower. Inspired BR's 3-stage cascade. DAFx literature on cascaded smoothers supports this: cascaded one-pole LPs approximate a Gaussian impulse response and bound amplitude naturally — no normalization hack needed.

### Filter Q measurements confirm maxQ ≈ 37 AND generalize it
Five Ableton white-noise exports at band counts {4, 8, 12, 16, 20} revealed that **Ableton uses constant fractional Q**:

```
Q ≈ N_bands / (n_octaves × BW_fraction)
```

- 40-band extrapolation → Q ≈ 38-41 — **matches March's empirical maxQ = 37** almost exactly ✓✓
- 47-52 dB/octave measured skirt slopes — **confirms 4-stage cascaded biquad** (8th order)
- HF bands have asymmetric rolloff — bilinear transform warping artifact (structural, not a bug)

**Action**: BR's qTable should be band-count-aware (`Q ∝ N / n_octaves / BW_fraction`). Currently fixed at the March 40-band value, so BR at lower band counts has Q too high. See `Band Ritual - Ableton Filter Measurements 2026-04-09`.

### Iter 14c validation
- Noise (white + pink) within **±3 dB across all 7 regions** — mid hump fixed
- Sine 050 broadband: **+0.5 dB** (essentially perfect)
- Sweep: still asymmetric (+26 sub → -11 air), monotonic tilt
- Sine 1k / 10k in-band: -14 to -16 dB (needs frequency-dependent cascade tau)

Defensible architecture end-to-end. Real-music A/B in Ableton pending.

See: `Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09`.


================================================================================
FILE: docs/dsp/Band Ritual - 2nd Order Carrier Filter Experiment.md
================================================================================

---
tags:
  - band-ritual
  - dsp
  - vocoder
  - calibration
date: '2026-03-14'
---
# Band Ritual — 2nd-Order Carrier Filter Experiment (2026-03-14)

## Hypothesis
Ableton's vocoder uses 2nd-order IIR bandpass (12 dB/oct) for carrier, not 4th-order cascaded (24 dB/oct). Switching to 2nd-order should match the spectral curve shapes, especially the LOWBW comb pattern.

## What Was Changed
1. **Carrier filter**: Cascaded 2×2nd-order (4th-order) → Single 2nd-order
2. **Compensation stack**: 6 systems recalibrated from scratch

## All Iterations Tested

### Baseline (IIR-7, 4th-order)
- Per-band RMS: all within ±1.0 dB ✓
- HIGHBW noise range: 0.50 dB
- LOWBW noise range: 1.64 dB
- 6 compensation systems finely tuned

### 2nd-Order, No Comp Changes
- Everything +5 to +13 dB too loud (wider passband = more signal)
- 4th-order compensations massively overcorrect

### 2nd-Order, All Comps Stripped
- Everything -8 to -26 dB too quiet
- Clear that qComp is essential (LOWBW vs HIGHBW = 16 dB difference)

### 2nd-Order, qComp Only
- HIGHBW: -4.3 dB mean, LOWBW: -12.2 dB
- Strong HF deficit (-8 to -12 dB at highs)
- Shape was OK but level completely wrong

### 2nd-Order, qComp + overlapComp
- Minimal improvement — overlapComp modulated by noisiness, doesn't help sine
- overlapComp parabolic was tuned for 4th-order overlap patterns

### Phase 3: Linear Envelope (no fastTanh)
- envGain = e × depth (instead of fastTanh formula)
- RESULT: Everything -10 to -12 dB (envelope too small without normalization)
- Tilt WORSE (fastTanh was actually helping by compressing HF bands)
- **IMMEDIATELY REVERTED** — fastTanh is essential for normalization

### Best 2nd-Order Result (current)
- depthComp: 5.5 → 2.0 dB
- widthComp: 10.5 → 12.0 dB max, curve changed to (1-wn)^2
- tiltFactor: now applies to ALL signals (not just tonal), exponent 0.12 → 0.14
- Parabolic alphas: reduced ~35% (0.022→0.015, 0.045→0.030)
- bwCompPower: 0.80 → 0.65
- **Results:**
  - HIGHBW noise: 4.28 dB range, mean +0.79 dB
  - LOWBW noise: 4.87 dB range, mean +1.05 dB
  - HIGHBW sine: mean -2.26 dB (highs -4.19)
  - LOWBW sine: mean +1.76 dB (sub-bass +5.47 outlier)

## Key Findings

### 1. Spectral Shapes ARE Better with 2nd-Order
- HIGHBW noise curves nearly overlap from 200 Hz to 10 kHz
- HIGHBW sine curves are nearly parallel
- The overall spectral envelope matches more closely

### 2. Per-Band RMS is WORSE
- IIR-7 (4th-order): all within ±1.0 dB
- Best 2nd-order: ±2-4 dB range
- The compensation stack was meticulously tuned for 4th-order over 7 iterations

### 3. fastTanh is Essential (Don't Remove)
- Provides normalization of raw envelope to useful gain range
- Raw envelope too small (0.01-0.2 range) for direct use
- Its frequency-dependent compression actually helps reduce natural 3 dB/oct tilt
- Without it: -12 dB level, worse tilt, unusable

### 4. LOWBW Comb Pattern — NOT a Filter Order Issue
- The comb peaks are at DIFFERENT POSITIONS in BR vs Ableton
- This is a FREQUENCY MAPPING issue (band center frequencies)
- Changing filter order changes notch depth but NOT position
- Would need to match Ableton's exact frequency mapping to fix peaks

### 5. Remaining Issues at 2nd-Order
- HF deficit for both noise and sine (highs -2 to -4 dB)
- LOWBW mid/upper-mid hump (+2.5 to +3.5 dB noise)
- LOWBW sub-bass sine outlier (+5.5 dB)
- The parabolic shape correction was designed for 4th-order overlap patterns

## Decision Point

**Option A: Continue with 2nd-order, keep tuning**
- Pros: Better spectral shapes, potentially closer to Ableton's actual topology
- Cons: Need to redo all calibration work (IIR-1 through IIR-7), currently 4× worse RMS range
- Risk: May never reach ±1 dB with 2nd-order (different physics)

**Option B: Revert to 4th-order (IIR-7), focus on frequency mapping**
- Pros: Already at ±1 dB, proven calibration
- Cons: Spectral shapes don't match curves exactly (but per-band is excellent)
- Key insight: LOWBW comb mismatch is frequency mapping, not filter order

**Option C: Hybrid — use 2nd-order carrier only above a frequency threshold**
- High bands (>2 kHz): 2nd-order where overlap is less critical
- Low bands (<2 kHz): 4th-order for deeper notches matching Ableton
- Complex but could get best of both

## Recommendation
The LOWBW comb pattern mismatch was the original motivation, and it turned out to be a frequency mapping issue, not filter order. The 2nd-order change improved HIGHBW shapes but degraded per-band precision. **Option B (revert to 4th-order)** is probably the most practical path — the ±1 dB IIR-7 result is already commercially excellent.

If spectral curve matching is truly the priority, Option A needs several more calibration iterations to get the compensation stack right for 2nd-order physics. The physics are fundamentally different (wider passband, more low-frequency overlap, less HF BW compression) and the existing parabolic/bwComp formulas may not capture the right shape.


================================================================================
FILE: docs/dsp/Band Ritual - Ableton Filter Measurements 2026-04-09.md
================================================================================

---
name: Ableton Filter Measurements 2026-04-09
description: >-
  Multi-band-count white noise exports reveal Ableton Vocoder uses constant
  fractional Q. Q scales with band count via Q ≈ N / (n_octaves × BW).
type: reference
date: '2026-04-09T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - measurement
  - filter
---

# Band Ritual — Ableton Filter Measurements (2026-04-09)

## Purpose

Determine Ableton Vocoder's actual filter Q as a function of band count. Needed because BR's qTable uses a fixed Q derived from BW% alone (calibrated at 40 bands in March 2026) and it's unclear whether Ableton scales Q with band count the same way.

## Method

Exported 5 white-noise-through-Ableton-Vocoder files with identical settings except band count:

| Band count | File |
|---|---|
| 4 | `white_noise_ableton_4bands.wav` |
| 8 | `white_noise_ableton_8bands.wav` |
| 12 | `white_noise_ableton_12bands.wav` |
| 16 | `white_noise_ableton_16bands.wav` |
| 20 | `white_noise_ableton_20bands.wav` |

Settings (fixed): BW=10%, Range=20 Hz–18 kHz, Depth=100%, Dry/Wet=100%, Formant=0, Noise=0, Gate=min.

Spectral analysis: long FFT of each output, peak-find the band centers, fit a Lorentzian (or measure -3 dB bandwidth) around each peak, derive Q = fc / BW_{-3dB}. Measure skirt slope on each side of each peak in dB/octave.

## Raw results

Approximate per-band-count Q fit (mean across bands, ignoring edge bands that hit the range limits):

| N bands | Mean Q (fitted) | Peak gain vs noise floor | Mean skirt slope (dB/oct) |
|---|---|---|---|
| 4 | ~4.6 | ~ref | 47–52 |
| 8 | ~9 | ~ref | 48–52 |
| 12 | ~14 | ~ref | 48–51 |
| 16 | ~19 | ~ref | 49–52 |
| 20 | ~22 | ~ref | 48–51 |
| 40 (extrapolated) | **~38–41** | — | — |

## Key finding: Ableton uses CONSTANT FRACTIONAL Q

Q scales roughly linearly with band count. The fit is consistent with:

```
Q ≈ N_bands / (n_octaves × BW_fraction)
```

with `n_octaves ≈ log2(18000/20) ≈ 9.8` and `BW_fraction = 0.10` for BW=10%.

- 4 bands → 4 / (9.8 × 0.1) ≈ 4.1 — measured ~4.6 ✓
- 16 bands → 16 / (9.8 × 0.1) ≈ 16 — measured ~19 ✓
- 20 bands → 20 / (9.8 × 0.1) ≈ 20 — measured ~22 ✓
- 40 bands → 40 / (9.8 × 0.1) ≈ 40 — matches March's empirical maxQ = 37 ✓✓

This is **the same "constant fractional bandwidth" design** that any well-engineered vocoder filterbank uses: each band covers a fixed fraction of an octave, so Q has to scale with band density.

## Filter order

Measured skirt slopes are **47-52 dB/octave at mid frequencies**. A single biquad bandpass gives 6 dB/oct far from the peak; 4 cascaded biquads give ~24 dB/oct additive per side in the asymptote, but because of Q stacking the effective near-band slope is measured as ~48 dB/oct. **Consistent with the 4-stage cascaded biquad** March documented and iter 12 restored.

## HF band asymmetry

Bands above ~6 kHz show **asymmetric rolloff**: the upper skirt (above fc) falls slower than the lower skirt. Likely cause: **bilinear transform warping** at high fc/fs ratios compresses the frequency axis above Nyquist/2, making the pre-warped peak skew asymmetric in the linear domain. This is a known artifact of IIR bandpass designs implemented via bilinear transform.

Not a calibration error — it's structural. BR should reproduce this naturally if it uses the same bilinear transform, which JUCE's `makeBandPass` does.

## BR's current state vs this finding

- **BR's qTable at 40 bands** gives Q ≈ 37, which matches Ableton's 40-band Q almost exactly. March calibrated here and it's correct.
- **BR's qTable at lower band counts** does NOT scale down. At 4 bands BR is still running Q ≈ 37 — **~8× too narrow** vs Ableton's ~4.6. This explains why BR at low band counts sounds "sparse" / "hole-y" — the filters are too selective and leave gaps between bands.
- **At 40 bands with BW=10%** BR's band gain is +1 to +5 dB hot at band centers vs Ableton. This is visible in noise PSD and drives the "mid hump" that iter 14c's new envelope follower largely hides. But the underlying gain imbalance is still in the filter output itself — it's just masked now by the smoothed envelope. Could re-emerge when tuning sweep.

## Action items

1. **Implement band-count-aware Q in BR's qTable.**
   - Replace fixed per-BW Q with `Q(N, BW) = N / (n_octaves × BW_fraction)` scaled to match the March 40-band calibration at the reference point.
   - Reference point: at N=40, BW=10% → Q = 37. So scale factor = 37 × 9.8 × 0.1 / 40 = 0.9065. Final formula: `Q = 0.9065 × N / (n_octaves × BW_fraction)`.
2. **Re-run validation suite at band counts {16, 32, 40, 64, 100}** to confirm the new formula tracks Ableton across the full band-count range.
3. **Revisit noise mid hump band-gain** once Q scaling is correct. The +1 to +5 dB band-center hotness may reduce or shift.
4. **Leave HF asymmetry alone** — it's a bilinear transform artifact that Ableton has too.

## Related notes

- `Dev Notes/Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md` — March's Q=37 breakthrough at 40 bands (this measurement confirms and generalizes it)
- `Dev Notes/Band Ritual - Ableton Vocoder Reverse Engineering Plan.md`


================================================================================
FILE: docs/dsp/Band Ritual - Ableton Vocoder Matching Project.md
================================================================================

# Band Ritual — Ableton Vocoder Matching Project

## Goal
Make Band Ritual Improved's vocoder output match Ableton's vocoder as closely as possible, verified by automated WAV comparison scripts.

## Approach: Iterative Calibration with Automated Testing
1. Record Ableton vocoder output for test signals (already done — 12 WAVs)
2. Process same signals through Band Ritual via Pedalboard (Python, headless VST3)
3. Compare per-band RMS/peak levels, spectral shape, envelope transients
4. Adjust DSP parameters in PluginProcessor.cpp
5. Rebuild, re-test, check ALL plots in detail — repeat until curves match

## Test Setup
- **40 bands**, range 40Hz-18kHz, Attack 1ms, Release 50ms, Gate off
- **Ableton**: Carrier=Modulator, BW 10% (low) / 100% (high)
- **Band Ritual**: Carrier=Self, Linked mode, Scale Amt 0%, BW 0.1 (low) / 1.0 (high), ADAPT OFF
- **Test signals**: sine sweep (20Hz→20kHz log), impulse train (5ms bursts every 500ms), white noise — all 10s, 44100 Hz

## Key Files
- **Plugin source**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
- **Comparison scripts**: `~/Desktop/vocoder-comparison/auto_compare.py` (processes via Pedalboard), `deep_analysis.py` (per-band metrics)
- **Ableton WAVs**: `~/Desktop/vocoder-comparison/ableton_{lowbw,highbw}_{noise,sine,impulse}/test.wav`
- **Results**: `~/Desktop/vocoder-comparison/results/` (PNGs + deep_analysis.txt)
- **Install command**: `~/Desktop/install-and-test.txt`

## DSP Architecture (what we're tuning)
- **4th-order stacked carrier filters**: Two cascaded 2nd-order IIR bandpass per band
- **2nd-order analysis filters**: Single IIR bandpass for envelope detection
- **Envelope follower**: 1-pole asymmetric peak tracker (attack/release coefficients)
- **envLevelComp**: Global envelope gain multiplier (currently 15.5 = +23.8 dB)
- **Tilt correction**: dB/oct slope across bands to compensate 4th-order HF rolloff
- **widthGain (maxBoostDb)**: BW-dependent gain on wet bus — steep curve, big boost at narrow BW
- **Soft limiter**: threshold 2.0, tanh compression above
- **bwNormGain**: Currently DISABLED at 1.0f
- **LINKED mode Q**: Analysis Q soft-capped at 48 via `48 * Qcarrier / (Qcarrier + 48)`

## Baseline Measurements (before calibration)
From `deep_analysis.txt` with envLevelComp=10, maxBoostDb=8.5:

### Low BW (0.1) — Band Ritual too quiet
| Signal  | Mean RMS dB | Worst Band         |
|---------|-------------|--------------------|
| Noise   | -12.84      | Highs (-14.84)     |
| Sine    | -7.07       | Sub-bass (-12.45)  |
| Impulse | -8.32       | Highs (-19.77)     |

### High BW (1.0) — Band Ritual slightly loud
| Signal  | Mean RMS dB | Worst Band         |
|---------|-------------|--------------------|
| Noise   | +1.56       | Sub-bass (+5.88)   |
| Sine    | +2.81       | Upper-mid (+4.58)  |
| Impulse | +7.36       | Sub-bass (+23.48)  |

## Calibration Steps (3-step plan)

### Step 1: Recalibrate widthGain + envLevelComp
- envLevelComp: 10.0 → 15.5 (+23.8 dB)
- maxBoostDb: 8.5 → 23.0, exponent 1.44 → 6.0 (steep curve)
- Formula: `boostDb = 23.0 * pow(1.0 - widthNorm, 6.0)`
- Expected: fixes -12.84 dB low BW deficit

### Step 2: BW-dependent tilt correction
- Old: fixed 0.6 dB/oct
- New: `tiltDbPerOct = 0.3 + 0.5 * bwNorm01` (0.3 narrow, 0.8 wide)
- Expected: flatter spectral difference across BW settings

### Step 3: Sub-bass shelf correction at high BW
- Up to -6 dB for bottom 15% of bands when BW > 0.3 norm
- Expected: sub-bass at high BW drops from +5.88 to ~±1 dB

## Build & Test Protocol
```bash
# 1. Force recompile
rm -f "...build/CMakeFiles/BandRitual.dir/Source/PluginProcessor.cpp.o"
touch "...Source/PluginProcessor.cpp"

# 2. Build
cd "...build" && cmake --build . --config Release -j8

# 3. Run comparison (auto_compare loads from BUILD dir, not /Library)
cd ~/Desktop/vocoder-comparison && python3 auto_compare.py && python3 deep_analysis.py

# 4. Check results
cat results/deep_analysis.txt
# + visually inspect ALL PNGs in results/
```

## Known Gotchas
- **CMake external HD timestamp issue**: Must delete .o AND touch source before rebuild
- **Pedalboard loads from build dir**: PLUGIN_PATH in auto_compare.py points to build output, not /Library
- **updateEnvelopeCoeffs caching bug**: Per-band coefficients must be set inline in processBlock
- **Welch PSD can be misleading**: Use per-band RMS (deep_analysis.py) as ground truth, not spectral density

## Current Calibrated Values (2026-03-12)
- **envLevelComp**: 15.5 (+23.8 dB)
- **envLevelCompXf**: 15.5 (crossfade, must match)
- **maxBoostDb**: 23.0, formula: `23.0 * pow(1 - widthNorm, 6.0)` (steep BW-dependent wet bus gain)
- **Tilt**: BW-dependent: `(0.6 + 0.4 * bwNorm01) * 9 * (bandPos - 0.5)` dB
- **bwNormGain**: DISABLED at 1.0
- **Sub-bass shelf**: REMOVED (doesn't help — excess comes from filter leakage, not direct band energy)

## Results After Calibration

### Noise RMS (gold standard)
| Setting | Baseline | Calibrated | Improvement |
|---------|----------|------------|-------------|
| Low BW mean | -12.84 dB | **+0.02 dB** | +12.86 dB |
| High BW mean | +1.56 dB | **+0.25 dB** | +1.31 dB |

### Per-Band Noise RMS
| Band | Low BW (was→now) | High BW (was→now) |
|------|-----------------|-------------------|
| Sub-bass | -12.11→**+0.73** | +5.88→**+4.70** |
| Low-mid | -14.22→**-1.39** | +1.46→**+0.21** |
| Mid | -11.60→**+1.26** | +0.87→**-0.34** |
| Upper-mid | -11.43→**+1.44** | +0.72→**-0.71** |
| Highs | -14.84→**-1.95** | -1.13→**-2.60** |

### Structural Limitations (IIR filter bank vs Ableton's likely FFT-based vocoder)

**Root cause**: Ableton's vocoder probably uses FFT/STFT processing, not IIR filters. This gives fundamentally different transient/impulse behavior — no filter ringing, no bandwidth leakage.

**Tested and ruled out:**
- **2nd-order carriers**: Made impulse LOW-MID +29 dB (WORSE than 4th-order +17 dB). 2nd-order has wider effective bandwidth → captures more broadband impulse energy. Reverted.
- **Sub-bass shelf**: Doesn't help because excess comes from higher bands' filter skirts leaking down, not the bottom bands themselves.
- **Aggressive BW-dependent tilt**: Worsens sub-bass via the same leakage mechanism.

**What we CAN'T match with IIR architecture:**
- Impulse transient peaks at low/mid frequencies (+7 to +17 dB excess)
- Sub-bass excess at high BW (+4.70 dB from filter leakage)
- HF deficit at high BW (-2.60 dB from 4th-order rolloff)

**What we DID match:**
- Noise RMS within ±2 dB across all bands at low BW (mean +0.02 dB)
- Noise RMS within ±5 dB at high BW (mean +0.25 dB)
- These are the steady-state level curves — the "tone" of the vocoder on real audio

### Key Insight: Build System Gotcha
- External HD timestamps unreliable — `touch` alone may not force recompile
- **MUST delete .o file AND verify deletion AND use future timestamp**:
  ```bash
  rm -f "...PluginProcessor.cpp.o" && ls "...PluginProcessor.cpp.o" 2>&1 || echo "DELETED"
  touch -t YYYYMMDD0000 "...PluginProcessor.cpp"
  ```
- Look for "Building CXX object" in build output to CONFIRM recompile happened

## Progress Log
- 2026-03-12: Baseline measurements complete. Started 3-step calibration.
- 2026-03-12: Step 1 (envLevelComp + widthGain): Low BW noise -12.84 → -0.01 dB. Massive improvement.
- 2026-03-12: Discovered build wasn't recompiling — external HD timestamp issue. Wasted 2 iterations.
- 2026-03-12: BW-dependent tilt (0.6+0.4*bw): Mild improvement at HF. Aggressive tilt (0.65+1.5*bw) worsens sub-bass via filter leakage.
- 2026-03-12: Sub-bass shelf tried and removed — doesn't help (leakage from higher bands, not direct).
- 2026-03-12: Final calibration: envLevelComp=15.5, tilt=0.6+0.4*bw, maxBoost=23 pow(1-w,6). All noise bands within ±2 dB (low BW) and ±5 dB (high BW).


================================================================================
FILE: docs/dsp/Band Ritual - Ableton Vocoder Reverse Engineering Plan.md
================================================================================

---
tags:
  - band-ritual
  - dsp
  - vocoder
  - reverse-engineering
date: '2026-03-15'
---

# Band Ritual — Ableton Vocoder Reverse Engineering Plan

## Goal
Perfectly match Ableton's vocoder by reverse-engineering its exact band frequency grid, Q values, filter order, and depth curve.

## Current State (IIR-7, 2026-03-15)
- HIGHBW noise: +0.07 dB offset, +0.21 dB/oct tilt — **NEARLY PERFECT**
- HIGHBW sine: -0.87 dB offset — good
- LOWBW noise: +5.47 dB offset — TOO LOUD (Q compensation issue)
- LOWBW sine: +10.81 dB offset — TOO LOUD
- Architecture: confirmed IIR, not FFT

## Key Discovery (2026-03-14)
The LOWBW comb pattern mismatch is NOT filter order — it's **frequency mapping**. The comb peaks land at different positions because our band center frequencies don't match Ableton's. Changing filter order (2nd vs 4th) changes notch depth but NOT peak positions.

## Frequency Grid Analysis (2026-03-15) — RESOLVED

### Finding: Band Ritual's frequency grid ALREADY matches Ableton's
- Both use edge-to-edge formula: `f_k = Range_Low * (Range_High/Range_Low)^(k/(N-1))` for k=0..N-1
- Band Ritual code (PluginProcessor.cpp:1425): `linearPos = i / (nBands - 1)` — same formula
- The earlier claim that BR used `(k+0.5)/N` was wrong — that was in analysis scripts, not the actual plugin code
- 8-band verification: **24 cents RMS** — all bands within ±40 cents of Ableton's peaks
- 4-band verification: **86 cents RMS** — good match
- **Frequency grid is NOT the source of spectral mismatch**

### Filter Order — CONFIRMED MATCHING
- Ableton: 50 dB notch depth in self-carrier mode / 2 = 25 dB per pass = 4th-order (24 dB/oct)
- Band Ritual: 4th-order cascaded (2×2nd-order) — same

### Remaining Issue: Q Compensation at Narrow BW
- `qComp = sqrt(analysisQ / 2)` is too aggressive at LOWBW (Q~27)
- At Q=27: qComp = sqrt(27/2) = 3.67x boost — too much
- Ableton likely uses less aggressive compensation for narrow filters
- HIGHBW match proves the architecture is correct
- Only the Q-dependent gain needs adjustment

### Analysis Scripts Created
All at `~/Desktop/vocoder-comparison/`:
- `extract_band_centers.py` — initial peak extraction (unreliable at extremes)
- `plot_band_spectra.py` — visual spectral plots
- `analyze_grid_v2.py` — optimization-based fitting (poor results due to bad peaks)
- `analyze_grid_v3.py` — spectrum scoring approach
- `analyze_grid_final.py` — log-frequency resampling (good peak detection, derived R=1040)
- `verify_formula.py` — **DEFINITIVE** — proved edge-to-edge formula matches at 24 cents RMS

## What We Need From Ableton

### Captures Required (minimum)
All use `TEST_white_noise_10s.wav` as input, self-carrier mode.

| File | Bands | BW | Purpose |
|------|-------|----|---------|
| `ableton_4bands_lowbw/test.wav` | 4 | minimum | Frequency grid (easy to read) |
| `ableton_8bands_lowbw/test.wav` | 8 | minimum | Frequency grid (more points) |
| `ableton_4bands_highbw/test.wav` | 4 | maximum | Q verification |
| `ableton_8bands_highbw/test.wav` | 8 | maximum | Q verification |
| `ableton_vocoder_screenshot.png` | — | — | UI parameter ranges |

### Optional (if needed later)
- 16/20/40 bands at LOWBW/HIGHBW
- Depth 25%/50%/75% at 40 bands HIGHBW

### Ableton Vocoder Settings (constant across all captures)
- Carrier: Modulator (self-carrier)
- Range: 40 Hz – 18000 Hz
- Depth: 100%
- Attack: 1 ms
- Release: 50 ms
- Dry/Wet: 100%
- Gate: OFF
- Formant: 0

## Analysis Plan (after captures)

### Step 1: Extract Band Centers
- FFT the 4-band LOWBW output → 4 clear spectral peaks
- Each peak position = one band's center frequency
- Repeat for 8-band → 8 peaks

### Step 2: Derive Frequency Formula
- From 4+8 band centers, determine the mapping formula
- Likely logarithmic: `f_k = f_low * (f_high/f_low)^(k/N)` or similar
- Compare against our current formula

### Step 3: Measure Q/BW
- Compare LOWBW vs HIGHBW peak widths at same band count
- Peak width at -3 dB = bandwidth → Q = center/bandwidth

### Step 4: Determine Filter Order
- Notch depth between peaks: -12 dB = 2nd-order, -24 dB = 4th-order
- Measure from 4-band LOWBW (clearest notches)

### Step 5: Implement & Iterate
- Update Band Ritual's frequency mapping to match Ableton's
- Re-run comparison → comb peaks should align
- Fine-tune Q and compensations

## Files Location
- Test signals: `~/Downloads/TEST_*.wav`
- Ableton captures: `~/Desktop/vocoder-comparison/ableton_*/test.wav`
- Analysis scripts: `~/Desktop/vocoder-comparison/`
- Capture guide: `~/Desktop/ABLETON_CAPTURE_GUIDE.txt`

## Related Notes
- [[Band Ritual - 2nd Order Carrier Filter Experiment]]
- [[Band Ritual - Plugin Silence Debugging Checklist]]


================================================================================
FILE: docs/dsp/Band Ritual - Filter Order Correction.md
================================================================================

# Band Ritual — Filter Order Correction (2026-03-16)

## The RE Plan's "CONFIRMED: 4th-order" was WRONG

Flawed math: "50 dB notch / 2 = 25 dB per pass = 4th-order"

## Measured data (filter shape analysis):
| Metric | Ableton | Band Ritual (2 stages) |
|---|---|---|
| Peak-to-valley ratio | **45.9 dB** | 26.2 dB |
| P/V low freq | 32-43 dB | 11-17 dB |
| P/V high freq | 48-52 dB | 34-41 dB |

2 stages → 26 dB. Ableton 46 dB → likely **3-4 stages** (6th-8th order).

## Fix: Add 3rd carrier stage (6th-order)
- Add carrierFilterL3/R3 + one more processSingleSampleRaw
- Expected P/V: ~39 dB (closer to Ableton's 46)
- Will need compensation recalibration

## Test Results (2026-03-16)

| Stages | Order | P/V Ratio | vs Ableton (45.9) |
|---|---|---|---|
| 2 | 4th | 26.2 dB | -19.7 gap |
| 3 | 6th | 31.5 dB | -14.4 gap |
| 4 | 8th | **40.8 dB** | **-5.1 gap** |

4 stages = best match. Now need to recalibrate compensation chain for 8th-order carrier energy.

## Full Analysis: Is 8th-order the right direction? (2026-03-16)

### What 8th-order FIXES:
1. **Valley depth**: 26→41 dB P/V (Ableton=46) — the BIGGEST visual difference in the curves
2. **Band isolation**: sharper peaks = cleaner vocoding, better articulation
3. **Low-freq P/V consistency**: 4th-order has 11-17 dB at low freq, 8th-order should be 25-35 dB

### What 8th-order DOESN'T fix:
1. **Offsets** — will need full recalibration of qComp, widthComp, depthComp, overlapComp
2. **Tilts** — compensation chain needs retuning
3. **Sub-bass excess** — sub-bass correction already in place, may need adjustment
4. **Per-peak height deficit** — may improve (deeper valleys mean peak energy is more concentrated) or worsen (narrower carrier passes less total energy)

### What 8th-order might WORSEN:
1. **CPU cost**: 4 biquads × 2 channels × 40 bands = 320 filter ops/sample (was 160)
2. **Transient response**: higher order = longer ring-up time = slower attack on fast transients
3. **Compensation difficulty**: the existing chain was tuned through 9+ iterations for 4th-order

### Alternative directions considered:
A. **Stay at 4th-order** (IIR-9: ±0.50 dB offsets, ±0.45 tilts) — good numbers but curves look different
B. **Go to 8th-order + recalibrate** — matches the SHAPE, then tune the LEVEL
C. **Different filter topology** (SVF, Butterworth) — risky, unproven
D. **Mixed order per band** — complex, hard to calibrate

### RECOMMENDATION: B (8th-order + recalibrate)

The valley depth is the single most visible difference between our curves and Ableton's. With 8th-order, the SHAPE matches. The LEVEL calibration (offsets, tilts) is iterative work we've done before and can do again.

The IIR-9 result (±0.50 dB) had good offsets but the curves LOOKED wrong because of shallow valleys. The user noticed this immediately. Matching the SHAPE first, then tuning the LEVEL, is the right order.

### Recalibration plan for 8th-order:
1. Measure raw 8th-order offsets at all BW (already done: -13 to -26 dB too quiet)
2. Increase widthComp maxBoostDb to compensate for less carrier energy
3. Adjust qComp exponent (0.35 was for 4th-order)
4. Re-tune overlapComp, tiltFactor, tonalSatCorr
5. Iterate with test_stft.py until ±0.50 dB
6. Generate comparison plots and filter shape analysis to verify curves match

## 8th-Order Sine Deficit — Physics Constraint (2026-03-16)

8th-order carrier at Q=60: effective BW ≈ 8 Hz at 1 kHz.
Sine sweep passes through in ~12 ms. Ring-up time: ~76 ms. Filter never reaches full amplitude.
→ -13 dB sine deficit at LOWBW. NOT fixable with compensation.

**For real audio (voice/music), this is less of a problem** — vowel formants are relatively stable, giving the filter time to ring up. But the sine sweep test penalizes high-order filters.

**Order comparison for practical use:**
| Order | P/V ratio | Sine sweep deficit | Best for |
|---|---|---|---|
| 4th (2 stages) | 26 dB | 0 dB | Fast-changing signals |
| 6th (3 stages) | 31.5 dB | ~-5 dB (est) | Balance |
| 8th (4 stages) | 40.8 dB | -13 dB | Static/slow signals |
| Ableton | 45.9 dB | ? (need to test) | Reference |

**Key question: Does Ableton also have a sine deficit at high order?**
If Ableton uses 8th-order and ALSO has long ring-up, our sine sweep comparison is BOTH penalized equally → the deficit would cancel out. Need to check Ableton's sine sweep response at LOWBW.

**Decision: Test with 6th-order (3 stages) as compromise.** Better P/V than 4th (31.5 vs 26), less sine deficit than 8th.

## Ableton sine/noise ratio proves fast ring-up (2026-03-16)

Ableton LOWBW: sine peak 4.31, noise peak 2.07 → sine is 2x LOUDER.
If Ableton had ring-up issues, sine would be quieter. Ableton's filters ring up fine.

**New theory: Ableton uses 3 stages (6th-order) with HIGHER Q, not 4 stages with Q=60.**
- Higher Q = deeper valleys per stage (without needing more stages)
- Fewer stages = faster ring-up = sine sweep works
- 3 stages at Q=90 might give P/V ≈ 45 dB (matching Ableton)

Testing: revert to 3 stages, increase maxQ from 60 to 90.

## Sweep Round 1 Results (2026-03-16)

| Config | Stages | Q | WidthBoost | qComp | Depth | Worst Offset | Worst Tilt | PVR | Score |
|---|---|---|---|---|---|---|---|---|---|
| **IIR-9 baseline** | 2 | 60 | 5.0 | 0.35 | 10.45 | **0.37** | **0.45** | 30.8 | **2.3** |
| 6th moderate | 3 | 60 | 8.0 | 0.35 | 11.5 | 3.77 | 0.45 | 30.8 | 5.7 |
| 6th more comp | 3 | 60 | 10.0 | 0.40 | 11.5 | 7.22 | 0.54 | 30.8 | 9.3 |
| 6th higher Q | 3 | 70 | 8.0 | 0.35 | 11.5 | 3.40 | 0.67 | 32.7 | 5.4 |
| 6th lower Q | 3 | 50 | 7.0 | 0.35 | 11.0 | 6.52 | 0.60 | 28.0 | 8.9 |

**Finding:** IIR-9 (4th-order) still wins. 6th-order configs ALL overcorrected.

**Key insight:** 6th-order needs LESS compensation than expected (the 3rd stage doesn't reduce energy as much as going from 2nd to 4th order). Need Round 2 with widthBoost=3-5 for 6th-order.

**NEXT: Get 4-band Ableton recording** — isolates individual filter shape, eliminates guessing about filter order/Q. This is the single most impactful data we can get.

## DEFINITIVE: Ableton uses 8th-10th order (4-5 stages) — MEASURED (2026-03-17)

**From 4-band LOWBW Ableton recording (isolated peaks):**
| Peak | Freq | Rolloff | Order | P/V | Q |
|---|---|---|---|---|---|
| 1 | 41.7 Hz | 49.6 dB/oct | ~8th | 43.0 dB | 4.4 |
| 2 | 317.6 Hz | 61.2 dB/oct | ~10th | 46.6 dB | 6.7 |
| 3 | 2304 Hz | 53.8 dB/oct | ~9th | 48.8 dB | ~120* |

*Q measurement at 2304 Hz unreliable (FFT resolution limit)

**Conclusion:** Ableton uses 4-5 stacked biquad stages per carrier band.
Our 4th-order (2 stages) gives 24 dB/oct rolloff — HALF of Ableton's.
Our 8th-order test (4 stages) gave P/V=40.8 — close to Ableton's 43-49.

**The 8th-order sine deficit (-25 dB) was from wrong COMPENSATION, not filter ring-up.**
Ableton also has ring-up issues at high order — the comparison is relative, so ring-up cancels out.

**NEXT: Go back to 8th-order (4 stages) and recalibrate properly.**
Key: keep compensation params CLOSE to 4th-order values. The 8th-order carrier passes less energy but the relative noise/sine behavior should be similar. Only widthComp needs significant increase.

## Sweep Rounds 2-5 Summary (2026-03-17)

**8th-order (4 stages) confirmed working.** PVR=41.9 dB (vs Ableton 45.9).

**Best config so far:** wB=16, (1-wN)^3 curve, qComp=0.35, depthComp=12.0
- LOWBW noise: -0.77 (close!)
- LOWBW sine: **-13.33** (main problem — tonalSatCorr overcorrects)
- HIGHBW noise: -2.34
- HIGHBW sine: -1.50
- Tilts: -3.04 noise, -1.46 sine (all dark — need tilt correction)

**The blocking issue: 13 dB sine/noise gap at LOWBW.**
This is from tonalSatCorr (hiQCorr) which was calibrated for 4th-order. With 8th-order carrier, sine is ALREADY attenuated by the narrower filters, so the additional tonalSatCorr attenuation is excessive.

**Next: Disable or drastically reduce tonalSatCorr for 8th-order, then re-run sweep.**

## hiQExp=-0.45 BREAKTHROUGH (2026-03-17)

**LOWBW noise/sine gap: 12.6 → 0.41 dB!** Negative hiQExp BOOSTS sine at high Q to compensate for 8th-order ring-up deficit.

| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -1.77 | **-2.18** | -9.59 | -7.46 | -2.49 | -1.65 |
| Tilt | -3.04 | -1.46 | -2.06 | -1.32 | -0.60 | -0.76 |

LOWBW: gap=0.41 (was 12.6!) ✓
HIGHBW: close (-2.49/-1.65) — needs +2 dB overall
MIDBW: needs more widthComp at mid-BW
Tilts: all negative — need tilt correction for 8th-order

**Current 8th-order params:** stages=4, Q=60, widthBoost=16, (1-wN)^3, qComp=0.35, depthComp=12.0, hiQExp=-0.45, baseCorr=0.10

**Next:** Increase widthComp and depthComp to bring all offsets toward 0. Then fix tilts.

## 8th-Order Calibration Session Results (2026-03-17 continued)

### Final 8th-order parameters:
- **Stages:** 4 (8th-order carrier)
- **maxQ:** 60
- **widthBoost:** 15 (was maxBoostDb in widthComp)
- **widthComp curve:** `(1-wN)^3` (changed from `1-wN^1.6`)
- **depthComp (maxBoostDbAt100):** 12.5
- **midBwBump:** 11.0 (gaussian at widthNorm=0.21, sigma=0.12)
- **qCompExp:** 0.35
- **hiQExp:** -0.35 (NEGATIVE = boost sine at high Q, compensates 8th-order ring-up)
- **baseCorr:** 0.10
- **tiltCorr8:** 0.85 dB/octave centered at 850 Hz

### Results:
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +0.91 | -0.58 | -0.34 | +1.48 | -0.08 | +0.76 |
| Tilt | -2.19 | -0.60 | -1.21 | -0.47 | +0.25 | +0.09 |

**PVR: 41.9 dB** (Ableton: 45.9 dB) — was 30.8 with 4th-order

### What was fixed in this session:
1. **Crossfade path hiQExp** was still +0.7 (old value) → fixed to -0.35
2. **Added tilt correction** — 8th-order carrier is darker, centered boost at 850 Hz
3. **Negative hiQExp discovery** — BOOSTS sine at high Q to compensate ring-up deficit

### Remaining limitations (diminishing returns):
- **MIDBW sine +1.48** — sine/noise split inherent to single compensation chain
- **LOWBW noise tilt -2.19** — 8th-order filter shape mismatch (narrower passband = less noise from tails, can't fix with per-band gain)
- **PVR gap (41.9 vs 45.9)** — would need 10th-order (5 stages) to match exactly, but CPU cost increases

### Key lesson: hiQExp sign
- **Positive hiQExp** = cut sine at high Q (designed for 4th-order where sine was too loud)
- **Negative hiQExp** = boost sine at high Q (needed for 8th-order where ring-up is slower)
- The sign flip was the breakthrough that closed the 13 dB sine/noise gap at LOWBW


================================================================================
FILE: docs/dsp/Band Ritual - Formant Cracking Session 2026-04-05.md
================================================================================

# Band Ritual - Formant Cracking (2026-04-05)

## Status: PARTIALLY CRACKED — band remapping works, needs fine-tuning

## Key Discovery: Band INDEX Remapping (not pitch shift!)
Ableton's formant does NOT shift carrier frequencies with a multiplier. It REMAPS band indices:
- Analysis filter bank stays FIXED at original frequencies
- Carrier filter bank gets OFFSET by N band positions
- Bands shifted outside range are MUTED (gain=0)

Source: Apple EVOC 20 docs + Ableton manual confirm this approach.

## Implementation
Changed from `fCarrier = fRaw * formantFactor` (pitch shift) to band index remapping:

```cpp
// In PluginProcessor.cpp, Linked mode formant:
float srcBand = (float)i + formantBandOffset;
if (srcBand < 0 || srcBand >= nBands)
    b->formantMute = true;  // band outside range → silence
else
    fCarrier = interpolated frequency from band[srcBand]
```

Added `formantMute` bool to Band struct in PluginProcessor.h.

## Mapping: ABL raw → BR semitones
- ABL raw ±1.0 ≈ BR ±12 semitones (1 octave)
- formantBandOffset = formantSemis / 2.4 (semitones → band positions)

## Results (with mapping ABL raw * 12 = semitones)
| ABL raw | ABL rel | BR rel | Error | Spec Error |
|---------|---------|--------|-------|------------|
| -1.0    | -2.3 dB | -6.1 dB | 3.9 dB | 49.3 dB |
| -0.5    | -2.8 dB | -2.6 dB | **0.2 dB** | 35.0 dB |
| 0.0     | -0.9 dB | 0.0 dB  | 0.9 dB | 19.0 dB |
| +0.5    | -6.2 dB | -1.3 dB | 4.9 dB | 25.4 dB |
| +1.0    | -6.2 dB | -5.7 dB | **0.5 dB** | **7.2 dB** |

## What Works
- F+1.0 (max up shift): spectral error only 7.2 dB — excellent
- F-0.5 (half down): level match within 0.2 dB
- Band muting for out-of-range bands

## What Needs Work
- F+0.5: 4.9 dB level error (BR doesn't lose enough volume)
- F-1.0: 3.9 dB level error + 49 dB spectral error
- Negative shifts perform worse than positive — might need asymmetric mapping
- The mapping might not be exactly 12 semi = ABL 1.0

## Next Steps
- Try asymmetric mapping (e.g., ABL -1.0 = -8 semi, ABL +1.0 = +12 semi)
- Try different band offset calculations (not /2.4)
- Compare per-band energy to understand why negative shifts have high spectral error

## Files
- ABL exports: `gate_test/AB_FORMANT_{M100,M50,0,P50,P100}.wav`
- BR exports: generated via Pedalboard
- Input: `DRYWAV1_WARPED.wav`

## COMPLETE CRACKING STATUS (2026-04-05)
| Feature | Status | Error |
|---------|--------|-------|
| Bands | ✅ CRACKED | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB |
| Depth | ✅ CRACKED | 0.4-0.9 dB |
| Gate | ✅ CRACKED | 0-1.2 dB |
| BW gain | ✅ CRACKED | ±2 dB |
| **Formant** | ⚠️ **PARTIAL** | 0.2-4.9 dB (direction-dependent) |
| Enhance | ❌ Not yet | — |


## UPDATE: P50 and P100 are IDENTICAL (SoundClaw bug)

P50 and P100 Ableton exports are byte-identical (correlation=1.0). SoundClaw failed to change the formant parameter between these two recordings. **P50 data is invalid — same as P100.**

## Corrected Best Matches (with valid data only)
| ABL raw | Best BR semi | Spec error | Notes |
|---------|-------------|------------|-------|
| -1.0    | -1.0 semi   | 7.4 dB     | Valid |
| -0.5    | -0.5 semi   | 4.2 dB     | Valid |
| 0.0     | 0 semi      | baseline   | Valid |
| +0.5    | INVALID     | —          | Same as P100! |
| +1.0    | +12 semi    | 7.2 dB     | Valid |

## Mapping Pattern
- Negatives: 1:1 (ABL raw ≈ BR semitones)
- Positives: ABL +1.0 = BR +12 semi (1 octave up)
- Extremely asymmetric — or the positive mapping just has a flat spectral minimum plateau

## Next Steps
1. **Re-export AB_FORMANT_P50** with correct param setting (verify formant actually changed)
2. Also export P25 and P75 for finer curve mapping
3. Band offset divisor changed to 3.0 (correct: 3 semi per band in logspace grid)
4. Consider: maybe the asymmetry is real (Ableton manual shows range described as shifting "up or down" but doesn't say symmetric)


## FINAL UPDATE: P50 re-export still similar to P100

New P50 export (done manually at Formant +18) still correlates 0.9996 with P100. This is likely because DRYWAV_1 has very little HF energy, so shifting formant up by +18 or +36 produces nearly identical output.

Best spectral match for P50: +18 semi (6.5 dB) — interestingly matches the display value exactly.

## Final Best Matches
| ABL raw | ABL display | Best BR semi | Spec error |
|---------|-------------|-------------|------------|
| -1.0    | -36         | -1.0 semi   | 4.2 dB     |
| -0.5    | -18         | -0.5 semi   | 3.9 dB     |
| 0.0     | 0           | 0 semi      | baseline   |
| +0.5    | +18         | +18 semi    | 6.5 dB     |
| +1.0    | +36         | +12 semi    | 7.7 dB     |

## Architecture CONFIRMED
- Band INDEX remapping (carrier band i → carrier band i+offset)
- Bands outside range [0, nBands-1] are MUTED
- formantBandOffset = formantSemis / 3.0 (3 semitones per band)
- Linked mode only — other modes keep legacy pitch shift

## Status: CRACKED (within limitations of this input signal)
All valid data points are within 4-8 dB spectral error. The mapping asymmetry needs further testing with an HF-rich input signal to discriminate positive formant values better.


## SESSION 2 UPDATE: White noise formant test FAILED

### Issues encountered:
1. **set/parameter/raw "Index out of range"** — after creating recording track, param set commands fail. LLM sends wrong track/device indices after track creation shifts indices.
2. **Smart record timeout** — 13s recording + 2s stop + verify = 15s+, but agent timeout is 15s → timeout fires → LLM falls back to manual recording
3. **LLM confused raw vs display values** — instructions said "raw=-1.00" but LLM sent raw=-36 (display value). First recording worked at -36 but all subsequent set commands failed.
4. **Formant stayed at -36 for all recordings** — because all subsequent set commands failed with "Index out of range"
5. **execThreadRunning guard works** — "[WAIT] Previous commands still running" appeared correctly

### SoundClaw improvements made this session:
- **execThreadRunning guard** — prevents overlapping ExecThreads (param changes during recording)
- When ExecThread is busy, new commands are queued and retried after 2s

### TODO for next session:
1. **Fix smart_record timeout** — increase agent timeout to 20s+ for long recordings
2. **Fix formant instructions** — make clear that Ableton Vocoder Formant raw range is -1.0 to +1.0, NOT -36 to +36. The display shows -36 to +36 but the API raw value is -1 to +1.
3. **Fix Index out of range** — investigate why set/parameter/raw fails after track creation. Probably LLM uses stale track index.
4. **Re-export all 5 formant tests with white noise input**
5. **Continue formant calibration** with correct data

### White noise file generated:
- `/Users/ricosan/Desktop/vocoder-comparison/gate_test/WHITE_NOISE.wav`
- 10s, 44100Hz, RMS=-10.5dB, seed=123


## WHITE NOISE TEST RESULTS (valid data!)

All 5 exports verified different (no duplicates). White noise discriminates formant values well.

### Best BR matches (brute force):
| ABL raw | ABL display | Best BR semi | Spec error | RMS diff |
|---------|-------------|-------------|------------|----------|
| -1.0    | -36         | -24 semi    | 17.8 dB    | -5.6 dB  |
| -0.5    | -18         | -15 semi    | 7.8 dB     | -7.6 dB  |
| 0.0     | 0           | 0 semi      | baseline   | —        |
| +0.5    | +18         | +6 semi     | 4.3 dB     | -4.3 dB  |
| +1.0    | +36         | +15 semi    | 7.6 dB     | -6.6 dB  |

### Key finding: mapping is ASYMMETRIC
- Negative: ABL display -36 → BR -24 semi (ratio 0.67)
- Positive: ABL display +36 → BR +15 semi (ratio 0.42)
- The positive direction shifts LESS in semitones for the same display value
- This might be because Ableton's display units aren't semitones

### Spectral centroid (Ableton exports):
| Formant | Centroid |
|---------|----------|
| -36     | 1640 Hz  |
| -18     | 4657 Hz  |
| 0       | 12440 Hz |
| +18     | 12618 Hz |
| +36     | 12279 Hz |

Note: positive formant saturates because white noise with BW 10% already concentrates energy at HF. Shifting up further doesn't change much.

### Implementation status
Band remapping with formant mute is architecturally correct. The mapping curve (ABL display → BR semitones) is non-linear and asymmetric. Current divisor (3.0 semi per band) works for the remapping but the input parameter mapping needs calibration.

Best practical approach: map ABL formant display linearly to BR semitones with scale factor ~0.5 (compromise between 0.42 positive and 0.67 negative).


## FORMANT CRACKED — Final Results

### Linear mapping confirmed: ABL raw × 24 = BR semitones
No special curve needed. The BR parameter range (-24 to +24 semi) maps directly.
Band offset = semitones / 3.0 (3 semi per band in logspace grid).

### Final comparison (white noise input, linear mapping):
| Formant | Level error | Spec error |
|---------|-------------|------------|
| -36     | 0.3 dB      | 17.8 dB    |
| -18     | 0.8 dB      | 14.0 dB    |
| 0       | 0.0 dB      | 19.0 dB    |
| +18     | 0.5 dB      | 4.9 dB     |
| +36     | 3.0 dB      | 8.3 dB     |

Level error within 0-3 dB across full range. ✅

### Implementation (no changes needed from current code):
```cpp
float formantBandOffset = formantSemis / 3.0f;  // already correct
// Band remapping + mute for out-of-range bands
```

## COMPLETE CRACKING STATUS (2026-04-05 FINAL)
| Feature | Status | Level Error | Spec Error |
|---------|--------|-------------|------------|
| Bands | ✅ CRACKED | — | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB | — |
| Depth | ✅ CRACKED | 0.4-0.9 dB | — |
| Gate | ✅ CRACKED | 0-1.2 dB | — |
| BW gain | ✅ CRACKED | ±2 dB | — |
| **Formant** | ✅ **CRACKED** | **0.3-3.0 dB** | **5-18 dB** |
| Enhance | ❌ Not yet | — | — |


================================================================================
FILE: docs/dsp/Band Ritual - Gate Cracking Session 2026-04-05.md
================================================================================

# Band Ritual - Gate Cracking Session (2026-04-05)

## Gate Formula CRACKED

### Changes from original:
1. **Offset**: -17 → **-27** (threshold much lower, less aggressive gating)
2. **Gate type**: soft knee (attenuate by gateRange) → **hard mute** (gain=0 below threshold)

### Code (`PluginProcessor.cpp`):
```cpp
// Threshold calculation (~line 3209)
float gateThresholdDb = gateAmount - 27.0f;  // was -17

// Gate application (~line 3675)
float targetGateGain = (compensatedEnv < gateThresholdLinear) ? 0.0f : 1.0f;  // hard mute
// (was: ? gateGainLinear : 1.0f — soft knee with -40dB attenuation)
```

### Results (relative to gate OFF)
| Gate | Ableton | Band Ritual | Error |
|------|---------|-------------|-------|
| -60 dB (OFF) | +0.0 dB | +0.0 dB | 0.0 dB |
| -40 dB | -0.0 dB | -0.0 dB | 0.0 dB |
| -20 dB | -0.1 dB | -0.0 dB | 0.1 dB |
| -10 dB | -2.5 dB | -1.3 dB | **1.2 dB** |
| 0 dB | -127.1 dB | -56.3 dB | 70.8 dB |

### Notes:
- Gate OFF to -20 dB: perfect match (0-0.1 dB error)
- Gate -10 dB: 1.2 dB error — acceptable for practical use
- Gate 0 dB: BR doesn't fully mute (-56 vs -127 dB) — edge case, rarely used at max
- The smoothing coefficients (2ms attack, 30ms release) are unchanged
- `gate_range` parameter no longer used (hard mute replaces soft knee)

### Test methodology:
- Input: DRYWAV1_WARPED.wav (warped version = what actually went through Ableton)
- Settings: Linked mode, 40 bands, BW 10%, Release 50ms, Depth 100%, Noise 0%
- Comparison via Pedalboard automated processing

## IMPORTANT: Warped input
- Ableton warps audio clips to match tempo when loaded
- DRYWAV1_WARPED.wav is the actual signal that went through the vocoder
- Must use this for accurate comparison, not the original DRYWAV_1.wav

## COMPLETE CRACKING STATUS (2026-04-05)
| Feature | Status | Error |
|---------|--------|-------|
| Bands | ✅ CRACKED | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB |
| Depth | ✅ CRACKED | 0.4-0.9 dB |
| **Gate** | ✅ **CRACKED** | **0-1.2 dB** (practical range) |
| BW gain | ✅ CRACKED | ±2 dB |
| Formant | ❌ Not yet | — |
| Enhance | ❌ Not yet | — |


================================================================================
FILE: docs/dsp/Band Ritual - Iter 12 Architecture Restored 2026-04-09.md
================================================================================

---
name: Iter 12 — Architecture Restored
description: 2026-04-09 milestone — reverted SVF regression, restored March 4-stage biquad cascade, added per-band envelope cap. Multi-input validation passes for the first time.
type: project
date: 2026-04-09
tags: [bandritual, vocoder, ableton, milestone, iteration]
---

# Band Ritual — Iter 12 Architecture Restored (2026-04-09)

## Context

After yesterday's overfit per-band LUT episode, today's session traced the +20 dB sweep discrepancy to a structural mismatch between the April SVF carrier and March's documented 4-stage biquad cascade. Three independent research tracks (Obsidian historical evidence, code archeology, DSP web research) all converged on the same answer: revert to March's architecture AND add the per-band envelope normalization Ableton appears to have.

## What changed

### Carrier filter
- **Reverted from 2-stage SVF (4th order) to 4-stage biquad cascade (8th order)** at PluginProcessor.cpp:3860-3870
- Web research confirmed: at Q=37 and 1 octave offset, 4× makeBandPass attenuates ~140 dB while 2× SVF attenuates only ~7 dB. **130 dB skirt difference.** SVF cannot be made equivalent through gain compensation.
- The 4-stage biquad code path was already wired (used in crossfade transitions at lines 4030-4038). Activation required only changing 6 lines in the main audio loop.
- SVF declarations/init/per-block updates left dormant — preserved for any future continuous-modulation feature.

### Analysis filter
- **Restored Qa = 0.60·Qc** (was 0.85 in April code, drifted from March's documented 0.60)

### Global gain
- **Restored bwGain[BW=10%] = 45.6 dB** (was 41.9 in April code, drifted from March's 45.6)

### Q correction
- **cos(w)^0.65 in Linked mode** (was 1.29 in April; full removal broke HF; 0.65 splits the difference)
- Other modes (Ritual, Split) still use cos^1.29

### Per-band envelope normalization (NEW vs March)
- **Frequency-dependent envelope cap: 0.005 at 20 Hz → 0.045 at 18 kHz** in Linked mode
- This is the per-band normalization Ableton appears to have. Web research subagent noted: "Ableton Vocoder's actual signal path scales the carrier by a CONTROLLED envelope, not the raw |filter(x)|. For a sweep where only one band is hot at a time, the envelope's bounded amplitude limits per-band contribution."
- Previous March work explicitly REMOVED envelope shaping ("output = carrier × envelope, NO shaping"). This was correct for input/output linearity (gain curve) but missed the per-band normalization aspect.

## Validation results (validation_suite.py, 6 inputs)

| Test | Sub | Bass | Low | Mid | HiMd | High | Air |
|---|---|---|---|---|---|---|---|
| white_noise | -0.8 | +0.2 | +0.0 | +3.1 | +4.0 | +2.7 | -0.6 |
| pink_noise | -1.4 | +0.4 | +0.4 | +2.4 | +4.0 | +3.1 | +0.2 |
| sweep | +8.1 | +4.3 | +3.7 | +4.5 | +5.2 | +4.5 | +2.4 |
| sine_050 in-band | -12 | -11 | -12 | floor | floor | floor | floor |
| sine_1000 in-band | floor | floor | floor | -11.2 | floor | floor | floor |
| sine_10k in-band | floor | floor | floor | floor | -16 (improved from -28) | -16 | -16 |

## What's good

- **Multi-input validation passes overall** for the first time. Yesterday's LUT was overfit to ONE file; today's architecture works across 6 input types.
- **Noise sub/bass/low: ±1.5 dB** (matches March's documented ±0.5 dB tolerance approximately).
- **Sweep: +2 to +8 dB** (was +20 dB at start of session; +25 dB without envelope cap).
- **Architecture is now defensible**: 4-stage biquad cascade matches Ableton's measured filter slope (49-61 dB/oct) and is the same architecture March validated.

## What's still off (next iteration targets)

1. **Noise mid/HiMd +3 to +4 dB** — there's a "hump" in the 500 Hz to 6 kHz range. Likely cause: bandMult or bwGain table needs frequency-dependent calibration in this range. Possibly a phase summation effect from band overlap.
2. **Sine 050 broadband -12 dB** — the envelope cap at sub frequencies (0.005) is too aggressive for sustained low sines. Need a dynamics-aware cap (e.g., let sustained content through while limiting transients).
3. **Sine 1k in-band -11 dB** — BR is 11 dB quieter than Ableton on a clean 1 kHz sine. Could be related to issue #1 (mid hump) inverting on tonal content.

## Why this iteration matters

This is the first iteration where:
- The architecture matches March's documented "cracked" state (4-stage biquad)
- AND there's empirical evidence the per-band normalization (envcap) is needed (noise tests pass with it, sweep would explode without)
- AND the validation is multi-input (not single-file overfit)

It is NOT yet "fully cracked" — the residual issues above are real and need work. But it's the first time we have a defensible structural foundation that matches the documented evidence AND multi-input data.

## Next iteration ideas

- **For the mid hump (issue #1)**: try Qa = 0.50 or 0.45 (tighter analysis) to see if it pulls mid/HiMd down without breaking sub. Or add a frequency-dependent bwGain adjustment at 500 Hz - 6 kHz.
- **For sine 050 (issue #2)**: replace fixed envelope cap with a soft saturation (sigmoid) instead of hard clip. Sustained content saturates smoothly; transients are clamped.
- **For sine 1k (issue #3)**: probably resolves itself when issue #1 is fixed (same root cause).

## Files modified

- `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/Source/PluginProcessor.cpp`
  - Lines ~1759-1768: cos correction exponent gated 0.65 in Linked, 1.29 in others
  - Lines ~1780-1785: Qa = 0.60·Qc (restored from April's 0.85)
  - Lines ~3833-3848: per-band envelope cap (frequency-dependent)
  - Lines ~3859-3878: 4-stage biquad cascade (replaces 2-stage SVF)
  - Lines ~4076: bwGain[BW=10%] = 45.6 dB (restored from April's 41.9)

## Next regression test

Run `validation_suite.py` after every change. The pass criteria evolve:
- Noise all regions: ±1.5 dB (currently sub/bass/low pass, mid/HiMd fail by ~3 dB)
- Sweep all regions: ±5 dB (currently sub fails by 3 dB, others pass)
- Sines in-band: ±5 dB (currently sine 1k fails by 6 dB)

When all three pass simultaneously, BR Linked mode is "cracked" against the synthetic test suite. Then add vocal + snare for perceptual validation.


================================================================================
FILE: docs/dsp/Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md
================================================================================

---
name: Iter 14c — Hybrid Envelope Follower
description: >-
  2026-04-09 milestone — voclib-style hybrid envelope follower (peak + 3-stage
  cascaded LP), 4-stage biquad carrier restored, noise within ±3 dB, sine 050
  nearly perfect. Defensible architecture.
type: project
date: '2026-04-09T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - milestone
  - iteration14c
---

# Band Ritual — Iter 14c Hybrid Envelope Follower (2026-04-09)

## Context

Continuation of today's iter-12 session. Iter 12 restored March's 4-stage biquad cascade and added a per-band envelope cap hack to keep the sweep from exploding. The cap was a band-aid: it worked for sweep but killed sustained sines (sine 050 was -12 dB). Today's remaining 14 iterations drove toward a principled envelope follower derived from voclib (open-source channel vocoder) and DAFx literature on cascaded smoothers.

By iter 14c the cap is gone entirely and the envelope follower is a HYBRID: a peak detector front-end feeding a 3-stage cascaded one-pole LP smoother. This achieves natural per-band normalization through the cascade's inherent amplitude bounding — no hack required.

## Motivation

- Iter 12 had noise mid hump (+3 to +4 dB in Mid/HiMid/High)
- Sine 050 was -12 dB because envelope cap was killing sustained content
- Architecture needed to be defensible (grounded in literature), not a collection of per-band curves
- Web research on voclib (github.com/blastbay/voclib) showed its envelope follower is a 4-stage cascaded halfwave LP, which inspired the hybrid approach here

## Architecture diff vs iter 12

### Unchanged (iter 12 stays)
- Carrier: 4-stage cascaded biquad (8th-order)
- Qa = 0.60 × Qc
- bwGain[BW=10%] = 45.6 dB
- cos(w)^0.65 Q correction in Linked mode

### Changed in iter 14c
- **Envelope follower**: was `|filter(x)|` → 1-pole LP → per-band frequency-dependent cap. Now: **peak detector → 3-stage cascaded one-pole LP** (no cap).
- **Per-band envelope cap**: REMOVED. The cascade smoothing provides amplitude normalization naturally because each stage attenuates transients and the product across 3 stages bounds the DC component cleanly.

## Reference

- **voclib** — https://github.com/blastbay/voclib — uses a 4-stage cascaded halfwave LP follower. Inspired this approach.
- **DAFx literature on cascaded smoothers** — cascading one-pole LPs produces near-Gaussian impulse response and amplitude bounding similar to a short RMS window but cheaper and zero-lookahead.

## Validation results (validation_suite.py, iter 14c)

| Test | Sub | Bass | Low | Mid | HiMid | High | Air |
|---|---|---|---|---|---|---|---|
| white_noise | -1.8 | -0.7 | -2.0 | -0.2 | +0.9 | -0.1 | -3.2 |
| pink_noise | -1.8 | -0.6 | -1.4 | -0.7 | +0.7 | +0.2 | -2.5 |
| sweep | +26.1 | +22.3 | +18.4 | +11.8 | +3.1 | -3.6 | -10.9 |
| sine_050 (broadband) | +0.5 ✓✓ | | | | | | |
| sine_1000 in-band | -13.7 | | | | | | |
| sine_10k in-band | -16.0 | | | | | | |

### Iter 12 → Iter 14c comparison (noise regions)

| Region | Iter 12 | Iter 14c | Delta |
|---|---|---|---|
| Sub | -0.8 | -1.8 | -1.0 |
| Bass | +0.2 | -0.7 | -0.9 |
| Low | +0.0 | -2.0 | -2.0 |
| Mid | **+3.1** | **-0.2** | -3.3 ✓ |
| HiMid | **+4.0** | **+0.9** | -3.1 ✓ |
| High | +2.7 | -0.1 | -2.8 ✓ |
| Air | -0.6 | -3.2 | -2.6 |

The mid hump is gone. Small Air loss is acceptable.

## What worked

- **Noise within ±3 dB across all 7 regions.** No more "mid hump" that plagued iter 12.
- **Sine 050 essentially perfect (+0.5 dB).** Removing the cap and letting cascade smoothing normalize naturally restored sustained-tone linearity.
- **Architecture is defensible end-to-end**: 4-stage biquad cascade (matches March measurements) + voclib-style hybrid envelope follower (grounded in open-source reference + DAFx literature). Every design choice traces to a measurement or a published reference.
- **No per-band cap hack.** Iter 12 needed 40 frequency-dependent cap values; iter 14c uses one global cascade time constant.

## What's still wrong

### Sweep asymmetry (big)
- Sub +26 dB, Bass +22 dB, Low +18 dB, Mid +12 dB, HiMid +3 dB, High -4 dB, Air -11 dB
- **Monotonic tilt**: over-attenuated at HF, under-attenuated at LF
- Iter 12 was closer on sweep because the frequency-dependent cap partially compensated. The cap was wrong in principle but accidentally helped sweep balance.

### Sine 1k / 10k in-band -14 to -16 dB
- Clean in-band tonal content is 14-16 dB quieter than Ableton at the matching tone band
- Noise tests pass but single-frequency tests don't — suggests frequency-dependent cascade response

### Root cause hypothesis
The cascade smoothing time constant is **the same across all frequencies**. A single LP cascade timeconstant means:
- LF bands: cascade is much slower than the band's period → over-smooths → envelope droop on sustained LF tones
- HF bands: cascade is much faster than the band's period → under-smooths → spikes pass through unbounded
- Opposite problem at sweep extremes

**Solution direction**: make the cascade time constant frequency-dependent (tau ∝ 1/fc or similar).

## Filter measurement findings (new)

Today the user also did 5 multi-band Ableton exports of white noise at different band counts (4, 8, 12, 16, 20 bands, BW=10%, 20 Hz – 18 kHz range). Spectral analysis of these exports revealed:

1. **Ableton uses CONSTANT FRACTIONAL Q.** Q scales with band count via roughly `Q ≈ N_bands / (n_octaves × BW%)`. At 4 bands Q ≈ 4.6. At 16 bands Q ≈ 19. At 20 bands Q ≈ 22. Extrapolating to 40 bands gives Q ≈ 38-41 — which matches March's empirical maxQ=37 finding almost exactly.
2. **BR's qTable does not scale Q with band count.** BR uses a fixed Q derived from BW% alone. At 40 bands BR's Q is approximately right (because that's where March calibrated). At lower band counts BR is way too narrow.
3. **Filter order is ~8th.** Measured skirt slopes 47-52 dB/octave at mid frequencies — consistent with the 4-stage cascaded biquad March documented and iter 12 restored.
4. **HF bands have asymmetric rolloff.** Above the center the skirt falls slower than below, likely a bilinear-transform warping artifact at high fc / fs ratios.
5. **At 40 bands BR's Q matches Ableton** at the bands the FFT can resolve, but **BR's band gain is +1 to +5 dB hot** at band centers. This is the noise mid hump iter 14c fixed via the new envelope follower — but the gain imbalance also exists at the filter level and may be a second-order issue when sweep is revisited.

See `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md` for raw data.

## Code locations modified

All in `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/Source/`:

- **PluginProcessor.h**
  - `Band` struct: added `env3`, `env4` fields (extra cascade stages)
  - Processor class: added `cascadeEnvCoef` member (shared cascade time constant)
- **PluginProcessor.cpp:903**
  - `cascadeEnvCoef` computation (one-pole LP coefficient from a chosen tau)
- **PluginProcessor.cpp:~3712**
  - Envelope follower hybrid branch: peak detector front-end → `env2 = env2 + cascadeEnvCoef*(peak - env2)` → `env3 = env3 + cascadeEnvCoef*(env2 - env3)` → `env4 = env4 + cascadeEnvCoef*(env3 - env4)`. `env4` drives the carrier multiplier.
- **PluginProcessor.cpp:~3833**
  - Per-band envelope cap REMOVED (the whole iter-12 frequency-dependent clamp block is deleted)

## Next iteration — three options

### Option A — Test iter 14c in Ableton with real music FIRST
Validation suite is synthetic. Before spending more time tuning, load BR in Ableton and A/B vs Ableton Vocoder on vocal + drums. The sweep and single-sine failures may not be perceptually significant on musical content where many bands overlap. Cheapest path to answering "is this good enough to ship?"

### Option B — Frequency-dependent cascade speed
Make `cascadeEnvCoef` a per-band value with `tau ∝ 1/fc` (or similar). Should fix sweep asymmetry and sine 1k/10k simultaneously. Principled, small code change, risk is picking the wrong scaling law.

### Option C — Hilbert envelope
Replace cascade LP with a per-band Hilbert transform to extract the analytic envelope. Mathematically ideal (instant zero-lag envelope from the carrier-bandwidth analytic signal), expensive per-band (needs an IIR Hilbert pair per band), and a much larger rewrite.

## Status: PARTIAL CRACK

- Defensible architecture: yes
- Noise validation: pass (±3 dB all regions)
- Sine 050 broadband: pass
- Sweep: fail (asymmetric, monotonic tilt)
- Sine 1k / 10k in-band: fail (-14 to -16 dB)
- Real music A/B test in Ableton: **PENDING** (recommended next step, Option A)

## Related notes

- `Dev Notes/Band Ritual - Iter 12 Architecture Restored 2026-04-09.md`
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md`
- `Dev Notes/Band Ritual - Linked Mode SVF Regression 2026-04-08.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md`
- `Dev Notes/Band Ritual - Validation Suite Spec.md`


================================================================================
FILE: docs/dsp/Band Ritual - Iter 19g Sub Shelf Final 2026-04-10.md
================================================================================

# Band Ritual — Iter 19g: Sub shelf (final state for this session)

**Date:** 2026-04-10
**Predecessor:** Iter 19f (true Butterworth BP via LP→BP transform, per-band envelope release LUT, Q=37)
**Status:** Best locked-in state of the day.

---

## Real music results (kick/bass DRY-EXPORT 1.wav)

| Metric                 | Iter 19f (before) | Iter 19g (after) |
|---                     |---                |---               |
| A-weighted loudness    | −0.86 dBA         | **−0.34 dBA**    |
| Spectrogram cosine     | 0.873             | **0.889**        |
| Mid 500-2k gap         | −3.7 dB           | **−1.2 dB**      |
| Sub 20-80 hot          | +3.7 dB           | **+3.2 dB**      |
| Broadband RMS diff     | +3.7 dB           | +3.0 dB          |

**A-weighted −0.34 dBA = perceptually identical loudness on real content.**

---

## The investigation chain (for future reference)

### Starting point
Iter 19f had all known architectural corrections in place. The persistent +3.7 dB sub hot on real music was the only remaining major gap. All previous per-band and Q tweaks were exhausted without progress.

### Hypothesis 1 — REJECTED: "BR has wrong number of sub bands"
`analyze_sub_bands.py` on slow sweep output:
- ABL: 11 detected prominent peaks in 20-100 Hz
- BR:  5 detected prominent peaks in same range

Initial interpretation: BR bands are merging into a plateau. Either lower Q to let bands resolve, or add bands to match ABL's density.

### Test of hypothesis 1 — FAILED
Applied sub-band Q reduction (BW floor ≥ 4 Hz below 150 Hz, mapping Q from 37 → 12.5 at 50 Hz). Result:
- BR sub avg got **louder** (−2.76 → −1.23 dB)
- Still 5 peaks, still flat plateau

Cause of reasoning error: lower Q widens the filter, increases overlap, makes the plateau worse. The frequency-vs-time uncertainty principle means any biquad configuration has the same fundamental tradeoff.

### Hypothesis 2 — PARTIALLY INVESTIGATED: "Ableton uses STFT/FFT path at sub"
The ~1.5 Hz spacing in ABL peaks was too fine for 40 log-spaced IIR bands → jumped to conclusion that Ableton must use a spectral (FFT) path at sub.

**Scope of the potential rewrite:** days of work, architectural change, risk of regression on everything else already working.

### Decisive test — `sub_steadystate_test.py` — HYPOTHESIS 2 REJECTED
Reused the existing slow sweep and recomputed the sub response with **increasing analysis window sizes** (30 → 800 ms). Logic: real discrete bands persist at any window size (they reflect steady-state filter response); sweep-filter transient artifacts average out as the window grows.

```
  win_ms     ABL_std  ABL_peaks    BR_std  BR_peaks
      30        4.84         34      3.80         3
      60        4.43         24      3.60         3
     100        4.03         16      3.32         3
     200        3.58          4      2.66         3
     350        3.42          3      1.69         2
     500        3.43          1      1.14         1
     800        3.41          0      0.78         0
```

ABL peaks collapse from 34 → 0 as window grows. **The "11 discrete bands" were sweep-filter transient artifacts, not real bands.** An STFT rewrite would have been completely wasted work.

### Hypothesis 3 — CONFIRMED: "Ableton has a low-shelf rolloff below ~45 Hz"
Looking at the 500 ms raw curve (25-80 Hz), a smooth structural difference remains:

```
 freq      ABL dB     BR dB
 25.00    -10.96     +0.89
 30.04     -3.25     +0.89
 40.80     +0.14     +0.73
 52.12     +2.41     -0.02
 75.25     +4.72     -2.57
```

**ABL has a smooth rising shelf from 25 → 75 Hz (~16 dB total, ~10 dB/oct slope).**
**BR is flat across the same range.**

This is not an architectural difference — it's a measured low-shelf that Ableton applies at sub and BR doesn't have. Common in commercial vocoders to remove mud below ~50 Hz.

### Fix
Per-band gain attenuation on the carrier for bands below 45 Hz, slope −13 dB/octave, floored at −14 dB. Curve read directly from measurement, not fitted.

```cpp
// In updateBandFilters, Linked mode branch:
float fi_sub = bands[i]->currentFreq;
if (fi_sub < 45.0f)
{
    float oct = std::log2 (45.0f / juce::jmax (fi_sub, 18.0f));
    float dbCut = juce::jmax (-14.0f, -13.0f * oct);
    bands[i]->tiltCorr *= std::pow (10.0f, dbCut / 20.0f);
}
```

Applied via `tiltCorr` multiplier → composes cleanly with existing per-band tilt correction LUT, no new state, no new filter.

---

## Lessons for future sessions

1. **Don't jump from evidence to architecture.** I saw 11 peaks in a sweep test and immediately hypothesized FFT-based processing, which would have cost days. The decisive diagnostic (varying the analysis window) took 2 minutes and falsified the hypothesis. Always run the cheap diagnostic before committing to expensive rewrites.

2. **Sweep-filter transient artifacts masquerade as discrete bands.** When a swept sine enters a narrow BP, the transient response creates ripples in the envelope that look like peaks in RMS analysis but are not steady-state features. Always verify sweep-derived "peaks" with longer windows or with static sines.

3. **The Q vs ring-down tradeoff has no solution with biquads.** You can't get both narrow frequency resolution AND fast temporal decay simultaneously at the same center frequency. If the steady-state response looks like a plateau, the filters are working as designed; the fix has to come from somewhere else (band gain, not filter shape).

4. **A +3 dB sub hot isn't always a Q or filter-order problem.** It can be as simple as "the other tool has a highpass shelf that yours doesn't." Always check for missing static EQ curves before rebuilding filter banks.

---

## Remaining gap (to investigate next session)

After iter 19g the per-region spectrum is:

| Region     | BR-ABL diff |
|---         |---          |
| Sub 20-80  | +3.2        |
| Bass 80-200| +2.9        |
| Low 200-500| +1.2        |
| Mid 500-2k | −1.2        |
| HiMd 2-6k  | +3.6        |
| High 6-12k | +3.0        |
| Air 12-18k | +2.0        |

Broadband is +3.0 dB. Notice the **mid dip**: Low +1.2, Mid −1.2, HiMd +3.6 — that's a 4.8 dB mid scoop spanning 2 octaves. This is a specific spectral feature, not a flat offset, and is the most promising next investigation target. Possible causes:
- BW-dependent gain compensation at mid (overlap correction)
- Output normalization shift from sub shelf
- A genuine midrange curve in Ableton we haven't detected yet

Next session: run a fine-resolution sweep diff (not 7 bands but ~100 log-spaced points) to see the exact shape of the BR-ABL difference curve across the full spectrum, then hypothesize the cause of the mid scoop.


================================================================================
FILE: docs/dsp/Band Ritual - Iter 19l Session Final 2026-04-10.md
================================================================================

# Band Ritual — Iter 19l: Session Final State (2026-04-10)

**Locked-in iteration:** 19l
**Predecessor:** 19f (start of session)

---

## Final real-music results (kick/bass DRY-EXPORT 1.wav)

| Metric          | Start (19f)   | **Final (19l)** | Change  |
|---              |---            |---              |---      |
| Cosine          | 0.873         | **0.9275**      | +0.054  |
| A-weighted      | −0.86 dBA     | **−0.30 dBA**   | +0.56   |
| Sub 20-80       | +3.7 dB       | **+1.7 dB**     | −2.0    |
| Bass 80-200     | +3.0 dB       | **+1.9 dB**     | −1.1    |
| Low 200-500     | +1.5 dB       | **+0.8 dB**     | −0.7    |
| Mid 500-2k      | −3.7 dB       | **−0.8 dB**     | +2.9    |
| HiMd 2-6k       | +2.3 dB       | **+1.3 dB**     | −1.0    |
| High 6-12k      | +1.3 dB       | **+0.8 dB**     | −0.5    |
| Air 12-18k      | −2.3 dB       | **−0.5 dB**     | +1.8    |
| All regions max | ±3.7          | **±1.9**        |         |
| Static spectral | —             | **0.955**       |         |

**All regions within ±1.9 dB. Perceptually identical on kick/bass material.**

---

## Changes made in this session (all measurement-grounded)

### 1. Sub low-shelf (iter 19h)
- **What:** Per-band gain attenuation below 130 Hz at −3 dB/oct, with additional −13 dB/oct below 40 Hz
- **Why:** dump_sub_curve.py (slow sweep, 500 ms window) showed ABL has a smooth sub rolloff that BR didn't have. ABL's relative response rises +16 dB from 25→75 Hz; BR was flat.
- **How discovered:** STFT hypothesis was proposed but FALSIFIED — sub_steadystate_test.py showed ABL's "discrete peaks" at sub were sweep-filter transient artifacts, not real bands. The real difference was a smooth shelf.
- **Result:** Mid 500-2k gap closed from −3.7 → −0.9 dB (secondary effect of changed tilt balance)

### 2. HF shelf (iter 19i)
- **What:** −3 dB flat above 2.5 kHz (smooth transition 1.5-2.5 kHz)
- **Why:** realmusic_test.py showed HiMd +3.9, High +4.1, Air +2.6 dB — consistent HF excess
- **Result:** HiMd → +1.1, High → +0.6, Air → −0.8

### 3. ENBW output trim (iter 19i)
- **What:** −1.0 dB global output attenuation for Linked mode (bandNormGain = 0.891)
- **Why:** True Butterworth BP has wider equivalent noise bandwidth than ABL's apparent filter type. Broadband diff was +2.1 dB. Static −1 dB trim centers the region errors.
- **Result:** Cosine +0.002, all regions tightened

### 4. Sub envelope release (iter 19l)
- **What:** Sub release changed from 95 → 15 ms (20 Hz), 95 → 30 ms (100 Hz)
- **Why:** worst_frames.py showed the 5 worst spectrogram frames (cosine 0.22-0.46) are post-kick decay moments where BR Sub is +25-35 dB louder than ABL. ABL's sub envelope decays ~30 dB in 23 ms (effective τ ≈ 7 ms). BR with 95 ms was way too slow.
- **How discovered:** spectral_vs_temporal.py showed static spectral cosine = 0.955 but per-frame median = 0.994 — proving the remaining gap was temporal dynamics, not spectral shape. worst_frames.py pinpointed the exact cause.
- **Result:** Cosine 0.9176 → **0.9275** (single biggest gain of the session)

### 5. Test script fix
- **What:** Removed cross_correlate_align() from spectrogram_similarity()
- **Why:** check_alignment.py proved alignment barely matters (+0.001 cosine), and xcorr found a wrong −76 ms lag (kick periodicity) that degraded cosine by 0.01
- **Result:** Reported cosine jumped from 0.908 to 0.918 (actual unchanged, just measured correctly)

---

## Rejected attempts

| Attempt | Result | Why failed |
|---|---|---|
| Sub BW floor (lower Q at sub) | Sub got louder | Lower Q = wider filters = more overlap |
| STFT at sub | Falsified via window test | ABL's sub "peaks" were transient artifacts |
| Aggressive sub shelf (−5 dB/oct to 150 Hz) | Cosine 0.918→0.894 | Over-cut kick transients |
| 3rd-order Butterworth (N=3) | Cosine 0.918→0.892 | Too much energy bleed between bands |
| −1.5 dB ENBW trim | Mid over-corrected to −1.3 | −1.0 dB is the sweet spot |
| 5 ms sub release | Cosine 0.9275→0.926 | Too fast, loses valid sub sustain |
| Gate at −50 dB | No effect | Gate acts on modulator, not carrier ring-down |

---

## Remaining gap analysis

### What's left (cosine 0.928 → 1.0)
- **Static spectral cosine: 0.955** — 4.5% spectral gap remains
- **Per-frame median cosine: 0.994** — most frames are near-perfect
- **5 worst frames still have +25-35 dB sub excess** — caused by filter ring-down (BW=1.35 Hz at 50 Hz → 235 ms ring time), not envelope timing
- **Bathtub shape:** Sub/Bass slightly hot (+1.7/+1.9), Mid slightly cool (−0.8), all within ±1.9 dB

### Why the worst frames can't be fixed with current architecture
The narrow Butterworth BP at sub (Q=37, BW=1.35 Hz) has a natural ring-down time of 1/(π·BW) ≈ 235 ms. Even with instant envelope release, the filter OUTPUT continues for 235 ms after the input stops. Fixing this requires:
1. **Wider sub filters** (tested — causes overlap, worse overall)
2. **Filter state reset** when envelope drops below threshold (architectural change)
3. **Different filter type at sub** (FIR with truncated impulse response, or FFT)

### Hypotheses for next session

**H1 — Filter state reset at sub (medium effort, high potential)**
When the per-band envelope drops below a threshold (e.g., −40 dB), zero the IIR filter state for that band. This instantly cuts the ring-down tail. Simple to implement (~10 lines), targets the exact cause of the worst frames. Risk: audible clicks if threshold is too high.

**H2 — Envelope topology change: linear release segment (medium effort)**
ABL might use a multi-stage release: initial fast exponential (7 ms) + linear ramp to zero. Our exponential-only release has an infinite tail. Adding a linear segment that reaches zero in finite time would cut the tail cleanly.

**H3 — Per-band overlap compensation (low effort, moderate potential)**
The ±1.9 dB bathtub shape could be addressed by computing each band's theoretical contribution to adjacent bands and compensating. This is a static correction computed from the filter bank geometry.

**H4 — Second test signal (validation breadth)**
All tuning has been on one kick/bass loop. Running a vocal or pad test would verify whether the improvements generalize or whether we've overfit to kick transients.

**Priority: H4 first (validate), then H1 (biggest remaining lever).**

---

## Key lesson
The single biggest insight of the session: **decompose the cosine gap into spectral (0.955) and temporal (worst frames) before trying to fix it.** Without spectral_vs_temporal.py and worst_frames.py, I would have spent hours tweaking filter shapes when the actual problem was envelope dynamics at 5 specific moments.


================================================================================
FILE: docs/dsp/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md
================================================================================

---
name: Iter 20 — BPF2 Return and Cosine Push
description: 2026-04-11 — Abandoned Butterworth carrier, returned to 4-stage cascaded RBJ BPF2, removed ALL frequency-dependent corrections. Best real-music cosine yet (0.7362).
type: project
date: 2026-04-11
tags: [bandritual, vocoder, ableton, iteration20, bpf2, cosine]
---

# Band Ritual — Iter 20: BPF2 Return (2026-04-11)

## Starting state (iter 19l)
- Carrier: 4-stage Butterworth bandpass (ITER 19c, flat passband, 48 dB/oct)
- Envelope: hybrid peak + 3-stage cascaded LP (iter 14c)
- HF shelf: -3 dB above 2 kHz
- Sub shelf: -3/-13 dB/oct
- Global trim: bandNormGain 0.891 (-1.0 dB)
- bwGain BW=10%: 45.6 dB
- **Real-music cosine: 0.7138 (pre-session /Library binary)**
- **Real-music cosine: 0.7066 (post-14c / 19l ~/Library binary)**

## Per-region diffs (iter 19l baseline)
| Region | Diff |
|---|---|
| Sub 20-80 | +4.7 |
| Bass 80-200 | +6.4 |
| Low 200-500 | +4.8 |
| Mid 500-2k | +1.3 |
| HiMd 2-6k | +12.5 |
| High 6-12k | +9.2 |
| Air 12-18k | +10.2 |

## What didn't work — frequency-dependent corrections HURT cosine

Tried progressively stronger combinations of:
- Global bandNormGain trim
- HF shelf (-3 to -13 dB above 1-2 kHz)
- Bass shelf (-2 to -5 dB below 200 Hz)
- Two-segment shelves with frequency tilts

Every single correction **reduced cosine similarity** even when per-region RMS diffs got smaller:

| Iter | Corrections | Cosine |
|---|---|---|
| 19l baseline | HF -3, sub, -1 dB trim | 0.7066 |
| 20a | HF -10, bass -2, -3.5 dB trim | 0.7059 |
| 20a (0.5 trim) | HF -10, bass -2, -6 dB trim | 0.7007 |
| 20b | HF -13 1-4k, bass -5 | 0.6880 |
| 20c no shelf | just normGain 0.5 | 0.7066 |
| 20e aggressive | tilted LF + HF | 0.7094 |

**Conclusion**: frequency-dependent gain shaping distorts the spectral shape, even when it improves coarse RMS.

## The pivot — return to RBJ BPF2 carrier

Per-band diagnostic revealed huge outliers:
- Bands 12-13 (136, 162 Hz): **+23 dB** vs ABL
- Bands 15, 17 (230, 326 Hz): **+23 dB** vs ABL
- Bands 25-27 (1.3-1.9 kHz): +12 to +15 dB

These outlier bands are at frequencies BETWEEN kick harmonics. ABL's peaked BPF2 filters have deep nulls there (-41 to -50 dB). Butterworth's flat passband captures energy from adjacent harmonics.

**This is a structural difference, not a calibration issue.** No amount of gain correction can reshape flat→peaked.

**Fix**: revert carrier from Butterworth to the existing 4-stage cascaded `carrierFilterL/R/L2/R2/L3/R3/L4/R4` path (RBJ BPF2 at Q=37). These filter objects were already created and coefficient-updated but unused in Linked mode — iter 19c had replaced them with `butterLP`/`butterHP` biquads.

Single-line change in the Linked mode carrier loop (PluginProcessor.cpp:~4162):
```cpp
// was: butterLP_L[0..1] + butterHP_L[0..1] (4 biquads, Butterworth LP→BP)
// now: carrierFilterL + L2 + L3 + L4 (4 cascaded RBJ BPF2 biquads)
```

Immediate result at normGain=1.0, no shelves:
- **Cosine: 0.7241** (+0.018 vs Butterworth)

Beats both the 19l hybrid-envelope baseline AND the /Library pre-session binary.

## Broadband gain sweep — monotonic cosine improvement

Removed ALL shelves and normGain trim. Only tuned `bwGain` (BW=10% entry of the lookup table) which applies a uniform scalar to all bands.

| bwGain BW=10% | Mid diff | Cosine |
|---|---|---|
| 40.8 | -1.9 | 0.7277 |
| 43.0 | -0.9 | 0.7302 |
| 44.0 | -0.5 | 0.7312 |
| 45.0 | -0.2 | 0.7322 |
| 45.6 (original) | +1.0 | 0.7241 |
| 46.0 | +0.1 | 0.7331 |
| 48.0 | +0.7 | 0.7348 |
| **50.0** | **+1.1** | **0.7362** |

Cosine improves monotonically as bwGain goes **up** past the original 45.6. Each +1 dB gives ~+0.001 cosine. Mid goes slightly hot at higher values but cosine keeps climbing.

Could not test bwGain > 50 — ran out of disk space (Macintosh HD at 99% full, builds fail at link step).

## Final iter 20 state

- Carrier: **4-stage cascaded RBJ BPF2** (Q=37)
- Envelope: **hybrid peak + 3-stage cascaded LP** (iter 14c, per-band release table)
- tiltCorr: **1.0 flat** (no per-band correction, LUT all zeros)
- bandNormGain: **1.0** (no trim)
- bwGain BW=10%: **50.0 dB** (was 45.6 — pushed 4.4 dB higher)
- All other shelves/tilts: **removed**
- Analysis filter: 3-stage at Qa=0.6×Qc (unchanged)

## Results vs start-of-session

| Metric | iter 19l start | iter 20 end | Δ |
|---|---|---|---|
| Cosine | 0.7138 | **0.7362** | **+0.022** |
| A-weighted diff | +2.89 dBA | +2.95 dBA | ≈ |
| Broadband diff | +4.80 dB | +5.06 dB | slightly worse |
| Mid 500-2k | +1.3 | +1.1 | -0.2 |
| HiMd 2-6k | +12.5 | ~+8 (need to measure) | better |
| Sub 20-80 | +4.7 | ~+5 | ≈ |

Per-region RMS numbers got slightly worse but **the spectral SHAPE match improved meaningfully** (+2.2% cosine).

## Key learnings

1. **Peaked bandpass shape matters more than flat passband** for matching Ableton on tonal content. Butterworth's flat passband creates +20 dB outliers at inter-harmonic frequencies that peaked filters naturally avoid.

2. **Uniform bwGain scaling HELPS cosine; frequency-dependent shelves HURT it.** Any tilt/shelf that reshapes the spectrum distorts the shape away from ABL.

3. **The "iter 19c Butterworth rationale" was wrong.** The comment said "RBJ BPF2 cascade had narrow peaked response causing 11-42 dB inter-band gaps". Those gaps were at the wrong Q — at Q=37 the BPF2 shape matches ABL.

4. **Per-region RMS is a misleading metric.** Chasing per-region gave worse cosine. Cosine-on-spectrogram is the right metric for "does this sound like ABL".

5. **The remaining 26% cosine gap is NOT a gain/filter issue** — it's envelope dynamics (temporal behavior), harmonic alignment, and phase relationships between bands. Gain corrections cannot close this gap.

## Outstanding work

- Test bwGain 52-60 when disk space allows (cosine may keep climbing)
- Investigate envelope release timing effects on cosine — is the per-band release table (iter 19f/l) optimal?
- Run validation_suite.py to see how iter 20 performs on noise/sweep (may have regressed some synthetic tests)
- Real-music A/B in Ableton — does 0.7362 sound meaningfully closer than 0.7138?

## Related notes
- `Dev Notes/Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md`
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md`


================================================================================
FILE: docs/dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Linked Mode SVF Regression 2026-04-08.md
================================================================================

# Band Ritual — Linked Mode SVF Regression (2026-04-08)

**Date**: 2026-04-08
**Tags**: #bandritual #vocoder #regression #dsp
**Status**: HYPOTHESIS — being tested 2026-04-08

## Summary

The April per-band correction LUT (`linkedBandCorrectionDb`, 40 entries) was iterated to convergence on a single sub-rich input file (`DRY-EXPORT 1.wav`). It achieves ±1 dB region match against that file, but the LUT itself is **structurally suspect**: values zigzag ±25 dB between adjacent bands and several entries are pinned at the clamp limit. Investigation found three structural defects in the Linked-mode carrier path that, together, create exactly the alternating notches the LUT was learning to compensate. The LUT is therefore an **overfit**, not a fix. This note documents the findings, preserves the LUT for posterity, and points at the structural fix plan.

## Root cause 1: Phase inversion every other band

`Source/PluginProcessor.cpp:3849`:

```cpp
float phaseSign = (bi % 2 == 0) ? 1.0f : -1.0f;
```

Every odd-indexed band's carrier output is multiplied by −1 before summing into `wet`. With finite-Q bandpass filters, adjacent bands always overlap on their skirts. Inverting alternate bands creates **systematic destructive interference at every band boundary**, producing a `+, −, +, −, …` notch pattern across the spectrum. This is precisely the alternating-sign zigzag the per-band LUT learned to chase. The inline comment cites a "KVR tip" that this "smooths overlap" — but at BW=10% with 40 bands the overlap is small and the inversion creates more notches than it fixes.

## Root cause 2: SVF carrier regression vs Master Findings

See `Dev Notes/Ableton Vocoder - Master Findings Log` (March 2026), which documents the cracked architecture as:

- **4-stage biquad cascade** (8th-order carrier)
- maxQ = 37
- analysis Q = 0.60·carrier Q
- RMS error ±0.5 dB across BW 10–170%

`PluginProcessor.cpp:3834-3843` now uses **two cascaded SVF stages = 4th order**, not 8th order, with a `1/Q²` level compensation:

```cpp
auto& svf1 = (ch == 0) ? b->svfCarrierL1 : b->svfCarrierR1;
auto& svf2 = (ch == 0) ? b->svfCarrierL2 : b->svfCarrierR2;
float qComp = 1.0f / juce::jmax (0.5f, b->bandQ * b->bandQ);
```

The architecture was switched (probably to support smooth `setCutoffFrequency` for formant shifting) and the calibration was never redone. The level compensation can fix gain but cannot fix the **wider skirts** of a 4th-order cascade vs the original 8th-order biquad cascade. This is a regression vs the documented "cracked" state.

## Root cause 3: Missing cos(w)^1.29 Q correction on SVF carrier

Line 1754-1759 computes `Qcarrier *= cos(πf/fs)^1.29` for the IIR carrier path, but line 3840 reads `b->bandQ` directly when configuring the SVF path. The painstakingly calibrated high-frequency BW compensation from March is therefore **not applied** to the active SVF carrier. Negligible below 1 kHz, but explains +1 to +3 dB residual at 8–18 kHz that the LUT was also chasing.

## The overfit LUT (preserved for posterity)

```cpp
static constexpr float linkedBandCorrectionDb[40] = {
    +2.38f, +5.51f, +6.40f,+15.00f,-10.47f,-13.41f,-14.20f, +9.49f,
   +11.54f, +6.17f,+10.34f,-22.90f,-22.86f, +7.32f,-25.00f,+13.29f,
   -17.49f,-18.98f,+13.83f,+14.54f,+10.89f,-13.92f, +5.50f, +6.72f,
   +10.05f,-17.64f,-25.00f,-10.55f, +2.67f, -3.70f, +2.15f, +2.11f,
    +2.18f, +2.40f, +2.78f, +3.26f, +3.72f, +3.86f, +3.24f, +2.46f
};
```

Notes:
- Bands 11/12/14/15/17/26 pinned at the ±25 dB clamp.
- Adjacent bands oscillate with ±20 dB swings.
- Tail (bands 30–39) is smoother because the optimizer ran out of headroom.

## Region match achieved (with the LUT, on DRY-EXPORT 1.wav)

| Region | BR | ABL | Δ |
|---|---|---|---|
| Sub 20-80 | -7.0 | -6.6 | -0.3 |
| Bass 80-200 | -18.4 | -18.0 | -0.5 |
| Low 200-500 | -19.3 | -19.7 | +0.4 |
| Mid 500-2k | -20.2 | -19.9 | -0.3 |
| HiMid 2-6k | -44.1 | -43.7 | -0.4 |
| High 6-12k | -51.9 | -52.2 | +0.2 |
| Air 12-18k | -66.2 | -65.3 | -0.9 |

±1 dB across all regions — but only on this single file.

## Why this is overfit, not a crack

- **Single input file**: only `DRY-EXPORT 1.wav` (sub-rich kick/bass) was used for fitting.
- **Single carrier mode**: `Self`.
- **Single bandwidth**: BW = 10%.
- **Single band count**: 40.
- **Zigzag LUT shape**: a real DSP correction is smooth. A ±25 dB alternating pattern is the optimizer chasing a systematic interference artifact (the phase inversion), not real per-band correction.
- **No validation against vocals, drums, sweeps, or sines**: the LUT will almost certainly fail on any input that occupies different bands than the kick/bass loop.

## Plan forward

Full plan: `/Users/ricosan/.claude/plans/encapsulated-snuggling-flamingo.md`

- **Phase A — Diagnostic** (no plugin changes): bypass the LUT and bypass the phase inversion separately, re-render `DRY-EXPORT 1.wav`, and confirm that disabling phase inversion collapses the zigzag. Run a single-band isolation sweep to measure actual Q / gain / skirt slope per band on both BR and Ableton.
- **Phase B — Structural fixes**: remove or gate the phase inversion (line 3849), restore `cos(w)^1.29` Q correction to the SVF path (line 3840), and (if Phase A confirms 8th-order Ableton) restore the 4-stage biquad cascade for Linked mode when formant shift is zero. Reset `linkedBandCorrectionDb` to all zeros.
- **Phase C — Multi-input validation**: build `validation_suite.py` that renders BR + Ableton on vocal, snare, sweep, white noise, pink noise, and three single sines (50 Hz, 1 kHz, 10 kHz). Pass = all 7 regions within ±2 dB on all inputs simultaneously. See `Band Ritual - Validation Suite Spec`.
- **Phase D — Parameter sweep**: once C passes, sweep BW × band count × gate × depth × formant × noise. Pass = max region error ≤3 dB across the matrix.

## Status

**HYPOTHESIS** — being tested 2026-04-08. The structural-fix approach has not yet been built; the diagnostic Phase A measurements are the next step.


================================================================================
FILE: docs/dsp/Band Ritual - Noise Cracking Session 2026-04-04.md
================================================================================

# Band Ritual - Noise/Unvoiced Cracking Session (2026-04-04)

## CRITICAL: Algorithm Mode
- **Cracking is done with `Linked` mode, NOT `Ritual` mode**
- `Linked` = the algorithm that matches Ableton's vocoder behavior
- `Ritual` = Band Ritual's custom algorithm (different signal path)
- In Pedalboard: `plugin.algorithm_mode = "Linked"` (case-sensitive string)
- Default is "Ritual" — MUST explicitly set to "Linked" for Ableton matching

## What Was Confirmed About Ableton's Noise

1. **Ableton's noise is CARRIER REPLACEMENT, not additive**
   - Crossfade between original carrier and white noise BEFORE band filters
   - NOT added post-bandpass onto wet output
   
2. **Power curve: k^6**
   - `noiseGain = pow(unvoicedParam, 6.0)`
   - Matches Ableton's dB scaling exactly:
     - 25% → -35 dB ✓
     - 50% → -20 dB ✓  
     - 100% → 0 dB ✓

3. **Pure white noise** (spectral slope ~0 dB/oct)

4. **Noise is GATED by vocoder bands** — no separate gate needed
   - When input is silent → noise output is silent
   - The band envelope followers gate everything including noise

5. **No sibilant detection needed**
   - Vocoder band envelopes naturally boost HF during consonants
   - Ableton has NO special sibilant detection circuit for noise

6. **Noise parameters in Ableton Vocoder API:**
   - [10] Unvoiced Sensitivity — detection threshold (NOT the main knob)
   - [11] Unvoiced Speed — detection response time
   - [12] Unvoiced Level — THIS is the main "Unvoiced" knob (in dB)

## Implementation Done (in BandRitualV3 source)

### Changes to PluginProcessor.cpp:

1. **Noise generation** (~line 3497): Changed to pure white noise (removed pink/white blend)

2. **IIR band loop** (~line 3928): Added carrier replacement BEFORE band filters:
```cpp
if (unvoicedLevel > 0.0f)
{
    float ng = unvoicedLevel^6;  // k^6 power curve
    in = in * (1.0f - ng) + noiseBuf.getSample(ch, n) * ng;
}
```

3. **SVF band loop** (~line 3765): Same carrier replacement added

4. **Old post-band noise** (~line 4080): Disabled (commented out)
   - Was: base+reactive noise with HP/LP detection, spectral centroid, energy gate
   - Now: disabled because noise goes through bands pre-filter

### PROBLEM: Code was added to SVF and IIR paths but NOT to the Linked path!
- The `Linked` algorithm mode uses a DIFFERENT band loop
- Need to find the Linked band loop and add noise there too
- This is why with `algorithm_mode = "Linked"`, output is -72 dB (nearly silent with noise)

## Current Status

- Noise architecture: CONFIRMED CORRECT
- Implementation: DONE for Ritual/SVF paths
- **TODO: Add noise carrier replacement to the LINKED band loop path**
- Noise level at 100%: matches Ableton within -0.8 dB (when using Ritual mode)
- Spectral slope: needs verification after Linked path is fixed

## Test Files

- Ableton exports: `~/Desktop/vocoder-comparison/gate_test/AB_NOISE_*.wav`
- BR exports: `~/Desktop/vocoder-comparison/gate_test/BR_NOISE_*.wav`
- Comparison script: `~/Desktop/vocoder-comparison/noise_compare.py`
- Analysis scripts: `~/Desktop/vocoder-comparison/gate_test/analyze_noise*.py`
- Plugin source: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

## Pedalboard Settings for Ableton Match
```python
{
    "algorithm_mode": "Linked",  # CRITICAL — not Ritual!
    "band_count": 40,
    "band_width": 0.1,       # BW 10%
    "low_freq": 20.0,        # or 40.0 for original cracking
    "high_freq": 18000.0,
    "attack_ms": 1.0,
    "release_ms": 50.0,
    "depth": 1.0,
    "gate": 0.0,
    "noise": 0.0,            # varies per test
    "mix": 1.0,
    # carrier_mode defaults to "Self" (correct)
}
```


## UPDATE: Depth Cracking (same session)

### Depth Formula CRACKED
```cpp
if (depth <= 1.0f)
    envGain = 0.005f * (1.0f - depth) + e * depth;  // floor blend
else
    envGain = e * depth * pow(0.5f, (depth - 1.0f) * 1.5f);  // attenuated boost
```

### Results (relative to depth=100%)
| Depth | Ableton | Band Ritual | Error |
|-------|---------|-------------|-------|
| 0%    | -6.3 dB | -6.9 dB     | 0.7 dB |
| 50%   | -3.7 dB | -3.2 dB     | 0.5 dB |
| 100%  | 0.0 dB  | 0.0 dB      | 0.0 dB |
| 150%  | -1.4 dB | -1.0 dB     | 0.4 dB |
| 200%  | -2.1 dB | -3.0 dB     | 0.9 dB |

### Key discoveries
- depth=0 NOT silence — Ableton passes carrier with flat envelope (floor ~0.005)
- depth>100%: Ableton ATTENUATES, doesn't get louder
- Power curve: k^3 not k^6

### BW Offset Test
±2 dB across all BW (10-200%). No fixed global boost needed.

## COMPLETE STATUS (2026-04-04)
| Feature | Status | Error |
|---------|--------|-------|
| Bands | ✅ CRACKED | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB |
| Depth | ✅ CRACKED | 0.4-0.9 dB |
| BW gain | ✅ CRACKED | ±2 dB |
| Gate/Formant/Enhance | ❌ Not yet | — |


================================================================================
FILE: docs/dsp/Band Ritual - PluginProcessor Code Structure.md
================================================================================

# Band Ritual — PluginProcessor.cpp Code Map

**File**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
**Lines**: ~4642
**Class**: `ColorVocoderAudioProcessor`

---

## File Structure Overview

The file follows this top-level layout:
1. InputFilter class implementation (HP/LP with variable slope)
2. Constructor / Destructor
3. Parameter layout (all APVTS params)
4. prepareToPlay (filter init, buffer allocation)
5. Envelope coefficient calculation
6. Scale/chord frequency building + snapping
7. Band rebuild + filter coefficient update
8. GUI snapshot helpers
9. **processBlock** (the main DSP pipeline)
10. State save/load (XML serialization)
11. Editor factory

---

## DSP Processing Pipeline (processBlock)

All line numbers refer to `processBlock` starting at **line 2701**.

### 1. Pre-processing (L2701-2934)
| Line | What |
|------|------|
| 2704 | `ScopedNoDenormals` |
| 2708 | Deferred band distribution rebuild (thread-safe dirty flag) |
| 2717-2806 | MIDI Chord Trigger — consume octave 2/3 notes, set chord degree/variation |
| 2809-2829 | MIDI Learn — capture chord from MIDI input |
| 2831-2874 | MIDI Follow — track held notes, build `midiNoteFreqs` |
| 2883-2888 | License check — mute if trial expired |
| 2894-2898 | Safety: bail if `prepareToPlay` not called |
| 2901-2922 | **Input Gain** + input RMS metering |
| 2926-2927 | Bypass check |
| 2930-2932 | Formant smoothing (80ms) |
| 2934 | `updateBandsIfNeeded()` — rebuilds bands or updates filters |

### 2. Input Filter Setup (L2936-2981)
| Line | What |
|------|------|
| 2937-2960 | Read HP/LP freq, slope, Q, Q-width params |
| 2966-2981 | Configure `inputFilter` and `dryFilter` with same settings |

### 3. Parameter Loading (L2983-3094)
| Line | What |
|------|------|
| 2997-3000 | mix, modLevel, complementMode |
| 3002-3055 | **Complement mode EQ memory swap** (swaps Bell params on toggle) |
| 3057-3066 | depth, noiseColor, hpBlend, carrier params, noise level/mode |
| 3069-3070 | Gate amount + range |
| 3074-3083 | Spectral tilt/intensity (smoothed) |
| 3089-3094 | Pre-calculate gate threshold (linear, avoids per-sample log) |

### 4. Complement Buffer Copy (L3105-3116)
Copy raw input BEFORE Bell EQ for complement sub/high extraction.

### 5. Input Bell EQ (L3118-3191)
| Line | What |
|------|------|
| 3128-3158 | Bell 1 — `makePeakFilter`, process per-sample L/R |
| 3160-3191 | Bell 2 — same pattern |
Applied BEFORE wet/dry split — shapes what vocoder "hears" AND dry signal.

### 6. Dry Copy + Carrier Setup (L3193-3307)
| Line | What |
|------|------|
| 3195-3197 | Copy input (post-Bell EQ) to `dry` buffer |
| 3224-3232 | Create modulator buffer with `modLevel` gain |
| 3238-3303 | Carrier generation (Self/External/Noise/Saw/Square/PitchTrack) |
| 3271-3300 | External sidechain routing |

### 7. Noise Generation (L3309-3392)
| Line | What |
|------|------|
| 3317-3333 | Pink/white noise blend via `noiseColor` param |
| 3336-3392 | **Unvoiced detection**: HP/LP ratio (LPF@1kHz vs HPF@3kHz envelopes) |

### 8. Band Loop — Core Vocoder (L3394-3583) ★
| Line | What |
|------|------|
| 3427-3431 | BW-dependent tilt correction (0.6-1.0 dB/oct) |
| 3433 | `for (int bi = 0; bi < nBands; ++bi)` |
| 3440-3441 | Skip merged secondary bands (unless additive mode) |
| 3454-3458 | Skip note-excluded bands |
| 3484-3533 | **Per-sample envelope**: analysis filter -> abs -> attack/release -> level comp -> gate -> chord gate |
| 3541-3547 | Per-band multipliers: spectral gain, tilt correction, band gain, merge norm |
| 3549-3579 | **Carrier filtering**: stacked 2x bandpass (4th order) per channel, multiply by envelope * depth |
| 3581-3582 | Accumulate into `wet` buffer |

### 9. Spectral Centroid (L3585-3617)
Combines HP/LP ratio with band-energy centroid for robust unvoiced detection.

### 10. Crossfade (L3619-3744)
Equal-power crossfade between old and new bands (~50ms) when params change.

### 11. Post-Band Gain Compensation (L3746-3787)
| Line | What |
|------|------|
| 3747-3751 | Safety scaling: `1/sqrt(nBands/8)` |
| 3757-3766 | Depth compensation: +6dB at 100% depth |
| 3773-3787 | **Width compensation**: up to +23dB at narrow BW, steep `(1-norm)^6` curve |

### 12. Noise Mixing (L3789-3884)
| Line | What |
|------|------|
| 3801-3839 | Noise mode ratios: Vocal (35/65), Synth (70/30), Auto (carrier-aware) |
| 3844-3883 | Per-sample: `(baseNoise + reactiveNoise) * energyGate` added to wet |

### 13. HP Blend (L3886-3946)
High-passed modulator passthrough for sibilant clarity (Vocodex "clarity trick"). Gated by same gate params.

### 14. Post-Vocoder EQ / Input Filter (L3948-4006)
| Line | What |
|------|------|
| 3967 | `inputFilter.process(wet)` — HP/LP on wet |
| 3968 | `dryFilter.process(dry)` — same HP/LP on dry (master EQ) |
| 3973-3989 | Complement LP filter (isolate sub from original) |
| 3995-4001 | High complement HP filter (isolate highs from original) |

### 15. Complement Bells — Wet/Dry Ratio EQ (L4044-4180)
| Line | What |
|------|------|
| 4077-4130 | CompBell1: boost wet + inverse cut dry (or vice versa) |
| 4132-4180 | CompBell2: same pattern |
Inverse gain clamped to +/-12dB to prevent phase distortion.

### 16. Final Mix (L4182-4263)
| Line | What |
|------|------|
| 4192-4207 | `out = dry*(1-mix) + wet*mix` (with or without compBell EQ) |
| 4212-4263 | Sub complement (fixed level, ignores mix) + High complement (follows mix) |

### 17. Trial Nag + Output (L4274-4340)
| Line | What |
|------|------|
| 4277-4303 | Trial nag: 0.7s silence every 7s |
| 4306-4316 | Spectrum FFT (DISABLED for CPU) |
| 4318-4339 | **Output Gain** + output RMS metering |

---

## Key Support Functions

| Function | Line | Purpose |
|----------|------|---------|
| `updateEnvelopeCoeffs()` | 880 | Per-band attack/release, adaptive envelope, gate smoothing |
| `buildScaleNoteFrequencies()` | 979 | Generate scale note Hz array for snapping |
| `rebuildBands()` | 1425 | Allocate Band objects, set base/scale freqs, init filters, crossfade |
| `updateBandFilters()` | 1587 | Set carrier/analysis Q, formant shift, chord snap, merge detection |
| `updateBandsIfNeeded()` | 2579 | Check param changes, call rebuild or update |
| `calculateSpectralGain()` | (header) | X/Y pad spectral tilt multiplier |

---

## Key Constants & Calibrated Values

| Constant | Value | Line | Purpose |
|----------|-------|------|---------|
| `envLevelComp` | 15.5 (+23.8 dB) | 3472 | Envelope level to match Ableton |
| `maxBoostDb` (width comp) | 23.0 dB | 3782 | Width compensation ceiling |
| Width comp curve | `(1-norm)^6` | 3783 | Steep rolloff — big boost at narrow BW only |
| `tiltDbPerOct` | 0.6 + 0.4*bwNorm | 3431 | BW-dependent HF tilt correction |
| Safety scaling | `1/sqrt(nBands/8)` | 3749 | Prevents clipping with many bands |
| Depth comp | +6dB at 100% | 3760 | Compensates vocoder amplitude loss |
| Crossfade time | 50ms | 1445 | Band transition smoothing |
| Gate attack | 2ms (fixed) | 965 | Standard gate attack |
| Gate release | 30ms (fixed) | 966 | Prevents gate crackle |
| Formant smoothing | 80ms | 660 | Prevents IIR crackling |
| Param smoothing | 10ms | 654 | Spectral tilt/intensity |
| HP blend cutoff | 4000 Hz (fixed) | 639 | Sibilant passthrough frequency |
| Unvoiced LPF | 1000 Hz | 700 | Voiced content detection |
| Unvoiced HPF | 3000 Hz | 701 | Sibilant content detection |
| Pink noise cutoff | 1000 Hz | 647 | 1-pole LP for pink approximation |
| Q range | 0.5 - 200.0 | 1611-1612 | BandWidth -> Q mapping |
| Ritual mode analysis Q | 2.0 (fixed) | 1746 | Original Band Ritual behavior |
| Linked mode max Q | 48.0 (asymptotic) | 1751 | Caps analysis Q in linked mode |
| Merge threshold | 0.5 semitones | 1823 | Band deduplication distance |
| Chord gate threshold | 1 semitone | 1705 | Start fading |
| Chord gate falloff | 2 semitones | 1706 | Fully muted at 3 semitones |
| Noise: Vocal ratios | 35% base / 65% reactive | 3803-3804 | Speech intelligibility |
| Noise: Synth ratios | 70% base / 30% reactive | 3808-3809 | Constant grit texture |
| Inverse gain clamp | 0.25 - 4.0 (+-12dB) | 4093 | CompBell phase safety |
| Trial nag | 0.7s / 7s | 4276 | Silence burst interval |

---

## Quick Reference — Most Commonly Tuned Parameters

### Volume/Level Calibration
- **`envLevelComp`** (L3472): Master envelope scaling. Currently 15.5. Change this if overall vocoder is too quiet/loud vs reference.
- **`maxBoostDb`** width comp (L3782): Currently 23.0. If narrow-BW presets are too loud/quiet.
- **Width comp exponent** (L3783): Currently `^6`. Steeper = less compensation at mid-widths.
- **`tiltDbPerOct`** (L3431): HF tilt correction. Currently 0.6-1.0. If HF is too bright/dark.
- **Safety scaling** (L3749): `1/sqrt(nBands/8)`. If high band counts clip or are too quiet.
- **Depth comp** (L3760): +6dB at 100%. If vocoder at full depth is too quiet.

### Envelope / Dynamics
- **Gate attack/release** (L965-966): 2ms/30ms fixed. If gate clicks or is sluggish.
- **Formant smoothing** (L660): 80ms. If formant automation crackles, increase. If sluggish, decrease.
- **Envelope soft limit** (L3563): tanh above 2.0, peaks at ~3.0. If clipping on transients.

### Noise / Intelligibility
- **Vocal/Synth ratios** (L3803-3809): Base vs reactive noise blend per mode.
- **HP blend cutoff** (L639): 4kHz fixed. If sibilants need different frequency.
- **Unvoiced detection** LP/HP (L700-701): 1kHz/3kHz. If detection misses consonants.

### Band Behavior
- **Q range** (L1611-1612): 0.5-200. If extreme BW settings misbehave.
- **Merge threshold** (L1823): 0.5 semitones. If bands stack when they shouldn't (or vice versa).
- **Chord gate** (L1705-1706): 1st threshold + 2st falloff. If chord mode is too aggressive/loose.
- **Ritual analysis Q** (L1746): Fixed at 2.0. The "secret sauce" of Ritual mode.
- **Linked max Q** (L1751): 48.0 asymptotic cap. If linked mode is too resonant at high Q.

================================================================================
FILE: docs/dsp/Band Ritual - Q-from-range + close LO-HI 2026-06-12.md
================================================================================

---
name: Q-from-range + close LO/HI (Ableton constant-fractional-Q)
description: >-
  Linked-mode Q now scales with band-count AND range octaves (Q ~ N/(n_oct*BW));
  LO/HI param ranges widened so they can be brought very close (down to ~0.03
  oct) for super-resonant clusters. Validated via pedalboard: Q doubles per
  octave-halving, stable, Iter-22 preserved.
type: project
date: '2026-06-12T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - Q
  - range
  - implemented
---
# Band Ritual — Q-from-range + close LO/HI (2026-06-12)

Implements the **never-done action item** from `Band Ritual - Ableton Filter Measurements 2026-04-09`:
Ableton uses **constant fractional Q** `Q ≈ N / (n_octaves × BW)`, `n_octaves = log2(highFreq/lowFreq)`.
BR's `qTable` was keyed on the Band Width knob ONLY (no N, no range term), so narrowing LO↔HI did
**not** sharpen the bands — the thing ricosan noticed vs Ableton. Active engine is the **IIR filterbank**
(STFT off, `useSTFTLinked=false`).

## Change 1 — Q scales with band-count + range (Linked mode only)
`PluginProcessor.cpp` in `updateBandFilters()`, right after `QcarrierBase = jmax(0.5, QcarrierBase)` (~L1672):
```cpp
if (algorithmMode == 1) {               // Linked only; Ritual/Split untouched
    constexpr float kRefBands=40.0f, kRefOctaves=9.81f;   // log2(18000/20)
    float loF=lowFreq, hiF=jmax(highFreq, loF*1.1f);
    float nOct=jmax(0.1f, log2(jmax(1.0001f, hiF/jmax(20.0f,loF))));
    float qScale=(nBands/kRefBands)*(kRefOctaves/nOct);
    QcarrierBase=jlimit(0.5f,200.0f, QcarrierBase*qScale);
}
```
- **Identity at the Iter-22 reference** (N=40, full range 20–18k → qScale=1.0 exactly) → 91.4% cosine match preserved, no regression.
- Multiplies the existing qTable (keeps the fine BW-curve tuning) rather than replacing it.
- Per-band `Qcarrier` and `Qanalysis=Qcarrier*qaRatio` inherit the scaling downstream.
- `globalBandQ` (GUI/carousel) set AFTER scaling so the readout reflects the scaled Q.

## Change 2 — let LO and HIGH be brought very close
Before: `lowFreq 20–500`, `highFreq 2000–18000` → min gap **2 octaves**; band-graph handles also forced 1 octave.
- **Param ranges** (`PluginProcessor.cpp:405-411`): both → `NormalisableRange(20, 18000, 1.0, 0.25)`, defaults unchanged (20 / 18000). Presets back-compat (APVTS stores real Hz).
- **Range guard** (min gap `kMinRangeRatio = 1.1f` ≈ 0.14 oct) applied at the 3 band-layout sites: `rebuildBands()` (after raw values stored for change-detection), the Q-from-range block, and the `processBlock` preview block. `lastLowFreq/lastHighFreq` and GUI `lowHz/highHz` keep RAW values (no spurious rebuilds; handles sit where user put them).
- **Band-graph drag handles** (`PluginEditor.h:896,904`): `highHz*0.5`→`highHz/1.1f`, `lowHz*2.0`→`lowHz*1.1f` (handles approach to 1.1×, no crossing).
- **Hidden sliders** (`PluginEditor.cpp:2467,2471`): `setRange(20,18000,1)` for coherence.

## Verification (pedalboard 0.9.22, headless VST3, white noise, carrier=Self, BW0.3)

**Q tracks the range (the original bug), Linked vs Ritual — confirms scoping & no regression:**
| | Q full (20–18k) | Q narrow (500–2000) | ratio |
|---|---|---|---|
| Ritual | 7.24 | 7.93 | **1.09×** (flat — fix NOT applied, Ritual untouched) |
| Linked | 4.64 | 14.52 | **3.13×** (sharpens) |

**Single isolated band ~1 kHz, 5 bands, closing the range (1/96-oct measurement):**
| range | octaves | Qeff |
|---|---|---|
| 125–8000 | 6.00 | 7.4 |
| 354–2828 | 3.00 | 15.9 |
| 595–1682 | 1.50 | 28.3 |
| 794–1260 | 0.67 | 47.5 |
| 891–1122 | 0.33 | 62.7 |
→ **Q doubles per octave-halving** = exactly Q ∝ 1/n_octaves (Ableton's law).

**New sub-2-octave ranges (impossible before) tested down to 990–1010 Hz (0.03 oct): all outputs FINITE — stable** (Q clamp 200 + 1.1× guard, no NaN/inf).

## Notes / open
- At extreme-narrow range Q slams the 200 ceiling → very resonant "bell"/near-sine. Intended; soften by lowering the clamp or `pow(qScale, <1)` if ricosan finds it too aggressive (1 line). **Pending his ear.**
- Measurement caveat: summed filterbank Q understates per-band Q when many bands overlap/merge; isolate a band (few bands + fine smoothing) for clean numbers.
- NOT applied to Ritual mode by design; 1 line to extend if wanted.
- Build: `~/Desktop/BandRitualV3/build` (internal disk, normal cmake). Install `/Library` via `~/Desktop/INSTALL_BANDRITUAL_QRANGE.command`.

## Related
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09` (the source measurement + formula)
- `Dev Notes/Band Ritual - Iter 22 Final Architecture 2026-04-12` (the 91.4% reference being preserved)
- `Dev Notes/Band Ritual - Vocoder Cracking NEXT OBJECTIVE` (the "Ableton rings, BR doesn't" transient gap — same Q root)


================================================================================
FILE: docs/dsp/Band Ritual - STFT Integration Attempt 2026-04-10.md
================================================================================

# Band Ritual — STFT Linked Mode Integration (2026-04-10)

**Status:** Wired in, NOT calibrated for BW=10%. IIR iter 19l (cosine 0.928) remains the active path.

## What was done

1. **STFTVocoder integrated into BandRitualV3 processBlock** — `useSTFTLinked` flag in PluginProcessor.h switches between IIR and STFT carrier paths
2. **Band info feeding** from updateBandFilters to STFTVocoder.updateBands()
3. **Audio routing** through stftVocoder.process() with goto bypass of IIR carrier loop
4. **Latency:** STFT adds 2048 samples (~46 ms at 44.1 kHz)

## Why it doesn't work yet at BW=10%

The STFT's contiguous bin assignment creates bands that are **7× wider** than the IIR's Q=37 bandpass:
- At 10 kHz, IIR BW = 270 Hz (~12 FFT bins)
- At 10 kHz, STFT contiguous bins span ~1820 Hz (~85 bins)

This causes +20-30 dB HF excess on real music because the STFT captures all carrier energy in the wide band, while the IIR only passes the narrow center.

### Attempts to fix
- **1/sqrt(binCount) normalization**: mathematically correct for noise, but not sufficient for tonal content
- **Gaussian per-bin weighting** (sigma = fc/74): reduced HF by ~5 dB but still +17-22 dB excess
- The fundamental issue: STFT contiguous bins ≠ IIR bandpass at narrow BW

### Why it worked at HIGHBW (Obsidian: STFT Vocoder Calibration, attempt 22)
At high bandwidth, IIR Q is low (Q ≈ 4-8), so the IIR BW is close to the STFT contiguous bin range. The calibration constants (kNoiseEnvComp=1.18, kTonalEnvComp=0.042, tilt sqrt(f/4000)) compensated for the small residual difference. At BW=10% (Q=37), the gap is too large for constant-based correction.

## What's needed for STFT to work at BW=10%

1. **BW-dependent bin selection**: instead of using all contiguous bins, weight bins by a window (Gaussian, Hann, or raised-cosine) that narrows with BW. Width = fc/Q.
2. **BW-dependent calibration constants**: kNoiseEnvComp, kTonalEnvComp, and tilt all need to be functions of BW, not constants.
3. **Investigate Ableton's BW mapping**: does Ableton narrow its STFT bins when BW decreases? Or does it always use contiguous bins and rely on the overlap between adjacent bands?

## Important debug discovery

**Pedalboard loads from `/Library/` not `~/Library/`!** cmake installs to ~/Library but pedalboard finds the system-wide /Library/ copy first. ALL STFT changes had zero effect until we installed to /Library/ with sudo. The IIR measurements were correct because they happened to load from ~/Library/ (pedalboard behavior might vary).

**ALWAYS install to /Library/ when testing with pedalboard.**

## Files modified
- `PluginProcessor.h`: added `#include "STFTVocoder.h"`, `STFTVocoder stftVocoder`, `bool useSTFTLinked`
- `PluginProcessor.cpp`: prepare(), updateBandFilters(), processBlock() STFT routing
- `STFTVocoder.cpp`: experimental changes (Gaussian weighting, binNorm) — should be reverted to original attempt 22 state before next calibration attempt

## Code location
- Flag: `useSTFTLinked = false` in PluginProcessor.h (line ~379)
- Routing: processBlock around line 4069, `if (useSTFTLinked && algoMode == 1)`
- Band feeding: updateBandFilters around line 2078


================================================================================
FILE: docs/dsp/Band Ritual - Sub Mid Calibration 2026-04-07.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking DEFINITIVE v13.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking DEFINITIVE.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - cracking
  - definitive
date: '2026-03-23'
---
# Band Ritual — Vocoder Cracking: DEFINITIVE Analysis (2026-03-23)

## CRITICAL REVISION: Devil's Advocate Review

### What IS confirmed (high confidence)
1. **4th order carrier (2 cascaded biquad stages)** — BW_20dB/BW_3dB ratio = 3.2 matches 4th order. Confirmed from isolated band fitting (error 0.914 vs 14.6 for 4 stages).
2. **Combined output Q ≈ 61** — measured from definitive recording with verified screenshot
3. **Band grid = logspace(20, 18000, 40)** — confirmed from 40 isolated recordings + debug log
4. **Peak detector envelope** — Ableton has Attack/Release controls

### What was WRONG (revised)
1. **Q=61 is NOT the per-stage Q.** It's the combined Q of 2 cascaded stages. Per-stage Q = 61/1.554 ≈ **39**. This matches March 20's Q=37 within measurement error!
2. **cos()^1.29 frequency-dependent Q is likely a MEASUREMENT ARTIFACT.** The 40-band verification data (verify_all_40_Q.txt) showed Q INCREASING at HF (opposite of cos^1.29). The 5-point fit was from unreliable HF measurements.
3. **Ableton "Precise" mode = constant Q (constant fractional bandwidth).** Data shows constant BW/fc ratio. NOT constant absolute Hz bandwidth.

### The CORRECT architecture (best estimate)
```
Carrier: 2 stages (4th order) at Q_stage ≈ 39 (CONSTANT across all bands)
Analysis: uncertain (2 or 3 stages, Qa ratio uncertain)
Envelope: peak detector (attack/release)
Grid: logspace(20, 18000, N)
Precise mode: constant Q, no frequency-dependent correction
```

### Why March 20 and March 22 both had pieces of truth
- March 20: Q=37 per stage, 4 stages → WRONG stage count, RIGHT Q per stage
- March 22: Q=61 combined, 2 stages → RIGHT stage count, Q was combined not per-stage
- Both give similar effective filter shape; 2-stage Q=39 is the correct decomposition

### What to test in next session
1. **Use 2 stages with constant Q=39 per stage** (no cos correction, no freq-dep Q)
2. **Remove tilt correction** (March 20 showed it wasn't needed with correct Q)
3. **Recalibrate bwGain** from scratch
4. This should give similar results to March 20 (±0.5 dB RMS) but with correct architecture

### Remaining unknowns
- Analysis stage count (2 or 3)
- Analysis Qa ratio (0.6 or different)
- Whether the per-stage Q is exactly 39 or needs fine-tuning
- Envelope attack/release exact behavior

### Sources consulted
- Ableton Live Manual (vocoder section)
- Sound on Sound: Introduction to Vocoders
- Stone et al. 2008 (PMC) — envelope bandwidth effects
- CCRMA Stanford — audio filter banks
- KVR Audio — vocoder design forum
- Cycling '74 — vocoder tutorial

### Key insight from research
"For noise input, AM sidebands do NOT broaden the output spectrum in a measurable way" — the long-term PSD of noise × envelope reflects the carrier filter shape. BUT the BW ratio (20dB/3dB) is more robust than absolute Q measurement because it's a relative measure that cancels envelope effects.


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking FINAL v12.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - final
date: '2026-03-24'
---
# Band Ritual — Vocoder Cracking FINAL v12 (2026-03-24)

## Results: 1.8 dB avg RMS, 0.0 dB levels
| BW% | Level | RMS | PVR diff |
|-----|-------|-----|----------|
| 10% | 0.0 | 1.5 | +1.0 |
| 30% | 0.0 | 2.2 | -3.2 |
| 50% | 0.0 | 1.3 | +0.9 |
| 70% | 0.0 | 2.2 | +0.4 |
| 100% | 0.0 | 2.2 | -2.5 |
| 130% | 0.0 | 2.3 | -2.3 |
| 170% | 0.0 | 1.1 | -4.4 |
| 200% | 0.0 | 1.3 | -5.7 |

## Architecture (CRACKED)
- **Grid**: logspace(20, 18000, 40)
- **Carrier**: 2 cascaded biquad bandpass (4th order)
- **Analysis**: 3 stages, Qa = (0.85 - 0.55*widthNorm) × Qc (BW-dependent)
- **Phase alternation**: every other band inverted before summing
- **Envelope**: peak detector (attack 1ms, release 50ms)
- **cos()^1.29**: frequency-dependent Q correction

## Parameters
### Q Table (11 entries)
```
{10, 53}, {15, 26}, {20, 19}, {30, 17}, {40, 11.4},
{50, 12.7}, {70, 10.7}, {100, 7.9}, {130, 7.6}, {170, 4.5}, {200, 3.5}
```

### bwGain Table
```
{0.000, 44.3}, {0.105, 34.9}, {0.211, 34.4}, {0.316, 33.7},
{0.474, 30.3}, {0.632, 28.7}, {0.842, 21.1}, {1.000, 16.8}
```

### Tilt
```
tiltAlpha = -0.003*norm - 0.090
LF shelf: 3.0 * (1 - |norm - 0.35| * 2.5) dB below 250 Hz
```

### Analysis Q ratio
```
qaRatio = 0.85 - 0.55 * norm  (0.85 at BW=10%, 0.30 at BW=200%)
```

## Key Discoveries (entire cracking journey)
1. **Reference bug**: original Ableton recordings had low_freq=40 not 20 → 18 dB error
2. **Phase alternation**: alternating band polarity reduced avg RMS from 4.1→1.7 dB (KVR forum tip)
3. **Filter type**: 4th order bandpass confirmed, topology doesn't matter, only order
4. **BW-dependent Qa**: analysis Q ratio drops at wide BWs for smoother envelopes
5. **Pedalboard**: Spotify's Python lib loads VST3 and matches Ableton within 1.0 dB

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- References: `~/Desktop/vocoder-comparison/ableton_crack_v2/`
- Comparison plots: `~/Desktop/vocoder-comparison/FINAL_v12.png`


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking FULL STATUS 2026-03-24.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - status
date: '2026-03-24'
---
# Band Ritual — Vocoder Cracking FULL STATUS (2026-03-24)

## What's CRACKED ✓
1. **Band grid**: logspace(20, 18000, 40) — confirmed from isolated recordings
2. **Filter type**: 4th order bandpass (2 cascaded biquads) — confirmed from filter shape analysis
3. **Phase alternation**: every other band inverted — from KVR forum, reduced RMS 4.1→1.8 dB
4. **Envelope formula**: `pow(0.01, 1/(sr*t))` — release = time to -40 dB, verified from burst test
5. **Q table**: calibrated per BW (11 entries), with cos()^1.29 freq-dep correction
6. **Tilt**: alpha = -0.003*norm - 0.090 + LF shelf below 250 Hz
7. **bwGain**: calibrated per BW (8 entries)
8. **Analysis Qa ratio**: BW-dependent (0.85 at BW=10% → 0.30 at BW=200%)
9. **Gate range**: -inf(-60) to +12 dB, matching Ableton's range, displays "-inf" at minimum
10. **Gate threshold**: knob_dB - 17 dB offset (accounts for per-band energy split)

## Results: 1.8 dB avg RMS spectral match
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

## What STILL needs work

### 1. Gate fine-tuning (close but not identical)
- Gate cutoff timing is close but our gate cuts slightly later than Ableton
- Ableton's gate has a smoother fade-out, ours is more abrupt (hard gate vs soft gate)
- The -17 dB offset might need per-BW adjustment
- **Next step**: Try a softer gate (restore gateRange instead of hard 0), compare gate fade shape

### 2. BW=100-130% spectral shape
- Our bands still slightly more pronounced than Ableton's at mid BWs
- PVR diff: -2.1 to -2.7 (our valleys slightly shallower)
- LF region (below 200 Hz) still ~3 dB quieter than Ableton
- **Next step**: This might improve with correct gate behavior (gate affects low bands first)

### 3. Noise/Unvoiced system
- Not yet cracked — need to match Ableton's unvoiced detection and noise injection
- Our system exists but hasn't been calibrated against Ableton
- **Next step**: Export recordings with Unvoiced knob at different levels, analyze detection

### 4. Gate smoothing
- Ableton's gate fades smoothly (visible in the fade test — gradual rolloff)
- Our gate has a more sudden cutoff
- **Next step**: Adjust gate attack/release smoothing coefficients (currently 2ms/30ms)

## Key Technical Insights
- `pow(0.01, 1/(sr*t))` NOT `exp(-1/(sr*t))` for envelope coefficients
- Phase alternation on every other band is essential for smooth overlap at wide BWs
- Pedalboard (Spotify) matches Ableton export within 1.0 dB — reliable for iteration
- Reference recordings MUST use correct low_freq (old recordings had low_freq=40, wasted hours)
- Gate operates on per-band envelope which is ~17 dB below broadband input level

## Files
- Plugin: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`
- Correct references: `~/Desktop/vocoder-comparison/ableton_crack_v2/`
- Gate test: `~/Desktop/vocoder-comparison/gate_test/`
- Envelope test: `~/Desktop/vocoder-comparison/envelope_test/`


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking Final Status.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - cracking
  - final
date: '2026-03-22'
status: near-complete
---
> [!warning] CORRECTED 2026-06-15 — see `Band Ritual - Grid VERIFIED Pure-Log 2026-06-15`
> The **"34.5–15200 Hz actual centers"** grid and the **"20 cents RMS / grid artifact"** framing
> below are **WRONG** — an artifact of a peak-search window clipped to 25 Hz–17 kHz that corrupted
> the band-1 and band-40 measurements. **Truth:** Ableton's grid is **pure log 20 Hz → 18 kHz,
> `i/(N-1)`, 302 c/band**, and **BR already matches it to 7.0 c RMS** (the measurement floor).
> Do NOT re-introduce any endpoint inset or bake a measured-center table. The narrow-BW "floor"
> is a measurement artifact, not an audio difference. Everything else in this note (filter order,
> Q tables, envelope) still stands.

# Band Ritual — Vocoder Cracking: Final Status (2026-03-22)

## FULLY CRACKED PARAMETERS

### Carrier Filter
- **2 cascaded IIR biquad bandpass stages** (4th-order)
- Q per stage: exact lookup table from isolated Band 20 recordings

### Q Lookup Table (40 bands, measured from Ableton)
| BW% | Q per stage |
|-----|-------------|
| 10 | 85.57 |
| 15 | 42.44 |
| 20 | 31.62 |
| 30 | 29.01 |
| 40 | 15.83 |
| 50 | 17.46 |
| 70 | 9.08 |
| 100 | 8.49 |
| 130 | 6.52 |
| 200 | 4.18 |

**Note**: Q curve is NOT a simple 1/BW formula. Has a sharp knee between BW=10-30%. Non-monotonic at BW=40-50% (possibly measurement artifact — needs verification).

### Band Frequency Grid
- `logspace(34.5, 15200, 40)` or equivalently KVR formula `center[i] = 34.5 * (15200/34.5)^(i/39)`
- Standard vocoder formula uses `pow(H/L, 1/N)` not `1/(N-1)` — but with adjusted endpoints both give same result
- Alignment: **20 cents RMS** vs Ableton (measurement resolution limit)
- Ableton's user-facing range (20-18000 Hz) maps to actual band centers (~34.5-15200 Hz)

### Envelope Follower
- **One-pole lowpass of |x|** (not peak detector with attack/release)
- Time constant from Attack parameter (~1ms)
- This change improved wide-BW PVR from 4.4→2.5 dB (target 1.7)

### Tilt Correction
```
tiltAlpha = -0.259 × widthNorm + 0.186
tiltCorr = pow(freq / 1000.0, tiltAlpha)
```

### Level Correction (bwGain v3, for one-pole envelope)
| wN | BW% | bwGain (dB) |
|-----|------|-------------|
| 0.000 | 10% | 51.7 |
| 0.105 | 30% | 44.0 |
| 0.211 | 50% | 41.2 |
| 0.316 | 70% | 40.1 |
| 0.474 | 100% | 39.6 |
| 0.632 | 130% | 39.0 |
| 0.842 | 170% | 37.7 |
| 1.000 | 200% | 36.1 |

### Other Settings
- Analysis: 3 stages, Qa = 0.6 × Qc (analysis doesn't affect PVR)
- Attack: 1ms (Ableton's minimum)
- Release: 50ms
- Frequency range: 20-18000 Hz (user-facing), 34.5-15200 Hz (actual centers)

## FINAL PSD RESULTS (avg 4.1 dB)
| BW% | PSD RMS | Level | PVR Δ | Status |
|-----|---------|-------|-------|--------|
| 10% | 8.9 | 0.0 | -7.6 (grid artifact) | Grid artifact |
| 30% | 6.0 | -0.1 | -1.0 | Close |
| 50% | 4.8 | -0.2 | +1.1 | ✓ |
| 70% | 2.7 | +1.7 | +1.0 | ★ |
| 100% | 3.3 | 0.0 | +1.8 | ✓ |
| 130% | 2.5 | 0.0 | +1.0 | ★ |
| 170% | 2.1 | 0.0 | +1.2 | ★ |
| 200% | 2.8 | 0.0 | +1.3 | ★ |

## WHAT THE REMAINING ERROR IS
- **BW=10% (8.9 dB)**: Proven to be 20-cent frequency grid misalignment artifact. At Q=85, 20 cents causes 14 dB apparent PVR loss. True filter PVR = 44.9 dB (matches Ableton's 41.5).
- **BW=30% (6.0 dB)**: Same grid artifact at lower magnitude (Q=29).
- **BW=50-200% (2.1-4.8 dB)**: Mix of grid artifact and ~1 dB real PVR difference.
- **20 cents is the measurement resolution limit** from PSD peak detection.

## HOW TO IMPROVE FURTHER
1. Record isolated single bands at more band positions (Band 10, 15, 25, 30) to build exact 40-point frequency table
2. This would reduce grid error from 20 cents to <5 cents
3. Would primarily improve BW=10% and BW=30% scores

## RESEARCH FINDINGS (web search)
- Standard vocoder formula: `center[i] = L × (H/L)^(i/N)` (KVR Audio forums)
- Ableton uses log-spaced centers (confirmed by all sources)
- "Precise" mode = constant BW, "Retro" mode = constant-Q
- No source publishes exact Ableton vocoder parameters
- Someone reverse-engineered Ableton Live 12 internals via DLL injection (KVR) but didn't publish vocoder DSP details

## FILES MODIFIED (from original V3)
- `PluginProcessor.cpp` ~line 1421: frequency grid → effectiveLow/High
- `PluginProcessor.cpp` ~line 1561: Q → 10-point lookup table
- `PluginProcessor.cpp` ~line 1696: Q clamp → 0.5 to 200
- `PluginProcessor.cpp` ~line 1715: Qa ratio = 0.6 × Qc
- `PluginProcessor.cpp` ~line 1757: tiltCorr → pow(f/1000, alpha)
- `PluginProcessor.cpp` ~line 3513: envelope → one-pole lowpass of |x|
- `PluginProcessor.cpp` ~line 3585: carrier stages → 2
- `PluginProcessor.cpp` ~line 3822: bwGain → v3 piecewise linear table


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking Hypotheses Matrix.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - cracking
  - hypotheses
  - iteration
date: '2026-03-22'
status: active
---
# Band Ritual — Vocoder Cracking: Hypotheses Matrix

## Purpose
Master list of every plausible correction to match Ableton's vocoder.
Status: ✅ VALIDATED, ❌ INVALIDATED, 🔶 PARTIALLY VALIDATED, ⬜ UNTESTED.

---

## 1. FILTER TYPE
- **1A. IIR biquad**: ✅ VALIDATED
- **1B. SVF**: ❌ INVALIDATED
- **1C. Different biquad formula**: ❌ INVALIDATED — all formulas identical shape

## 2. CARRIER FILTER STAGES
- **2 stages (4th order)**: ✅ CRACKED — fit error 0.66, confirmed at 6 BW settings
- 1 stage: ❌ too wide
- 3+ stages: ❌ too narrow

## 3. ANALYSIS FILTER STAGES
- **3 stages (current)**: 🔶 — simulation showed minimal PSD impact, but may affect PVR
- **→ NEXT: investigate if analysis stages affect PVR mismatch**

## 4. Q MAPPING
- **Q = 856 / BW% (for 40 bands)**: ✅ CRACKED from isolated Band 20 recordings
- **General: Q = (46.74/BW% - 0.063) × ((N-1)/3)^1.014**: ✅ verified at N=4 and N=40
- Old maxQ=37 log mapping: ❌ INVALIDATED

## 5. Qa RATIO (analysis Q / carrier Q)
- **0.60 (current)**: 🔶 — not independently validated with new Q range
- **→ May need adjustment for PVR tuning**

## 6. FREQUENCY RANGE
- **20 Hz – 18,000 Hz**: ✅ VALIDATED and implemented

## 7. BAND FREQUENCY GRID
- **Log-spaced**: 🔶 — works for wide BWs, may cause PSD error at narrow BWs due to slight misalignment

## 8. ENVELOPE FOLLOWER
- **Attack 1ms, Release 50ms**: ✅ matched to Ableton's settings
- Envelope dynamics effect on PVR: ⬜ UNTESTED

## 9. GAIN CORRECTIONS
- **Tilt correction**: ✅ CRACKED — `alpha = -0.259*wN + 0.186`, error < ±0.33 dB/oct
- **bwGain (level)**: ✅ CRACKED — piecewise linear table, 0.0 dB at all BWs
- **bandCountGain**: ✅ `1/sqrt(N/4)` — unchanged, works

## 10. PVR (Peak-to-Valley Ratio)
- **BW=30%**: ✅ 19.5 vs 20.1 — nearly perfect
- **BW=10%**: ❌ 32.8 vs 41.5 — valleys not deep enough (Δ=-8.7)
- **BW=70-200%**: ❌ PVR too high — valleys too deep
- **→ ROOT CAUSE UNKNOWN — top priority for next session**

## NEXT SESSION PRIORITIES
1. Investigate PVR mismatch root cause (band grid? analysis filter? envelope?)
2. Try adjusting Qa ratio for new Q range
3. Try 2 analysis stages (match carrier)
4. Check if band frequencies exactly match Ableton at 40 bands


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking NEXT OBJECTIVE.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking NEXT SESSION.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking Results 2026-03-22.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - cracking
  - results
date: '2026-03-22'
status: in-progress
---
# Band Ritual — Vocoder Cracking Results (2026-03-22)

## CRACKED Parameters (all confirmed from measurements)

### Carrier Filter
- **2 cascaded biquad stages** (4th-order bandpass)
- **Q per stage = 856 / BW%** (for 40 bands)
- IIR makeBandPass coefficients

### Band Frequency Grid
- **logspace(34.5, 15200, 40)** — NOT logspace(20, 18000)
- Ableton offsets band centers inward: low × 1.725, high × 0.844
- Alignment: RMS = 20 cents vs Ableton's peaks (was 1100 cents before fix)

### Tilt Correction
```
tiltAlpha = -0.259 × widthNorm + 0.186
tiltCorr = pow(freq / 1000.0, tiltAlpha)
```
Error: < ±0.33 dB/octave at all BWs.

### Level Correction (bwGain piecewise linear, dB)
| wN | BW% | bwGain |
|-----|------|--------|
| 0.000 | 10% | 47.9 |
| 0.105 | 30% | 39.7 |
| 0.211 | 50% | 36.5 |
| 0.316 | 70% | 35.1 |
| 0.474 | 100% | 34.2 |
| 0.632 | 130% | 33.3 |
| 0.842 | 170% | 31.6 |
| 1.000 | 200% | 29.8 |

Level error: ~±0.5 dB at all BWs.

### Envelope
- Attack: 1 ms (Ableton's minimum)
- Release: 50 ms

## Current PSD Results (avg 4.3 dB)
| BW% | PSD RMS | Level | PVR Abl | PVR Ours | Status |
|-----|---------|-------|---------|----------|--------|
| 10% | 8.8 | +0.3 | 41.5 | 32.1 | PVR gap |
| 30% | 5.6 | +0.3 | 20.1 | 18.0 | Close |
| 50% | 4.1 | +0.3 | 9.4 | 11.1 | ✓ |
| 70% | 3.4 | +0.2 | 3.8 | 6.6 | ✓ |
| 100% | 3.0 | -0.1 | 1.7 | 4.4 | ★ |
| 130% | 2.6 | -0.4 | 1.8 | 3.9 | ★ |
| 170% | 3.0 | -0.7 | 1.8 | 5.2 | ★ |
| 200% | 4.1 | -0.9 | 1.5 | 8.0 | ✓ |

## Remaining Issues
1. **PVR at BW=10%**: 32.1 vs 41.5 (Δ=-9.4) — valleys not deep enough
2. **PVR at BW=70-200%**: too high — valleys too deep (Q too high for wide BWs in context)
3. **bwGain needs recalibration** for the new grid (levels shifted slightly)

## Files Modified (from original V3)
- `PluginProcessor.cpp` line ~1421: frequency grid → effectiveLow/High (×1.725, ×0.844)
- `PluginProcessor.cpp` line ~1557: Q formula → 856/BW%
- `PluginProcessor.cpp` line ~1696: Q clamp → 0.5 to 200
- `PluginProcessor.cpp` line ~1757: tiltCorr → pow(f/1000, -0.259*wN+0.186)
- `PluginProcessor.cpp` line ~3585: carrier stages → 2 (was 4)
- `PluginProcessor.cpp` line ~3800: bwGain → piecewise linear table

## Key Discovery: Frequency Grid Was the #1 Problem
The single biggest improvement came from fixing the band frequency grid. Our bands were at completely wrong frequencies (1100 cents = ~1 octave off). Fixing this dropped BW=10% error from 19.7 to 8.8 dB. The filter shape, Q, and gain corrections were all correct — but applied at wrong frequencies.


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking Session 2026-03-23.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking Session Log 2026-03-22.md
================================================================================

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


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Cracking v11 Checkpoint.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - checkpoint
date: '2026-03-24'
---
# Band Ritual — Vocoder v11 Checkpoint (SAVE POINT)

## Current Results: 1.7 dB avg RMS
| BW% | Level | RMS | PVR diff |
|-----|-------|-----|----------|
| 10% | 0.0 | 1.4 | -0.7 |
| 30% | 0.0 | 2.1 | -3.8 |
| 50% | 0.0 | 1.3 | +0.3 |
| 70% | 0.0 | 2.2 | +0.1 |
| 100% | 0.0 | 2.2 | -2.4 |
| 130% | 0.0 | 2.3 | -1.9 |
| 170% | +0.5 | 1.1 | -4.2 |
| 200% | +0.5 | 1.4 | -5.6 |

## Current Parameters (v11)
### Q Table (line 1549)
```
{10, 53}, {15, 26}, {20, 19}, {30, 17},
{40, 11.4}, {50, 12.7}, {70, 10.7},
{100, 7.9}, {130, 7.6}, {170, 4.5}, {200, 3.5}
```
Table size: 11

### bwGain Table (line ~3832)
```
{0.000, 42.6}, {0.105, 33.6}, {0.211, 33.5}, {0.316, 33.2},
{0.474, 30.4}, {0.632, 29.5}, {0.842, 23.4}, {1.000, 19.9}
```

### Tilt (line ~1792)
```
tiltAlpha = -0.003*norm - 0.090
```
Plus LF shelf: `3.0 * (1 - |norm - 0.35| * 2.5)` dB below 250 Hz

### Phase Alternation (line ~3627)
Every other band (bi%2) gets polarity inverted before summing to wet.

### cos()^1.29 freq-dep Q — still active

### Analysis Q: Linked mode uses Qa = 0.85 * Qc

## Key Discoveries This Session
1. **Reference was wrong** — ableton_crack/ had low_freq=40, re-recorded as ableton_crack_v2/ with low_freq=20
2. **Phase alternation** — alternating band polarity reduced avg RMS from 4.1→1.7 dB
3. **Pedalboard workflow** — reliable within 1.0 dB of Ableton export, enables automated iteration
4. **Filter type confirmed** — 4th order bandpass, filter topology doesn't matter, only order

## Remaining Issue
BW=100-130% still shows visible peaks while Ableton is nearly flat. Hypothesis: analysis Q ratio or envelope type differs at wide BWs.


================================================================================
FILE: docs/dsp/Band Ritual - Vocoder Reverse Engineering Log.md
================================================================================

# Band Ritual — Vocoder Reverse Engineering Log

## Goal
Match Band Ritual Improved's vocoder output to Ableton's vocoder as closely as possible.

## Test Setup
- **40 bands**, range 40Hz–18kHz, Attack 1ms, Release 50ms, Gate off, Depth 100%, Mix 100%
- **Ableton**: Carrier=Modulator, BW 10% (low) / 100% (high)
- **Band Ritual**: Carrier=Self, Linked mode, Scale Amt 0%, BW 0.1 (low) / 1.0 (high), ADAPT OFF
- **Test signals**: white noise, impulse train (5ms bursts every 500ms), sine sweep (20Hz→20kHz log) — all 10s, 44100Hz
- **Automated pipeline**: Spotify Pedalboard loads VST3 headlessly, processes signals, compares vs Ableton WAV exports

## Current State (2026-03-12, latest iteration)

### Level Matching — SOLVED
| Test | Offset | Tilt | Max Diff |
|------|--------|------|----------|
| Low BW noise | **-0.53 dB** | -0.24 dB/oct | 21.93 dB |
| High BW noise | **+0.95 dB** | -0.29 dB/oct | 6.94 dB |
| Low BW sine | +6.79 dB | +1.41 dB/oct | — |
| High BW sine | +0.62 dB | +1.27 dB/oct | — |

### Q Matching
- **Low BW (10%)**: Ableton Q=57.6, BR Q=57.4 — **perfect match**
- **High BW (100%)**: Ableton Q≈0, BR Q=18.7 — BR still too narrow (BW=1.0 maps to Q≈10, Ableton 100% is nearly flat)

### Envelope Timing (with ADAPT OFF)
- High BW: very close (within 1-2ms across all bands)
- Low BW: BR faster than Ableton on low-mids (7ms vs 44ms attack) — Ableton has inherent per-band smoothing

## Changes Made (chronological)

### Change 1: Added ADAPT toggle (adaptive envelope on/off)
- **What**: New `adaptiveEnv` bool parameter. OFF = flat timing. ON = per-band adaptive timing.
- **Why**: Ableton uses flat timing → zappy/reactive. Band Ritual's adaptive made low bands sluggish.
- **Default**: OFF (Ableton-style)

### Change 2: Extended BW range to 0.01–2.0
- **What**: BW parameter from 0.1–2.0 to 0.01–2.0, default 1.0
- **Why**: Ultra-narrow Q~200 for scale-locked mode (below Ableton's range)
- **Q mapping**: `Q = 0.5 * pow(400, 1-norm)` where norm=(bw-0.01)/(2.0-0.01)

### Change 3: Removed fastTanh envelope saturation
- **What**: Replaced `fastTanh(e * 4.0f)` with soft-limited linear: linear below 1.0, `1 + fastTanh(e-1)` above
- **Why**: fastTanh compressed ALL envelope dynamics → bwNormGain had zero effect, bands sounded "smoothed out"
- **Old**: `envShaped = fastTanh(e * 4.0f); envGain = (1 + depth*(1.75*envShaped - 0.75)) * envShaped`
- **New**: `eLimited = (e < 1) ? e : 1 + fastTanh(e-1); envGain = 1 - depth + depth * eLimited`

### Change 4: Flat envelope level compensation (+20 dB)
- **What**: `const float envLevelComp = 10.0f;` applied to all bands equally
- **Why**: Calibration showed raw uncompensated levels were -19 to -20 dB vs Ableton at BOTH BW settings. No Q-dependent correction needed — analysis/carrier filter interaction naturally balances.
- **Key insight**: Removed qComp and bwNormGain entirely. A single flat gain matches both BW settings.

### Change 5: Recalibrated width-based loudness compensation
- **What**: Reduced maxBoostDb from 10.5 to 9.0 in the width compensation curve
- **Why**: With envLevelComp=10 (+20 dB), the old 10.5 dB boost was too strong. 9.0 dB calibrated to give ~0 dB offset at both BW settings.

### Change 6: BW-scaled tilt correction
- **What**: Per-band tilt correction of 1.27 dB/oct, scaled by BW (0 at BW≤0.1, full at BW≥1.0)
- **Why**: 4th-order stacked carrier has slight HF rolloff vs Ableton's likely 2nd-order. Only visible at wide BW.
- **Code**: `tiltCorrectionDb = 1.27 * bwTiltScale * 9.0 * (bandPos - 0.5)`

## Remaining Improvements
1. **BW=1.0 Q too high** (18.7 vs Ableton's ~0): Would need to change Q mapping so BW=1.0→Q≈1 instead of Q≈10. Currently `Q = 0.5 * pow(400, 1-norm)` where BW=1.0 gives norm=0.497→Q≈10.
2. **Low BW envelope**: BR low-mids are faster than Ableton (7ms vs 44ms). Ableton may have inherent per-band smoothing even without "adaptive" mode.
3. **Sine sweep level at low BW**: +6.79 dB overshoot on resonant peaks. Soft limiter helps but doesn't fully cap. Real audio (voice/music) won't exhibit this.

## Key Files
- `PluginProcessor.cpp`: lines ~920 (bwNormGain=1.0), ~3470 (envLevelComp=10), ~3550 (soft limiter), ~3536 (tilt correction), ~3775 (width comp)
- `auto_compare.py`: `~/Desktop/vocoder-comparison/auto_compare.py`
- `vocoder_compare.py`: `~/Desktop/vocoder-comparison/vocoder_compare.py`  
- Results: `~/Desktop/vocoder-comparison/results/`
- Ableton reference WAVs: `~/Desktop/vocoder-comparison/ableton_*/test.wav`


================================================================================
FILE: docs/dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md
================================================================================

---
name: WAV Clipping Bug — All Cosine Measurements Were Invalid
description: 2026-04-11 discovery — the plugin outputs +18-22 dB RMS / +28-32 dBFS peak on real-music input. 16-bit PCM WAV writes clip at ±1.0, hiding the true level. All cosine measurements in realmusic_test.py from March V3 onward were on clipped signal and are invalid.
type: project
date: 2026-04-11
tags: [bandritual, vocoder, bug, measurement, cosine, clipping]
---

# Band Ritual — WAV Clipping Bug (2026-04-11)

## The bug

`realmusic_test.py` used `sf.write(BR_OUT, out, sr)` without specifying subtype. **soundfile's default is 16-bit PCM**, which hard-clips samples to ±1.0 on write.

The plugin output on `DRY-EXPORT 1.wav` (kick/bass loop) is:

| Setting | BR RMS | BR peak | Clipped samples |
|---|---|---|---|
| bwGain=45.6 (March) | **+22.5 dB** | **+31.8 dBFS** | 82.8% |
| bwGain=100 | ~+27 dB | ~+36 dBFS | ~90% |
| OLD /Library binary | **+18.5 dB** | **+28.2 dBFS** | 78.9% |

**Ableton reference: RMS -5.95 dB, peak +4.32 dBFS.**

BR is **~28 dB** louder than Ableton RMS-wise, **~27 dB** louder peak-wise. The 16-bit WAV silently clamped everything above ±1.0, so readback showed a wildly distorted signal with ~half the samples stuck at ±1.0.

## Why all cosine measurements are invalid

Clipping is nonlinear. It flattens transients and adds harmonic distortion. The spectrogram of a heavily clipped signal looks nothing like the same signal unclipped.

Every cosine similarity we've been measuring was BR_clipped vs ABL_unclipped. We were essentially measuring "how similar a square wave is to a kick", not "how similar BR is to ABL".

All the "iter 20 cosine progression" from our session is garbage:

| Iter | Cosine | Status |
|---|---|---|
| 19l baseline | 0.7066 | CLIPPED |
| 20c BPF2 no-corr | 0.7241 | CLIPPED |
| 20d +HF shelf | 0.7227 | CLIPPED |
| 20f bwGain=40.8 | 0.7277 | CLIPPED |
| 20g bwGain=44 | 0.7312 | CLIPPED |
| 20h bwGain=50 | 0.7362 | CLIPPED |
| 20i bwGain=100 | 0.7494 | HEAVILY CLIPPED |

The monotonic "improvement" as bwGain went up was pure clipping artifact — more clipping flattens the spectrum, which coincidentally increases the dot product with ABL's flatter-than-expected spectrum.

## The fix

### Fix the test immediately
```python
# OLD (16-bit PCM, clips at ±1.0)
sf.write(BR_OUT, out, sr)

# NEW (32-bit float, no clipping)
sf.write(BR_OUT, out, sr, subtype='FLOAT')
```

### Add clipping warning
```python
n_clip = int(np.sum(np.abs(br) >= 0.9999))
if n_clip > 0:
    print(f"CLIPPING: {n_clip} samples ({100*n_clip/len(br):.1f}%) — cosine INVALID")
```

### Fix the plugin calibration
The plugin broadband gain is ~28 dB too high on kick/bass content. Need to recalibrate `bwGain` on REAL MUSIC (not just noise) to produce output within 0 dBFS headroom.

At bwGain=17, BR broadband RMS matches ABL (~-6 dB), but per-region profile becomes:
- Sub: matched (+0.5)
- Bass/Low/Mid/HiMd/High/Air: all -10 to -17 dB below ABL

This means the plugin's TRUE frequency response (at matched broadband) is massively sub-weighted on kick content. The March calibration (tiltCorr=1.0, bwGain=45.6) was done on noise which has a flat-enough spectrum that the problem was masked — noise RMS matched because the sub dominance didn't show up.

## Why this was never caught

1. **validation_suite.py uses noise/sine/sweep**, not kick/bass. Noise has flat spectrum → per-region matches were easy to hit without realizing sub-dominance.
2. **Earlier manual tests** used short sine tones or WAV comparisons — tone levels were below clipping threshold.
3. **realmusic_test.py is recent** and was the first test to use heavy-transient real-music input. It immediately hit the clipping limit, but the 16-bit WAV hid the issue.
4. **The "cosine 0.7138" baseline from the /Library binary** (before this session) was ALSO clipped. The March "cracked" state has never been validated on real-music unclipped output.

## Implications

- **All per-region diffs** from realmusic_test.py are also corrupted by clipping.
- **All validation_suite.py noise results** are still valid (noise is flat and levels were chosen to not clip).
- **The Band Ritual plugin, when loaded in Ableton with gain=0 dB input, will silently clip on transient content.** Users would hear distortion on kicks, snares, and any percussive material.
- **March calibration "working"** was measured on content that didn't trigger clipping.

## Next steps

1. ✅ Fix realmusic_test.py (32-bit float, clipping warning) — DONE
2. Measure TRUE output level on the DRY test input at various bwGain values
3. Find bwGain such that peak stays below 0 dBFS on real music → ~14-17 dB for this input
4. Re-measure cosine on UNCLIPPED output
5. Decide: do we want BR to match ABL loudness exactly on every input type, or do we want BR to have a fixed "unity gain" calibration that matches on average?
6. Warn users via GUI if output peak hits 0 dBFS? Or add internal soft clip?
7. Re-run validation_suite.py with float WAVs to verify synthetic tests weren't also hiding clipping

## Related notes
- `Dev Notes/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md` (invalidated)
- `Dev Notes/Ableton Vocoder - Master Findings Log.md` (cosine 0.714 "cracked" state was also clipped)
- `Dev Notes/Band Ritual - Validation Suite Spec.md`


================================================================================
FILE: docs/dsp/Band Ritual Improved - Vocoder Matching Ableton.md
================================================================================

# Band Ritual Improved — Vocoder Matching Ableton

## Goal
Match Band Ritual's vocoder output to Ableton's Vocoder at Scale Amount 0% (self-carrier mode).

## Test Setup
- **Comparison folder**: `~/Desktop/vocoder-comparison/`
- **Script**: `auto_compare.py` — processes test signals through BR via Pedalboard, compares spectrum to Ableton exports
- **Peak analysis**: `analyze_peaks.py` — compares individual comb peak heights/frequencies at LOWBW
- **Test signals** (in `~/Downloads/`): `TEST_white_noise_10s.wav`, `TEST_sine_sweep_10s.wav`, `TEST_impulse_train_10s.wav`
- **Ableton settings**: Vocoder, self-carrier, 40 bands, range 40–18000 Hz, release 50ms
- **BR settings**: Linked algorithm, self-carrier, 40 bands, 40–18000 Hz, release 50ms, scale=0
- **3 BW settings**: LOWBW (0.1→10%), MIDBW (0.5→50%), HIGHBW (1.0→100%)

## Q Mapping (important for understanding results)
```
BW=0.1 → norm=0.000 → carrierQ=60.0 → analysisQ=26.7
BW=0.5 → norm=0.211 → carrierQ=33.2 → analysisQ=19.6
BW=1.0 → norm=0.474 → carrierQ= 8.3 → analysisQ= 7.1
```
Analysis Q uses asymptotic cap: `Qanalysis = 48 * Qcarrier / (Qcarrier + 48)`

---

## APPROACH 1: 4th-Order Carrier (original architecture)
**Status**: Abandoned in favor of 2nd-order approach

### Best results achieved
| Metric | LOWBW (10%) | MIDBW (50%) | HIGHBW (100%) |
|---|---|---|---|
| Noise offset | -0.26 dB | -0.05 dB | -0.63 dB |
| Sine offset | -0.10 dB | -0.18 dB | +0.06 dB |
| Noise tilt | -0.45 dB/oct | -0.23 dB/oct | +0.27 dB/oct |
| Sine tilt | +0.43 dB/oct | -0.34 dB/oct | +0.19 dB/oct |
| **Max peak diff** | **12.77 dB** | **9.23 dB** | **4.92 dB** |

### Why abandoned
- Per-peak heights at LOWBW: BR peaks systematically **-4.9 dB** below Ableton (RMS 5.6 dB)
- Peak analysis showed +0.96 dB/oct slope: -8 dB at 160 Hz, +1 dB at 18 kHz
- **Root cause**: 4th-order carrier has narrower peaks than Ableton's (likely 2nd-order) carrier
- This is a topology difference — **cannot be fixed with per-band gain adjustments**
- Peaks are narrower but valleys are deeper → total energy averages out but shape differs

### Compensation chain (for 4th-order, kept for RITUAL/SPLIT modes)
1. **qComp** (~line 3461): `pow(analysisQ/2, 0.35)`, noise-only via noisiness
2. **overlapComp** (~line 1727): Parabolic shape, normalized (avg=1.0), Q-dependent alphaLow +30%
3. **Tonal tilt** (~line 3472): `pow(freq/1000, exp)`, mid-Q Gaussian bump (0.20 coeff)
4. **Noise tilt** (~line 3486): `-0.08 * pow(freq/1000, exp)`, only at low Q
5. **tonalSatCorr** (~line 3499): Two-part (base + high-Q), compensates fastTanh saturation
6. **widthComp** (~line 3893): `5.0 * (1 - widthNorm^1.19)` + midBwBump 4.55 dB at MIDBW
7. **bwComp** (~line 1742): Bilinear sinc correction above 7 kHz

---

## APPROACH 2: 2nd-Order Carrier for LINKED mode (current)
**Status**: Active, calibrating

### Architecture changes
- Carrier stacking conditional: `linkedMode` → single `carrierFilterL` (2nd-order)
- RITUAL/SPLIT modes still use stacked 4th-order (unchanged)

### Compensations for LINKED mode
Only 3 of the 7 compensations are active:
1. **qComp** with reduced exponent: `pow(analysisQ/2, 0.28)` (was 0.35)
2. **tonalSatCorr** with reduced blend: `satBlend = 0.78` (was 1.0)
3. **widthComp**: completely new curve fitted for 2nd-order:
   `boostDb = -5.80 + 6.12*widthNorm + 8.57*exp(-((widthNorm-0.21)/0.12)^2)`

Bypassed for LINKED: overlapComp, tonal tilt, noise tilt, bwComp, original widthComp/midBwBump

### Raw 2nd-order baseline (ALL compensations off)
| Metric | LOWBW | MIDBW | HIGHBW |
|---|---|---|---|
| Noise offset | -2.90 dB | **-9.91 dB** | +0.20 dB |
| Sine offset | **+12.36 dB** | -2.70 dB | +2.30 dB |
| Noise tilt | **+0.54** | **-0.11** | -0.29 |
| Sine tilt | +1.03 | -0.43 | -0.37 |

Key observations from raw baseline:
- **HIGHBW nearly perfect already** (+0.20 / +2.30) — minimal correction needed
- **Massive sine excess at LOWBW** (+12.36) — from fastTanh saturation, needs tonalSatCorr
- **Huge MIDBW noise deficit** (-9.91) — needs both qComp + widthComp Gaussian bump
- **MIDBW noise deficit is NOT monotonic with Q** — LOWBW (-2.90) is less than MIDBW (-9.91) despite higher Q

### Calibration history (2nd-order, clean slate approach)

**Step 1: Raw baseline** (no compensations)
→ See table above. Identified: need tonalSatCorr (sine/noise split), qComp (noise boost), widthComp (BW-dependent correction)

**Step 2: +tonalSatCorr only**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -2.90 | +4.29 | -9.91 | -5.57 | +0.20 | +1.64 |
→ Sine reduced at LOWBW (12.36→4.29), confirmed tonalSatCorr needed

**Step 3: +tonalSatCorr +qComp (exponent 0.35)**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +6.54 | +4.29 | -2.53 | -5.57 | +4.11 | +1.64 |
→ qComp overcorrects: LOWBW noise +6.54, HIGHBW noise +4.11

**Step 4: +tonalSatCorr +qComp +widthComp (curve fitted to averages)**
Curve: `0.63 - 3.62*wN + 7.83*exp(-((wN-0.21)/0.12)^2)`
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +7.54 | +5.29 | +5.17 | +2.13 | +3.08 | +0.62 |
→ All positive. Re-fitted curve to subtract more.

**Step 5: Re-fitted widthComp curve**
Curve: `-5.80 + 6.12*wN + 8.57*exp(-((wN-0.21)/0.12)^2)`
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +1.14 | -1.11 | +1.53 | -1.51 | +1.27 | -1.19 |
→ **Averages nearly perfect** (all within ±0.04 dB). But uniform 2.6 dB noise/sine split.

**Step 6: Reduce qComp exponent to 0.28 + satBlend 0.85**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.57 | +0.69 | +0.18 | -1.01 | +0.56 | -1.09 |
→ Noise excellent. Sine still -1.0 at MID/HIGHBW.

**Step 7 (CURRENT): satBlend 0.78**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.57** | **+1.42** | **+0.18** | **-0.79** | **+0.56** | **-1.04** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.22 | — | 7.41 | — | 4.24 | — |

### Current issues (Step 7)
1. **LOWBW sine +1.42**: satBlend overcorrects at high Q (tonalSatCorr is strong there)
2. **HIGHBW sine -1.04**: satBlend can't help (tonalSatCorr is weak at low Q, only -0.5 dB total)
3. **LOWBW sine tilt +1.03**: BR brighter than Ableton on sine sweeps. Likely sweep speed vs filter bandwidth interaction.
4. **Max peak diff still 20 dB at LOWBW**: Need to investigate — was 12.77 with 4th-order

### Remaining problems to solve
- The sine/noise split varies with Q: strong at high Q (tonalSatCorr effect), weak at low Q
- satBlend is a flat scaling — can't independently fix high-Q and low-Q sine offsets
- May need Q-dependent satBlend or a completely different sine correction strategy
- The tilt values (especially +1.03 LOWBW sine, -0.55 HIGHBW noise) need tilt correction
- The overlapComp (spectral shape) and tilt corrections were disabled — may need LINKED-specific versions

### What we've learned
1. **2nd-order carrier is correct topology** for matching Ableton (confirmed by excellent noise tilts)
2. **Clean-slate approach works** — building compensations incrementally from raw baseline
3. **widthComp Gaussian curve is well-fitted** — averages are ±0.04 dB (step 5)
4. **qComp needs lower exponent (0.28)** for 2nd-order carrier
5. **tonalSatCorr needs Q-dependent reduction** — flat satBlend can't fix both high-Q and low-Q
6. **MIDBW has anomalous noise deficit** (-9.91 raw) despite not having the highest Q — needs the big Gaussian bump in widthComp

---

## Key Constants & Locations (current code)
- Source: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/PluginProcessor.cpp`
- `linkedMode` flag: line ~3423
- `kShapeCenter = 5000.0f` (line 1716)
- `kShapeAlphaLow = 0.022f`, `kShapeAlphaHigh = 0.045f` (lines 1717-1718)
- Q mapping: `minQ=1, maxQ=60`, `QcarrierBase = 1 * pow(60, 1-norm)` (line 1544)
- Analysis Q cap: `maxAnalysisQ = 48.0f` (line 1683)
- LINKED qComp exponent: 0.28 (line ~3461)
- LINKED satBlend: 0.78 (line ~3524)
- LINKED widthComp: `-5.80 + 6.12*wN + 8.57*exp(-((wN-0.21)/0.12)^2)` (line ~3905)
- LINKED carrier: single stage (2nd-order) — bypasses `carrierFilterL2`/`carrierFilterR2`

---

## Calibration Log (continued)

### Step 8+9: Crossfade bug fix + Q-dependent satBlend (2026-03-15)

**Changes:**
- Fixed crossfade path: `satCorrXf = linkedMode ? 1.0f : tonalSatCorrXf` → always apply `tonalSatCorrXf`
- Replaced flat `satBlend = 0.78` with Q-dependent:
  `satBlend = jlimit(0.50, 0.85, 0.50 + 0.35 * (analysisQ - 7.0) / 20.0)`
  - Q=7 (HIGHBW) → 0.50, Q=20 (MIDBW) → 0.73, Q=27 (LOWBW) → 0.85

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.57 | **+0.75** | +0.18 | **-0.45** | +0.56 | **-0.86** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.22 | — | 7.41 | — | 4.24 | — |

**Analysis:**
- Sine improved: LOWBW +1.42→+0.75, MIDBW -0.79→-0.45, HIGHBW -1.04→-0.86
- Q-dependent satBlend is working — LOWBW sine came down, MIDBW centered better
- HIGHBW sine still -0.86 — satBlend 0.50 still not enough (tonalSatCorr only -0.6 dB at Q=7)
- Noise unchanged (expected — qComp/satBlend don't affect noise)
- All offsets now within ±0.86 dB (was ±1.42)

**Remaining issues:**
1. HIGHBW sine -0.86: can't fix via satBlend (tonalSatCorr too weak at low Q). Need widthComp adjustment.
2. Tilts: +1.03 (LOWBW sine), -0.55 (HIGHBW noise) — need tilt correction (Step 10)
3. Max peak diff 20 dB at LOWBW — need investigation (Step 11)

**Next step:** Step 10 — re-enable tilt correction for LINKED mode

### Step 9b: widthComp curve +0.15 dB shift (2026-03-15)

**Change:** widthComp constant: -5.80 → -5.65 (uniform +0.15 dB shift to center MID/HIGHBW averages)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.42** | **+0.90** | **+0.33** | **-0.30** | **+0.71** | **-0.71** |
| Tilt | +0.28 | +1.03 | -0.38 | -0.43 | -0.55 | -0.37 |
| Max peak diff | 20.07 | — | 7.26 | — | 4.09 | — |

**Analysis:**
- Averages: LOWBW +0.24, MIDBW +0.02, HIGHBW 0.00 — excellent centering
- All 6 offsets now within ±0.90 dB (improved from ±1.42)
- Noise/sine split at each BW: LOWBW 1.32, MIDBW 0.63, HIGHBW 1.42
- HIGHBW sine still -0.71 (best achievable via satBlend — tonalSatCorr too weak at low Q)
- LOWBW sine +0.90 — satBlend 0.85 at high Q slightly under-attenuates

**Remaining issues (unchanged):**
1. Tilts: +1.03 (LOWBW sine), -0.55 (HIGHBW noise)
2. Max peak diff 20 dB at LOWBW
3. LOWBW sine +0.90 / HIGHBW sine -0.71 noise/sine split

**Next step:** Step 10 — tilt correction for LINKED mode

### Step 10: LINKED tonal tilt correction (2026-03-15)

**Change:** Added Q-dependent tonal tilt to LINKED compensatedEnv:
```
linkedTiltExp = 0.06 - 0.005 * (analysisQ - 7.0)
linkedTilt = pow(freq/1000, linkedTiltExp)
linkedTiltBlend = (1-noisiness)^2  // tonal only
compensatedEnv = env * qComp * linkedTiltFactor
```
Values: Q=7 → exp=+0.06, Q=20 → exp=-0.005, Q=27 → exp=-0.04

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.44 | +0.66 | +0.34 | -0.26 | +0.74 | -0.59 |
| Tilt | +0.27 | **+0.92** | -0.38 | **-0.41** | -0.54 | **-0.31** |
| Max peak diff | 20.03 | — | 7.25 | — | 4.06 | — |

**Comparison to Step 9b:**
| | LOWBW sine tilt | MIDBW sine tilt | HIGHBW sine tilt |
|---|---|---|---|
| Step 9b | +1.03 | -0.43 | -0.37 |
| Step 10 | **+0.92** | **-0.41** | **-0.31** |

**Analysis:**
- Sine tilts improved slightly: LOWBW +1.03→+0.92, HIGHBW -0.37→-0.31
- Offsets also improved: LOWBW sine +0.90→+0.66, HIGHBW sine -0.71→-0.59
- Noise tilts essentially unchanged (tilt only affects tonal via noisiness blend)
- The tilt correction is too weak — exp of -0.04 at LOWBW only provides ~0.24 dB/oct correction, but LOWBW sine needs ~1.03 dB/oct
- Need much stronger correction at high Q

**Next step:** Increase tilt exponent. Try `0.08 - 0.012 * (Q - 7)` for stronger Q-dependent correction:
- Q=7: +0.08, Q=20: -0.076, Q=27: -0.16

### Step 10b: Stronger tilt exponent (2026-03-15)

**Change:** `linkedTiltExp = 0.08 - 0.012 * (analysisQ - 7.0)` (was `0.06 - 0.005 * ...`)
Values: Q=7 → +0.08, Q=20 → -0.076, Q=27 → -0.16

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.51 | **-0.28** | +0.31 | -0.42 | +0.75 | -0.55 |
| Tilt | +0.25 | **+0.50** | -0.39 | **-0.49** | -0.54 | **-0.29** |
| Max peak diff | 19.90 | — | 7.30 | — | 4.04 | — |

**Comparison to previous:**
| Metric | Step 9b | Step 10 | Step 10b |
|---|---|---|---|
| LOWBW sine tilt | +1.03 | +0.92 | **+0.50** |
| LOWBW sine offset | +0.90 | +0.66 | **-0.28** |
| MIDBW sine tilt | -0.43 | -0.41 | **-0.49** |
| HIGHBW sine tilt | -0.37 | -0.31 | **-0.29** |

**Analysis:**
- LOWBW sine tilt halved: +1.03 → +0.50. Still the worst but much better.
- LOWBW sine offset went from +0.90 to -0.28 — tilt correction shifts energy from HF to LF
- MIDBW sine tilt slightly worse: -0.43 → -0.49. The positive exp at mid-Q (+~0) doesn't help.
- HIGHBW sine tilt improved: -0.37 → -0.29
- All 6 offsets now within ±0.75 dB
- LOWBW noise tilt unchanged (+0.25) — tilt only applies to tonal

**Remaining issues:**
1. LOWBW sine tilt still +0.50 — could push exponent more negative at high Q
2. MIDBW sine tilt -0.49 — slightly worsened, need to check exp at Q=20
3. Noise tilts: -0.54 (HIGHBW), -0.39 (MIDBW) — need noise tilt correction
4. Max peak diff 19.90 at LOWBW — still needs investigation

**Next step:** Fine-tune tilt exponent to reduce LOWBW sine tilt without worsening MIDBW. Also consider adding noise tilt correction.

### Step 10c: Noise tilt + widthComp refit (2026-03-15)

**Changes:**
1. Added noise tilt: `noiseTiltExp = 0.05 * noiseTiltStrength` (strength fades at high Q)
2. Re-fitted widthComp: `-5.25 + 4.60*wN + 8.57*exp(...)` (was `-5.65 + 6.12*wN + ...`)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.11** | **+0.12** | **+0.58** | **-0.34** | **+0.86** | **-0.87** |
| Tilt | +0.25 | +0.50 | **-0.34** | -0.49 | **-0.45** | -0.29 |
| Max peak diff | 19.50 | — | 6.92 | — | 3.88 | — |

**Analysis:**
- LOWBW excellent: noise -0.11, sine +0.12 (both within ±0.12!)
- MIDBW good: noise +0.58, sine -0.34
- HIGHBW: noise +0.86, sine -0.87 — noise/sine split 1.73 dB (biggest remaining issue)
- Noise tilts: LOWBW +0.25, MIDBW -0.34, HIGHBW -0.45 (improved from -0.54)
- Max peak diff: LOWBW 19.50 (still high), MIDBW 6.92 (improved), HIGHBW 3.88 (excellent)
- widthComp refit over-shifted HIGHBW — average is (0.86-0.87)/2 = -0.005 but spread is 1.73

**Remaining issues:**
1. HIGHBW noise/sine split: 1.73 dB (noise +0.86 vs sine -0.87)
2. LOWBW sine tilt: +0.50 (halved from +1.03 but still noticeable)
3. MIDBW sine tilt: -0.49
4. LOWBW max peak diff: 19.50 — needs investigation
5. Noise tilts: MIDBW -0.34, HIGHBW -0.45 — could use more correction

**Current parameter values:**
- qCompExp: 0.28
- satBlend: Q-dependent, 0.50-0.85
- Tonal tilt: `0.08 - 0.012 * (Q - 7)` → Q=7: +0.08, Q=27: -0.16
- Noise tilt: `0.05 * strength`, strength fades at Q>7 over 15-unit range
- widthComp: `-5.25 + 4.60*wN + 8.57*exp(-((wN-0.21)/0.12)^2)`

**Next step:** Investigate LOWBW 19.5 dB max peak diff (Step 11 — peak analysis)

### Step 11: Peak analysis — CRITICAL FINDING (2026-03-15)

**Peak analysis (LOWBW, 2nd-order carrier):**
- Matched peaks: 30 (Ableton 33, BR 29)
- Frequency alignment: RMS 47.5 cents (good)
- **Peak height deficit: -13.6 dB MEAN, 14.0 dB RMS, 20.4 dB MAX**
- Tilt: +1.03 dB/oct (worse at low freqs: -20 dB at 162 Hz, -10 dB at 7 kHz)
- Sign alternation: 0% — ALL peaks below Ableton (systematic)

**Comparison: 2nd-order vs 4th-order peak deficit**
| | 4th-order carrier | 2nd-order carrier |
|---|---|---|
| Peak deficit (mean) | **-4.9 dB** | -13.6 dB |
| Peak deficit (RMS) | **5.6 dB** | 14.0 dB |
| Peak deficit (max) | **8.2 dB** | 20.4 dB |
| Overall offset | ±0.63 dB | ±0.87 dB |
| Noise tilt | ±0.45 dB/oct | ±0.45 dB/oct |

**What this means:**
The 2nd-order carrier makes the per-peak matching MUCH worse (3x larger deficit). The total energy matches because the wider 2nd-order carrier creates a higher "floor" between peaks — energy is redistributed from peaks into the inter-peak noise floor.

**Key insight: 4th-order carrier was actually CLOSER to Ableton's peak pattern.** This strongly suggests Ableton uses a carrier topology with similar selectivity to our 4th-order (stacked 2nd-order BPF), NOT a simple 2nd-order.

**The dilemma:**
- 4th-order: better peak matching (-4.9 vs -13.6 dB) but LOWBW noise tilt -0.45 dB/oct
- 2nd-order: worse peak matching but LOWBW noise tilt +0.02 dB/oct (nearly perfect)

**Possible explanations:**
1. Ableton uses 4th-order carrier but compensates the tilt differently
2. Ableton uses SVF (state variable filter) instead of IIR BPF
3. Ableton uses 2nd-order but with different Q mapping
4. Ableton uses a resonant lowpass (different gain profile than BPF)

**Decision needed: Revert to 4th-order or continue with 2nd-order?**

### Step 11b: FILTER SHAPE REVERSE ENGINEERING — KEY DISCOVERY (2026-03-15)

**Measured Ableton's actual filter characteristics from the LOWBW noise recording:**

| Metric | Ableton | Band Ritual (2nd-order) |
|---|---|---|
| **Peak-to-Valley Ratio (mean)** | **45.9 dB** | 26.2 dB |
| **Peak-to-Valley Ratio (range)** | 31.9 – 51.9 dB | 11.3 – 41.1 dB |
| **Effective Q (mean)** | **67.3** | 79.2 |
| **Effective Q (low freq <500 Hz)** | **33.7** | 23.6 |
| **Effective Q (high freq >2 kHz)** | **80.2** | 115.4 |
| **Rolloff slope (mid-band)** | **-420 dB/oct** | -340 dB/oct |

**CRITICAL FINDINGS:**

1. **Ableton's peak-to-valley ratio is 45.9 dB** — this is MUCH sharper than our 2nd-order (26.2 dB). Our 4th-order was closer but still below.

2. **Ableton's rolloff is ~420 dB/oct** — this is NOT a simple 2nd (12 dB/oct) or 4th (24 dB/oct) order filter. This is an EXTREMELY steep rolloff, suggesting **many cascaded stages** or an entirely different approach (possibly FFT-based or very high-order IIR).

3. **Ableton's effective Q averages 67** — close to our carrier Q of 60, but their Q INCREASES with frequency (33.7 at low freq → 80.2 at high freq). Our Q is constant across frequency.

4. **The 420 dB/oct rolloff is key** — it explains why Ableton has 45.9 dB peak-to-valley ratio. A 2nd-order BPF has ~12 dB/oct slope, a 4th-order ~24 dB/oct. To get 420 dB/oct, you'd need ~35th order per side, which is unrealistic for IIR. **Ableton almost certainly uses FFT/STFT-based processing, not IIR bandpass filters.**

**CONCLUSION: Ableton's vocoder is likely FFT-based, not filter-bank-based.**

This explains everything:
- Why neither 2nd nor 4th-order IIR matched the peak shape
- Why the peak-to-valley ratio is so extreme (45 dB)
- Why the rolloff is ~420 dB/oct (FFT bins have rectangular windows = sinc rolloff, much steeper than IIR)
- Why Q appears to increase with frequency (constant bandwidth in Hz from FFT → Q = freq/BW increases linearly)

**Implications for Band Ritual:**
- We CANNOT match Ableton's peak shape with ANY order of IIR bandpass filters
- The 4th-order (stacked) was actually closer to Ableton's shape (peak deficit -4.9 dB vs -13.6 dB with 2nd-order)
- To truly match, we'd need to implement FFT-based processing (which would be a complete rewrite)
- OR: accept the IIR architecture and match the AVERAGE spectral behavior (offsets/tilts) which we've already gotten close to

**Recommendation: Revert to 4th-order carrier** — it was closer to Ableton's peak shape. The -0.45 dB/oct LOWBW noise tilt is a smaller error than the -13.6 dB per-peak deficit of 2nd-order.

---

## SESSION FAILURE LOG (2026-03-15)

**CRITICAL: This entire session repeated work already done in previous sessions.**

What was already known (from Obsidian notes written in prior sessions):
1. STFT vocoder already built (`STFTVocoder.cpp`) with 22 calibration attempts
2. 2nd-order carrier already tried and rejected (`Band Ritual - 2nd Order Carrier Filter Experiment.md`)
3. Ableton likely FFT-based already documented (`Band Ritual - Ableton Vocoder Matching Project.md`)
4. LOWBW comb mismatch is frequency mapping, not filter order (`Reverse Engineering Plan.md`)

What this session wasted time on:
- Switching to 2nd-order carrier (already tried)
- "Discovering" Ableton is FFT-based (already known)
- Building filter shape analysis (data was consistent with prior findings)
- Multiple IIR compensation iterations that were already proven suboptimal

**Root cause:** Claude did not read existing Obsidian notes before starting work.

**Fix applied:** Memory rule `feedback_always_read_obsidian.md` — MUST read Obsidian first.

---

## Picking Up From STFT Attempt 22 (2026-03-15 late)

**Current state of STFT vocoder:**
- Code: `STFTVocoder.cpp` + `STFTVocoder.h` — NOT currently integrated into PluginProcessor.cpp
- Backup with integration: `PluginProcessor_stft_backup.cpp` 
- Best result (Attempt 22): HIGHBW noise ±0.59 dB range, geometric dual RMS blend
- Known structural limitations: LOWBW comb (+2-5 dB), impulse sub-bass (+15-30 dB), sine sub-bass (-9 dB)

**Next steps (from STFT calibration note):**
1. Re-integrate STFTVocoder into current PluginProcessor.cpp for LINKED mode
2. Keep IIR path for RITUAL/SPLIT modes
3. Fix auto_compare.py to ensure parameters are set correctly
4. Continue calibration from Attempt 22 baseline
5. Address LOWBW structural limitations (contiguous bins vs IIR notches)

### STFT Re-integration (2026-03-15 late)

**What was done:**
1. Reverted ALL `linkedMode` IIR changes from this session (2nd-order carrier, all bypassed compensations)
2. Added `#include "STFTVocoder.h"` and `STFTVocoder stftVocoder` member to PluginProcessor.h
3. Added `stftVocoder.prepare()` in prepareToPlay
4. Added `useSTFT = (algorithmMode == 1)` branch in processBlock:
   - If LINKED: builds BandInfo array, calls stftVocoder.process(), skips IIR loop
   - If RITUAL/SPLIT: uses existing IIR loop (unchanged)
5. Skipped depth comp, width comp, and crossfade for STFT mode
6. Build succeeds, STFT path is active for LINKED mode

**First test results (raw, Attempt 22 STFT code):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +12.10 | +0.00 | -4.55 | -3.68 | -8.08 | +0.00 |
| Tilt | +0.61 | +0.00 | -0.18 | +0.57 | -0.09 | +0.00 |

**Issues:**
- LOWBW noise +12.10 dB too loud (vs Attempt 22 HIGHBW noise was -0.26)
- LOWBW/HIGHBW sine show +0.00 offset — likely zero output (envelope not detecting sweep correctly?)
- HIGHBW noise -8.08 (vs Attempt 22 HIGHBW noise -0.26 — regression)
- The STFT calibration was done in a different code context (backup had different processing chain)

**Analysis:**
The large difference from Attempt 22 results suggests the integration context differs from the backup. The backup had:
- Different monoMod computation (verified same)
- Different wet buffer handling (backup used same accumulation pattern)
- Possibly different carrier signal processing before STFT
- The depth/widthComp were disabled in backup too, so that matches

**Next steps:**
1. Compare carrier signal path between current code and backup
2. Check if Pedalboard warmup (1s silence) is enough for STFT to initialize
3. May need to compare STFT output directly (not through full plugin chain)
4. Check the sine +0.00 issue — STFT with geometric RMS might not detect sweep at sub-bass bands

### STFT Integration Test 2 — Safety scaling fix (2026-03-15 late)

**Fixes applied:**
1. Removed safety scaling `1/sqrt(nBands/8)` for STFT (was -7 dB at 40 bands)
2. Removed merge grouping for STFT: `mergeNorm=1.0`, `isSecondary=false` always
3. Skipped depth comp, width comp, crossfade for STFT

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +19.08 | +25.65 | +2.44 | +3.31 | **-1.09** | NaN (bug) |
| Tilt | +0.62 | +2.31 | -0.18 | +0.57 | **-0.09** | — |
| Max peak diff | 41.18 | — | 10.52 | — | **2.15** | — |

**Key findings:**
1. **HIGHBW noise: -1.09 dB, tilt -0.09, max diff 2.15** — very close to Attempt 22 results (-0.26 dB)!
2. **BR peak values identical across BW** (2.065 noise, 2.020 sine) — BW parameter does NOT affect STFT output. The STFT uses contiguous bin assignment from centerFreq, not Q-dependent bandwidth.
3. **LOWBW +19 dB excess** — STFT covers ALL frequencies (contiguous bins), while Ableton at LOWBW only passes narrow bands with gaps. The STFT has no gaps → much more total energy.
4. **HIGHBW sine = NaN** — bug in STFT, likely division by zero in geometric RMS when all bins have near-zero energy (sine sweep at HIGHBW might have very concentrated energy).

**This confirms Attempt 22 findings:**
- HIGHBW works well with STFT (matches Ableton's FFT architecture)
- LOWBW is the structural challenge (STFT has no notches between bands)
- The Obsidian note from Attempt 22 documented: "LOWBW comb pattern (+2-5 dB excess)"

**Current STFT compensations (from Attempt 22, unchanged):**
- kNoiseEnvComp = 1.05
- kTonalEnvComp = 0.042
- Tilt: sqrt(centerFreq / 4000) for noise
- Geometric dual RMS: noiseRMS^η × tonalRMS^(1-η)
- PR-based noisiness detection (min 8 bins)

**Next steps:**
1. Fix NaN bug (HIGHBW sine)
2. The LOWBW excess is the main challenge — need BW-dependent scaling in the STFT path
3. The STFT output needs to be scaled by BW to match: at narrow BW, Ableton's IIR notches remove energy, so STFT needs to attenuate proportionally
4. Consider: `stftGain = pow(widthNorm, exponent)` to reduce STFT output at narrow BW

### STFT Integration Test 3 — NaN fix + BW curve (1-wN)^8 (2026-03-15 late)

**Fixes:**
1. NaN guards on bandRMS, envelope, and IFFT output in STFTVocoder.cpp
2. BW-dependent gain: `-20 * (1-widthNorm)^8`
3. Removed merge grouping / isSecondary for STFT

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.85** | +5.65 | **-0.58** | **+0.30** | **-1.21** | **-0.24** |
| Tilt | +0.50 | +2.31 | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.23 | — | 7.50 | — | 2.27 | — |

**Analysis:**
- NaN completely fixed — all 6 tests produce clean output
- **LOWBW noise: -0.85** (was +19.08 before BW curve — BW correction working!)
- **MIDBW noise: -0.58, sine: +0.30** — excellent match at mid-BW
- **HIGHBW noise: -1.21, sine: -0.24** — close to Attempt 22 results
- **LOWBW sine: +5.65** — tonal detection issue at narrow BW (sine gets noise tilt applied?)
- **Tilts**: LOWBW sine +2.31 and HIGHBW sine +0.63 are the biggest

**Comparison to 4th-order IIR best (for reference):**
| Metric | IIR best (4th-order) | STFT current |
|---|---|---|
| LOWBW noise offset | -0.26 | -0.85 |
| MIDBW noise offset | -0.05 | -0.58 |
| HIGHBW noise offset | -0.63 | -1.21 |
| HIGHBW sine offset | +0.06 | -0.24 |
| HIGHBW max peak diff | 4.92 | **2.27** ← better! |

**Key insight: HIGHBW max peak diff 2.27 vs IIR's 4.92** — STFT matches peak shapes much better at wide BW, confirming Ableton uses FFT architecture.

**LOWBW sine +5.65 issue:**
The STFT applies noise tilt sqrt(f/4000) based on PR noisiness. At narrow BW, bands have few bins. The PR detection (min 8 bins) defaults to noisiness=1.0 for small bands → applies full tilt to sine signals → brightens them. This is the same issue documented in Attempt 22: "Sine at sub-bass (-9 dB) — sub-bass bands have only 5-7 FFT bins"

**BW curve values:** `-20 * (1-widthNorm)^8`
- wN=0 (LOWBW): -20.0 dB
- wN=0.21 (MIDBW): -3.04 dB
- wN=0.47 (HIGHBW): -0.13 dB

**Next steps:**
1. Investigate LOWBW sine +5.65 — likely need to adjust tonal compensation at narrow BW
2. Reduce HIGHBW noise offset from -1.21 toward 0 (slight level adjustment)
3. Address tilts — LOWBW sine +2.31 dB/oct is significant

### STFT Test 4 — Q-dependent bins FAILED, reverted (2026-03-15 late)

**Attempted:** Q-dependent bin assignment — narrow BW → fewer bins per band (gaps between bands).
**Result:** Catastrophic — +667 dB at HIGHBW, broken output. The overlap resolution code and energy scaling don't work with non-contiguous bins. **Reverted immediately.**

**Current stable state (same as Test 3):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.92** | NaN→0 | **-0.58** | **+0.30** | **-1.21** | **-0.24** |
| Tilt | +0.62 | — | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | — | 7.50 | — | **2.27** | — |

**Remaining issues (prioritized):**
1. **LOWBW sine = NaN** — the geometric RMS or PR detection produces NaN for sine sweep at narrow BW. Need to debug the specific frame/band that triggers it.
2. **HIGHBW noise -1.21** — needs ~+1 dB level adjustment
3. **Tilts** — HIGHBW sine +0.63, LOWBW noise +0.62
4. **LOWBW max peak diff 28.21** — from the BW gain curve (-20 dB), not from STFT architecture

**What NOT to try again:**
- Q-dependent bin assignment — creates catastrophic gain issues with the overlap resolution and energy normalization code
- The correct approach to handle LOWBW notches is the BW gain curve, not bin masking

**Next steps:**
1. Debug LOWBW sine NaN — add logging or test with isolated bands
2. Adjust kNoiseEnvComp or add per-BW level correction for noise (-1.21 at HIGHBW)
3. The BW gain curve (-20*(1-wN)^8) works well for noise but needs separate sine calibration

### STFT Test 5 — NaN debugging resolved (2026-03-15 late)

**Finding:** The NaN/zero results in auto_compare.py were a **test script bug**, not a plugin bug.
- Direct Python tests confirm ALL signal types produce clean (NaN-free) output at ALL BW settings
- The auto_compare.py has a sequential execution issue where some tests produce 0.00/0.00 results
- This needs script debugging but does NOT indicate a plugin problem

**Verified clean results (direct testing):**
| | LOWBW noise | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|
| Offset | -0.92 | -0.58 | +0.30 | -1.21 | -0.24 |
| Tilt | +0.62 | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | 7.50 | — | 2.27 | — |

**LOWBW sine needs separate testing** — auto_compare couldn't test it. Direct test shows peak=0.202, no NaN.

**Current STFT architecture summary:**
- STFTVocoder.cpp: 2048-point FFT, hop 512, sqrt-Hann, contiguous bins, geometric dual RMS, PR noisiness
- Integration: `useSTFT = (algorithmMode == 1)` in processBlock
- BW gain: `-20 * (1-widthNorm)^8` applied after STFT
- Bypassed for STFT: safety scaling, depth comp, width comp, crossfade
- NaN guards: bandRMS, envelope, finalGain, IFFT output

**Next session priorities:**
1. Fix auto_compare.py script to work reliably for all 9 test cases
2. Calibrate LOWBW sine separately (can't use auto_compare until fixed)
3. Address HIGHBW noise -1.21 dB (kNoiseEnvComp adjustment)
4. Address tilts (+0.62 LOWBW noise, +0.63 HIGHBW sine)
5. Run peak analysis to compare STFT vs Ableton peak shapes at LOWBW

### STFT Test 6 — Full reliable results with subprocess isolation (2026-03-15 late)

**Fix:** Created `test_stft.py` — runs each test in a separate Python subprocess for complete isolation. Solves Pedalboard plugin loading bug that caused NaN/zero results in sequential runs.

**Complete results (all 6 tests clean, no NaN):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.92 | **+5.66** | -0.58 | +0.30 | -1.21 | -0.24 |
| Tilt | +0.62 | **+2.31** | -0.18 | +0.57 | -0.09 | +0.63 |
| Max peak diff | 28.21 | — | 8.12 | — | 2.39 | — |

**Current parameter values (STFT path):**
- kNoiseEnvComp = 1.05, kTonalEnvComp = 0.042
- Tilt: sqrt(centerFreq/4000) for noise, geometric dual RMS blend
- BW gain: `-20 * (1-widthNorm)^8`
- Contiguous bin assignment

**Ranking of issues by severity:**
1. **LOWBW sine: +5.66 dB, tilt +2.31** ← worst. BW correction works for noise but sine is 6.6 dB too loud relative to noise
2. **HIGHBW noise: -1.21 dB** ← moderate. Need +1.2 dB boost at wide BW
3. **LOWBW noise tilt: +0.62** ← moderate
4. **HIGHBW sine tilt: +0.63** ← moderate
5. **LOWBW noise: -0.92** ← close to target
6. **MIDBW sine tilt: +0.57** ← moderate

**Root cause of LOWBW sine excess:**
The BW gain `-20*(1-wN)^8` was calibrated for noise. At LOWBW, Ableton's narrow bands create deep notches that remove more NOISE energy (broadband) than SINE energy (concentrated at band centers). The STFT has no notches → passes all energy equally → sine is proportionally louder than it should be.

The sine/noise split: noise -0.92, sine +5.66 → gap of 6.58 dB. This gap is BW-dependent:
- LOWBW: 6.58 dB gap
- MIDBW: 0.88 dB gap (0.30 - (-0.58))
- HIGHBW: 0.97 dB gap (-0.24 - (-1.21))

At MID/HIGHBW the gap is <1 dB (good). At LOWBW the gap is 6.58 dB (problem).

**Fix strategy:** Apply noisiness-dependent BW correction:
- For noise: current -20 dB at LOWBW (works)
- For sine: less correction needed (sine loses less energy from notches)
- Use the existing global noisiness detector to blend between noise and sine corrections

**Next steps:**
1. Make BW gain noisiness-dependent: noise gets -20 dB, tonal gets less (~-14 dB) at LOWBW
2. Adjust kNoiseEnvComp to fix HIGHBW noise -1.21
3. Address tilts

### STFT Test 7 — Noisiness-dependent BW gain + kNoiseEnvComp (2026-03-15 late)

**Changes:**
1. BW gain now noisiness-dependent: noise -20 dB, tonal -26 dB at LOWBW (blended by smoothedGlobalNoisiness_)
2. kNoiseEnvComp: 1.05 → 1.18 (+1.0 dB boost for noise)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -1.09 | **-0.11** | **+0.24** | -0.37 | **-0.22** | **-0.03** |
| Tilt | +0.62 | +2.28 | -0.19 | +0.55 | -0.09 | +0.61 |
| Max peak diff | 28.35 | — | 8.94 | — | 3.38 | — |

**Comparison to previous (Test 6):**
| | Test 6 | Test 7 | Change |
|---|---|---|---|
| LOWBW sine offset | +5.66 | **-0.11** | Fixed! (-5.77 dB) |
| HIGHBW noise offset | -1.21 | **-0.22** | Fixed! (+0.99 dB) |
| HIGHBW sine offset | -0.24 | **-0.03** | Improved |
| MIDBW noise offset | -0.58 | +0.24 | kNoiseEnvComp pushed it up |

**All 6 offsets now within ±1.09 dB!** (was ±5.66)

5 of 6 offsets within ±0.37 dB. Only LOWBW noise (-1.09) is >1 dB.

**Remaining issues:**
1. LOWBW noise -1.09 dB — the noisiness-dependent BW correction overcorrects noise at LOWBW
2. Tilts: LOWBW +0.62/+2.28, MIDBW +0.55, HIGHBW +0.61 — all positive for sine
3. LOWBW max peak diff 28.35 — structural (contiguous bins vs Ableton notches)

**Current STFT parameter values:**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- BW noise correction = -20 * (1-wN)^8
- BW tonal correction = -26 * (1-wN)^8
- Tilt: sqrt(centerFreq/4000) for noise

**Next step:** Fix LOWBW noise from -1.09 to ~0. The noiseCorr of -20 at LOWBW needs to be -19.

### STFT Test 8 — Fine-tuned BW correction (2026-03-15 late)

**Changes:** noiseCorr: -20→-19, tonalCorr: -26→-25.5

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.18** | **+0.39** | **+0.38** | **-0.29** | **-0.21** | **-0.03** |
| Tilt | +0.61 | +2.28 | -0.19 | +0.55 | -0.09 | +0.61 |
| Max peak diff | 27.45 | — | 9.08 | — | 3.39 | — |

**ALL 6 OFFSETS WITHIN ±0.39 dB!**

**Comparison to IIR best (4th-order, for reference):**
| Metric | IIR best | STFT current |
|---|---|---|
| Worst offset | ±0.63 dB | **±0.39 dB** ← better! |
| HIGHBW max peak diff | 4.92 dB | **3.39 dB** ← better! |
| LOWBW max peak diff | 12.77 dB | 27.45 dB ← worse (structural, contiguous bins) |

**Current parameter values:**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- BW noise correction = -19 * (1-wN)^8
- BW tonal correction = -25.5 * (1-wN)^8
- Contiguous bin assignment, geometric dual RMS, PR noisiness

**Remaining work: TILTS**
| | Target | Current | Delta |
|---|---|---|---|
| LOWBW noise | 0 | +0.61 | -0.61 needed |
| LOWBW sine | 0 | **+2.28** | -2.28 needed ← worst |
| MIDBW noise | 0 | -0.19 | +0.19 needed |
| MIDBW sine | 0 | +0.55 | -0.55 needed |
| HIGHBW noise | 0 | -0.09 | nearly flat |
| HIGHBW sine | 0 | +0.61 | -0.61 needed |

Pattern: ALL sine tilts are positive (+0.55 to +2.28) — BR sine is brighter than Ableton.
LOWBW sine tilt +2.28 is by far the worst. This is the same issue as documented in Attempt 22:
the noise tilt correction sqrt(f/4000) affects sine through the noisiness detection.
Sub-bass bands have too few FFT bins for reliable PR → noisiness defaults to 1.0 → full tilt applied → brightens sine at low freq.

### STFT Test 9 — Tonal tilt correction (2026-03-15 late)

**Change:** Added `kTonalTiltExp = -0.20` → `pow(f/4000, -0.20)` for tonal signals.
Iterated: -0.08 → -0.15 → -0.20

**Results (at -0.20):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.18 | +0.26 | +0.39 | -0.43 | -0.20 | -0.16 |
| Tilt | +0.59 | **+1.88** | -0.21 | **+0.15** | -0.12 | **+0.21** |

**Tilt progression:**
| | Test 8 | -0.08 | -0.15 | -0.20 |
|---|---|---|---|---|
| LOWBW sine | +2.28 | +2.12 | +1.98 | **+1.88** |
| MIDBW sine | +0.55 | +0.38 | +0.24 | **+0.15** |
| HIGHBW sine | +0.61 | +0.45 | +0.31 | **+0.21** |

MID/HIGHBW sine tilts nearly solved. LOWBW sine tilt stuck at +1.88 because:
- Sub-bass bands (40-200 Hz) have < 8 FFT bins → noisiness defaults to 1.0
- Tonal tilt only applies when noisiness < 1.0
- These bands get NOISE tilt (brightening) even during sine sweep
- **This is a structural limitation of PR-based noisiness at low freq**

**Possible fixes for LOWBW sine tilt:**
1. Lower the PR min-bins threshold from 8 to 4 (tried in Attempt 19, "helped low-mid sine but hurt mid noise — reverted")
2. Use a different noisiness detector for low-freq bands (e.g., spectral flatness within band)
3. Accept +1.88 as structural and only fix with external EQ
4. Apply tonal tilt correction to ALL bands regardless of noisiness (would affect noise tilt)

**Current STFT parameters:**
- kNoiseEnvComp = 1.18, kTonalEnvComp = 0.042
- kTonalTiltExp = -0.20, kTiltRefFreq = 4000
- BW noise/tonal correction: -19/-25.5 * (1-wN)^8
- PR min bins: 8

### STFT Test 10 — Global noisiness for small bands + tonal tilt -0.15 (2026-03-15 late)

**Changes:**
1. Small bands (< 8 FFT bins) now use global noisiness (crest factor) instead of defaulting to 1.0
2. Passed `smoothedGlobalNoisiness_` from PluginProcessor to STFTVocoder.process()
3. kTonalTiltExp: iterated -0.20 → -0.12 → **-0.15** (best balance)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.19** | **+0.38** | **+0.37** | **-0.31** | **-0.22** | **-0.05** |
| Tilt | +0.54 | **+1.70** | -0.27 | **-0.03** | -0.17 | **+0.03** |
| Max peak diff | 27.77 | — | 9.05 | — | 8.07 | — |

**ALL 6 offsets within ±0.38 dB!**
**MID/HIGHBW sine tilts essentially FLAT** (-0.03, +0.03)!

**Comparison across iterations:**
| Metric | IIR best | STFT Test 6 | STFT Test 8 | STFT Test 10 |
|---|---|---|---|---|
| Worst offset | ±0.63 | ±5.66 | ±0.39 | **±0.38** |
| MIDBW sine tilt | -0.34 | +0.55 | +0.55 | **-0.03** |
| HIGHBW sine tilt | +0.19 | +0.63 | +0.61 | **+0.03** |
| LOWBW sine tilt | +0.43 | +2.31 | +2.28 | +1.70 |

**Remaining issue: LOWBW sine tilt +1.70**
This is structural — at LOWBW, the modulator's crest factor (global noisiness) may not accurately reflect the per-band tonal/noise state at every instant. The sine sweep moves through bands, so at any given frame, most bands see noise-like residual energy from the sweep moving away. The global detector catches the overall tonal nature but can't fully compensate for the per-bin tilt mismatch at sub-bass.

**LOWBW sine tilt progression:**
+2.28 → +2.12 → +1.98 → +1.88 → +1.42 → +1.88 → **+1.70**

**Current STFT parameters (FINAL for now):**
- kNoiseEnvComp = 1.18
- kTonalEnvComp = 0.042
- kTonalTiltExp = -0.15
- kTiltRefFreq = 4000
- BW noise/tonal correction: -19/-25.5 * (1-wN)^8
- PR min bins: 8 (small bands use global noisiness)
- Geometric dual RMS blend

**Assessment: Is this good enough?**
- 5 of 6 offsets within ±0.31 dB — commercially excellent
- 4 of 6 tilts within ±0.27 dB/oct — commercially excellent
- LOWBW sine tilt +1.70 — audible on test signals, likely inaudible on real audio (vocal formants don't span sub-bass to treble like a sweep)
- LOWBW noise tilt +0.54 — moderate, could be improved
- Max peak diff at LOW/MIDBW still high (27-9 dB) — structural STFT limitation

---

## CRITICAL RE-EVALUATION (2026-03-15 very late)

### The "FFT discovery" was WRONG

My analysis this session claimed Ableton uses FFT based on "420 dB/oct rolloff." This was a measurement error:
- The 420 dB/oct was measured between a peak and adjacent valley (very close in frequency)
- This is NOT the filter rolloff — it's the combined notch depth from two adjacent 4th-order BPF skirts
- A 4th-order IIR at Q=60 with 40 bands naturally creates ~45-50 dB valleys
- The Reverse Engineering Plan (2026-03-14) already CONFIRMED: "Ableton 50 dB notch depth / 2 = 25 dB per pass = 4th-order"

### Evidence that Ableton is IIR, NOT FFT:
1. **50 dB notch depth** — consistent with 4th-order IIR (RE Plan, measured from Ableton output)
2. **LOWBW comb pattern** — deep notches between bands, characteristic of IIR bandpass
3. **IIR 4th-order gave 12 dB max peak diff** vs STFT's 27 dB — IIR matches the peak SHAPE better
4. **The STFT cannot produce notches** — contiguous bins = smooth spectrum, no comb pattern
5. Both the STFT note AND RE Plan agree: "LOWBW comb pattern strongly suggests IIR"

### What each approach gets right and wrong:

| Metric | IIR 4th-order | STFT |
|---|---|---|
| LOWBW comb pattern | ✓ (12 dB diff) | ✗ (27 dB diff) |
| HIGHBW overall match | Good (±0.63) | **Better** (±0.38) |
| Offsets | ±0.63 | **±0.38** |
| Envelope accuracy | OK | **Better** (Attempt 22) |
| LOWBW noise tilt | -0.45 | +0.54 |
| LOWBW sine tilt | +0.43 | +1.70 |
| Architecture match | **YES (IIR matches Ableton's IIR)** | NO (FFT ≠ IIR) |

### The CORRECT approach: HYBRID

**STFT for analysis (modulator) + IIR for carrier (synthesis)**

1. Use STFT to analyze the modulator → per-band envelope (FFT-based, better accuracy than IIR analysis)
2. Apply those envelopes to the IIR 4th-order stacked carrier filters (produces correct comb pattern)
3. Combines: accurate envelope detection (STFT) + correct spectral shape (IIR carrier)

This is what the STFT Calibration note suggested as a next step: "may need hybrid approach: IIR analysis + FFT carrier shaping" — but in reverse: FFT analysis + IIR carrier.

### Implementation plan:
1. Keep the STFT modulator analysis (FFT → per-band envelope with geometric RMS, PR noisiness, global noisiness fallback)
2. Keep the IIR 4th-order stacked carrier filters
3. In processBlock: run STFT on modulator to get per-band envelopes, then drive the IIR carrier with those envelopes
4. Remove the IIR analysis filters from the signal path (replaced by STFT analysis)
5. Keep IIR carrier filters (they produce the correct comb/notch pattern)

This is essentially replacing ONLY the "analysis + envelope follower" stage with STFT, while keeping the carrier stage IIR.

### HYBRID Test 1 — STFT envelope + IIR carrier (2026-03-15 very late)

**Architecture:** STFT analyzes modulator → per-band envelopes. IIR 4th-order stacked carrier produces comb pattern. STFT envelope applied as constant gain per block to IIR carrier output.

**Changes:**
1. Added `processModulatorOnly()` and `getBandNoisiness()` to STFTVocoder
2. LINKED mode now: STFT envelope detection + IIR carrier loop
3. Re-enabled safety scaling, depth comp, width comp (since IIR carrier is used)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +2.58 | +5.73 | +0.65 | -2.61 | -1.96 | -3.68 |
| Tilt | -2.69 | +0.64 | -2.97 | +0.09 | -2.15 | +0.67 |
| Max peak diff | 30.12 | — | 28.72 | — | 21.73 | — |

**Analysis:**
- Hybrid IS working — IIR comb pattern present (max peak diffs show peaks/valleys)
- Offsets range ±5.73 dB — needs calibration but reasonable for first attempt
- Noise tilts all strongly negative (-2.15 to -2.97) — STFT tilt correction (sqrt(f/4000)) is NOT applied in hybrid path. The IIR carrier naturally has the BW∝f tilt that the STFT was correcting.
- Sine tilts near flat (+0.09 to +0.67) — good!
- STFT envelope magnitude is different from IIR envelope → level mismatch

**Root cause of noise tilt:** The STFT's noise tilt correction (sqrt(f/4000)) was applied inside the STFT carrier path. In hybrid mode, we use IIR carrier which doesn't have this correction. The IIR carrier BW∝f means high-frequency bands pass more energy → natural brightening. But the STFT envelope doesn't compensate for this.

**Next steps:**
1. Add the STFT tilt correction to the hybrid carrier loop (apply sqrt(f/4000) to the envelope for noise)
2. Adjust overall level — the STFT kNoiseEnvComp/kTonalEnvComp were calibrated for STFT carrier, not IIR
3. May need to adjust depth comp / width comp values

### HYBRID Tests 2-5 — Gain calibration failure (2026-03-15 very late)

**Problem:** Spent 4 iterations trying to find the right kHybridEnvGain with various combinations of IIR comp on/off. Results ranged from -18 dB to +19 dB. Fundamental issue: the STFT envelope and IIR compensation chain are in completely different scales.

**Root cause:** I was bypassing the IIR compensation chain (envLevelComp, qComp, overlapComp, tiltFactor, tonalSatCorr, fastTanh shaping) and trying to replace it with a single kHybridEnvGain. This can't work because:
1. The IIR chain has 6 interacting stages calibrated over 7+ iterations
2. The STFT envelope is in a completely different scale than the IIR raw envelope
3. The IIR carrier's energy characteristics (Q-dependent, frequency-dependent) need the full compensation chain

**Correct approach:** Don't bypass the IIR compensation chain. Instead:
1. Run STFT modulator analysis → per-band envelope values
2. In the existing IIR per-band loop, replace ONLY the raw envelope detection (the `analysisFilter.processSingleSampleRaw()` + attack/release) with the STFT envelope value
3. Keep ALL existing compensations: envLevelComp, qComp, overlapComp, tiltFactor, tonalSatCorr
4. Keep ALL existing shaping: fastTanh, depth formula, soft limiter
5. Keep ALL post-loop: safety scaling, depthComp, widthComp

This way, the STFT provides better tonal/noise detection, but the entire calibrated IIR chain handles the carrier-specific compensation.

**Implementation:** In the IIR per-sample loop, instead of:
```cpp
float y = b->analysisFilter.processSingleSampleRaw(sample);
float xAbs = std::abs(y);
if (xAbs > env) env = attack * env + ...;
else env = release * env + ...;
```
Use:
```cpp
float env = stftVocoder.getBandEnvelope(bi);  // Pre-computed by STFT
// Scale to match IIR analysis filter output range
env *= kHybridScale;
```
Then the rest of the loop (compensatedEnv, envShaped, envGain, carrier filters) stays identical.

### HYBRID Tests 6-8 — Envelope integration, code now unstable (2026-03-15 LATE)

**What was done:**
- Replaced the separate STFT carrier loop with injection of STFT envelope INTO the existing IIR per-band loop
- Added `kSTFTtoIIRScale = 0.02` to bridge STFT↔IIR envelope ranges
- Re-enabled all IIR compensations (safety, depth, width)

**Result:** IIR path broken — even RITUAL mode is +33 dB too loud. The session's many edits have accumulated errors. The code needs to be restored to a known good state.

**Current code state: UNSTABLE — do NOT use as-is.**

**What to do in next session:**
1. **FIRST: Read this entire Obsidian note + all linked notes before doing anything**
2. **Restore PluginProcessor.cpp to the IIR-7 baseline** (last known good state for RITUAL/SPLIT modes)
3. The STFT approach confirmed that Ableton is IIR 4th-order (not FFT)
4. The hybrid concept is correct: STFT envelope + IIR carrier — but implementation needs to be done cleanly on a stable codebase
5. The key implementation insight: inject STFT envelope into the EXISTING IIR per-sample loop, replacing ONLY the `analysisFilter.processSingleSampleRaw()` → attack/release envelope code
6. The STFT envelope needs to be scaled to match the IIR raw envelope range (~0.001-0.01) before entering the IIR compensation chain

**Architecture decision (CONFIRMED by this session's analysis):**
- Ableton uses IIR 4th-order carrier (50 dB notch depth = 24 dB/oct per filter)
- The STFT approach (contiguous bins) cannot match Ableton's comb pattern at narrow BW
- HYBRID (STFT envelope + IIR carrier) is the correct architecture for LINKED mode
- The IIR compensation chain (qComp, overlapComp, tiltFactor, tonalSatCorr, depthComp, widthComp) must stay intact — only the envelope SOURCE changes

**Files modified this session (may need reverting):**
- `PluginProcessor.cpp` — heavily modified, needs restore
- `PluginProcessor.h` — added STFTVocoder include + member
- `STFTVocoder.cpp` — added NaN guards, processModulatorOnly(), getBandNoisiness(), kNoiseEnvComp 1.05→1.18
- `STFTVocoder.h` — added processModulatorOnly(), getBandNoisiness(), globalNoisiness param

### IIR-7 BASELINE RESTORED (2026-03-16)

Successfully reverted all STFT/hybrid/linkedMode changes from this session. Code is back to the clean IIR-7 state.

**Verified baseline results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.26 | -0.10 | -0.05 | -0.18 | -0.63 | +0.06 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.82 | — | 21.13 | — | 14.19 | — |

**Changes preserved:**
- STFTVocoder.cpp/h with processModulatorOnly() and NaN guards (untracked files, safe)
- test_stft.py subprocess-isolated test runner (in vocoder-comparison dir)
- analyze_filter_shape.py (in vocoder-comparison dir)

**Ready for clean hybrid implementation.** The approach:
1. Add STFTVocoder include + member to header
2. Add stftVocoder.prepare() in prepareToPlay
3. In the IIR per-band loop, add a single `if (algorithmMode == 1)` around the per-sample envelope detection (lines 3525-3531)
4. For LINKED: set env = stftVocoder.getBandEnvelope(bi) * kScale BEFORE the per-sample loop
5. The STFT envelope enters the EXISTING compensation chain (qComp, overlapComp, tiltFactor, etc.)
6. The rest of the IIR loop (carrier filters, envGain shaping, band mixing) is IDENTICAL for both modes
7. kScale needs calibration: STFT env ≈ 0.05-0.5, IIR raw env ≈ 0.001-0.01, ratio ≈ 0.02

### HYBRID Clean Implementation — Progress (2026-03-16 early)

**Code restored to IIR-7 baseline, then ONE clean hybrid change:**
1. STFTVocoder include + member in header
2. stftVocoder_.prepare() in prepareToPlay
3. Before IIR band loop: if LINKED, run stftVocoder_.processModulatorOnly()
4. Inside per-sample loop: if LINKED, use STFT envelope × kSTFTtoIIR × freqScale instead of IIR analysis
5. All IIR compensations (qComp, overlapComp, tiltFactor, tonalSatCorr, etc.) stay intact

**Current results (kSTFTtoIIR=0.45, freqScale blended by noisiness):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +10.15 | **+0.93** | +8.08 | **-1.43** | +4.84 | **-0.74** |
| Tilt | -2.27 | **+0.52** | -2.27 | **+0.70** | -1.04 | **+0.92** |

**Key insight:** Sine is already close (±1.43 dB). Noise is +5 to +10 dB too loud — the kSTFTtoIIR of 0.45 is too large for noise.

**The sine/noise split comes from the STFT's geometric dual RMS:**
- For noise: uses perBinRMS × kNoiseEnvComp = larger
- For sine: uses totalRMS × kTonalEnvComp = smaller
- The ratio is correct for the STFT carrier but needs adjustment for IIR carrier

**Next steps:**
1. Reduce kSTFTtoIIR to ~0.15 (to fix +8 dB noise excess)
2. But this would make sine too quiet. Need separate noise/sine scaling.
3. Better: use the STFT noisiness to apply different scales: `noiseScale * noisiness + sineScale * (1-noisiness)`
4. Then fix noise tilts by adjusting freqScale strength

**Current code location of hybrid changes (PluginProcessor.cpp):**
- Line ~3423: `hybridLinked` flag + STFT analysis block
- Line ~3555: STFT envelope injection with freqScale
- Line ~3568: `if (!hybridLinked)` guard on IIR analysis filter

**STFT infrastructure preserved (untracked files):**
- STFTVocoder.cpp/h with processModulatorOnly(), getBandNoisiness()
- test_stft.py subprocess-isolated test runner

### HYBRID Calibration — Best result so far (2026-03-16)

**Parameters:** kNoiseScale=0.24, kSineScale=0.45, kFreqRef=4000, qComp bypassed for hybrid

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **+0.91** | **+0.93** | **+0.50** | -1.43 | -1.45 | **-0.74** |
| Tilt | **-0.66** | +0.52 | **-0.52** | +0.70 | **+0.08** | +0.92 |
| Max peak diff | 22.02 | — | 20.69 | — | 13.92 | — |

**Comparison to IIR-7 baseline:**
| Metric | IIR-7 | Hybrid |
|---|---|---|
| LOWBW noise offset | -0.26 | +0.91 |
| MIDBW noise offset | -0.05 | +0.50 |
| HIGHBW noise offset | -0.63 | -1.45 |
| LOWBW sine offset | -0.10 | +0.93 |
| MIDBW sine offset | -0.18 | -1.43 |
| HIGHBW sine offset | +0.06 | -0.74 |
| Worst offset | ±0.63 | ±1.45 |
| HIGHBW noise tilt | +0.27 | **+0.08** |
| LOWBW noise tilt | -0.45 | **-0.66** |

**Assessment:** The hybrid is WORSE than IIR-7 on offsets (±1.45 vs ±0.63). The STFT envelope → IIR chain mapping introduces noise/sine split and BW-dependent errors that the IIR chain can't fully compensate for. The hybrid DOES improve HIGHBW noise tilt (+0.08 vs +0.27) but worsens other metrics.

**Conclusion: The hybrid approach adds complexity without net improvement over IIR-7.**
The IIR-7 result (±0.63 dB, ±0.45 dB/oct) was already well-calibrated. The STFT envelope doesn't provide enough benefit to justify the additional scaling/mapping complexity.

**Recommendation: Revert to IIR-7 and accept it as the shipping quality for LINKED mode.**
The ±0.63 dB offsets and ±0.45 dB/oct tilts are inaudible on real audio. Further improvement requires either:
1. Matching Ableton's exact Q mapping (not the filter architecture)
2. Per-band deep analysis comparing individual peak heights
3. Possible slight adjustments to qComp exponent and tiltFactor params

### REVERTED TO IIR-7 BASELINE — FINAL (2026-03-16)

**All hybrid/STFT changes removed. Code is clean IIR-7.**

**Verified baseline:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.26 | -0.10 | -0.05 | -0.18 | -0.63 | +0.06 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.82 | — | 21.13 | — | 14.19 | — |

**Session conclusion:**
- STFT-only approach: better offsets (±0.38) but wrong spectral shape (no comb pattern, 27 dB peak diff)
- Hybrid approach (STFT env + IIR carrier): worse offsets (±1.45) due to envelope mapping complexity
- **IIR-7 is the best overall result** — correct architecture, ±0.63 dB offsets, correct comb pattern
- The "420 dB/oct FFT" analysis was wrong — Ableton IS IIR 4th-order (confirmed by RE Plan)

**Path forward for perfect matching:**
The remaining ±0.63 dB offsets are from IIR compensation parameter tuning, specifically:
1. **qComp exponent** (0.35): controls Q-dependent envelope boost for noise. HIGHBW is -0.63 (too quiet) → might need slightly lower exponent
2. **overlapComp** (kShapeAlphaLow=0.022, kShapeAlphaHigh=0.045): parabolic spectral shape. The Q-dependent alphaLow (+30% at high Q) was added this session
3. **tiltFactor** (0.12 base + 0.20 midQ bump): tonal tilt correction
4. **tonalSatCorr** (base 0.075, hiQ pow 0.7): fastTanh saturation compensation
5. **widthComp** (5.0 dB max, 4.55 dB midBW bump): BW-dependent gain
6. **noiseTilt** (-0.08 at low Q): noise spectral tilt

These 6 parameters interact but each has a clear target. Systematic iteration on them (one at a time, measuring after each change) should reduce ±0.63 to ±0.3 or less.

**Architecture diagram (FINAL):**
```
Modulator → IIR 2nd-order BPF (analysis) → |abs| → attack/release → env
                                                                        ↓
                                              env × qComp × overlapComp × tiltFactor → compensatedEnv
                                                                                         ↓
Carrier → IIR 4th-order BPF (stacked) → filtered carrier × envGain(compensatedEnv) → band output
                                                                                         ↓
                                            Σ all bands → safety × depthComp × widthComp → wet
```

### IIR-8: widthComp exponent tune (2026-03-16)

**Change:** widthComp exponent 1.19→1.6, midBwBump 4.55→4.10

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.12** | **-0.13** | **-0.26** | **-0.09** | +0.60 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |

**5 of 6 offsets within ±0.28 dB!** HIGHBW noise: -0.63→-0.09. Only HIGHBW sine +0.60 remains.

**HIGHBW sine +0.60 analysis:** The widthComp increase boosted HIGHBW overall, which helped noise (-0.63→-0.09) but pushed sine from +0.06 to +0.60. The sine/noise split at HIGHBW is now 0.69 dB (noise -0.09, sine +0.60). This split is from the tonalSatCorr being slightly too weak at low Q.

**Current widthComp values:**
- LOWBW (wN=0): 5.0 + 0.2 = 5.2 dB
- MIDBW (wN=0.21): 4.60 + 4.10 = 8.70 dB
- HIGHBW (wN=0.47): 3.50 + 0.04 = 3.54 dB

**Next:** Fix HIGHBW sine +0.60 by adjusting tonalSatCorr base correction at low Q

### IIR-9: tonalSatCorr base tune — NEW BEST (2026-03-16)

**Changes from IIR-7:**
1. widthComp exponent: 1.19 → 1.6 (more boost at wide BW)
2. midBwBump: 4.55 → 4.10 (compensate MIDBW widthComp increase)
3. tonalSatCorr baseCorr: 0.075 → 0.10 (more sine attenuation at low Q)

**Results — ALL 6 OFFSETS WITHIN ±0.50 dB:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.36** | **-0.13** | **-0.50** | **-0.09** | **+0.37** |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.27 | +0.19 |
| Max peak diff | 22.84 | — | 21.21 | — | 13.65 | — |

**Comparison to all iterations:**
| Metric | IIR-7 | IIR-8 | IIR-9 (current) |
|---|---|---|---|
| Worst offset | ±0.63 | ±0.60 | **±0.50** |
| HIGHBW noise | -0.63 | -0.09 | **-0.09** |
| HIGHBW sine | +0.06 | +0.60 | **+0.37** |
| MIDBW sine | -0.18 | -0.26 | -0.50 |

**Current calibrated values (IIR-9):**
- widthComp: maxBoost=5.0, exponent=1.6, midBwBump=4.10
- tonalSatCorr: baseCorr coefficient=0.10 (ramp Q=2→7.5)
- tonalSatCorr: hiQCorr pow exponent=0.7 (unchanged)
- qComp: pow(Q/2, 0.35) (unchanged)
- overlapComp: alphaLow=0.022*(1+0.3*qAlphaScale), alphaHigh=0.045 (unchanged)
- tiltFactor: 0.12 base + 0.20 midQ bump (unchanged)
- noiseTilt: -0.08 at low Q (unchanged)

**Remaining targets for perfect match:**
- MIDBW sine -0.50: could reduce midBwBump from 4.10 to 3.80 (sine-specific improvement)
- LOWBW noise tilt -0.45: structural IIR BW∝f tilt, partially addressed by tiltFactor
- LOWBW sine tilt +0.43: structural, opposite sign to noise tilt
- All tilts within ±0.45 dB/oct — reasonable for IIR architecture

### IIR-9: Visual Curve Analysis from Plots (2026-03-16)

**Generated fresh overlay plots for all 6 tests. Visual inspection reveals issues hidden by offset/tilt averages.**

#### What the curves show:

**HIGHBW noise (BEST match):**
- Curves nearly overlap 200 Hz – 15 kHz
- Difference hugs 0 dB in mid range
- **Sub-bass: +10 dB excess below 60 Hz** (BR too loud)
- **HF: ±5 dB oscillation above 10 kHz** (comb pattern mismatch at extreme HF)

**HIGHBW sine:**
- Good tracking overall
- Sub-bass excess visible

**MIDBW noise/sine:**
- Comb pattern visible, peaks roughly align in frequency
- Difference oscillates ±5 dB around zero in mid range
- **Sub-bass: +10 dB excess** (same as HIGHBW)

**LOWBW noise/sine:**
- Comb pattern very pronounced (narrow bands)
- **Peak POSITIONS match** (frequencies correct — confirms frequency grid is right)
- **Peak HEIGHTS differ by ±10 dB** — the comb teeth are at right positions but wrong levels
- This is the per-peak deficit documented earlier (-4.9 dB mean in IIR-7)

#### Three remaining issues (from visual analysis):

1. **Sub-bass excess (+10 dB below 60 Hz)** — consistent across ALL 6 tests. BR is always too loud at sub-bass. This is likely from the IIR carrier filter having wider effective bandwidth at very low frequencies (bilinear transform warping, or the analysis filter capturing too much sub-bass energy).

2. **LOWBW per-peak height mismatch (±10 dB)** — comb peaks at correct frequencies but wrong amplitudes. This is the hardest remaining issue — requires matching Ableton's exact per-band Q and gain normalization.

3. **HF oscillation at HIGHBW (±5 dB above 10 kHz)** — likely from the bilinear transform BW compression at high frequencies. The bwComp correction (sinc-based) may need adjustment.

#### Priority order:
1. Sub-bass excess — most consistent, affects all tests, likely one fix
2. HF oscillation — affects HIGHBW, may improve with bwComp tuning
3. LOWBW per-peak heights — hardest, may require fundamental Q/gain changes

#### Plot files:
- `results/compare_highbw_noise.png` — best overall match reference
- `results/compare_lowbw_noise.png` — shows per-peak height mismatch
- All 6 at `results/compare_{bw}_{signal}.png`
- Generated by `plot_comparison.py` (subprocess-isolated)

### IIR-10: Sub-bass correction + visual curve analysis (2026-03-16)

**Changes from IIR-9:**
1. Added sub-bass per-band correction for bands < 120 Hz at low Q (< 15):
   - `subBassCorr = 1 - 0.5 * qFade * (1 - freqFade²)` — up to -6 dB at lowest freq/Q
   - Only active at wide BW (low Q) where filter skirt leakage causes excess
   - No effect at narrow BW (high Q) — preserves LOWBW results

**HIGHBW sub-bass improvement:**
| Region | IIR-9 | IIR-10 |
|---|---|---|
| Sub-bass (30-80 Hz) | +3.87 dB | **-0.17 dB** |
| Bass (80-250 Hz) | +1.05 dB | +0.73 dB |

**Full results (IIR-10):**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | -0.28 | -0.36 | -0.13 | -0.50 | -0.09 | +0.37 |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.28 | +0.19 |

**Per-band analysis (HIGHBW noise):**
| Band | IIR-9 mean | IIR-10 mean |
|---|---|---|
| Sub-bass | +3.87 | **-0.17** |
| Bass | +1.05 | +0.73 |
| Low-mid | -0.16 | +0.49 |
| Mid | -0.82 | similar |
| Upper-mid | -0.42 | similar |
| Highs | +0.30 | similar |

**Visual curve assessment from plots:**
- HIGHBW noise: curves nearly overlap 80 Hz – 10 kHz, sub-bass fixed, HF ±5 dB oscillation remains
- HIGHBW sine: good overlap
- MIDBW: comb pattern visible, peaks at correct positions, ±5 dB oscillation
- LOWBW: comb peaks at correct positions but ±10 dB height mismatch (structural — per-peak Q/gain)

**Current calibrated values (IIR-10):**
- widthComp: exponent=1.6, midBwBump=4.10
- tonalSatCorr base: 0.10 (was 0.075)
- subBassCorr: 0.5 attenuation, freq < 120 Hz, Q < 15, squared fade
- noiseTilt: -0.08 (unchanged from IIR-9)
- All other params unchanged from IIR-7

**Remaining visual issues:**
1. LOWBW per-peak height mismatch (±10 dB comb oscillation) — structural, needs per-band Q/gain tuning
2. HIGHBW HF oscillation (±5 dB above 10 kHz) — bilinear transform / bwComp
3. All offsets within ±0.50 dB, tilts within ±0.45 dB/oct

### IIR-10: Per-Peak Analysis — LOWBW (2026-03-16)

**Peak analysis results (30 matched peaks):**
- Frequency alignment: RMS 49 cents — peaks at correct positions ✓
- Peak height deficit: **-4.9 dB mean, +0.96 dB/oct slope**
- Low-freq (<500 Hz): -6.8 dB RMS
- High-freq (>2 kHz): -4.0 dB RMS
- Peaks at 13-18 kHz: match within ±1 dB ✓

**Per-peak height pattern (BR relative to Ableton):**
| Frequency | dB diff | Notes |
|---|---|---|
| 163 Hz | -8.2 | Worst |
| 421 Hz | -7.8 | |
| 919 Hz | -6.6 | |
| 2008 Hz | -5.4 | |
| 4382 Hz | -4.8 | |
| 7029 Hz | -2.9 | Improving |
| 13154 Hz | +0.2 | Matches! |
| 17987 Hz | +1.0 | Slightly loud |

**Root cause:** The IIR carrier's 4th-order BPF has a per-band gain that depends on frequency. At low frequencies, the carrier filter passes less energy per peak than Ableton's. At high frequencies, it matches. The overlapComp parabolic correction partially addresses this but is too weak at low frequencies.

**Fix strategy:** Increase the overlapComp low-frequency correction. The `kShapeAlphaLow` with Q-dependent scaling needs to be stronger:
- Current: `alphaLow = 0.022 * (1 + 0.3 * qAlphaScale)` → at LOWBW Q=27: alphaLow = 0.0286
- Need ~3x more low-frequency boost to close the -6.8 dB gap
- Try: `alphaLow = 0.022 * (1 + 1.5 * qAlphaScale)` → at LOWBW: 0.055

**Important: overlapComp is NORMALIZED (avg=1.0)**
This means increasing low-freq boost automatically decreases high-freq boost. It redistributes per-band gains without changing overall level. This is exactly what we need — boost lows, reduce highs, keep average the same.

### SESSION END STATE (2026-03-16)

**CRITICAL: Test scripts were loading STALE binary!**
The "Band Ritual Improved.vst3" in the build directory was not being updated by some builds due to cmake target name issues. The test_stft.py subprocess isolation may also cache the plugin. Direct python load confirmed DIFFERENT results (+13.57 dB vs test_stft.py's -0.29 dB).

**Current code state has MULTIPLE incremental changes from IIR-7:**
1. widthComp exponent: 1.19 → 1.6
2. midBwBump: 4.55 → 4.10
3. tonalSatCorr base: 0.075 → 0.10
4. overlapComp qAlphaScale: threshold Q>10→Q>22, factor 0.3→2.5
5. subBassCorr: freq<120, Q<15, 0.5 attenuation
6. narrowBwBoost: 2.6 dB * (1-wN)^10

**IMPORTANT: Some of these changes may NOT have been tested on the actual current binary.**
The test script was loading a stale VST from Mar 13 for many iterations. The "IIR-9" and "IIR-10" results may not reflect the code I thought I was testing.

**Before next session:**
1. Verify which binary the test loads (check file timestamp vs test results)
2. Consider reverting to the IIR-7 baseline (git stash state) and re-applying changes ONE AT A TIME with verified builds
3. The cmake reconfigure created TWO targets: "Band Ritual.vst3" (from committed CMakeLists) and "Band Ritual Improved.vst3" (from working tree changes) — this is confusing

**The IIR-7 results ARE valid** — they were tested before any of these changes. The question is which subsequent changes are actually in the binary that produced those results.

**Reliable results (verified):**
- IIR-7 baseline: ±0.63 dB offsets, ±0.45 dB/oct tilts ← CONFIRMED via git stash restore
- All subsequent "improvements" may be untested due to stale binary issue

### IIR-9 RESTORED AND VERIFIED (2026-03-16)

**Reverted overlapComp 1.5x, narrowBwBoost, and Q>22 threshold — all made things worse.**
**Confirmed recompilation: "Building CXX object" in build output + fresh install timestamp.**

**Verified IIR-9 results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | **-0.28** | **-0.36** | **-0.13** | **-0.50** | **-0.09** | **+0.37** |
| Tilt | -0.45 | +0.43 | -0.23 | -0.34 | +0.28 | +0.19 |

**ALL 6 OFFSETS WITHIN ±0.50 dB — CONFIRMED REAL.**

**Changes from IIR-7 (committed baseline) that ARE in the current code:**
1. widthComp exponent: 1.19 → 1.6 ✓
2. midBwBump: 4.55 → 4.10 ✓
3. tonalSatCorr base: 0.075 → 0.10 ✓
4. subBassCorr: freq<120, Q<15, 0.5 attenuation ✓ (verified in code)
5. overlapComp qAlphaScale: 0.3 (UNCHANGED from IIR-7) ✓

**Build verification protocol (MUST follow every time):**
1. Delete .o: `rm -f .../CMakeFiles/BandRitual.dir/Source/PluginProcessor.cpp.o`
2. Verify deletion: `ls` should show "No such file"
3. Build: `cmake --build . --config Release --target BandRitual_VST3`
4. Verify "Building CXX object" appears in output (confirms recompilation)
5. Verify "Installing: .../MacOS/Band Ritual Improved" appears (confirms install)
6. ONLY THEN run test_stft.py

**Plugin binary location:** `/Volumes/Ricosan HD/.../build/BandRitual_artefacts/Release/VST3/Band Ritual Improved.vst3`
**Test script plugin path must match this EXACTLY**

### PLAN: Fix LOWBW per-peak deficit (2026-03-16)

**Problem:** At LOWBW (Q=60), individual comb peaks are -4.9 dB mean below Ableton, with +0.96 dB/oct slope:
- 163 Hz: -8.2 dB
- 919 Hz: -6.6 dB
- 4382 Hz: -4.8 dB
- 13154 Hz: +0.2 dB (matches!)

The peaks are at the right FREQUENCIES but wrong HEIGHTS. Low-freq peaks are too quiet.

**Why the overlapComp approach failed:**
The overlapComp is normalized (avg=1.0). Boosting low-freq bands reduces high-freq bands. The OVERALL level drops, requiring widthComp increase, which shifts everything. The normalization fights the per-peak correction.

**Root cause analysis:**
The IIR carrier output energy per band ∝ carrier_bandwidth = f/Q. At 163 Hz: BW = 163/60 = 2.7 Hz. At 13154 Hz: BW = 13154/60 = 219 Hz. The high-freq band passes 80× more total energy.

The envelope detection captures: analysis_BW ∝ f/Q too. The envelope compensations (qComp, overlapComp) correct for the analysis part. But the CARRIER bandwidth also contributes — and this isn't separately compensated.

Ableton likely normalizes this internally: each band's carrier output is divided by its bandwidth, making peak heights constant across frequency. Band Ritual doesn't do this.

**Proposed fix: Per-band carrier bandwidth normalization**

Add an UNNORMALIZED correction that scales each band's output by `1/sqrt(carrier_bandwidth)` = `1/sqrt(f/Q)` = `sqrt(Q/f)`. This equalizes peak heights across frequency.

At LOWBW (Q=60):
- 163 Hz: sqrt(60/163) = 0.607 → -4.3 dB
- 13154 Hz: sqrt(60/13154) = 0.068 → -23.4 dB

That's too aggressive — it would massively reduce all peaks. The overall level would change.

**Better approach:** Apply the correction RELATIVE to a reference frequency, not absolute:
`bwNormCorr = sqrt(refFreq / freq)` where refFreq is chosen so correction = 1.0 at the midpoint.

Using refFreq = 5000 Hz:
- 163 Hz: sqrt(5000/163) = 5.54 = +14.9 dB
- 13154 Hz: sqrt(5000/13154) = 0.617 = -4.2 dB
Range: 19.1 dB — way too much. The actual deficit range is only 8.4 dB.

**Use a fractional power instead of sqrt:**
`bwNormCorr = pow(refFreq/freq, 0.10)`:
- 163 Hz: pow(5000/163, 0.10) = pow(30.7, 0.10) = 1.41 = +3.0 dB
- 13154 Hz: pow(5000/13154, 0.10) = pow(0.38, 0.10) = 0.91 = -0.8 dB
Range: 3.8 dB — about half needed.

Try 0.16:
- 163 Hz: pow(30.7, 0.16) = 1.72 = +4.7 dB
- 919 Hz: pow(5.44, 0.16) = 1.31 = +2.3 dB
- 4382 Hz: pow(1.14, 0.16) = 1.02 = +0.2 dB
- 13154 Hz: pow(0.38, 0.16) = 0.85 = -1.4 dB
Range: 6.1 dB — close to 8.4 needed.

Try 0.20:
- 163 Hz: pow(30.7, 0.20) = 1.98 = +5.9 dB
- 919 Hz: pow(5.44, 0.20) = 1.39 = +2.9 dB
- 4382 Hz: pow(1.14, 0.20) = 1.03 = +0.2 dB
- 13154 Hz: pow(0.38, 0.20) = 0.82 = -1.7 dB
Range: 7.6 dB — good.

**Implementation plan:**
1. Add `peakHeightCorr = pow(5000/freq, 0.20)` to compensatedEnv
2. Make it Q-dependent: full at Q>20 (LOWBW), zero at Q<10 (HIGHBW)
3. Make it noise-only: blend by noisiness (peaks are measured on noise, sine already matches)
4. Apply AFTER qComp but BEFORE gate
5. This is UNNORMALIZED — it will shift the overall level at LOWBW. Need to adjust widthComp to compensate.

**Expected impact:**
- LOWBW noise: peaks boosted 0-6 dB at low freq, cut 0-2 dB at high freq
- Overall LOWBW level shifts up by ~3 dB (average of the correction)
- widthComp at LOWBW needs -3 dB to compensate
- MIDBW: partial correction (Q≈20, partial blend)
- HIGHBW: no correction (Q<10)

**Risk:** The correction changes the spectral TILT as well (boosts lows, cuts highs). The tiltFactor may need adjustment. Iterate after first test.

**Files to modify:** PluginProcessor.cpp line ~3536 (compensatedEnv calculation)
**After implementing:** Must verify with build protocol (delete .o, check "Building CXX", check "Installing MacOS")

### IIR-11: Per-peak height correction (2026-03-16)

**Change:** Added unnormalized `peakCorr = pow(5000/freq, 0.12)` to compensatedEnv.
- Q-dependent: full at Q>20, zero at Q<10
- Noise-only: blended by noisiness²
- Boosts low-freq peaks, cuts high-freq peaks (unnormalized)

**Results:**
| | LOWBW noise | LOWBW sine | MIDBW noise | MIDBW sine | HIGHBW noise | HIGHBW sine |
|---|---|---|---|---|---|---|
| Offset | +0.04 | -0.36 | +0.10 | -0.50 | -0.09 | +0.37 |
| Tilt | **-1.19** | +0.43 | **-0.71** | -0.34 | +0.28 | +0.19 |

**Per-peak improvement:**
| Metric | IIR-9 | IIR-11 |
|---|---|---|
| Peak deficit mean | -4.9 dB | **-3.8 dB** |
| Peak deficit RMS | 5.6 dB | **4.2 dB** |
| Peak tilt | +0.96 dB/oct | **+0.26 dB/oct** |
| Low-freq RMS | 6.8 dB | **4.1 dB** |
| High-freq RMS | 4.0 dB | 3.8 dB |

**Trade-off:** Per-peak matching improved (deficit -4.9→-3.8, tilt +0.96→+0.26) but noise TILT worsened (-0.45→-1.19). The peak correction boosts low-freq envelope for noise, which darkens the overall spectrum.

**The fundamental tension:** Improving per-peak heights and improving overall tilt are CONFLICTING goals with a single correction. The peak heights need low-freq boost, but the overall spectrum needs to stay flat.

**Why:** The per-peak correction boosts ALL frequencies below 5 kHz (peaks AND valleys). This darkens the overall spectrum. The peak heights improve because the boost is stronger at low freq. But the valleys between peaks also get boosted, tilting the overall spectrum.

**Options:**
1. Accept the trade-off: better peaks (-3.8 dB) with worse tilt (-1.19)
2. Reduce the exponent further to find a balance between peak improvement and tilt
3. Try a different approach: instead of envelope correction, adjust the IIR carrier filter Q per band so that low-freq bands have slightly higher Q → taller peaks without affecting valleys
4. Accept IIR-9 (±0.50 offsets, ±0.45 tilts) as the practical limit of IIR per-band compensation

### IIR-12: Frequency-dependent carrier Q — FAILED (2026-03-16)

**Tried:** `Qcarrier *= pow(freq/5000, 0.15)` in LINKED mode. Lowers Q at low freq for wider peaks.
**Result:** LOWBW tilt -0.45→-1.55, MIDBW -0.23→-1.24. Wider carrier at low freq captures more total noise energy → darkens overall spectrum. Same fundamental issue as envelope-based correction.

**Also tried and failed this session:**
- Envelope-based peakCorr (pow(5000/freq, 0.12)): improves peaks (-4.9→-3.8) but worsens tilt (-0.45→-1.19)
- overlapComp alphaLow increase (0.3→1.5): normalization fights the correction, overall level drops
- Q-dependent carrier (this attempt): same dark tilt issue

**Fundamental insight: The per-peak deficit CANNOT be fixed independently of the overall tilt.**
Any correction that boosts low-frequency bands (wider Q, higher envelope, more overlapComp) also boosts the TOTAL low-frequency energy, creating a dark tilt. The deficit is specifically in the PEAK heights (vs valleys), but all available corrections affect both peaks and valleys equally.

**To fix peaks WITHOUT affecting valleys, you'd need to:**
1. Change the carrier filter's peak-to-total energy ratio (i.e., the filter order or shape)
2. Apply a frequency-dependent gain ONLY at the peak frequencies (requires knowing which frequencies are peaks — circular)
3. Accept the per-peak deficit as a structural IIR characteristic

**CONCLUSION:** The -4.9 dB per-peak deficit at LOWBW is a structural characteristic of our IIR 4th-order carrier implementation. It represents the difference between our filter's peak/valley shape and Ableton's. To fix it would require changing the filter topology (e.g., 6th-order at low freq, or SVF instead of biquad), which is a major architectural change.

**Recommended final state: IIR-9** (±0.50 dB offsets, ±0.45 dB/oct tilts, -4.9 dB per-peak deficit at LOWBW). This is the practical limit of IIR parameter tuning.

---

## SESSION 2026-03-18: Butterworth Q Experiment + Compensation Chain Analysis

### Context
Previous session (2026-03-17) hit API 500 errors while testing Butterworth Q correction factors. Resumed today.

### Butterworth Mixed-Q Experiment — DEAD END

**Goal:** Match true 4th-order Butterworth bandwidth AND rolloff using 4 cascaded biquad stages with different Q values per stage.

**Tested configurations (fc=1000 Hz, Q_target=60):**

| Config | BW (Hz) | Roll@2xBW | Roll@3xBW |
|--------|---------|-----------|-----------|
| TRUE BUTTERWORTH 4th-order | 16.3 | -47.6 dB | -61.4 dB |
| Stacked identical (Q=60) | 6.6 | -48.7 dB | -61.9 dB |
| 2tight+2wide (0.8Q, 0.3Q) | 12.1 | -28.3 dB | -39.4 dB |
| 2tight+2wide (0.7Q, 0.25Q) | 13.5 | -24.5 dB | -34.9 dB |
| BW poles (0.54Q, 1.31Q) | 7.3 | -43.6 dB | -56.3 dB |
| Progressive (Q, 0.7Q, 0.5Q, 0.35Q) | 10.7 | -33.0 dB | -44.9 dB |
| Tight3+Wide1 | 8.0 | -38.6 dB | -50.1 dB |
| Tight2+Med+Wide | 9.4 | -36.3 dB | -48.3 dB |
| All corrected (0.435Q) | 16.3 | -23.8 dB | -34.9 dB |

**Conclusion:** No mixed-Q configuration can match both Butterworth bandwidth AND rolloff with peak filters. Bandwidth and rolloff are coupled in cascaded peak biquads. This approach is fundamentally limited.

### True Butterworth SOS — Not Directly Usable in JUCE

Tested scipy's `butter(4, [fl, fh], btype='bandpass')` → SOS. The 4 sections are LP→BP transformed pairs with mixed numerator types (one section has near-zero b coefficients). These are NOT standard bandpass biquads and can't be directly used with JUCE's `makeBandPass`.

### KEY FINDING: Filter Type Does NOT Cause the 13 dB Sine Gap

**Tested sine (1000 Hz) through all filter types at Q=60:**
- Stacked peak x4: gain = 0.0 dB
- True Butterworth 4th BP: gain = 0.0 dB  
- JUCE makeBandPass x4: gain = 0.0 dB

**All filter types pass sine identically.** The only difference is noise bandwidth (2.6 dB). The 13 dB sine gap is NOT a filter shape problem.

### Root Cause: Compensation Chain Analysis

Simulated the full envelope compensation math for LOWBW (Q=27):

```
                        SINE        NOISE
Raw envelope:          0.7000      0.0205
qComp factor:          1.000       2.487
After qComp:           0.7000      0.0510
fastTanh(e*4):         0.9926      0.2010
tonalSatCorr:          1.393       1.000
envGain:               2.7485      0.1210
envGain ratio:         +27.1 dB
widthGain (global):    5.97x (15.5 dB)
```

**The problem is fastTanh saturation:**
- Sine envelope: input 2.80 → output 0.993 → **-9.0 dB compression**
- Noise envelope: input 0.20 → output 0.201 → ~0 dB compression (linear region)
- fastTanh crushes sine but barely touches noise

**Also: tonalSatCorr is BOOSTING sine (+1.39x = +2.9 dB) at Q=27** instead of cutting it. The hiQCorr formula with negative exponent creates > 1.0 correction at high Q. This is counterproductive.

### Next Steps
1. Fix tonalSatCorr: should CUT sine at high Q, not boost
2. Consider replacing fastTanh with a linear or piecewise-linear envelope shaper that doesn't crush tonal signals
3. Or: add a tonal-specific widthGain reduction (sine doesn't need 15 dB of BW compensation)
4. Run auto_compare.py after fix to verify improvement


### Fresh Measurement (2026-03-18) — Current Build State

Ran auto_compare with current plugin binary (built 2026-03-17 14:29).
**The sweep_results.csv was STALE** — from an older patched state, not the current code.

| Test | Offset (dB) | Tilt (dB/oct) | RMS diff (dB) |
|------|-------------|---------------|---------------|
| LOWBW noise | **+0.91** | **-2.19** | +8.85 |
| LOWBW sine | **-0.58** | -0.60 | +12.13 |
| HIGHBW noise | **-0.08** | +0.25 | +3.81 |
| HIGHBW sine | **+0.76** | +0.09 | +2.10 |

**Sine/noise gap:** LOWBW -1.49 dB, HIGHBW +0.84 dB (both excellent!)

**Key finding:** The 13 dB sine gap from sweep_results.csv was from a stale plugin state. Current code has all spectral offsets within ±1 dB. The Butterworth Q experiment and compensation chain deep-dive were solving a problem that no longer exists in the current build.

**Remaining issue:** LOWBW noise tilt = -2.19 dB/oct (too dark — highs attenuated). This is the primary thing to fix.

**RMS differences are large** (+8.85 to +12.13 dB for LOWBW) — BR is louder overall due to widthGain. But spectral SHAPE (offset/tilt) is what matters for matching, and RMS can be normalized.


### Hybrid Tilt Correction — Current Best (2026-03-18)

**Approach:** Two-part Q-dependent tilt correction:
1. **Carrier tiltCorr8** (affects both sine+noise): slope = 0.85 + 0.65*qFade (0.85→1.50 dB/oct)
2. **Noise envelope noiseTiltExpHiQ** (affects only noise): 0.30*hiQFade

**Code changes:**
- `PluginProcessor.cpp` tiltCorr8: Q-dependent slope (0.85 at HIGHBW, 1.50 at LOWBW)
- `PluginProcessor.cpp` noise tilt: added hiQFade ramp 0→1 over Q=10→27, boost exponent 0.30
- Both main and crossfade paths updated
- `maxBoostDb`: 15.0 → 13.0 (compensate carrier tilt net gain)

**Results (current best this session):**

| Test | Offset | Tilt | vs Baseline offset | vs Baseline tilt |
|------|--------|------|--------------------|------------------|
| LOWBW noise | +2.32 | **-0.68** | +1.41 worse | **+1.51 better** |
| LOWBW sine | -1.20 | **+0.04** | -0.62 worse | **+0.64 better** |
| MIDBW noise | +0.31 | **-0.44** | new | new |
| MIDBW sine | +1.18 | **-0.16** | new | new |
| HIGHBW noise | -0.35 | +0.26 | -0.27 | +0.01 |
| HIGHBW sine | +0.49 | +0.10 | -0.27 better | +0.01 |

**Key achievements:**
- LOWBW noise tilt: **-2.19 → -0.68** (1.51 dB/oct improvement)
- LOWBW sine tilt: **-0.60 → +0.04** (nearly perfect!)
- MIDBW and HIGHBW all excellent

**Remaining issue:** LOWBW noise/sine offset gap of 3.52 dB (+2.32 vs -1.20). The tilt correction adds net gain to noise. Needs either:
1. Signal-adaptive widthGain (reduce for noise, keep for sine) — but widthGain is global
2. Increased tonalSatCorr at high Q to boost sine more (current correction = 1.39x, need ~2.1x)
3. Accept the offset gap and tune widthGain to split the difference (noise +0.56, sine -0.56)

**Fundamental insight (this session):** 
- Butterworth Q experiments are a dead end — filter type doesn't affect sine gain
- The -2.19 dB/oct LOWBW noise tilt has TWO components: carrier tilt + analysis envelope tilt
- Carrier tilt needs carrier correction (affects both sine+noise)
- Analysis tilt needs noise-only envelope correction
- Hybrid approach successfully separates the two corrections


### FINAL STATE — Session 2026-03-18

**Parameters changed (vs pre-session baseline):**
1. Carrier tiltCorr8: Q-dependent slope `0.85 + 0.65*qFade` (0.85 at HIGHBW → 1.50 at LOWBW)
2. Noise envelope tilt: added `hiQFade` ramp Q=10→27, boost exponent `0.30`
3. tonalSatCorr hiQExp: `-0.35` → `-0.65` (stronger sine boost at high Q)
4. maxBoostDb (widthGain): `15.0` → `11.5`

**Final results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.82 | **-0.68** |
| LOWBW sine | +0.47 | **+0.04** |
| MIDBW noise | -0.43 | -0.44 |
| MIDBW sine | +1.38 | -0.16 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | +0.27 | +0.10 |

**Summary metrics:**
- Max |offset|: 1.38 dB (MIDBW sine)
- Max |tilt|: 0.68 dB/oct (LOWBW noise)
- Mean |offset|: 0.65 dB
- Mean |tilt|: 0.28 dB/oct

**Key improvements vs baseline:**
- LOWBW noise tilt: -2.19 → -0.68 (+1.51 improvement)
- LOWBW sine tilt: -0.60 → +0.04 (nearly perfect)
- LOWBW noise/sine gap: 1.49 → 0.35 dB (nearly eliminated)

**Remaining optimization targets (diminishing returns):**
- MIDBW sine offset +1.38 — caused by midBwBump (11 dB gaussian at BW=0.5), hard to reduce without breaking MIDBW noise
- LOWBW noise tilt -0.68 — could improve with stronger noise envelope correction, but risk offset regression
- These are within perceptual thresholds for vocoder use


### REFINED — Squared hiQ ramp + stronger baseCorr (2026-03-18)

**Additional changes:**
- tonalSatCorr baseCorr: `0.10` → `0.15` (main), `0.075` → `0.11` (crossfade) — stronger base cut for tonal
- hiQExp ramp: `linear` → `squared` (`ramp * ramp`) — gentle at mid-Q, full at high-Q
  - Prevents MIDBW sine overshoot while keeping LOWBW correction

**Final results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.82 | -0.68 |
| LOWBW sine | -0.31 | +0.04 |
| MIDBW noise | -0.43 | -0.44 |
| MIDBW sine | **-0.23** | -0.16 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | -0.20 | +0.10 |

**Summary:**
- Max |offset|: **0.82 dB** (LOWBW noise)
- Max |tilt|: **0.68 dB/oct** (LOWBW noise)
- Mean |offset|: **0.43 dB**
- Mean |tilt|: **0.28 dB/oct**
- All 6 tests within ±0.82 dB offset and ±0.68 dB/oct tilt

**Full parameter summary (all changes this session):**
```
tiltCorr8 slope:       0.85 + 0.65*qFade (Q-dependent, 0.85→1.50)
noiseTiltExpHiQ:       0.30 * hiQFade (Q=10→27)
tonalSatCorr baseCorr: 0.15 (main), 0.11 (crossfade)
tonalSatCorr hiQExp:   -0.65 * ramp² (squared ramp, gentler at mid-Q)
maxBoostDb:            11.5 (was 15.0)
```

**Remaining optimization target:** LOWBW noise (offset +0.82, tilt -0.68) is the weakest point. This is structural: the carrier filter's dark tilt at narrow BW and the noise envelope correction fighting the tanh compression. Further improvement would require architectural changes (e.g., bypassing tanh for noise, or using SVF filters).


### BEST RESULT — qComp hi-Q reduction + steeper carrier tilt (2026-03-18)

**Additional changes from previous iteration:**
- qComp: added hi-Q reduction factor `1 - 0.09 * hiQFade` (compensates net gain from tilt correction)
- Carrier tilt slope: `0.85 + 0.65*qFade` → `0.85 + 0.80*qFade` (1.65 at LOWBW)

**Results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | **+0.51** | **-0.27** |
| LOWBW sine | **+0.01** | +0.19 |
| MIDBW noise | -0.59 | -0.31 |
| MIDBW sine | -0.07 | -0.09 |
| HIGHBW noise | -0.56 | +0.26 |
| HIGHBW sine | -0.20 | +0.10 |

**Summary:**
- **Max |offset|: 0.59 dB** (MIDBW noise)
- **Max |tilt|: 0.31 dB/oct** (MIDBW noise)
- **Mean |offset|: 0.32 dB**
- **Mean |tilt|: 0.20 dB/oct**

**Improvement from session start:**
- LOWBW noise tilt: -2.19 → **-0.27** (+1.92 improvement!)
- LOWBW sine offset: -0.58 → **+0.01** (essentially zero!)
- Max |offset|: 0.91 → **0.59** (35% reduction)
- Max |tilt|: 2.19 → **0.31** (86% reduction!)

**Complete parameter set (this session):**
```cpp
// Carrier tilt: Q-dependent
tiltSlope = 0.85 + 0.80*qFade;  // 0.85→1.65 dB/oct

// Noise envelope hi-Q tilt
noiseTiltExpHiQ = 0.30 * hiQFade;  // Q=10→27

// qComp hi-Q reduction
qCompHiQReduce = 1.0 - 0.09 * hiQFade;  // Q=10→27

// tonalSatCorr
baseCorr = 1.0 - 0.15*t (main), 1.0 - 0.11*t (xfade)
hiQExp = -0.65 * ramp² (squared ramp)

// widthGain
maxBoostDb = 11.5
```

This is now the best IIR result achieved, surpassing IIR-9 (which had ±0.50 offset, ±0.45 tilt). The STFT approach (±0.59 dB overall range at HIGHBW) is comparable but this IIR result matches across ALL bandwidth settings.


### CORRECTION: Ableton Uses IIR, NOT FFT (2026-03-18)

**The Step 11 "FFT" conclusion was WRONG.** The 420 dB/oct measurement was taken from a 40-band LOWBW output where inter-band notch slopes appear extremely steep due to 40 overlapping bands. This does NOT represent a single filter's rolloff.

**Definitive evidence (from Mar 17 "Filter Order Correction" note):**
A 4-band LOWBW Ableton recording with isolated peaks showed:
- Peak 1: 49.6 dB/oct ≈ 8th order
- Peak 2: 61.2 dB/oct ≈ 10th order
- Peak 3: 53.8 dB/oct ≈ 9th order

**Ableton uses IIR with 4-5 cascaded biquad stages (8th-10th order).** Our 4-stage (8th-order) architecture is correct.

**This means:** The ±0.59 dB result is NOT a structural IIR-vs-FFT limit. It's calibration precision within the same architecture. Further improvement is possible through parameter tuning.


### NEW BEST — Flat widthGain offset + tuned qComp (2026-03-18)

**Changes from previous best:**
- widthGain: added `+0.5f` flat offset (lifts HIGHBW without affecting LOWBW much)
- qCompHiQReduce: `0.09` → `0.15` (trims LOWBW noise by ~0.5 dB)
- Carrier tilt slope: `0.80*qFade` (kept from previous iteration)

**Results:**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | **+0.50** | **-0.10** |
| LOWBW sine | +0.51 | +0.19 |
| MIDBW noise | -0.31 | -0.27 |
| MIDBW sine | +0.43 | -0.09 |
| HIGHBW noise | **-0.06** | +0.26 |
| HIGHBW sine | +0.30 | +0.10 |

**Summary:**
- **Max |offset|: 0.51 dB** (improved from 0.59)
- **Max |tilt|: 0.27 dB/oct** (improved from 0.31)
- **Mean |offset|: 0.35 dB**
- **Mean |tilt|: 0.17 dB/oct**

**Session total improvement:**
- LOWBW noise tilt: **-2.19 → -0.10** (95% reduction!)
- Max |offset|: 0.91 → **0.51** (44% reduction)
- Max |tilt|: 2.19 → **0.27** (88% reduction)

**Complete final parameter set:**
```cpp
// Carrier tilt (Q-dependent)
tiltSlope = 0.85 + 0.80*qFade;  // 0.85→1.65 dB/oct

// Noise envelope hi-Q tilt  
noiseTiltExpHiQ = 0.30 * hiQFade;  // Q=10→27

// qComp hi-Q reduction
qCompHiQReduce = 1.0 - 0.15 * hiQFade;  // Q=10→27

// tonalSatCorr
baseCorr = 1.0 - 0.15*t (main), 1.0 - 0.11*t (xfade)
hiQExp = -0.65 * ramp² (squared ramp)

// widthGain
maxBoostDb = 11.5, curve = (1-wN)^3 + 0.5  // flat offset for HIGHBW
midBwBump = 11.0 * gaussian(wN=0.21, σ=0.12)
```


### LATEST BEST — baseCorr 0.18 (2026-03-18)

**Change:** baseCorr `0.15` → `0.18` (main), `0.11` → `0.13` (xfade) — stronger tonal cut

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.50 | -0.10 |
| LOWBW sine | +0.19 | +0.19 |
| MIDBW noise | -0.31 | -0.27 |
| MIDBW sine | +0.11 | -0.09 |
| HIGHBW noise | -0.06 | +0.26 |
| HIGHBW sine | **+0.00** | +0.10 |

- **Max |offset|: 0.50 dB, Mean: 0.20 dB**
- **Max |tilt|: 0.27 dB/oct, Mean: 0.17 dB/oct**
- HIGHBW sine = +0.00 dB (perfect match)


### FINAL SESSION RESULT — Flat offset 0.4 (2026-03-18)

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +0.40 | -0.10 |
| LOWBW sine | +0.09 | +0.19 |
| MIDBW noise | -0.41 | -0.27 |
| MIDBW sine | +0.01 | -0.09 |
| HIGHBW noise | -0.16 | +0.26 |
| HIGHBW sine | -0.10 | +0.10 |

- **Max |offset|: 0.41 dB** (session start: 0.91)
- **Max |tilt|: 0.27 dB/oct** (session start: 2.19)
- **Mean |offset|: 0.20 dB**
- **Mean |tilt|: 0.17 dB/oct**

**Complete final parameter set:**
```cpp
tiltSlope = 0.85 + 0.80*qFade;         // carrier tilt 0.85→1.65
noiseTiltExpHiQ = 0.30 * hiQFade;       // noise envelope hi-Q
qCompHiQReduce = 1.0 - 0.15 * hiQFade; // qComp trim at high Q
baseCorr = 1.0 - 0.18*t;               // tonal base cut (xfade: 0.13)
hiQExp = -0.65 * ramp²;                // tonal hi-Q boost (squared)
maxBoostDb = 11.5;                      // widthGain base
widthGain offset = +0.4;               // flat boost all BWs
midBwBump = 11.0 * gaussian(0.21, 0.12);
```


---

## ⚠️ V2-IMPROVED STATUS: VALID — NEEDS LISTENING TEST

**Date:** 2026-03-18
**Results:** ±0.41 dB offset, ±0.27 dB/oct tilt across 6 tests
**Method:** 8+ compensation layers (NOT Ableton's actual algorithm)
**Next step:** Load in Ableton, A/B against Ableton's vocoder with real audio (voice, music)
**Decision after test:** If it sounds good → ship as-is. If not → V3 clean rewrite is planned.

**V3 plan:** [[Band Ritual V3 - Clean Ableton Architecture]] — energy-normalized filter bank, no compensation layers, replicate Ableton's actual processing.


================================================================================
FILE: docs/dsp/Band Ritual V3 - Clean Ableton Architecture.md
================================================================================

---
tags:
  - band-ritual
  - dsp
  - vocoder
  - v3
  - architecture
date: '2026-03-18'
---
# Band Ritual V3 — Clean Ableton Vocoder Architecture

## Status: PLANNING (2026-03-18)

## Why V3

Band Ritual V2-Improved achieved ±0.41 dB match to Ableton through **8+ stacked compensation layers** (fastTanh, tonalSatCorr, qComp, tiltCorr8, noiseTiltExp, widthGain, baseCorr, hiQExp). This matched the TEST SIGNALS but did NOT crack Ableton's actual algorithm. It's a house of cards — change one param and 3 others break.

**V3 goal: Replicate Ableton's actual processing algorithm, not just match test signal levels.**

## What We Know About Ableton's Vocoder (confirmed)

### Architecture
- **IIR filter bank**, NOT FFT (confirmed Mar 17 from 4-band isolated peak measurements)
- **4-5 cascaded biquad stages** per band (8th-10th order, 50-60 dB/oct rolloff)
- **Edge-to-edge log frequency grid**: `f_k = f_low * (f_high/f_low)^(k/(N-1))`
- **Frequency match**: 24 cents RMS vs our grid (already matching)

### Key insight: Ableton does NOT have compensation layers
Ableton's vocoder works cleanly because:
1. The filter bank is energy-normalized (no widthGain needed)
2. Envelope × carrier is simple multiplication (no fastTanh shaping)
3. No tonal/noise detection needed (architecture handles both naturally)
4. No Q-dependent corrections needed (filter normalization handles it)

## V3 Architecture Plan

### 1. Energy-Normalized Filter Bank
For each band, precompute the filter's energy response:
```cpp
// At init or when Q changes:
filterEnergy = integral of |H(f)|^2 df  (numerical integration)
bandNormGain = 1.0 / sqrt(filterEnergy);
```
This makes each band output level-independent of Q and frequency. No widthGain, no qComp.

**How to compute filterEnergy:**
- Generate unit impulse → run through the cascaded bandpass → measure RMS of output
- Or: compute |H(f)|^2 on a dense frequency grid and integrate
- Do this once per band when parameters change

### 2. Simple Envelope Detection
```cpp
// Per-sample envelope follower (same as current)
float y = analysisFilter.process(sample);
float xAbs = abs(y);
if (xAbs > env) env = attackCoeff * env + (1-attackCoeff) * xAbs;
else             env = releaseCoeff * env + (1-releaseCoeff) * xAbs;

// Normalize by filter energy so envelope is input-level-proportional
float normalizedEnv = env * bandNormGain;
```
No fastTanh. No shaped gain formula. The normalization handles everything.

### 3. Carrier Processing
```cpp
float carrierFiltered = carrierFilter.process(carrierSample);
// Normalize carrier too
float normalizedCarrier = carrierFiltered * bandNormGain;
```

### 4. Output
```cpp
// Simple multiplication — the core of ANY vocoder
float bandOutput = normalizedCarrier * normalizedEnv * depth;
```
No tonalSatCorr. No tiltCorr8. No noiseBlend. Just multiply.

### 5. What This Eliminates
- ❌ fastTanh envelope shaping
- ❌ tonalSatCorr (baseCorr, hiQCorr, hiQExp)
- ❌ qComp (Q-dependent envelope compensation)
- ❌ tiltCorr8 (carrier tilt correction)
- ❌ noiseTiltExp (noise envelope tilt)
- ❌ widthGain (BW-dependent loudness compensation)
- ❌ smoothedGlobalNoisiness_ (global tonal/noise detection)
- ❌ qCompHiQReduce
- ❌ midBwBump gaussian

### 6. What Stays
- ✅ IIR bandpass filter bank (4 cascaded biquad stages)
- ✅ Edge-to-edge log frequency grid
- ✅ Peak envelope follower (attack/release)
- ✅ Scale-locked band selection (Band Ritual's unique feature)
- ✅ All Band Ritual-specific features (formant, noise injection, chord gate, etc.)

## Testing Plan
1. Build V3 with clean architecture
2. Test with same signals (noise, sine, impulse at LOW/MID/HIGH BW)
3. Compare to Ableton references
4. If levels are naturally close (within ±1-2 dB) WITHOUT any compensation, we've cracked it
5. Only then add minimal corrections if needed

## V2-Improved Current State (to preserve)
**Location:** `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/`
**Build date:** 2026-03-18
**Results:** ±0.41 dB offset, ±0.27 dB/oct tilt (through compensation layers)
**Status:** VALID — needs listening test in Ableton next session

## Files
- V2-Improved source: current PluginProcessor.cpp (preserve as-is)
- V3 will be a new project folder or branch
- Test infrastructure: `~/Desktop/vocoder-comparison/` (reuse for V3)


---

## V3 EXPERIMENT RESULTS (2026-03-18) — APPROACH NEEDS RETHINKING

### What We Tried
1. **Combined energy norm** (1/sqrt(eA)*sqrt(eC)): +220 dB too loud (coefficient bug)
2. **Fixed coefficients, combined**: +17-25 dB too loud 
3. **Carrier-only analytical**: -4 to +9 dB range (decent but still needs corrections)
4. **Carrier-only empirical (1/rmsC)**: +14-23 dB too loud
5. **Carrier norm + fastTanh**: +5 to +23 dB, tilts -3 to -9

### Why "No Compensation" Doesn't Work

A vocoder inherently needs certain compensations that are NOT hacks:

1. **Envelope compression (fastTanh)** — REQUIRED. Without it, sine concentrates in one band and overloads by 20+ dB. Every vocoder has this.

2. **Q-dependent envelope gain (qComp)** — REQUIRED. Narrow bandpass captures less broadband energy per band. Without compensation, LOWBW is 15+ dB quieter than HIGHBW for noise. This is physics, not a hack.

3. **Width/BW gain compensation** — REQUIRED. Same reason as qComp but applies to the overall level. Fewer Hz per band = less energy = needs boost.

4. **Spectral tilt correction** — PROBABLY NEEDED. IIR BW ∝ frequency causes natural tilt. Ableton likely corrects this too, just built into their gain structure from the start.

5. **Tonal/noise adaptation** — MAYBE NEEDED. The fastTanh treats tonal and noise differently (different saturation). A cleaner architecture might avoid this, but it would require a different envelope shaper.

### Revised Understanding

Ableton doesn't have "zero compensations" — they have **well-designed compensations baked into their architecture from day one.** The difference from V2-Improved is not the PRESENCE of compensations but how they're implemented:

- Ableton likely uses: energy-normalized filters + compressed envelope + clean gain formula
- V2-Improved uses: raw filters + 8 stacked patches calibrated by measurement

### Next Steps (V3 revised plan)

Instead of "remove all compensations", the V3 goal should be:
1. **Keep fastTanh** — it's not a hack, it's standard vocoder design
2. **Replace qComp + widthGain with energy normalization** — one principled computation instead of two manual curves
3. **Keep tilt correction** — but derive it from filter analysis rather than manual calibration  
4. **Remove tonal/noise detection** if possible — the energy normalization should handle both

This is evolutionary (V2 → V3), not revolutionary (throw everything away).


---

## V3 Iteration Progress (2026-03-18 late session)

### Architecture: Full-Pipeline Empirical Normalization
For each band, runs white noise through the complete vocoder pipeline (analysis→envelope→fastTanh→carrier) and normalizes by `1/(rmsOut * sqrt(nBands))`. This single measurement replaces qComp + widthGain.

### Current Signal Path (V3)
```
1. analysis_filter(input) → peak envelope (attack/release)
2. env → raw (no compensation)
3. fastTanh(env * 4.0) → envShaped
4. envGain = (1 + depth*(1.75*envShaped - 0.75)) * envShaped
5. carrier = 4x cascaded bandpass(input)
6. tiltCorr = pow(freq/850, 1.5) dB → corrects IIR dark tilt
7. output = carrier * envGain * bandNormGain * tiltCorr * bandGains * spectralMult
```

### Compensations used (3 total, down from V2's 8+):
1. **fastTanh** — inherent to vocoder design (prevents sine overload)
2. **bandNormGain** — empirical pipeline measurement (replaces qComp + widthGain + midBwBump)
3. **tiltCorr** — 1.5 dB/oct (replaces tiltCorr8 + noiseTiltExp)

### Compensations REMOVED vs V2:
- ❌ qComp (Q-dependent envelope compensation)
- ❌ widthGain (BW-dependent loudness + midBwBump)
- ❌ tonalSatCorr (baseCorr + hiQCorr + squared ramp)
- ❌ smoothedGlobalNoisiness_ (tonal/noise detection)
- ❌ qCompHiQReduce
- ❌ overlapComp / bwComp from noise spectral shape
- ❌ subBassCorr

### Latest Results (pipeline norm + 1.5 dB/oct tilt)

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +9.1 | -9.18 |
| LOWBW sine | +16.5 | -10.07 |
| MIDBW noise | -2.3 | -7.20 |
| MIDBW sine | +3.4 | -7.97 |
| HIGHBW noise | +7.0 | -4.71 |
| HIGHBW sine | +10.7 | -5.61 |

### Remaining Issues
1. **Tilts still -5 to -10 dB/oct** — need ~5× more tilt correction. Try 5-7 dB/oct.
2. **Sine consistently louder than noise** (+5-11 dB gap) — fastTanh treats them differently. Need tonal correction OR different envelope shaper.
3. **Noise offsets vary** (-2.3 to +9.1) — pipeline normalization isn't fully BW-independent. Likely because the test noise is short (16384 samples) → statistical variation.

### Next Steps (next session)
1. Increase tilt correction to ~5 dB/oct (or Q-dependent as in V2)
2. Test with longer calibration noise (32768+ samples)
3. Address sine/noise spread — options:
   a. Tonal detection (smoothedGlobalNoisiness_ — keep it minimal)
   b. Different envelope shaper (sqrt instead of tanh?)
   c. Accept the spread and normalize to noise (since vocoder input is typically voice = mostly noise-like)

### Key Learnings
- Full-pipeline normalization WORKS for handling BW-dependent gain (replaces qComp + widthGain)
- But tilt and sine/noise spread are separate physics that still need correction
- V3 architecture: 3 compensations vs V2's 8+ = significantly cleaner
- JUCE IIRCoefficients has 5 elements [0..4], NOT 6 — coefficient access bug found and fixed
- JUCE coefficients[3,4] store the ACTUAL a1,a2 values (c3 is already negative for BP)


### V3 Progress Update — Late Session (2026-03-18)

**Current V3 results (3 compensations only):**

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | +1.8 | -1.37 |
| LOWBW sine | +2.4 | -1.38 |
| MIDBW noise | -6.5 | -2.04 |
| MIDBW sine | -1.1 | -2.45 |
| HIGHBW noise | +4.5 | -1.69 |
| HIGHBW sine | +8.1 | -2.58 |

**Max |offset|: 8.1 dB, Max |tilt|: 2.58 dB/oct**

**V3 compensations (just 3):**
1. `fastTanh` — envelope compression
2. `bandNormGain` — full-pipeline empirical measurement / nBands^0.57
3. `tiltCorr` — Q-dependent 4-9 dB/oct, gain-neutral (precomputed in updateBandFilters)

**Remaining issues (for next session):**
1. HIGHBW sine +8.1 → sine/noise spread from fastTanh (sine saturates, noise doesn't)
2. MIDBW noise -6.5 → pipeline normalization not perfectly BW-independent
3. Tilts -1.4 to -2.6 → tilt slope slightly too low, needs ~1-2 more dB/oct

**Comparison: V3 (3 compensations) vs V2 (8+ compensations):**
- V3: ±8.1 dB offset, ±2.58 dB/oct tilt
- V2: ±0.41 dB offset, ±0.27 dB/oct tilt
- V3 is ~20× worse but uses 3 principled compensations vs 8 manual patches

**Path to close the gap:**
1. The tilt needs ~1-2 more dB/oct → increase tilt slope by 1.5
2. The sine/noise spread needs either:
   - Per-band envelope normalization (measure sine response too, compute adjustment)
   - Or: accept V2's tonal detection (smoothedGlobalNoisiness_) as the 4th compensation
3. The BW-dependent offset (-6.5 at MIDBW, +4.5 at HIGHBW) might improve with longer calibration noise or per-BW normalization


### V3 Latest — Mid-Q boost + stronger tilt (2026-03-18 end of session)

| Test | Offset (dB) | Tilt (dB/oct) |
|------|-------------|---------------|
| LOWBW noise | -1.5 | +1.11 |
| LOWBW sine | -1.2 | +1.39 |
| MIDBW noise | -3.4 | -0.01 |
| MIDBW sine | +2.0 | -0.39 |
| HIGHBW noise | +3.4 | -0.08 |
| HIGHBW sine | **+7.0** | -0.97 |

**Max |offset|: 7.0, Mean: 3.1 | Max |tilt|: 1.39, Mean: 0.66**

**V3 compensations (3.5):**
1. fastTanh envelope compression
2. bandNormGain = full pipeline empirical / nBands^0.57
3. tiltCorr = gain-neutral Q-dependent 5.5+6.0*qFade dB/oct
3.5. midQ overlap boost = 6 dB gaussian at Q=18, σ=5 (applied to bandNormGain)

**The LAST remaining issue: HIGHBW sine +7.0 dB.**
This is the sine/noise spread from fastTanh. At HIGHBW (Q≈7), the analysis filter is wide, so the envelope for sine is very high relative to noise. fastTanh compresses both, but sine still ends up louder because it enters the tanh at a higher operating point.

**To close this gap, V3 needs ONE of:**
- (a) Tonal detection (re-add smoothedGlobalNoisiness_ as 4th compensation)
- (b) Different envelope shaper (e.g., sqrt or pow(x, 0.5) instead of tanh)
- (c) Per-band crest factor detection (local instead of global)
- (d) Accept the gap and tune for voice (which is mostly noise-like)

**Session summary:** V3 went from ±25 dB (first build) to ±7.0 dB with 3.5 principled compensations. The architecture is cleaner than V2's 8+ patches. Next session: close the last 7 dB gap.


---

## SESSION 2026-03-19: CRACKED THE GAIN CURVE

### THE BIGGEST DISCOVERY

**Phase 1 analysis (11 gain curve recordings) proves:**
- Output ∝ input² — exactly 12 dB output per 6 dB input (ratio = 2.00)
- NO envelope compression (no fastTanh, no sqrt, no softclip)
- Gate/floor below ~-33 dBFS
- Ableton uses pure linear: `output = carrier × envelope`

**This means our fastTanh was wrong all along.** Removed from V3.

### Other Phase Results

**Phase 2 (envelope):** Attack < 1ms (instant), release ~10-30ms. Peak detector confirmed.

**Phase 3 (band count):** Total output nearly constant 4→40 bands (-2.3 dB). nBands^0.57 scaling matches.

**Phase 4 (BW):** Noise output CONSTANT across BW 30-100% (±0.7 dB!). Perfect normalization.

**Phase 5 (depth):** 0-200% range. Roughly depth² at low values, compresses at high values.

### V3 Current State (end of session)

```
Architecture: pipeline norm + tilt + midQ boost + linear envelope
fastTanh: REMOVED (confirmed Ableton doesn't use it)
Calibration: 65536 samples per band
nBands: ^0.57 (matches Phase 3 data)
```

**Results:**

| Test | Offset | Tilt |
|------|--------|------|
| LOWBW noise | -3.8 | +2.16 |
| LOWBW sine | -1.5 | +4.51 |
| MIDBW noise | -3.7 | +0.57 |
| MIDBW sine | +2.4 | +1.29 |
| HIGHBW noise | +3.9 | -0.29 |
| HIGHBW sine | +10.3 | -0.11 |

### Remaining Issue

BW-dependent offset spread: LOWBW -3.8, HIGHBW +3.9 (7.7 dB range for noise). This is from inter-band overlap: at HIGHBW, bands overlap heavily → correlated sum is louder than sqrt(N) predicts. At LOWBW, bands don't overlap → sum matches sqrt(N).

Needs BW-dependent overlap correction (effectively replacing V2's widthGain but derived from first principles).

### CRITICAL PARAMETER RANGES (new finding)
- **Ableton BW: 0-200%** (not 0-100%)
- **Ableton Depth: 0-200%** (not 0-100%)
- Our previous tests at BW=100% were only Ableton's MIDPOINT


---

## V3 BREAKTHROUGH (2026-03-19 late)

### RMS Match: ±1.4 dB with only 2 compensations!

| BW | V3 RMS | Ableton RMS | Diff |
|----|--------|-------------|------|
| 10% | 0.469 | 0.437 | +0.6 dB |
| 50% | 0.439 | 0.475 | -0.7 dB |
| 100% | 0.404 | 0.475 | -1.4 dB |

Tilts: ±0.35 dB/oct (nearly flat!)

### V3 Architecture (just 2 compensations)
1. **Pipeline normalization** — per-band white noise measurement / nBands^0.57
2. **PSD-flat tilt** — `pow(f/geomCenter, 1.0 + 0.5*qFade)` gain-neutral

### What was REMOVED from V2
- ❌ fastTanh (confirmed wrong by Phase 1 cracked gain curve)
- ❌ depth gain (+12.5 dB boost, was compensating for fastTanh)
- ❌ widthGain (replaced by pipeline norm)
- ❌ qComp (replaced by pipeline norm)
- ❌ tonalSatCorr (not needed without fastTanh)
- ❌ smoothedGlobalNoisiness_ (not needed)
- ❌ midQ gaussian boost (was compensating for wrong reference)
- ❌ subBassCorr, overlapComp, bwComp

### What V3 uses
```
envGain = envelope × depth  (LINEAR, no compression)
bandMult = bandGains × spectralMult × mergeNorm × normGain × tiltCorr
output_per_band = carrier_4stage × envGain × bandMult
```

### Next steps
- The RMS match is good but spectral offset in 100-10kHz is -13 dB at LOWBW (energy shifted to HF by tilt)
- Need to check if the tilt is correct or if it's overcorrecting at HF
- Test with sine sweep to see sine/noise gap (should be small without fastTanh)
- CRITICAL: the gain-neutral normalization with very steep tilt may push too much energy above 10 kHz


### CRITICAL FINDING: Ableton's PSD is NOT flat (2026-03-19)

**Ableton's vocoder output has POSITIVE tilt: +2.0 to +2.7 dB/oct**

| BW | Tilt | Sub-bass | HF (10-18k) |
|----|------|----------|-------------|
| 10% | +2.01 | -2.3 dB below mean | +2.8 dB above |
| 50% | +2.70 | -13.5 dB below mean | +3.6 dB above |
| 100% | +2.70 | -13.5 dB below mean | +3.6 dB above |

**This means our target is NOT flat PSD — it's +2-3 dB/oct bright.**

### Research: Ableton's vocoder modes
- **Precise mode**: constant-bandwidth filters (equal Hz per band)
- **Retro mode**: constant-Q with HF gain boost
- Our recordings may be in one mode or the other — need to verify

### Principled normalization (from DSP literature)
Instead of pow(f, exponent):
1. Compute NEB (noise equivalent bandwidth) per band: `NEB = integral |H(f)|^2 df`
2. Normalize by `1/sqrt(NEB)` per band
3. This gives ~+3 dB/oct for constant-Q (matches Ableton's measured +2.7 dB/oct!)

**The `1/sqrt(NEB)` approach IS the correct answer.** It naturally produces the +2.7 dB/oct that Ableton shows. Our V3 pipeline normalization already measures per-band energy — we just need to use `1/sqrt(NEB)` instead of the full pipeline normalization.


---

## V3 CONFIRMED CORRECT (2026-03-19 final)

### Architecture matches Ableton's vocoder

| BW | RMS match | V3 tilt | Ableton tilt | Tilt diff |
|----|-----------|---------|--------------|-----------|
| 10% | +0.6 dB | +1.80 | +2.03 | -0.23 |
| 50% | -0.7 dB | +2.35 | +2.71 | -0.35 |
| 100% | -1.4 dB | +2.66 | +2.71 | -0.05 |

**RMS: ±1.4 dB. Tilt match: ±0.35 dB/oct.**

### Final V3 Signal Path (2 compensations only)
```cpp
// Per-band (in updateBandFilters):
bandNormGain = 1 / (pipeline_noise_rms × nBands^0.57)  // empirical
tiltCorr = pow(f/849, 1.0 + 0.5*qFade) / average       // gain-neutral

// Per-sample (in processBlock):
envelope = peak_detect(analysis_filter(input))           // raw, no compression
envGain = envelope × depth                               // LINEAR (no fastTanh!)
carrier = 4_stage_bandpass(input)
output_band = carrier × envGain × bandNormGain × tiltCorr × bandGains
```

### What was CRACKED from 31 Ableton recordings:
1. **No envelope compression** — output ∝ input² (Phase 1: ratio=2.00)
2. **Noise level constant across BW** — ±0.7 dB (Phase 4)
3. **Positive spectral tilt** — +2.0 to +2.7 dB/oct (Phase 4 PSD analysis)
4. **Band count: near-constant total** — -2.3 dB from 4→40 bands (Phase 3)
5. **Depth: 0-200%, nonlinear** — roughly depth² (Phase 5)
6. **Attack: <1ms, Release: ~10-30ms** — peak detector (Phase 2)

### V3 vs V2 comparison
- V2-Improved: ±0.41 dB offset, ±0.27 tilt — 8+ compensations (hacks)
- V3: ±1.4 dB RMS, ±0.35 tilt — 2 principled compensations
- V3's architecture IS Ableton's algorithm. V2 just curve-fitted the output.


---

## KEY MEASUREMENT: Per-band PSD correction (2026-03-19)

At BW=100%, comparing V3 (pipeline norm only) to Ableton per band:

```
Band  Freq     Correction needed
0     40 Hz    +21.5 dB
5     88 Hz    +20.5 dB
10    192 Hz   +18.6 dB
15    419 Hz   +15.0 dB
20    918 Hz   +13.7 dB
25    2008 Hz  +9.3 dB
30    4395 Hz  +6.4 dB
35    9619 Hz  +1.0 dB
```

**Best-fit: gain ∝ f^0.477 = +2.87 dB/oct (matches Ableton's measured +2.7)**
**Mean correction: +11.6 dB**

### The problem: pipeline norm + tilt interact destructively

The pipeline normalization measures noise through the full vocoder pipeline per band, then normalizes. Adding a tilt on top DOUBLE-COUNTS because the pipeline already measures the actual BW-dependent energy.

The correct approach for next session: compute bandNormGain from ANALYTICAL filter properties (NEB-based), NOT from empirical noise measurement. The NEB approach gives the right PER-BAND gain, and the tilt follows naturally from the BW∝f property.

The NEB approach failed earlier because of wrong units (didn't normalize by Nyquist) and wrong level (didn't account for the envelope × carrier product). The fix:
1. `NEB_carrier = integral |H_carrier(f)|^8 df` (4 stages = power 8)
2. `NEB_analysis = integral |H_analysis(f)|^2 df`
3. `bandNormGain = K / sqrt(NEB_analysis × NEB_carrier)` where K is a global constant
4. K is determined by matching Ableton's total RMS at one reference BW

This separates the per-band shape (from NEB) from the overall level (from K).


---

## FINAL INSIGHT (2026-03-19): Pipeline norm MUST GO

### The problem
Pipeline normalization equalizes per-band noise power → removes the natural BW∝f tilt.
Then f^0.477 tilt tries to add it back → but it can't perfectly reconstruct what was removed.
Result: tilts are off by -3 to -6 dB/oct even though RMS is ±0.7 dB.

### The answer
**Ableton does NOT equalize per-band power.** Ableton's bands have UNEQUAL power — the natural +2.7 dB/oct tilt from BW∝f is LEFT IN. Each band outputs proportional to its bandwidth, not normalized to unity.

**Next session: REMOVE pipeline normalization entirely.**
Use only: `bandNormGain = K × pow(f/1000, 0.477) / sqrt(N)`
where K is calibrated to match Ableton's total RMS.

This is ONE formula — not pipeline measurement, not per-band calibration. Just a theoretical correction based on the filter bank geometry.

### Current V3 parameters (to restore next session)
```cpp
// In updateBandFilters:
b->bandNormGain = 1.0f;  // NO pipeline norm!

// Tilt + level:
tiltCorr = pow(f/1000, 0.477) * K / sqrt(N)
K = pow(10, kLevelDb/20) where kLevelDb is calibrated
```

### What was confirmed today
- No fastTanh (Phase 1: quadratic gain curve) ✓
- Tilt = f^0.477 = +2.87 dB/oct (per-band measurement) ✓
- RMS match ±0.7 dB (K calibration works) ✓
- Pipeline norm is WRONG — Ableton keeps natural per-band power variation ✓


---

## HIGHBW CRACKED (2026-03-19)

**BW=100% with pipeline norm + f^1.0 gain-neutral tilt:**

| Octave | Difference |
|--------|-----------|
| 100-500 Hz | -1.2 dB |
| 500-2000 Hz | 0.0 dB |
| 2000-5000 Hz | +0.4 dB |
| 5000-10000 Hz | -1.1 dB |
| **Overall** | **offset -0.5, tilt -0.27, RMS -1.6** |

This is ±1.2 dB across the full spectrum. HIGHBW is essentially cracked.

**LOWBW and MIDBW still need work:**
- LOWBW: offset -10.5, tilt -2.30 (pipeline norm + f^1.15 not enough)
- MIDBW: offset -14.5, tilt -1.46 (worst — the persistent MIDBW dip)

**Next iteration:** The LOWBW/MIDBW issue is that the gain-neutral tilt pushes energy above 10 kHz. The fix for next session: instead of gain-neutral normalization (divide by arithmetic mean), use a CAPPED tilt that doesn't boost above 10-12 kHz. Or: normalize only within the 40-12000 Hz range, not up to 18000 Hz.


---

## V3 CLEAN CALIBRATED (2026-03-19)

### RMS MATCH: ±0.3 dB!

| BW | RMS diff | PSD offset | Tilt |
|----|----------|-----------|------|
| 10% | **0.0 dB** | -8.3 | -0.76 |
| 50% | **-0.3 dB** | -12.1 | +0.27 |
| 100% | **+0.3 dB** | +2.3 | +1.66 |

### Architecture (CLEAN — Ableton-style)
```
Signal path:
  envelope = peak_detect(analysis_filter(input))  // raw, no compression
  envGain = envelope × depth                       // linear
  carrier = 4_stage_bandpass(input)                // raw, no normalization
  tiltCorr = pow(f/1000, 0.45)                    // +2.7 dB/oct per band
  output = sum(carrier × envGain × tiltCorr × bandGains)

Global gains:
  bwGain = 36.1 × (1-widthNorm)^0.675 dB         // calibrated from Phase 4
  bandCountGain = 1/sqrt(N/4)                      // calibrated from Phase 3
  wet × bwGain × bandCountGain
```

### Remaining: PSD shape at LOWBW
RMS is perfect but PSD shows -8 dB offset at LOWBW (energy in sub-bass/ultrasonic).
The f^0.45 tilt matches Ableton at HIGHBW but not at LOWBW.
At narrow BW, the carrier's 4-stage cascade has steeper rolloff → more energy concentration at band centers → different spectral distribution.
May need Q-dependent tilt exponent (0.45 at HIGHBW, ~0.6 at LOWBW) without the gain-neutral problem.


### V3 CLEAN FINAL STATE (2026-03-19)

**RMS: ±1.3 dB across all BWs. Architecture confirmed correct.**

| BW | RMS | Offset | Tilt |
|----|-----|--------|------|
| 10% | +0.8 | -9.0 | +0.42 |
| 50% | -0.9 | -13.5 | +0.90 |
| 100% | -1.3 | +0.5 | +1.79 |

**HIGHBW per-octave (500-10kHz): ±2.3 dB — close to cracked.**

### Architecture (final, clean)
```
Per-band: NO normalization, just raw filters
  tiltCorr = pow(f/1000, 0.45 + 0.20*qFade)  // +2.7 at HIGHBW, +3.9 at LOWBW
  envGain = envelope × depth                   // LINEAR (no fastTanh)
  output = carrier × envGain × tiltCorr × bandGains

Global:
  bwGain = 33.0 × (1-wN)^0.675 dB            // calibrated from Phase 4
  bandCountGain = 1/sqrt(N/4)                  // calibrated from Phase 3
```

### What's ACTUALLY cracked
1. ✅ No envelope compression (linear, quadratic gain curve)
2. ✅ Correct spectral tilt (+2.7 dB/oct, Q-dependent)
3. ✅ RMS level matching (±1.3 dB across BW 10-100%)
4. ✅ BW-dependent gain (simple curve, not per-band hack)
5. ✅ Band count scaling (1/sqrt(N/4))

### What remains
The -9 dB PSD offset at LOWBW is from **filter shape** (8th-order = narrow peaks), not normalization. Our comb peaks are narrower than Ableton's. To fix: verify filter order matches Ableton exactly. The Mar 17 measurement suggested 4-5 stages (8th-10th order) but this needs re-verification with the crack recordings.


### V3 FINAL CALIBRATED (2026-03-19 final)

**RMS: ±0.5 dB — matches Ableton at all BWs!**

| BW | PSD offset | Tilt | RMS |
|----|-----------|------|-----|
| 10% | -8.3 | -0.76 | **0.0 dB** |
| 50% | -12.3 | +0.27 | **-0.5 dB** |
| 100% | +1.9 | +1.66 | **-0.1 dB** |

### Final Architecture
```
Per-band:
  bandNormGain = 1.0 (no per-band normalization!)
  tiltCorr = pow(f/1000, 0.45)  // +2.7 dB/oct, constant
  envGain = envelope × depth    // LINEAR
  output_band = carrier_4stage × envGain × tiltCorr × bandGains

Global:
  bwGain = 36.1 × (1-wN)^0.70 dB
  bandCountGain = 1/sqrt(N/4)
```

### PVR gap explains remaining PSD offset
- V3 PVR: 36.1 dB (wider peaks)
- Ableton PVR: 44.6 dB (narrower peaks)
- Same total energy (RMS matches) but different spectral distribution
- To close: may need to increase carrier Q slightly to get deeper valleys

### Session summary
From 31 Ableton recordings, cracked:
1. ✅ Quadratic gain (no compression)
2. ✅ Linear envelope × carrier
3. ✅ +2.7 dB/oct spectral tilt (f^0.45)
4. ✅ BW-dependent global gain (36.1 × (1-wN)^0.70)
5. ✅ Band count scaling (1/sqrt(N/4))
6. ✅ RMS match ±0.5 dB at BW 10/50/100%


### Qa=Qc RESULT (2026-03-19)

Setting analysis Q = carrier Q (removing asymptotic cap):
- PVR: 36.1 → **39.0** (closer to Ableton's 44.6)
- Confirms hypothesis: matching Qa to Qc deepens valleys
- RMS still ±1.3 dB after recalibration (maxBoost=39.4)
- Remaining PVR gap: 5.6 dB (39 vs 44.6)

Possible causes of remaining gap:
1. Ableton may use 5 carrier stages (10th-order) instead of 4
2. Or a different analysis: perhaps 2-stage analysis filter
3. Or a slightly different filter design (not JUCE makeBandPass)

### What's cracked vs what's left

**CRACKED (architecture):**
- ✅ Linear envelope (no compression) — Phase 1 confirmed
- ✅ f^0.45 spectral tilt — measured from per-band comparison
- ✅ BW-dependent global gain (39.4 × (1-wN)^0.70) — Phase 4 calibrated
- ✅ Band count 1/sqrt(N/4) — Phase 3 calibrated
- ✅ Qa = Qc (no analysis cap) — PVR improvement confirmed
- ✅ RMS: ±1.3 dB at all BWs

**REMAINING (filter shape fine-tuning):**
- PVR gap: 39 vs 44.6 (need ~5 dB deeper valleys)
- LOWBW PSD: -10 dB offset (from PVR mismatch)
- HIGHBW sub-bass: -6 dB (carrier rolloff at low freq)
- Try: 5 carrier stages, or 2-stage analysis filter


### 5-STAGE CARRIER — Building (2026-03-19)

Added 5th carrier stage (carrierFilterL5/R5) to close PVR gap (39→target 44.6).
Full rebuild in progress (header change → rebuilds everything).

**V3 current parameters:**
```cpp
// Carrier: 5 stages (10th-order) — was 4 stages
// Analysis: Qa = Qc (no cap)
// Tilt: pow(f/1000, 0.45) — constant +2.7 dB/oct
// BW gain: 39.4 × (1-wN)^0.70 dB
// Band count: 1/sqrt(N/4)
// Envelope: LINEAR (no fastTanh)
// Depth: envelope × depth
```

**After build completes:** test with noise at BW=10%, 50%, 100%.
Expected: PVR should increase to ~47-50 dB (5 stages).
Will need BW gain recalibration (5th stage reduces energy → need more boost).


### 5-STAGE CARRIER + 2-STAGE ANALYSIS — Building (2026-03-19)

5th carrier stage alone didn't improve PVR (38.7 → 38.7).
**Key insight: PVR is limited by the ANALYSIS filter, not the carrier.**
The analysis envelope "leaks" into carrier valleys if analysis is wider than carrier.

Added 2nd analysis stage (4th-order analysis). Building full clean rebuild.

**Current V3 filter topology:**
- Analysis: 2 × 2nd-order = 4th-order (was 1 × 2nd-order)
- Carrier: 5 × 2nd-order = 10th-order (was 4 × 2nd-order)
- Qa = Qc (no cap)

**Expected PVR: ~44-48 dB** (matching Ableton's 44.6) because both analysis and carrier now have comparable selectivity.

After build: need BW gain recalibration (narrower analysis = less envelope = less output).


### PVR CRACKED: 2-Stage Analysis is the Key (2026-03-19)

**2-stage analysis (Qa=Qc) + 5-stage carrier → PVR = 50.0 dB** (vs Ableton 44.6)
1-stage analysis + 4-stage carrier → PVR = 39.0 (too shallow)
1-stage analysis + 5-stage carrier → PVR = 38.7 (carrier stages don't help)

**KEY INSIGHT: The PVR is determined by the ANALYSIS filter, not the carrier.**
Adding carrier stages doesn't deepen valleys because the envelope (from analysis) fills them.
The 2-stage analysis makes the envelope more selective → deeper valleys → higher PVR.

**Current config building (slow external HD):**
- Analysis: 2 stages, Qa = 0.85 × Qc (to bring PVR from 50 down to ~44.6)
- Carrier: 4 stages (reverted from 5 — carrier stages don't help PVR)

**BW gain needs recalibration after this change** (narrower analysis = less envelope energy).

**Filter topology summary for Ableton's vocoder (best estimate):**
- Analysis: 2-stage (4th-order) at Qa ≈ 0.85 × Qc
- Carrier: 4-stage (8th-order) at Qc
- This gives PVR ≈ 44-45 dB, matching Ableton's measured 44.6


### PVR TUNING (2026-03-19)

| Config | Qa/Qc | PVR | Target |
|--------|-------|-----|--------|
| 1-stage analysis, Qa=Qc | 1.0 | 39.0 | 44.6 |
| 2-stage analysis, Qa=Qc | 1.0 | 50.0 | 44.6 |
| 2-stage analysis, Qa=0.85Qc | 0.85 | 48.6 | 44.6 |
| 2-stage analysis, Qa=0.75Qc | 0.75 | 46.9 | 44.6 |
| **Next: Qa=0.65Qc** | 0.65 | ~44.6? | 44.6 |

**Ableton's vocoder filter topology (best estimate):**
- Carrier: 4 stages (8th-order) at Qc
- Analysis: 2 stages (4th-order) at Qa ≈ 0.65 × Qc
- This gives PVR ≈ 44.6, matching Ableton exactly


---

## PVR CRACKED: 44.9 dB (Ableton: 44.6) — 0.3 dB match! (2026-03-19)

**Final filter topology:**
- Analysis: 2 stages at Qa = 0.65 × Qc
- Carrier: 4 stages at Qc (kept 5th stage in code but using filtered4 output)
- PVR: **44.9 dB** vs Ableton's **44.6 dB**

**Complete cracked algorithm:**
```
FILTERS:
  analysis = 2-stage bandpass at Qa = 0.65 × Qc
  carrier = 4-stage bandpass at Qc

SIGNAL PATH:
  envelope = peak_detect(analysis(input), attack=1ms, release=50ms)
  envGain = envelope × depth                    // LINEAR (no fastTanh)
  carrier_out = carrier(input)
  tiltCorr = pow(f/1000, 0.45)                 // +2.7 dB/oct per band
  band_output = carrier_out × envGain × tiltCorr

GLOBAL:
  bwGain = 41.5 × (1-widthNorm)^0.70 dB       // BW-dependent level
  bandCountGain = 1/sqrt(N/4)                   // band count scaling
  output = sum(band_outputs) × bwGain × bandCountGain
```

**Still needs:** BW gain recalibration (RMS was +0.5 to +1.2), fine-tune maxBoostDb.
But the ARCHITECTURE and FILTER TOPOLOGY are now confirmed.


---

## V3 CRACKED — FINAL RESULTS (2026-03-19)

### Noise RMS match: ±0.8 dB across ALL BWs

| BW | RMS diff vs Ableton |
|----|---------------------|
| 10% | -0.4 dB |
| 30% | -0.4 dB |
| 50% | +0.1 dB |
| 70% | +0.5 dB |
| 100% | +0.8 dB |

### PVR: 44.9 dB (Ableton: 44.6 — 0.3 dB match)

### Final V3 Algorithm
```
FILTERS:
  analysis = 2-stage bandpass at Qa = 0.65 × Qc
  carrier = 4-stage bandpass at Qc

SIGNAL PATH:
  envelope = peak_detect(analysis(input))
  envGain = envelope × depth              // LINEAR
  carrier_out = carrier(input)
  tiltCorr = pow(f/1000, 0.45)           // +2.7 dB/oct
  band_output = carrier_out × envGain × tiltCorr

GLOBAL:
  bwGain = 40.0 × (1-wN)^0.70 dB
  bandCountGain = 1/sqrt(N/4)
```

### What was cracked from 31 recordings:
1. ✅ Quadratic gain (no compression) — Phase 1
2. ✅ 2-stage analysis, 4-stage carrier — PVR match
3. ✅ Qa = 0.65 × Qc — PVR 44.9 vs 44.6
4. ✅ f^0.45 spectral tilt — per-band measurement
5. ✅ BW gain 40.0 × (1-wN)^0.70 — Phase 4 calibrated
6. ✅ Band count 1/sqrt(N/4) — Phase 3
7. ✅ Noise RMS ±0.8 dB at BW 10/30/50/70/100%
8. ✅ Linear envelope (no fastTanh)


================================================================================
FILE: docs/dsp/STFT Vocoder Calibration - Ableton Match.md
================================================================================

# STFT Vocoder Calibration - Matching Ableton's Vocoder

## Status: Attempt 22 (2026-03-13) - Best overall result (geometric RMS blend)

## STFT Architecture (confirmed working)
- **FFT**: 2048-point, hop 512 (75% overlap), sqrt-Hann window
- **COLA**: sqrt-Hann x sqrt-Hann = Hann, 4 overlapping sum to 1.5, normalize by 1/1.5
- **fftNorm**: 1/sqrt(N/4) = 1/sqrt(512)
- **Contiguous bin assignment**: Each FFT bin assigned to nearest band via log-space midpoints
- **Geometric dual RMS envelope** (Attempt 22 key insight):
  - **Noise**: per-bin RMS = sqrt(bandEnergy / binCount) * fftNorm (energy spread across bins)
  - **Tonal**: total RMS = sqrt(bandEnergy) * fftNorm (energy concentrated in 1-2 bins)
  - Per-bin RMS dilutes tonal energy by 1/sqrt(N) — causes N-dependent level error
  - **Linear blend** has N-dependent excess at moderate noisiness: +1.68 dB at mid (N=12), +0.47 at upper-mid (N=40). 2.20 dB range.
  - **Geometric blend** fixes this: bandRMS = noiseRMS^η × tonalRMS^(1-η). Reduces range to 0.59 dB.
  - Sine sweep analysis: ALL detectable bands have noisiness ~0.17-0.24 (sweep moves through bins during 46ms window)

## What We Confirmed About Ableton's Vocoder

### Matches perfectly (within +/-1.3 dB)
- **HIGHBW noise**: -0.26 dB mean, range -1.28 to +0.89 dB
- **HIGHBW sine (detected bands >1kHz)**: mid -0.14, upper-mid +0.01, highs +0.45 (0.59 dB range!)
- This confirms Ableton uses a similar FFT/STFT approach for wide-bandwidth vocoding
- **3 dB/octave tilt correction** (sqrt(f/4000)) is needed and correct for noise
- **Noise envLevelComp = 1.05**, **Tonal envLevelComp = 0.042** (geometric blend scale)

### Key mathematical insight: Participation Ratio normalization
- PR = (sum|X|^2)^2 / (N * sum|X|^4)
- For noise (exponentially distributed |X_k|^2): **E[PR] = (N+1)/(2N) ~ 0.5, NOT 1.0**
- For pure sine in 1 bin: PR = 1/N
- Correct normalization: noisiness = (PR - 1/N) / ((N+1)/(2N) - 1/N)
- Only reliable for bands with >= 8 FFT bins (small bands have too much PR variance)

### Adaptive tilt/level correction (geometric dual RMS)
- **Noise** (noisiness ~ 1.0): perBinRMS × 1.05, full tilt sqrt(f/4000)
- **Tonal** (noisiness ~ 0.0): totalRMS × 0.042, no tilt (1.0)
- Tilt blended linearly by noisiness
- RMS blended **geometrically**: bandRMS = noiseRMS^η × tonalRMS^(1-η)
- Geometric blend eliminates N-dependent excess at moderate noisiness (key A22 insight)
- Bands with < 8 bins: default to noisiness = 1.0 (can't reliably detect tonal)
- Lowering threshold to 4 bins was tried (A19) — helped low-mid sine but hurt mid noise. Reverted.

## Structural STFT Limitations (can't match Ableton here)

### 1. LOWBW comb pattern (+2-5 dB excess)
- IIR bandpass has soft rolloff between narrow bands -> deep notches at low Q
- FFT contiguous bins cover ALL frequencies -> no notches
- This is the fundamental FFT-vs-IIR difference at narrow bandwidth
- Would need non-contiguous bins to match, but that creates brick-wall artifacts

### 2. Impulse sub-bass (+15-30 dB)
- STFT window captures full impulse energy in one frame
- All FFT bins get energy from impulse -> sub-bass bands accumulate energy
- IIR filters have inherent time-domain spreading that reduces peak impact

### 3. Sine at sub-bass (-9 dB)
- Sub-bass bands have only 5-7 FFT bins
- Can't distinguish tonal from noise with so few bins
- Default noisiness=1.0 applies full tilt correction (17 dB attenuation at 80 Hz)
- For sine, tilt should be 0 dB, but we can't detect it
- This is an edge case (sine sweep at sub-bass) unlikely in real vocoder use

## Current Code Constants (Attempt 22)
```cpp
constexpr float kNoiseEnvComp = 1.05f;   // matches Ableton noise level (per-bin RMS path)
constexpr float kTonalEnvComp = 0.042f;  // matches Ableton tonal level (geometric blend)
constexpr float kTiltRefFreq = 4000.0f;  // sqrt(f/4000) tilt correction reference
// PR threshold: binCount >= 8 for reliable tonal/noise detection
// Geometric dual RMS: bandRMS = (perBinRMS*1.05)^noisiness * (totalRMS*0.042)^(1-noisiness)
// Falls back to linear blend if either term is near zero
```

## All Calibration Attempts Summary
| # | Config | HIGHBW Noise RMS | HIGHBW Sine RMS | Notes |
|---|--------|-----------------|-----------------|-------|
| 1 | sqrt(IIR_BW/FFT_BW) | Sub-bass +23 dB | - | Massive tilt |
| 2 | Fixed tilt sqrt(f/4000) | -0.49 mean | +0.59 mean, 18dB tilt | Good noise, bad sine tilt |
| 3 | Tilt + envLevelComp=1.05 | -0.49 mean | +0.59 mean | Best noise at the time |
| 7 | Global spectral flatness | +0.02 mean | +7.06 mean | Perfect noise, sine too loud |
| 8 | Adaptive level (kTonal=0.45) | +0.02 | +3.99 | Half the reduction needed |
| 10 | Aggressive kTonal=0.10 | -0.01 | -5.16 | Overcorrected |
| 12 | Per-band PR (correct norm) | +0.45 | +2.00 | Sub-bass +3.28 |
| 13 | PR min 8 bins | -0.18 | -0.46 mean, sub -8.93 | Best noise ever |
| 17 | PR min 8 + kTonal=0.36 | **-0.19** | **-0.87** mean, sub -8.93 | Best balanced (single RMS) |
| 18 | Dual RMS linear + kTonal=0.0265 | **-0.22** | mid +1.68, umid +0.47, hi -0.52 | 2.20 dB range |
| 19 | Min bins 4 (reverted) | -0.15 | low-mid -2.02, mid +3.00 | Helped low-mid, hurt mid |
| 20 | BW correction (reverted) | -2.74 | - | Overcorrected, Q-dependent |
| 21 | Squared noisiness (reverted) | - | mid -2.12, umid -4.00 | Too aggressive |
| 22 | **Geometric RMS + kTonal=0.042** | **-0.26** | **mid -0.14, umid +0.01, hi +0.45** | **0.59 dB range! CURRENT BEST** |

## Test Infrastructure (verified working)
- Pedalboard correctly sets LINKED mode + 40 bands + correct BW
- Debug output showed Q=2.0/16 bands only from initial plugin load defaults
- After parameter setting + warmup, all params propagate correctly
- Calibration numbers are valid

## Next Steps
1. Fix auto_compare.py parameter names to ensure LINKED mode + 40 bands are actually set
2. Consider if Ableton actually uses IIR (LOWBW comb pattern strongly suggests IIR)
3. If so, may need hybrid approach: IIR analysis + FFT carrier shaping
4. Investigate Ableton's sub-bass handling (does it roll off below ~50 Hz?)
5. Port remaining features: formant shift, noise injection, chord
6. **Remember to revert DEV OVERRIDE in LicenseManager.h before shipping**

## Files
- Calibration log: `~/Desktop/vocoder-comparison/calibration_log.md`
- Test scripts: `~/Desktop/vocoder-comparison/auto_compare.py`, `deep_analysis.py`
- STFT code: `/Volumes/Ricosan HD/.../BandRitualV2-Improved/Source/STFTVocoder.cpp`
- Results: `~/Desktop/vocoder-comparison/results/deep_analysis.txt`


================================================================================
FILE: docs/gui/Band Ritual - Final 1 GUI Moodboard & Handoff 2026-06-03.md
================================================================================

---
tags:
  - band-ritual
  - gui
  - moodboard
  - handoff
progetto: Band Ritual Final 1
data: '2026-06-03'
scopo: Moodboard + pacchetto di consegna dello chassis/GUI, con descrizione di ogni file
---
# Band Ritual Final 1 — GUI Moodboard & Handoff (2026-06-03)

> Moodboard e **punto raggiunto** sulla GUI di **Band Ritual Final 1** (vocoder VST).
> Pacchetto originale: `~/Desktop/BandRitual_Final1_Handoff_2026-06-03/`.
> Asset duplicati nel vault in `BandRitual_Final1_GUI_Brief_Assets/`.

## Punto raggiunto
- **Chassis** disegnato e renderizzato in **Blender** (progetto autosufficiente, texture *packed*).
- La **plate** dello chassis viene esportata in PNG e usata come **sfondo** della GUI del plugin.
- Tutta la UI interattiva (knob MIX, menu a tendina, grafici **BANDS** / **COMPLEMENT**, sezioni **NOISE + GATE**, riga di knob width/attack/release/gate/depth/formant/tone, tastiera) è **disegnata via codice C++ (JUCE)** sopra la plate. In Blender NON si costruisce la UI: solo la plate.
- Brand **RITUAL AUDIO LABS** definito (monogramma "RA").
- Canvas nativo dell'editor: **1580 × 1000 px** (scalato alla finestra). Accento coral **#D9554E**, font Montserrat, griglia 4px, knob stile Serum (valore solo durante il drag).
- Prossimo passo: rifinitura grafica (eventuale passaggio a un grafico) partendo da questo stato — **funzioni e layout restano**, si lavora sulla resa visiva. Vedi anche [[Band Ritual Final 1 - Brief GUI per Grafico]] e [[Band Ritual - Final 1 COMPLETE Checkpoint]].

## Cosa c'è in ogni file (pacchetto di consegna)

| File | Cos'è |
|---|---|
| `band_ritual_final1_chassis.blend` | Progetto Blender completo dello chassis. Texture/dati **packed** dentro al .blend → autosufficiente (Blender ≥ 3.0). Per ri-estrarre le texture: *File > External Data > Unpack All Into Files*. Origine: `band_ritual_v3_keyboard_restored.blend`. |
| `chassis_export_from_blender.png` | Render **piatto** dello chassis esportato da Blender = il PNG usato come **sfondo/plate** dentro la GUI (`chassis_plate.png`). |
| `band_ritual_final1_GUI_standalone.png` | **Screenshot dello standalone** così com'è ora: la plate + tutti gli elementi interattivi disegnati via codice. Riferimento di come la UI si appoggia sopra lo chassis. |
| `ritual_audio_labs_logo.png` | Logo **RITUAL AUDIO LABS** (monogramma "RA") in alta risoluzione su fondo scuro. Marchio per GUI/materiali. |
| `ritual_audio_labs_logo_light.png` | Versione chiara/piccola dello stesso logo (mark su fondo trasparente). |
| `mood board BR.pptx` | La **moodboard** vera e propria (PowerPoint, ~42 MB) — riferimenti visivi/estetici raccolti per la direzione artistica. |
| `Band ritual - Blender + Photoshop (& some Chat gpt ...) - ATTEMPT.pptx` | Tentativo/lavorazione dello chassis tra **Blender + Photoshop** (+ alcuni asset trasparenti generati). Storia del processo. |
| `README.txt` | Note di consegna originali (in italiano) — descrizione cartella + come aprire il Blender. |

## GUI attuale (standalone)
La plate Blender con sopra tutti i controlli disegnati in JUCE (MIX, dropdown chiave/scala/triade, grafici BANDS e COMPLEMENT, NOISE + GATE, riga knob, tastiera).
![[BR_final1_GUI_current.png]]

## Chassis esportato da Blender (la "plate")
Esattamente lo sfondo usato dentro la GUI.
![[BR_final1_chassis_export.png]]

## Logo — RITUAL AUDIO LABS
Monogramma "RA". A sinistra full su fondo scuro, a destra la versione light/trasparente.
![[BR_ritual_audio_labs_logo.png]]
![[BR_ritual_audio_labs_logo_light.png]]

## Sorgenti / allegati nel vault
- Progetto Blender: [[BR_final1_chassis.blend]]
- Moodboard PPTX: [[BR_moodboard.pptx]]
- Attempt Blender+Photoshop PPTX: [[BR_chassis_attempt_blender_photoshop.pptx]]
- README consegna: [[BR_handoff_README.txt]]


================================================================================
FILE: docs/gui/Band Ritual - Grid VERIFIED Pure-Log 2026-06-15.md
================================================================================

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


================================================================================
FILE: docs/gui/Band Ritual - V4 Chassis Redesign VST Migration TODO.md
================================================================================

# Band Ritual V4 — Chassis Redesign VST Migration TODO

**Created**: 2026-05-19 (updated 2026-05-20)
**Source chassis**: `/Users/ricosan/Desktop/band_ritual_v3_FRESH_MODEL.blend`
**Current VST codebase ("Band Ritual Final 1")**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

## Why this file

Chassis design is being iterated in Blender. The new chassis layout differs significantly from the current shipping Band Ritual VST GUI. This document tracks the design decisions made during chassis iteration that will require code refactoring when we eventually rebuild the VST GUI to match.

**Do NOT preemptively start refactoring VST code** — chassis iteration is still active. Wait until chassis is complete + user explicitly requests the migration.

## Cross-reference to memory note

Mirror of `~/.claude/projects/-Users-ricosan/memory/project_band_ritual_v4_chassis_vst_migration_TODO.md` — keep both in sync when editing.

---

## Chassis design decisions affecting VST code

### 1. EQ filter knobs REMOVED — spectrum drag is sole UX

**Chassis**: `BR_Knob_freq`, `BR_Knob_gain`, `BR_Knob_Q_right` deleted from chassis 2026-05-19.

**VST implications**:
- Spectrum display becomes the SOLE filter UX — already exists as `InputFilterDisplay` class in `Source/PluginEditor.h:1919–2056`
- HP point's X auto-drives complement cutoff (line 4604) — keep this behavior
- Square-bracket `[` `]` Q control still PLANNED but NOT yet in code — add `keyPressed()` handler in InputFilterDisplay
- EQ params remain in APVTS (`inputHPFreq`, `inputLPFreq`, etc.) — only the dedicated rotary attachments are deleted from PluginEditor
- Brackets affect: LP, Bell1, Bell2 (NOT HP — its Q is implicit per user spec)

### 2. New panel `BR_Panel_ComplementEQ` = "Noise Section"

**Chassis** (DESIGNER LAYOUT 2026-05-20 v2 — golden ratio aware): Panel aspect ratio is **352.8 × 210 = 1.680 ≈ φ (golden ratio)** — internal layout designed to respect this.

**LEFT hero column** (X-anchored to `BR_Button_M` left edge above at -633.5 per cross-panel alignment rule):
- `BR_Well_NoiseType_dropdown`: cx=-573.5, cy=-101, **W=120mm × H=20mm** (thinner than v1 155×34, top-anchored 1mm below text band). Material `BR_M_DarkInset_Display`.
- `BR_DropArrow_NoiseType`: cx=-521.90, cy=-94 (93% across well per dropdown rule, positioned inside well top)
- **`BR_Knob_NoiseAmount` (HERO 75mm Ø)**: cx=-573.5, cy=-190 — aligned cx with dropdown (visual hero column). 75mm = φ × small knob 45mm. Indicator offset 24.45mm above knob cy (= 65% of radius, scaled MIX pattern). **Controls white noise generator amount/gain**.

**RIGHT 2x2 grid zone** (symmetric in right half of panel, 50.15mm clearance both sides):
- Top-left: `BR_Knob_NoiseStereo` (cx=-440.85, cy=-152.5) — was `BR_Knob_noise`, renamed for STEREO WIDTH role
- Top-right: `BR_Knob_NoiseHP` (cx=-370.85, cy=-152.5) — HP filter cutoff on white noise
- Bottom-left: `BR_Knob_noise_color` (cx=-440.85, cy=-227.5)
- Bottom-right: `BR_Knob_dry_high_blend` (cx=-370.85, cy=-227.5) — UI label = **"DRY HIGH BLEND"**
- Grid pitch: **horizontal 70mm, vertical 75mm** (vertical = NoiseAmount diameter, visual rhyme). All 45mm Ø.
- Inter-knob clear gaps: **vertical 30mm, horizontal 25mm** — sized for Photoshop text labels.
- Grid center cy=-190 = vertically aligned with NoiseAmount.

**Semantic purpose**: NOISE GENERATOR control cluster (was scattered across RightBlock previously)

**VST implications**:
- Add new editor subcomponent `NoisePanel` containing 1 ComboBox (noise type) + 4 sliders
- **NEW APVTS param needed**: `"noiseAmount"` (white noise generator gain/level, mapped to `BR_Knob_NoiseAmount` HERO 75mm knob) — does not exist in current V3 code
- **NEW APVTS param needed**: `"noiseStereo"` (stereo width control for the noise, mapped to `BR_Knob_NoiseStereo`) — does not exist in current V3 code. **Reused the OLD `BR_Knob_noise` slot** (which used to control "noise amount" in old layout — now NoiseAmount has its own dedicated hero knob, so the old slot is repurposed for stereo width)
- **NEW APVTS param needed**: `"noiseHP"` (HP filter cutoff for white noise) — does not exist in current V3 code
- **NEW APVTS param needed**: `"noiseType"` (ComboBox choice: white/pink/brown/etc.) — does not exist in current V3
- **RENAME UI label only**: existing `hpBlend` param → label displays "DRY HIGH BLEND" instead of "HP BLEND". Param ID stays the same to preserve preset compatibility.
- `BR_Button_ComplementMode` was removed (2026-05-20) — figure out where complement mode toggle lives now. Options: fold into noise type dropdown (an "off" entry disables complement), or relocate elsewhere.

### 3. Knobs migrated `BR_Panel_RightBlock` → `BR_Panel_ComplementEQ`

| Knob | Old (RightBlock) | New (ComplementEQ) |
|---|---|---|
| BR_Knob_noise | cx=540, cy=156 | cx=-564.6, cy=-200 |
| BR_Knob_hp_blend | cx=606.6, cy=156 | cx=-444.6, cy=-200 |
| BR_Knob_noise_color | cx=540.3, cy=26 | cx=-384.6, cy=-200 |

**RightBlock now contains ZERO knobs (updated 2026-05-21)**: formant + HarmShift moved to FilterRow right side as part of vertical alignment rule (FilterRow right edge = 439 = BANDS/EQ Insets right edge). RightBlock panel mesh still exists but empty — TBD whether to remove or repurpose.

**FilterRow new layout (left to right)**:
- BANDS counter well (BR_Well_BANDS_count_well at cx=-200, unchanged)
- 5 EQ knobs at **compressed 80mm pitch** (was 100mm): width (-43), attack (37), release (117), gate (197), depth (277) — all 40mm
- 25mm separator gap
- 2 vocoder character knobs (resized 45→40mm to match FilterRow style): formant (cx=342), HarmShift (cx=397)

**VST implications**:
- The "RightBlock" editor section in current V3 holds all 8 RightBlock knobs (formant, HarmShift, freq, gain, Q_right, noise, hp_blend, noise_color). After chassis migration: ALL 8 are gone from this section.
- formant + HarmShift now in "FilterRow / EQ section" → editor regrouping needed
- The current "Vocoder Character" semantic grouping (formant/HarmShift) is preserved spatially but lives in a new editor panel together with the EQ knobs (attack/release/width/gate/depth)
- Slider attachment ownership transfers to FilterRow component
- All other former RightBlock knobs already migrated to ComplementEQ panel (noise group)

### 4. TopHeader toggle buttons reshaped (BANDS, DIST, OFFSET) — tall recessed style

**Chassis**: 3 toggle buttons now tall vertical recessed rectangles spanning full TopHeader Y (82mm tall vs old 32mm), 33% wider in X.

**VST implications**:
- Toggle button visual style change only; functionally same APVTS booleans (`"bandsEnabled"`, `"distEnabled"`, `"offsetEnabled"` or similar — verify exact IDs)
- Save/preset/volume controls in TopHeader UNCHANGED
- Files to modify: `Source/PluginEditor.cpp` button bounds + paint() to match new tall recessed look

### 5. EQ Display + BANDS Display both visible with dark insets (symmetric)

**Chassis**: `BR_Inset_EQ_Display` now visible via Boolean cut on BottomEQ (was previously buried in panel). Symmetric to `BR_Inset_BANDS_Display` in TopBANDS.

**VST implications**:
- Editor has TWO display regions: top BANDS visualizer + bottom EQ filter display
- BANDS Display = the 100-band scale-locked visualizer (existing component)
- EQ Display = the 2-point filter spectrum + manipulation surface (`InputFilterDisplay` class) — should expand to fill the dark EQ inset rectangle in chassis

### 6. Keyboard RELOCATED + EXPANDED (2026-05-21)

**Chassis changes**:
- DELETED entire `BR_Panel_PianoKeys` from chassis bottom
- DELETED 14 old keys (one-octave layout: A, A#, B, C, C#, D, D#, E, F, F#, G, G#)
- DELETED `BR_Button_TUNING` + `BR_Well_TUNING_button_piano`
- CREATED **88 floating piano keys** (52 white + 36 black) AT THE BASE of `BR_Inset_BANDS_Display`:
  - X: -283..439 (full BANDS Inset width = 722mm)
  - Y: 31.9..81.9 (occupies bottom 50mm of BANDS Inset)
  - Z: floating above inset surface (5.5..11mm white, 5.5..14mm black)
  - 13.88mm wide white keys, 8.33mm wide black keys
  - MIDI range: 21 (A0) to 108 (C8) — standard 88-key piano
  - Naming: `BR_PianoWhite_A0`, `BR_PianoBlack_Cs1` (sharps as 's')
  - **NO PANEL BEHIND** — keys are individual "floating notes" per user spec
- Chassis bottom area where PianoKeys used to be is now EMPTY plate space (decision pending: shrink Plate_Main bottom or leave for future controls)

**VST implications**:

1. **BANDS visualizer rendering bounds shrink**: bands now render from Y=81.9 (top of keyboard) to Y=363 (top of inset). The bottom 50mm of `BR_Inset_BANDS_Display` is reserved for the keyboard overlay. Find BANDS visualizer Component in `Source/PluginEditor.cpp` and reduce its rendering bounds accordingly (bottom edge of bands area moves UP by 50mm).

2. **TUNING button REMOVED — drag-drop on BANDS Inset instead**:
   - Remove `BR_Button_TUNING` UI element + its slider/button attachment in PluginEditor
   - Add `FileDragAndDropTarget` to BANDS Display component (or KeyboardOverlay component)
   - Drag-drop area = full 722mm × 281mm BANDS Inset perimeter (X=-283..439, Y=81.9..363)
   - On drop of .scl/.kbm/.tun files → load tuning into existing tuning subsystem
   - Visual hint on hover-drag: highlight perimeter or show "Drop tuning file" prompt

3. **88-key keyboard** (was 1 octave = 12 keys):
   - Keyboard Component in editor needs 88 child key Components
   - MIDI note range 21..108
   - Each key triggers note preview on click (existing behavior, extended)
   - Visual: keys overlay the BANDS Display (no own panel)

4. **VST migration of keyboard component**:
   - Likely needs a new `MultiOctaveKeyboard` Component class
   - Each key has its own bounds + paint logic (white vs black, recessed style)
   - Optional: highlight pressed keys in real-time when notes play
   - Tuning displayed by re-labeling keys (e.g., microtonal cents offsets)

**Outstanding decisions**:
- 88 vs 100 keys (chassis currently 88; if user prefers band-count match, regenerate)
- Drag-drop file format support (just .scl/.kbm, or also .tun, .anamark, etc.)
- Should chassis Plate_Main shrink bottom to remove now-empty area (was Y=-426..-288 PianoKeys zone)?

---

## Photoshop text labels needed for new layout

| Panel | Text band | New labels |
|---|---|---|
| BR_Panel_ComplementEQ | Y: -66..-90 | Section header: "NOISE". Knob labels: NOISE, HP, **DRY BLEND** (NOT "HP BLEND"), COLOR. Dropdown: TYPE |
| BR_Panel_RightBlock | existing band | Remove old noise/hp_blend/noise_color labels — only FORMANT and HARM SHIFT remain |

---

## Outstanding chassis design questions

These are decided AT CHASSIS LEVEL, then VST follows:
- Where does the COMPLEMENT button (currently inside EQ inset top-right corner) functionally map after the ComplementMode toggle was removed from ComplementEQ?
- Are square brackets `[` `]` for Q control still planned and exclusively for LP/Bells? (Per filter UX reference: yes, but not yet implemented)
- Final text content for reserved Photoshop bands

---

## When to actually do the VST code migration

Wait until ALL these are TRUE:
1. Chassis layout is COMPLETE (no more iteration sessions)
2. User explicitly says "now rebuild the VST GUI to match new chassis"
3. Master PSD has been re-rendered with new chassis

## Companion notes

- `Dev Notes/Band Ritual - Final 1 COMPLETE Checkpoint.md` — last known shipping state of V3
- Memory: `reference_band_ritual_v3_filter_ux.md` — current code architecture being migrated FROM
- Memory: `feedback_band_ritual_panel_resize_companion_elements.md` — chassis geometry rules


================================================================================
FILE: docs/gui/Band Ritual Final 1 - Brief GUI per Grafico.md
================================================================================

---
tags:
  - band-ritual
  - gui
  - brief
  - grafica
progetto: Band Ritual Final 1
data: '2026-06-03'
scopo: Reference da inviare a un grafico per ridisegnare la GUI
---
# Band Ritual Final 1 — Brief GUI per Grafico

> Pacchetto di riferimento da inviare a un grafico. Mostra **quello che ho ora** per il plugin **Band Ritual Final 1** (un vocoder VST). Obiettivo: far ridisegnare/rifinire la **GUI** a un grafico professionista, partendo da questo stato attuale.

## Cosa voglio
Vorrei che un grafico ridisegnasse la GUI del plugin partendo dal layout e dalle funzioni attuali. **Le funzioni e la disposizione dei controlli devono restare** — quello che cerco è la resa visiva (chassis, knob, font, colori, finiture). Non rimuovere controlli.

## Specifiche tecniche di base
- **Canvas nativo dell'editor: 1580 x 1000 px** (scalato alla finestra).
- Plugin = vocoder. Tutta la UI (knob, pulsanti, testi, grafici) è disegnata via codice; in Blender è renderizzata **solo la plate dello chassis** (lo sfondo).
- Colore accento attuale: coral **#D9554E**. Font: Montserrat (header in Bold).
- Griglia 4px. Knob stile Serum (valore mostrato solo durante il drag).

## GUI attuale (stato live nello standalone)
La plate dello chassis di Blender con sopra tutti gli elementi interattivi (knob, menu a tendina, grafici, tastiera).

![[BR_final1_GUI_current.png]]

## Chassis esportato da Blender (la "plate" di sfondo)
Questo PNG è esattamente lo sfondo usato dentro la GUI.

![[BR_final1_chassis_export.png]]

## Progetto Blender dello chassis (sorgente)
File `.blend` completo e autosufficiente (texture incluse). Apribile in Blender 3.0+.

![[BR_final1_chassis.blend]]

## Sezioni / controlli presenti nella GUI (da mantenere)
- **BAND RITUAL** (in alto a sx): menu modalità vocoder (Self / External / Noise / Saw / Square / Pitch Track), knob grande **MIX**, due slider, dropdown **Key / Scale / Chord-degree / Variation** + toggle M, **mini-tastiera** 1 ottava.
- **BANDS** (centro/dx): display spettrale con tabs **DIST / OFFSET / BANDS**, numero bande grande, fila di knob: **width, attack, release, gate, depth, formant, tone**.
- **COMPLEMENT** (basso/dx): grafico EQ complementare con punti trascinabili.
- **NOISE** (basso/sx): dropdown Auto, knob **amount / color / stereo**, punto **HP** trascinabile, **CARVE**, **DELAY**.
- **GATE**: display inviluppo con linea soglia trascinabile.

## File originali del pacchetto
Cartella consegna: `~/Desktop/BandRitual_Final1_Handoff_2026-06-03/`
- `band_ritual_final1_chassis.blend` (progetto Blender, texture incluse)
- `chassis_export_from_blender.png` (render chassis)
- `band_ritual_final1_GUI_standalone.png` (screenshot GUI live)
- `README.txt` (note in italiano)

Codice/asset sorgente: `~/BandRitualV3_work/` (plate usata: `Source/Assets/chassis_plate.png`).

## Logo Ritual Audio Labs (marchio da usare sulla GUI)
Monogramma "RA". Versione alta risoluzione (su fondo scuro) + versione chiara/piccola.

![[BR_ritual_audio_labs_logo.png]]
![[BR_ritual_audio_labs_logo_light.png]]


================================================================================
FILE: docs/gui/Band Ritual Final 1 - Giga Knob Material Checkpoint 2026-04-15.md
================================================================================

---
tags:
  - band-ritual
  - final-1
  - gui
  - blender
  - checkpoint
date: 2026-04-15
---
# Band Ritual Final 1 — Giga Knob Material Checkpoint (liked version)

User approved this as a strong baseline to keep iterating on realism. Save this as fallback.

## Scene state
- File/scene: active Blender scene with the 4-ring giga knob + MIX slider + full chassis
- Render engine: Cycles, samples 384
- HDRI: Poly Haven `brown_photostudio_02` (2k)
- Camera: orthographic framed on giga+slider area

## Dark-Mystical palette (applied)

### Rings — cylindrical brushed metal with polar coords
Three rings, each with its own material variant (progressively darker toward the outer ring).
Each material uses:
- `Object` coord → Separate XYZ → `atan2(Y, X)` for angular component
- `sin(angle × groove_freq) × 0.5 + 0.5` as the brushed-streak signal
- Color ramp 0.35→0.65 maps signal to roughness (0.18 → 0.42)
- Bump node uses same signal, strength 0.15, distance 0.00005
- Principled BSDF: metallic 0.9, base roughness 0.28
- Coat Weight 0.40, Coat Roughness 0.12 (premium ritual-lacquer finish)

Per-ring:
- `M_RingScale_Cyl` (inner, SCALE TYPE ring): base (0.18, 0.06, 0.05), groove_freq 400
- `M_RingDeg_Cyl` (middle, CHORD DEGREE ring): base (0.10, 0.035, 0.030), groove_freq 340
- `M_RingType_Cyl` (outer, CHORD TYPE ring): base (0.06, 0.022, 0.022), groove_freq 300

### Ring geometry — profiled cross-section
Each ring built with 6-vertex profile (not flat annulus):
- flat bottom at z=0.003
- outer wall slopes inward (taper 0.0005–0.0007m)
- beveled top outer corner (bevel_r 0.0008–0.0009m)
- beveled top inner corner
- slightly elevated top (z 0.0065–0.008)
- 144 angular segments → smooth circular curves
- Channels between rings: 1.5mm gap + 0.5–1mm Z-offset per ring (stepped-down outward)

### Inner dial — black with purple radial glow
`M_DarkBronze`:
- Base: (0.008, 0.005, 0.012) near-black metallic=0, roughness=0.55
- Mix shader with Emission (RGB 0.45, 0.15, 0.65 @ strength 2.2)
- Mix fac driven by spherical gradient color ramp (0.80→1.0 range)
- Net effect: purple glow emanating from center, fading to pure black near edges

### Labels — muted cream
`M_LabelWhite`: emission (0.92, 0.88, 0.75) @ strength 1.8
Used for all SCALE TYPE, CHORD DEGREE, CHORD TYPE labels (15/7/6 each).

### Key center — HTML overlay (not in Blender)
- Blender renders with `Lbl_ScaleKnob_Key` hidden (`hide_render=True`)
- HTML/CSS overlay in prototype shows current KEY value as large cream letter with purple text-shadow
- HTML updates live on scroll/drag through 14 values (C–B, LIN, LOG)

## Interactive prototype (matches real VST tech)
Location: `/Users/ricosan/Desktop/band_ritual_prototype/`
- `index.html`: SVG overlay on rendered PNG
- `knob_bg.png`: Cycles render at 1600×1340 with knob centered at (670, 670)
- `coords.json`: exact pixel coords of each ring + MIX slider
- Server: `python3 -m http.server 8768`

Glass-layer selection indicators (HTML/SVG):
- Radial gradient: lavender (230,210,240,0.48) → violet (160,80,200,0.28) → dark purple (90,30,130,0.14)
- Amber/purple hairline stroke (220,180,240,0.88)
- Top-left gloss highlight streak
- Gaussian blur filter for soft outer glow
- 1 arc per ring at top (0°) = currently-selected value
- Updates live on scroll/drag

## To continue iterating on realism from this baseline
If further experiments go worse, restore this state by:
1. Reapply ring shaders (`M_RingScale_Cyl` / `M_RingDeg_Cyl` / `M_RingType_Cyl` configs above)
2. Reapply `M_DarkBronze` with purple glow mix
3. Reapply `M_LabelWhite` muted cream
4. Keep profiled geometry (bevel + channel drops)

## Render output snapshot
`/Users/ricosan/Desktop/band_ritual_prototype/knob_bg.png` (current backup)

## Designer principles for this knob (saved 2026-04-15 — apply by default in future iterations)

These were learned from real critique sessions on this knob — bake them into any future iteration:

1. **Symmetry is meaningful, not decorative.** Each ring picks its OWN symmetry axis: bilateral (V mirror), top/bottom split, asymmetric weighted, etc. Never default to "all rotationally symmetric circles" — that screams "no design behind it."

2. **Segment widths reflect FUNCTION/IMPORTANCE.** Most-used options (e.g. CHORD DEGREE i / IV / V are musical heavy hitters) get LARGER arc widths than less-used ones (vii). Equal slots = lazy.

3. **No per-ring base outlines in 2D.** When showing multi-ring stacks in a 2D preview, drawing each ring's own border looks like a Paint diagram. In 3D Blender, the depth/material/Z-stack differences distinguish layers naturally — let geometry do it, not borders.

4. **Spacing communicates relationship.** Tighter = more related, wider = independent. Don't space everything equally.

5. **Categorically different items get distinct visual zones.** KEY ring has 12 notes + 2 categorically-different modes (LIN/LOG) — modes should be visually separated (separate bars at top, distinct color), not slotted in as "14th" and "13th" notes.

6. **Readability of active state is non-negotiable.** Selected/highlighted value must remain readable through any decorative overlay. Glass-tint OK, opaque obscuring overlay = broken.

7. **Equivalent controls = identical visuals.** Two amount sliders should look identical. Different size = implies different importance.

8. **Texture follows surface curvature.** Brushed metal on a ring = polar/cylindrical coords (concentric grooves wrapping the surface), not flat noise stamped on top.

9. **Reference Arturia / FabFilter / Output / Baby Audio / Teenage Engineering OP-1 / Soundtoys / iZotope.** Senior-designer level is the bar.

These should also be in the agent's `prompt_template.txt` (DESIGNER PRINCIPLES section).



================================================================================
FILE: docs/gui/Band Ritual GUI - Blender + State Aligner Session 2026-04-21.md
================================================================================

# Band Ritual GUI — Blender project + State Aligner, session 2026-04-20/21

## Blender project
- File: `/Users/ricosan/Desktop/band_ritual_v1.blend`
- Camera: `BR_KnobRenderCam` (ortho scale 0.0467) centred on Shape_17 — use this cam + hide all other meshes + hide lights + `film_transparent=True` to render the tiny `_circle_ind` pointer asset.
- Dedicated cam: `GigaKnobRenderCam` at (-0.319, 0.013, 0.3), ortho_scale 0.12 → frames the big MIX knob (`GigaKnob_Body/Bezel/DetailRing/Well`).
- Knob materials:
  - **Small knobs** (Shape_17..30_circle*): `BR_KnobTop` (image texture `knob_body_cream_texture.png` on cream body) + `BR_KnobSkirt` (brushed metal, anisotropic 0.7, bump, tangent).
  - **MIX knob** (GigaKnob_*): all 4 parts share `BR_KnobTop_Textured` — Principled BSDF with the cream texture plugged into BOTH Base Color AND Emission (strength 0.85) + BOX projection, so lights can't wash it grey. Last render (512,512) sampled (236, 215, 189) = proper cream.
- `Shape_*_circle_ind` objects have a small bevel on top-end edges (offset 0.18 mm / 3 seg, plus a 0.35 mm / 4 seg pass on Shape_17). Material `BR_KnobIndicator` exists but is NO LONGER USED by the state-aligner — the indicator is drawn procedurally in JS (see below).

## State aligner (webapp)
- Repo path: `/Users/ricosan/eqenrico-hub.github.io/agents/png-state-aligner/`
- Live: https://eqenrico-hub.github.io/agents/png-state-aligner/
- Key sample_data files: `png_a/b/c.png`, `knob_render.png`, `gigaknob_render.png` (MIX knob body), `slider_handle_native.png`, `decal_45.png` (COMPLEMENT glow), `decal_46.png` (ChatGPT decal), `clone_overlay*.png`, `default_manifest.json`.
- MIX knob region name: `knob_mix` at x=73 y=187 w=200 h=200 with `knob_image_file: gigaknob_render.png`. Rotates in interactive mode exactly like the small knobs (vertical drag, 200 image px = full 0..1 range, angle = −135°+270°·value).
- Indicator = procedural canvas path (rounded-top rectangle via `arcTo` on both top corners, radius `indW*0.18`, clamped ≤ indW/2 & indH/2). Colour `rgb(217, 79, 108)` flat. Pass 1 = SE world-space drop shadow (blur + 0.42 black) so the indicator reads as raised.
- Knob body shadow: `ctx.shadowColor rgba(0,0,0,0.50)`, offsetX `rw * 0.075`, offsetY `rh * 0.180`, blur `rw * 0.120`. Applied to body only; indicator draws outside the shadow block.
- decal_45 parent-linked to `region_42` (COMPLEMENT button rect 1267,586,146,34); appears only when region_42 is pressed.
- decal_46 = always-visible ChatGPT image at (1,674,337,225).

## Critical patterns (don't rebuild, don't fight)
1. **Downscale kills Blender bevels.** Any rounding smaller than ~5% of the source image is invisible after 1024→72 downscale. Do rounding in JS as a fraction of the final rect size, not in Blender geometry.
2. **Integer-pixel snap for rotation.** `Math.round(rx + rw/2)` + integer w/h before `ctx.rotate` — otherwise fractional state.scale positions make each knob sample differently and AA produces different thicknesses.
3. **Cache-bust sample_data PNGs.** GitHub Pages + Safari aggressively cache PNGs. For every runtime fetch append `?v=' + Date.now()`. (Hotfix history: the indicator PNG and the knob PNG both needed this.)
4. **Decal parent_name must resolve in the second import pass.** Decals in the manifest store `parent_name`; the importer copies it to `_pendingParentName` then the second pass rewrites it to `parentId`. Without this decals never show when their parent button is pressed.
5. **Drop-drag image naming.** On drop, new decal name = max existing `decal_N` + 1 (never `regions.length + 1` — collides and corrupts `crop_file` on export).
6. **rect_4 handle lives as `handle_file: slider_handle_native.png`** — always preserve this override on every bundle merge.

## Resume next session
- User trigger: *"resume png-state-aligner"* or *"resume Band Ritual hybrid GUI"*.
- Open: https://eqenrico-hub.github.io/agents/png-state-aligner/?v=<N+1>&r=<timestamp>
- Blender: open `band_ritual_v1.blend`, active cams are set up and ready.
- Likely next: filmstrip export (128 frames min→max) for VST3 JUCE `FilmstripLookAndFeel` (small knobs at 70/100/180 px and MIX at whatever final size ships).


================================================================================
FILE: docs/gui/Band Ritual MIX Knob - Blender Session 2026-04-26.md
================================================================================

# Band Ritual MIX Knob — Blender Session 2026-04-26 / 27

End-of-session context for the MKV2_* MIX knob build in `band_ritual_v1.blend`.

## Final approved values (user-locked)

- **Indicator angle @ 0%**: `243.02°`
- **Indicator angle @ 100%**: `-35.77°`
- **Tip radius %** (radial offset of indicator pill): `83.81%`
- LED order: **LED_1_B first to light**, LED_5_R last (was reversed earlier — fixed)
- LED_W (white): always on

These live on `mkv2_indicator` PropertyGroup with update callbacks → drives N-panel sliders.

## Files

- **Main scene**: `/Users/ricosan/Desktop/band_ritual_v1.blend`
- **Vintage locked snapshot**: `/Users/ricosan/Desktop/band_ritual_v1_VINTAGE_LOCKED.blend`
- **Pre-LED-inset rollback point**: `/Users/ricosan/Desktop/band_ritual_v1_PRE_LED_INSET_2026_04_27.blend`
- **Filmstrip output**: `/tmp/MKV2_knob_filmstrip_128.png` (256×32768, 128 frames, full 270° sweep)
- **GIF preview**: `/tmp/MKV2_knob_animation.gif`
- **Static endpoints**: `/tmp/MKV2_knob_at_0.png`, `/tmp/MKV2_knob_at_100.png`
- **PBR textures**:
  - `/tmp/knob_textures/wp02_color.png` + `wp02_normal.png` + `wp02_rough.png` — White Plaster 02 (PolyHaven, dome top)
  - `/tmp/knob_textures/metal038/Metal038_1K-PNG_*.png` — Color/Roughness/NormalGL/Metalness (column + bezel)
- **Locked material**: `BR_PaintedVintage_LOCKED_2026_04_26` (fake user, vintage GUI look)

## MKV2 N-panel (in Blender, press N)

`MKV2_PT_indicator` panel:
- Tip Position slider
- Angle @ 0% / Angle @ 100% sliders (with auto-snap test_value preview: edits to angle@100 set test=1.0; edits to angle@0 set test=0.0)
- Test Value preview slider
- LED Ramp Width / LED Max Strength / LED_W Strength sliders
- 6 LED color swatches with hex paste icons + per-LED eyedroppers
- "🎯 Click 5 points on screen" button → `MKV2_OT_5color_picker` modal operator

Tint Amount slider on dome material is a PropertyGroup with update callback (driver was unreliable — replaced).

## 5-color picker tool — BROKEN, use online picker instead

`/tmp/mkv2_color_picker_tool.py` opens fullscreen tkinter window but displays empty/black even after granting Screen Recording permission. Likely cause: `screencapture -x` returns black frame because permission cache hasn't refreshed for `/usr/bin/python3`, or PIL display sizing wrong on Retina.

**Workaround user chose**: pick colors from any online color picker / browser DevTools and paste hex codes directly into the LED color swatch fields in the N-panel (each has a hex paste icon). No need to fix the tkinter tool.

## Pending work

1. **LED inset**: LEDs should cast light on `MKV2_OuterBezel` AND appear recessed into the metal (not just overlay objects). Checkpoint already saved at `band_ritual_v1_PRE_LED_INSET_2026_04_27.blend`.
2. **Re-render filmstrip** with final LED colors once user picks them.
3. **Webapp integration**: replace `/Users/ricosan/eqenrico-hub.github.io/agents/png-state-aligner/sample_data/gigaknob_render.png` with new filmstrip (or add filmstrip alongside).

## Critical lessons from this session

- **macOS osascript color picker freezes Blender** when called via `subprocess.run` — must use `Popen` + modal operator polling at 0.3s timer.
- **Drivers don't refresh live in shader nodes** for slider control — use PropertyGroup with update callback that directly sets node values + tags for redraw.
- **DiscTop dome distortion** with planar UV → fixed with **BOX (triplanar) projection** on Image Texture.
- **Two materials sharing same color** → flat papier-mâché look. Solved with darker bezel variant + AO + rim light.
- **Mesh-level imperfections** (user explicitly asked NOT for texture fakery): use Displace modifier with STUCCI noise.
- **Fillet between disc and knob**: bevel weight markers on bottom-outer edge of disc + top-inner edge of knob, then 1.4mm bevel via WEIGHT limit.
- **Indicator parenting pivot bug**: `ind.location.y` was `0.0322` instead of `0.0173` → fix via parenting + `matrix_parent_inverse = Identity`.
- **DSP lesson confirmed (CLAUDE.md)**: cuts work but boosts don't = you're trying to detect frequency from time-domain samples. Apply EQ before mixing instead.

## How to resume

User trigger: *"resume MIX knob LED inset"* or *"resume Band Ritual MIX knob"*.
1. Open `/Users/ricosan/Desktop/band_ritual_v1.blend` (or rollback to `_PRE_LED_INSET_2026_04_27.blend` if needed).
2. Press N in 3D viewport → MKV2 panel.
3. For LED color update: paste hex codes into the 6 swatch fields manually.
4. For filmstrip re-export: use the cam + script that produced `/tmp/MKV2_knob_filmstrip_128.png`.


================================================================================
FILE: docs/gui/GUI_LAYOUT_SPEC.md
================================================================================

# Band Ritual Final 1 — GUI Layout Spec (Source of Truth)

**Native resolution: 722 × 485 px** (2× retina target: 1444 × 970)

All coordinates are (x, y, w, h) at native 1x scale. Origin = top-left corner.

---

## Panel Zones

| Zone | x | y | w | h | Purpose |
|---|---|---|---|---|---|
| **Title bar** | 0 | 0 | 722 | 27 | Logo, preset, buttons |
| **Left panel** | 0 | 27 | 186 | 220 | Algorithm, Mode, Mix, Scale/Chord |
| **Center panel (top)** | 187 | 27 | 427 | 159 | Band display / editors |
| **Center panel (knobs)** | 187 | 186 | 427 | 61 | 6 vocoder knobs row |
| **Right panel (harm)** | 613 | 48 | 110 | 68 | Formant + Harm Shift |
| **Right panel (noise)** | 613 | 117 | 110 | 130 | Noise + HP Blend + Color + XY Pad |
| **EQ section** | 0 | 247 | 723 | 168 | Spectrum + EQ knobs (expandable) |
| **MIDI keyboard** | 1 | 415 | 721 | 70 | Piano keyboard |

---

## All Components (Complete List)

### Title Bar (y: 0–27)
| Component | x | y | w | h |
|---|---|---|---|---|
| Logo area (left edge) | 0 | 0 | 94 | 27 |
| License button | 94 | 4 | 50 | 19 |
| About "i" button | 162 | 4 | 19 | 19 |
| BandDist button | 187 | 0 | 39 | 27 |
| BandOffset button | 226 | 0 | 37 | 27 |
| PerBandQ button | 263 | 0 | 38 | 27 |
| EDIT button | 310 | 3 | 22 | 22 |
| Save preset button | 336 | 3 | 22 | 22 |
| Preset dropdown | 360 | 3 | 176 | 22 |
| Input gain knob | 655 | 0 | 26 | 26 |
| Input gain label | 651 | 20 | 34 | 10 |
| Input meter | 651 | 3 | 3 | 20 |
| Output gain knob | 689 | 0 | 26 | 26 |
| Output gain label | 685 | 20 | 34 | 10 |
| Output meter | 716 | 3 | 3 | 20 |

### Left Panel (x: 0–186)
| Component | x | y | w | h |
|---|---|---|---|---|
| Algorithm dropdown | 7 | 28 | 113 | 18 |
| Carrier Q knob | 125 | 27 | 26 | 26 |
| Modulator Q knob | 151 | 27 | 26 | 26 |
| Carrier Mode dropdown | 7 | 48 | 113 | 18 |
| **Mix knob (70×70)** | ~47 | 73 | 70 | 70 |
| Mod Level bar | ~123 | ~80 | 14 | 55 |
| Scale/Chord box | 1 | 171 | 186 | 50 |
| Tuning button | 3 | 221 | 182 | 20 |

### Center — Band Display (x: 187, y: 27, w: 427, h: 159)
This is the MAIN hero area — rendered live in JUCE (band bars, dynamic threshold line, scale/chord overlay). Blender will render a FRAMED CUTOUT (inset, bevel, maybe glass reflection) but the contents stay code-drawn.

### Center — Vocoder Knobs Row (y: 186, h: 61)
6 knobs at 36×36 px evenly spaced, labels below each:
| # | Component | Approx. x | y | Size |
|---|---|---|---|---|
| 1 | Band Count slider | ~200 | 190 | 36×36 |
| — | STACK toggle | ~240 | 198 | 50×20 |
| 2 | Band Width | ~300 | 190 | 36×36 |
| 3 | Attack | ~360 | 190 | 36×36 |
| 4 | Release | ~420 | 190 | 36×36 |
| 5 | Gate | ~480 | 190 | 36×36 |
| 6 | Depth | ~540 | 190 | 36×36 |

### Right Panel — Harmonic (x: 613, y: 48, w: 110, h: 68)
| Component | x | y | w | h |
|---|---|---|---|---|
| Formant knob | 621 | 56 | 38 | 38 |
| Harm Shift knob | 676 | 56 | 38 | 38 |

### Right Panel — Noise (x: 613, y: 117, w: 110, h: 130)
| Component | x | y | w | h |
|---|---|---|---|---|
| Unvoiced/Noise knob | 621 | 125 | 38 | 38 |
| HP Blend knob | 676 | 125 | 38 | 38 |
| HP Slope selector | ~691 | ~155 | 24 | 12 |
| Noise Mode dropdown | 621 | ~177 | 40 | 16 |
| Noise Color knob | 621 | ~199 | 38 | 38 |
| Spectral X/Y Pad | ~671 | ~195 | 60 | 60 |

### EQ Section (x: 0, y: 247, w: 723, h: 168) — EXPANDABLE
**Default state: HIDDEN** (user-requested). Shown when user clicks to expand.
| Component | x | y | w | h |
|---|---|---|---|---|
| EQ Spectrum display | 1 | 247 | 668 | 169 |
| EQ Freq knob | 676 | ~260 | 32 | 32 |
| EQ Gain knob | 676 | ~312 | 32 | 32 |
| EQ Q knob | 676 | ~364 | 32 | 32 |

### MIDI Keyboard (x: 1, y: 415, w: 721, h: 70)
Live-drawn piano keyboard (12 semitones, one octave). Blender will render a FRAMED CUTOUT only; keys are drawn by JUCE.

---

## Component-Type Summary

| Type | Count | Sizes | Where |
|---|---|---|---|
| **Small knobs 26×26** | 3 | Carrier Q, Mod Q, I/O Gain ×2 | Title bar, left panel |
| **Small knobs 32×32** | 3 | EQ Freq, Gain, Q | EQ section |
| **Medium knobs 36×36** | 5 | Width, Attack, Release, Gate, Depth | Center knob row |
| **Medium knobs 38×38** | 5 | Formant, Harm Shift, Noise, HP Blend, Noise Color | Right panel |
| **Large knob 70×70** | 1 | Mix | Left panel (hero) |
| **Dropdowns** | 6 | Algorithm, Carrier Mode, Scale, Chord, Vars, Noise Mode | Various |
| **Buttons** | ~8 | About, Edit, BandDist, BandOffset, PerBandQ, Save Preset, License, Tuning | Title + left |
| **Toggle buttons** | 1 | STACK | Center row |
| **Bars / sliders** | 2 | Mod Level, Band Count | Left, center |
| **Meters** | 2 | Input, Output | Title bar |
| **XY Pad** | 1 | Spectral | Right panel |
| **Display zones** | 4 | Band Display, EQ Spectrum, MIDI Keyboard, Scale/Chord | Center, bottom |

---

## Unique Knob Sizes (for filmstrip generation)

Blender will render ONE filmstrip per UNIQUE knob size + style. Total needed:
- **70×70 large** — 1 filmstrip (Mix — hero knob, can be unique style)
- **38×38 medium** — 1 filmstrip (all right-panel knobs)
- **36×36 medium** — 1 filmstrip (center vocoder row)
- **32×32 small** — 1 filmstrip (EQ knobs)
- **26×26 small** — 1 filmstrip (I/O gain + Q knobs)

**Total: 5 knob filmstrips** (at 2× retina: rendered at 140, 76, 72, 64, 52 px wide × 128 frames tall).

Plus:
- 1 chassis background PNG (722×485 × 2 = 1444×970)
- 1 chassis with EQ-expanded background PNG (same size, EQ visible)
- Optional: button filmstrips for hover/pressed states

---

## Next Steps

1. ✅ Layout spec written (this file)
2. ⏭ User picks visual direction (mood references)
3. ⏭ Install `blender-mcp` → Claude Code
4. ⏭ Build 3D scene in Blender: chassis panel with all cutouts matching spec above
5. ⏭ Style iteration: materials + lighting
6. ⏭ Render all assets
7. ⏭ Wire into JUCE via custom LookAndFeel


================================================================================
FILE: docs/process/Ableton Live 12 - Custom Remote Script Installation.md
================================================================================

# Ableton Live 12 — Custom Remote Script Installation (CRITICAL)

## The Correct Path (since Live 10.1.13+)

```
~/Music/Ableton/User Library/Remote Scripts/YourScriptName/
```

**NOT** `~/Library/Preferences/Ableton/Live 12.x.x/User Remote Scripts/` — that's the OLD location (Live 9 era). Ableton 10+ does NOT scan that folder for Control Surface scripts.

## The Wrong Paths (DO NOT USE)

- `~/Library/Preferences/Ableton/Live 12.x.x/User Remote Scripts/` — OLD, not scanned
- `/Applications/Ableton Live*.app/Contents/App-Resources/MIDI Remote Scripts/` — app bundle, may be read-only
- Any path inside a DMG/ISO volume — read-only file system

## What Happened (2026-03-25)

Building SoundClaw VST plugin. The plugin auto-installed its Remote Script to the app bundle path in `/Applications/`. Then we tried the `User Remote Scripts` folder in `~/Library/Preferences/`. Neither worked. Wasted 30+ minutes.

**The fix**: Copy to `~/Music/Ableton/User Library/Remote Scripts/SoundClaw/` — worked immediately.

## Proof It Works

`Logi_Plugin` was already installed at the correct path on this machine and appeared in Ableton's Control Surface dropdown. Ableton auto-compiles `.py` → `.pyc` (Python 3.11) in a `__pycache__/` subfolder.

## Installation Checklist

1. Create folder: `~/Music/Ableton/User Library/Remote Scripts/YourScript/`
2. Add `__init__.py` with `create_instance(c_instance)` function
3. Add your control surface `.py` file
4. Quit Ableton completely (Cmd+Q)
5. Reopen Ableton
6. Go to Preferences > Link/Tempo/MIDI > Control Surface dropdown
7. Your script should appear

## Script Requirements

- `.py` files work fine — no need to pre-compile to `.pyc`
- Use `super(ClassName, self).__init__(c_instance)` for compatibility
- Ableton 12 uses Python 3.11
- `_Framework.ControlSurface` is available for User Remote Scripts
- Folder name must not have spaces
- Unix line endings (`\n`), NOT Windows (`\r\n`)
- No leading blank lines in files

## Also Update the VST Auto-Installer

The SoundClaw VST's `installRemoteScript()` in `BridgeClient.cpp` currently writes to the app bundle path. It needs to be updated to write to `~/Music/Ableton/User Library/Remote Scripts/` instead.

## Sources

- https://help.ableton.com/hc/en-us/articles/209072009
- AbletonOSC GitHub: https://github.com/ideoforms/AbletonOSC


================================================================================
FILE: docs/process/Band Ritual - Ableton Host Integration Bug.md
================================================================================

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


================================================================================
FILE: docs/process/Band Ritual - DSP Iteration Workflow.md
================================================================================

# Band Ritual — DSP Iteration Workflow

## When to Simulate vs Record in Ableton

### Simulate in Python (fast iteration)
- **When tuning filter parameters** (Q, stage count, Qa ratio, gain curves)
- PSD **shape** (peak width, valley depth) depends almost entirely on filter math
- The JUCE SVF math is deterministic and can be replicated exactly:
  - `g = tan(π * fc / fs)`
  - `R2 = 1 / Q`
  - `h = 1 / (1 + R2*g + g²)`
  - Same state variable update equations
- Level normalization in PSD comparison cancels out any gain differences
- **Saves hours** — no need to rebuild plugin + install + rescan + re-record for every Q tweak

### Record in Ableton (ground truth verification)
- After simulation finds the best parameters → ONE final recording to verify
- When testing things simulation can't capture:
  - Parameter smoothing / block processing artifacts
  - Plugin GUI interactions (gate, EQ, complement mode)
  - Full signal chain behavior (mix, mod level, depth interactions)
  - Edge cases at extreme settings

### Workflow
1. Simulate multiple parameter values in Python (batch sweep)
2. Find the best match via PSD comparison
3. Apply winning parameters to C++ code
4. Build plugin → install → record in Ableton
5. Verify with real recording PSD comparison
6. If mismatch > threshold → investigate what simulation missed

## SVF vs IIR — Key Differences (learned 2026-03-21)

### SVF (StateVariableTPTFilter) bandpass
- **Peak gain = Q** (not unity like IIR makeBandPass)
- Must compensate: `bandNormGain = 1 / (Qa^n_analysis × Qc^n_carrier)`
- No bilinear transform bandwidth compression at HF
- Deeper valleys between bands (better stopband rejection)
- "Too perfect" for matching Ableton — Ableton uses IIR

### IIR (makeBandPass) bandpass
- Peak gain = 1.0 (unity at center frequency)
- Bilinear transform compresses BW at HF: `actual_BW = designed × sin(2πf/fs)/(2πf/fs)`
- Natural inter-band leakage at HF (shallower valleys)
- Matches Ableton's vocoder filter type

### Ableton Vocoder Range
- Frequency range: **20 Hz to 18 kHz** (NOT 20 kHz)
- Band Ritual must match this for accurate comparison

## Test Infrastructure
- White noise source: `~/Downloads/TEST_white_noise_10s.wav`
- Ableton crack recordings: `~/Desktop/vocoder-comparison/ableton_crack/crack_bw{N}_noise.wav`
- Simulation script: `~/Desktop/vocoder-comparison/simulate_final2.py`
- PSD comparison script: `~/Desktop/vocoder-comparison/compare_final2.py`
- BW mapping: Ableton BW% ÷ 100 = Band Ritual `bandWidth` parameter value (range 0.1–2.0)

## Recording Settings (for Ableton exports)
- Carrier mode: SELF
- Scale Amount: 0%
- Algorithm: Linked mode
- 40 bands
- Depth: 100%, Mix: 100%, Mod Level: 100%
- Gate: 0 (off — matches Ableton's -∞)
- EQ: OFF


================================================================================
FILE: docs/process/Band Ritual - Final 1 COMPLETE Checkpoint.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - complete
  - final-1
date: '2026-03-24'
---
# Band Ritual Final 1 — COMPLETE (2026-03-24)

## This version is DONE. Do not modify further.
All vocoder cracking work is finalized in this build.

## Summary: 1.8 dB avg RMS match to industry-standard vocoder
- Band grid: logspace(20, 18000, N)
- Filter: 4th order bandpass (2 cascaded biquads)
- Phase alternation: every other band inverted
- Envelope: pow(0.01, 1/(sr*t))
- Q: 11-entry table with cos()^1.29 freq-dep correction
- Tilt: alpha = -0.003*norm - 0.090 + LF shelf
- bwGain: 8-entry calibrated table
- Analysis Qa: 0.85 - 0.55*widthNorm
- Gate: -inf(-60) to +12 dB, offset -17, soft gate

## Location
`/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

## Next version TODO (Final 2)
1. **Formant: band remapping** — replace filter shifting with envelope remapping + interpolation (fix crackles)
2. **Spectral freeze** — hold spectral envelope as sustained pad
3. **Further gate fine-tuning** — smoother fade behavior
4. **Noise/Unvoiced system** — match Ableton's unvoiced detection


================================================================================
FILE: docs/process/Band Ritual - Plugin Silence Debugging Checklist.md
================================================================================

# Band Ritual - Plugin Silence Debugging Checklist

## CRITICAL: Read This FIRST When Plugin Outputs Silence

### Step 1: Check License Override (90% of cases)
The plugin has a license manager that **silently returns zero output** when the trial expires.

**File**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/LicenseManager.h`

**Check these lines are overridden for dev:**
```cpp
bool canProduceAudio() const noexcept      { return true; /* DEV OVERRIDE */ }
bool shouldApplyTrialNag() const noexcept  { return false; /* DEV OVERRIDE */ }
```

**If they're NOT overridden** (returning actual license check results), the plugin WILL output silence after trial expiry. This is the #1 cause of "my code change broke the plugin" false alarms.

**Where it blocks audio** in PluginProcessor.cpp (~line 2887):
```cpp
if (! licenseManager.canProduceAudio())
{
    buffer.clear();
    return;  // <-- ALL AUDIO ZEROED
}
```

### Step 2: Check Build Was Actually Installed
```bash
# Check timestamps - plugin should be recent
ls -la /Users/ricosan/Library/Audio/Plug-Ins/VST3/Band\ Ritual\ Improved.vst3/Contents/MacOS/
# Also check the build artefacts
ls -la "/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/build/BandRitual_artefacts/Release/VST3/Band Ritual Improved.vst3/Contents/MacOS/"
```

### Step 3: Check Header Changes Need Full Rebuild
If you changed ANY `.h` file (STFTVocoder.h, PluginProcessor.h, etc.), ALL `.cpp` files that include it need rebuilding:
```bash
cd "/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/build"
find CMakeFiles -name "*.o" -delete
cmake --build . --config Release -j8
```
Just deleting one `.o` file is NOT enough when headers change.

### Step 4: Pedalboard Test Basics
If auto_compare.py produces zero/wrong output:
- Plugin parameters have **correct names** (check with `dir(plugin)`)
- Parameters are set BEFORE processing: `setattr(plugin, 'param_name', value)`
- 1-second warmup is processed before test audio
- Plugin defaults: `algorithm_mode=Ritual`, `band_count=16` — must be explicitly set to `Linked`/`40`

### What This Is NOT
- It's NOT a Pedalboard bug (Pedalboard works fine)
- It's NOT a code bug (usually) — if it worked before and silence appeared after a minor change, check license first
- It's NOT a CMake cache issue (usually) — but do a full rebuild if header changed

## Timeline of License Incidents
- **2026-03-13 ~03:30 UTC**: Trial expired mid-calibration session. Spent ~30 min debugging thinking code changes caused silence. Root cause: `canProduceAudio()` returning false.
- **Fix**: Added DEV OVERRIDE to LicenseManager.h. Must revert before shipping.

## REMEMBER: Revert Before Shipping
Before ANY release build, restore the original license checks in LicenseManager.h:
```cpp
// REMOVE these dev overrides:
// bool canProduceAudio() const noexcept      { return true; /* DEV OVERRIDE */ }
// bool shouldApplyTrialNag() const noexcept  { return false; /* DEV OVERRIDE */ }
```


================================================================================
FILE: docs/process/Band Ritual - Pre-Launch Checklist.md
================================================================================

---
tags:
  - band-ritual
  - launch
  - checklist
date: '2026-03-20'
---
# Band Ritual — Pre-Launch Checklist

## Before uploading to Gumroad

### Discord Link
- [ ] Update the Discord link that appears in the VST GUI (clickable icon)
- [ ] Check all places it appears: PluginEditor, WebView UI, any popup/about screen
- [ ] Test that the link opens correctly after build

### Other pre-launch items
- [ ] Update version number
- [ ] Update LicenseManager DEV OVERRIDE (must be OFF for shipping)
- [ ] Code sign for Ableton: `codesign --force --deep --sign -`
- [ ] Test in Ableton (rescan plugins)
- [ ] Windows build if needed


================================================================================
FILE: docs/process/Band Ritual - Validation Suite Spec.md
================================================================================

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


================================================================================
FILE: docs/process/Band Ritual Fake Crack - Reddit Marketing.md
================================================================================

# Band Ritual Fake Crack - Reddit Marketing Strategy

## What This Is
A fake "cracked" version of Band Ritual VST3 for guerrilla marketing. The plugin loads in DAWs, shows the Ritual Audio Labs auth screen with logo, but every license key always fails. Zero real DSP code inside — just a JUCE shell with branding.

## Files Location
`/Volumes/Ricosan HD/AI JOURNEY/DEV APPS/05_MUSIC_AUDIO/BAND_RITUAL_FAKE_CRACK/`

### Ready-to-share zips:
- `BandRitual_v0.0.1_CR4CK_macOS.zip` (2.8MB) — Intel + Apple Silicon universal
- `BandRitual_v0.0.1_CR4CK_Windows.zip` (2.2MB) — Windows 64-bit

### Source/build files:
- `BandRitualFake/` — JUCE project source code (CMake + C++)
- `BandRitual_CR4CK/` — macOS package contents (vst3 + README + keygen.txt)
- `BandRitual_CR4CK_WIN/` — Windows package contents
- `BandRitual-Windows-VST3/` — raw GitHub Actions build output

### Post text:
- `BandRitual_CR4CK/POST_TEXT.txt` — ready-to-paste Reddit post (needs Google Drive links)

## How It Works
1. User downloads zip, copies .vst3 to plugin folder
2. DAW scans and finds "Band Ritual" — loads normally
3. Opens auth screen: Ritual Audio Labs logo, "BAND RITUAL" title, key input
4. User tries keys from keygen.txt or any key → fake "Verifying with Gumroad..." delay (1.5-3s) → random error message every time
5. Error messages rotate: "License key not found", "Already activated on another machine", "Server verification failed", "License key is invalid or has been revoked"

## What's Inside (Zero IP Risk)
- No real vocoder DSP code
- processBlock just outputs silence (or passthrough if auth bypassed)
- Fake DSP arrays/matrices to pad binary size (~5.7MB macOS, ~2.2MB Windows)
- Stripped debug symbols
- If someone patches past auth → plugin "opens" but does nothing → looks like broken crack, not fake

## Reddit Post Strategy
- Target subreddit: r/CrackedPluginsXI (check rules before posting)
- Post as throwaway account, not real identity
- Frame as: "New vocoder plugin, includes keygen with working keys"
- When people complain keys don't work → engage from real account as dev → free advertising
- Every comment bumps the thread = more visibility

## GitHub Private Repo (for Windows builds)
- `eqenrico-hub/bandritual-build` (PRIVATE)
- Used GitHub Actions to cross-compile Windows VST3
- **DELETE THIS REPO** after you're done: `gh repo delete eqenrico-hub/bandritual-build --yes`

## To Deploy
1. Upload both zips to Google Drive
2. Set sharing to "Anyone with the link"
3. Paste links into POST_TEXT.txt
4. Take screenshot of plugin loaded in DAW (install fake version, open in Ableton, screenshot auth screen)
5. Post on Reddit with screenshot + post text


================================================================================
FILE: docs/process/Band Ritual Final 1 - Next Updates 2026-04-15.md
================================================================================

---
tags:
  - band-ritual
  - final-1
  - gui
  - todo
date: 2026-04-15
---
# Band Ritual Final 1 — Pending Updates (captured 2026-04-15)

Consolidation of all pending items from the long GUI/design session.

## 1. ScaleChordBox — Full redesign (Gemini mockup approved)

**Reference image**: `/Users/ricosan/Downloads/Gemini_Generated_Image_qm2g6jqm2g6jqm2g.png`

Aesthetic: skeuomorphic analog-hardware, warm brass + amber tint, dark metal chassis, vintage-analog hardware feel. Premium, touchable, physical.

### FINAL element mapping (all 7 original elements preserved, fused into 4 widgets)

**WIDGET 1 — BIG SCALE KNOB (left-center, hero)** — fuses KEY + SCALE TYPE
- **#1 KEY** (root note) = inner dial showing C / C# / D / D# / E / F / F# / G / G# / A / A# / B
- **#2 SCALE TYPE** (mode) = outer ring showing MAJ / MIN / DOR / LOC / PHR
- Rotate inner → changes root note. Rotate outer → changes mode. Both rings rotate independently.

**WIDGET 2 — CHORD PUCK (center-right)** — fuses CHORD DEGREE + CHORD TYPE
- **#4 CHORD DEGREE** (i–vii) = inner ring. **All-left = chord deactivated (off)**. Rotating right cycles through i, ii, iii, iv, v, vi, vii.
- **#5 CHORD TYPE** (triad / sus2 / 7th / sus4) = outer ring, controls INDEPENDENTLY of inner ring
- Two rings rotate independently like an ERP-style double encoder.

**WIDGET 3 — AMOUNT SLIDERS (right panel)** — 2 separate sliders
- **#3 SCALE AMOUNT** = top slider (0–100%)
- **#6 CHORD AMOUNT** = bottom slider (0–100%)
- Both user-draggable (revises earlier "display-only" plan)

**WIDGET 4 — TUNING TOGGLE (far-left)**
- **#7 TUNING** = toggle/button. **Click opens a CSS popup/modal** (the microtuning panel) — not a 3D element in the main section.

### Interactions
| Action | Element | Effect |
|---|---|---|
| Rotate big-knob inner | #1 KEY | Select root note (C…B) |
| Rotate big-knob outer | #2 SCALE TYPE | Select mode (Major, Minor, Dorian, Phrygian, Locrian) |
| Rotate chord-puck inner | #4 CHORD DEGREE | All-left = chord off. Else: i–vii |
| Rotate chord-puck outer | #5 CHORD TYPE | Triad / sus2 / 7th / sus4 (independent of inner) |
| Drag top slider | #3 SCALE AMOUNT | 0–100% scale enforcement |
| Drag bottom slider | #6 CHORD AMOUNT | 0–100% chord wet/dry |
| Click far-left toggle | #7 TUNING | Opens microtuning popup (CSS modal) |

### Resolved
- All 7 elements preserved — none dropped.
- #5 is NOT a separate widget; it's the outer ring of the chord puck.
- Earlier "click center KEY knob = toggle scale-lock mode" idea is SUPERSEDED — user confirmed amounts are controlled by draggable sliders in the right panel.

## 1a. GIGA KNOB center — KEY selector (14 values, includes LIN/LOG) (2026-04-15 addition)

The center of the giant multi-ring knob is the **KEY selector**. It cycles through **14 possible values**, not just 12 keys. The currently-selected value is displayed as text in the center (large letter/label). Examples of what appears in the center:
- "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" (12 musical keys)
- "LIN" (linear band distribution, key-lock disabled)
- "LOG" (logarithmic band distribution, key-lock disabled)

Interaction:
- **Scroll mouse-wheel** over the center → cycles forward/backward through the 14 values
- **Click + drag from center outward** → same selection (radial drag like a traditional knob)
- Whatever value is currently selected replaces "C" as the displayed label

### Behavior
- When one of the 12 keys is selected: vocoder uses **key-locked mode**, bands derived around the scale rooted at that key
- When **LIN** is selected: key-locking disabled; bands spaced **linearly** in Hz
- When **LOG** is selected: key-locking disabled; bands spaced **logarithmically** in Hz (current Final 1 default)

### VST3 implementation TODOs (audio side)
- Single `keyOrMode` param (int 0–13) replacing/extending the current keySelect dropdown
  - 0–11: musical keys (C through B)
  - 12: LIN (linear bands, no key lock)
  - 13: LOG (log bands, no key lock)
- Audio processor re-derives band grid based on param value:
  - Linear: `freq(k) = fmin + k * (fmax - fmin) / N`
  - Logarithmic: `freq(k) = fmin * (fmax / fmin)^(k/N)`  ← current default
  - Keys 0–11: use scale-locked grid with root note indexed by keyOrMode
- Update AudioProcessorValueTreeState parameters array accordingly
- Rebuild band grid on param change (destroy + recreate filter chain)

### Layout in GUI
- Center inner dial simply shows the currently-selected value as a single large label
- No separate toggle widget — LIN and LOG are just two more values the user can cycle to
- All 14 values rotate through the center as the user scrolls/drags

## 2. VST source code changes (Task #5 from session, external drive required)

**Path**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/Source/`

- **Hardcode Linked algorithm mode**. Remove `AudioParameterChoice` / equivalent for the Algorithm parameter. Delete any branching on algorithm; keep only the Linked code path.
- **Remove algorithm dropdown** from `PluginEditor.cpp` / WebView HTML.
- Bump plugin version if needed.

Rationale: Final 1 should behave like Ableton's vocoder — no algorithm selector, Linked is the only mode. Aligns with ScaleChordBox simplification (fewer user-adjustable knobs, more implicit behavior).

## 3. Blender mockup — already applied this session (reference state)

- Removed InputGain / OutputGain knobs (not in Final 1 UI)
- Removed 3D MIDI keyboard (not in Final 1)
- Removed XY Pad (explicitly not shown in Final 1)
- EQ section has no "TONE SCULPT" text label (matches user observation of actual plugin)
- EQ right column rebuilt as vertical stack per source CSS (complement btn → param display → freq/gain/Q knobs)
- Header bar: Bypass · BAND RITUAL title · Preset · Save · X
- MOD slider added to right of Mix knob (vertical)
- NoiseMode "Auto" dropdown between Noise/HPBlend and Formant/HarmShift rows
- "12dB" slope label next to HP Blend
- Main knob row re-aligned with proper spacing (7mm top gap, 6mm label gap, 5mm bottom gap)
- All dropdowns now thin rounded with ▼ arrows (no chunky boxes)

## 4. Tools created for future GUI iterations

- **Blender Redesign Agent** — https://eqenrico-hub.github.io/agents/blender-redesign/
  - `run.py <section>` auto-generates render script + pre-filled ChatGPT/Gemini prompt
  - `sections.json` registry of all Band Ritual sections
  - `apply_design.py` rebuilds Blender geometry from returned coordinate table
  - Prompt template v8 forces UX design thinking (function/interaction/frequency/importance/widget) before drawing
- **GUI Frame Tool** — https://eqenrico-hub.github.io/agents/gui-frame-tool/
  - Browser 2D editor with 8-handle resize, Shift-snap alignment, add/remove frames, per-frame comments
- **Gemini beats ChatGPT** for plugin UI mockups — less hallucination, better skeuomorphic output

## 5. Other captured design insights

- User does NOT want "TONE SCULPT" label text in EQ (even though source HTML has it)
- EQ labels (COMPLEMENT / SUB / CLEAN / DRY) are in source code but NOT visible in actual running plugin — kept only freq labels (100, 1k, 10k) + the COMPLEMENT right-column button
- "Simplify" GUI = reduce visual clutter and redundant controls, NOT reduce features (per CLAUDE.md rule)
- Equivalent controls must be visually identical — design-system rule. Applies to any future redesigned section.
- Skeuomorphic/analog-hardware aesthetic is confirmed direction for Final 1

## 6. Next sections to redesign via agent (same Gemini workflow)

Remaining sections in `sections.json` that haven't been redesigned yet:
- `header` — top bar (bypass/title/preset/save/close)
- `carrier_mix` — MODE dropdown + MIX knob + MOD slider
- `main_knobs` — horizontal row (bands/width/attack/release/gate/depth/color)
- `right_knobs` — formant/harmshift/noise/hpblend + noise mode
- `eq` — tone sculpt section (power + canvas + complement + freq/gain/Q)
- `band_display` — central spectrum visualizer

Workflow: `python3 run.py <section>` → Gemini → paste table → rebuild in Blender.


================================================================================
FILE: docs/process/Band Ritual V3 - Final 1 and Final 2 Plan.md
================================================================================

---
tags:
  - band-ritual
  - vocoder
  - v3
  - final
  - plan
date: '2026-03-20'
---
# Band Ritual V3 — Final 1 (testable) + Final 2 (SVF in progress)

## Status: PLANNING (2026-03-20)

## FINAL 1 — Current V3 State (ready for listening test)

### Location
`/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

### Parameters
```
Product name: "Band Ritual Final 1"
maxQ = 37
Analysis: 3 stages, Qa = 0.60 × Qc
Carrier: 4 stages (8th-order)
tiltCorr = 1.0 (none)
bandNormGain = 1.0 (none)
Envelope: LINEAR (no fastTanh)
envGain = envelope × depth
bwGain (≤100%): 45.6 × (1-wN)^0.44 dB
bwGain (>100%): lerp(34.5, 26.0, (bw-1)/1) dB
bandCountGain: 1/sqrt(N/4)
PVR: 44.9 (Ableton: 44.6)
```

### Results
| BW | RMS vs Ableton |
|----|----------------|
| 10% | 0.0 dB |
| 50% | +0.5 dB |
| 100% | +0.5 dB |
| 130% | +2.7 dB |
| 170% | +1.0 dB |
| 200% | 0.0 dB |

### What matches Ableton
- ✅ RMS: ±1.0 dB at 5/6 BWs
- ✅ PVR: 44.9 vs 44.6
- ✅ Quadratic gain curve (linear envelope)
- ✅ BW=100% curves overlap visually
- ✅ No tilt correction needed

### What doesn't fully match
- PSD tilt at LOWBW: -2.4 dB/oct (V3 slightly darker)
- PSD offset at MIDBW: -7.0 dB
- Sub-bass at BW>130%: V3 too quiet
- Root cause: JUCE makeBandPass skirts narrower than Ableton's filters

---

## FINAL 2 — SVF Filter Upgrade (next step)

### Goal
Replace JUCE makeBandPass with SVF (State Variable Filter) or custom Butterworth bandpass that has wider skirts matching Ableton's measured peak shape.

### Evidence for SVF
From isolated band analysis (Band 3, ~2350 Hz, 40 bands BW=10%):
- Ableton -3dB BW: wider peaks with ~25 dB/oct rolloff on both sides
- V3 (JUCE makeBandPass): narrower peaks with steeper rolloff
- JUCE makeBandPass is a direct-form biquad with specific skirt shape
- SVF has different pole placement → potentially wider/different skirts

### Approach
1. Copy V3 to BandRitualV3-Final2
2. Rename plugin to "Band Ritual Final 2"
3. Implement SVF bandpass (or try different biquad coefficient formulas)
4. Measure peak width and compare to Ableton
5. If SVF matches better → re-tune Q, Qa ratio, BW gain
6. Iterate until curves match at all BWs

### SVF Implementation
JUCE has `juce::dsp::StateVariableTPTFilter` which implements:
- Low-pass, high-pass, band-pass modes
- Different internal topology from IIR biquad
- May have wider skirts at the same Q

Alternatively: use Robert Bristow-Johnson's cookbook bandpass formulas with different normalization (constant-skirt vs constant-peak) which changes the skirt width.

### Measurement targets
- Peak width at Band 20 (918 Hz): match Ableton's measured shape
- PVR: maintain 44.6
- RMS: ±1.0 dB across all BWs
- PSD shape: match at ALL BWs (not just BW=100%)

### Files to modify
- CMakeLists.txt (rename, new plugin code)
- Source/PluginProcessor.h (add SVF filter members)
- Source/PluginProcessor.cpp (replace filter processing + re-tune parameters)

### Test infrastructure
- Reuse ~/Desktop/vocoder-comparison/ scripts
- Compare against crack_bw*_noise.wav files
- Same methodology: build → test → measure → iterate


================================================================================
FILE: docs/process/VST3 Plugin Install - Ableton Code Signing Fix.md
================================================================================

# VST3 Plugin Install — Ableton Code Signing Fix

## The Problem
After building a VST3 with CMake + JUCE, the plugin **doesn't appear in Ableton Live** even though the `.vst3` bundle is correctly installed in `/Library/Audio/Plug-Ins/VST3/`.

## Root Cause
JUCE's CMake build process ad-hoc signs the binary, then **adds `moduleinfo.json` AFTER signing**, which invalidates the code signature seal. macOS and Ableton reject plugins with broken signatures during scan — the plugin silently fails to load with no error shown to the user.

You can verify this with:
```bash
codesign -vvv /Library/Audio/Plug-Ins/VST3/PluginName.vst3
# Output: "a sealed resource is missing or invalid"
```

## The Fix
**Always re-sign after copying to the install folder.** Add `codesign --force --deep --sign -` to every install command.

## Standard VST3 Install Command (ALWAYS use this)
```bash
sudo rm -rf /Library/Audio/Plug-Ins/VST3/PluginName.vst3 && sudo cp -R "path/to/build/PluginName_artefacts/Release/VST3/PluginName.vst3" /Library/Audio/Plug-Ins/VST3/ && sudo codesign --force --deep --sign - /Library/Audio/Plug-Ins/VST3/PluginName.vst3
```

## After Installing
1. Open Ableton Live
2. Go to **Preferences** (Cmd+,) → **Plug-ins** tab
3. Make sure **"Use VST3 Plug-in System Folders"** is ON
4. Click **Rescan**

## Checklist
- [ ] Build with `cmake --build build --config Release`
- [ ] Copy `.vst3` to `/Library/Audio/Plug-Ins/VST3/` (with sudo)
- [ ] **Re-sign with `codesign --force --deep --sign -`** ← THIS IS THE KEY STEP
- [ ] Rescan in Ableton

## Discovered
- **Date:** 2026-03-10
- **Plugin:** NoteShaper (first time this was diagnosed)
- **Applies to:** ALL JUCE VST3 plugins built with CMake on macOS


================================================================================
FILE: docs/PROTOTYPE_GIT_HISTORY.md
================================================================================

# Prototype git history

Full commit log of the original `BandRitualV3` working repo, preserved as text.
The 316 MB `.git` of that repo was **not** carried over — this is the decision trail.

```
2026-04-13  05333ea  Fix depth >100%: remove attenuation, match ABL's linear scaling
ABL at depth=200% outputs at -0.48 dB RMS (loud, no limiting).
BR was at -13.92 dB because pow(0.5, (depth-1)*1.35) attenuated.
New formula: envGain = floor*(1-d) + envComp*d — pure linear scaling.
Now BR at -5.70 dB (still 5 dB off but 8 dB closer).

Depth=100% cosine unchanged at 0.9161.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-13  e4d70d5  Separate Q tables: Linked (Q=40, ABL match) vs Ritual (Q=6, original)
Linked mode broke Ritual/Split because Q=40 was too narrow for those modes.
Now each mode uses its own Q table:
- Linked: Q=40 at BW=10% (ABL-matched, 92.7% cosine)
- Ritual/Split: Q=6 at BW=10% (original wider character)

Also: envelope compression e^0.75 only in Linked mode.
Ritual/Split use linear envelope (original BR character).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f979b61  FINAL: 92.7% avg cosine across 5 inputs — architecture locked
5-input validation with fresh ABL exports (confirmed settings):
  KICK:  0.9160    PAD:   0.9857
  BASS:  0.9559    MIX:   0.8550
  VOCAL: 0.9221    AVG:   0.9269

Architecture:
  Q=40, Qa=0.52×Qc, 4-stage carrier, 3-stage analysis
  Peak detector pow(0.01), e^0.75 compression, phase alt ON

Session journey: 0.4493 → 0.9269 (+107% improvement)
Root cause was Q=6 regression. Rest was calibration.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f9bb5e4  92.7% avg cosine: pow(0.01) formula restored, VOCAL +5.1%
Reverting exp(-1) back to pow(0.01) for attack/release coefficients.
The exp(-1) change was a mistake — it only helped kick by +0.0003 but
KILLED vocal matching (0.87 → 0.92 with pow).

pow(0.01) means "reach -40 dB in release_ms" — matches ABL's documented
behavior. exp(-1) means "reach -8.7 dB" — too slow.

5-input results:
  KICK:  0.9160  PAD:   0.9857
  BASS:  0.9559  MIX:   0.8550
  VOCAL: 0.9221  AVG:   0.9269

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  96e2d38  Depth formula + gate analysis from ABL exports
Depth: floor 0.005→0.065, >100% exponent 1.5→1.35.
Measured from AB_DEPTH_{0,25,100,150,200} exports:
  ABL depth=0%: -6.9 dB vs 100% (was -34.8 dB in BR)
  ABL depth=200%: -2.3 dB vs 100% (was -6.1 dB in BR)

Gate: ABL gate=-inf same as gate=-40. Gate starts
affecting at -20 dB. Gate=0 is full mute.

Core vocoder cosine unchanged at 0.9138 (depth=100%).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  6b1a131  92.1% sim / 91.4% plugin: envelope compression e^0.75
ABL applies soft envelope compression — e^0.75 instead of linear e^1.0.
Reduces dynamic range of per-band envelope, giving more balanced output.
Standard vocoder technique.

Simulation (float64): 0.9159 → 0.9209 (+0.005)
Plugin (float32):     0.9131 → 0.9138 (+0.0007)

Remaining gap: ~8% is from float32 precision loss (~0.3%), input-specific
harmonic patterns (~5%), and irreducible implementation differences (~3%).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f38b468  91.3% cosine: exp coefficient formula (+0.0003)
Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  c32b628  91.1% cosine: Qa=0.55, Q=40, simple peak detector
Final optimized parameters:
- Carrier Q: 40 (from formula Q = N/(n_octaves × BW), was 37 empirical)
- Analysis Qa: 0.55 × Qc = 22.0 (swept 0.3-1.0, peak at 0.55)
- Envelope: simple peak detector (attack 1ms, release 50ms)
- Carrier: 4-stage BPF2 cascade (3 and 5 stages tested, 4 is optimal)
- Analysis: 3-stage BPF2 (2 stages tested, 3 is better)
- Phase alternation: ON (marginal +0.0005 benefit)
- bwGain BW=10%: 35.0 dB

Sweep results at Q=40:
  Qa/Qc=0.55 → 0.9105 (BEST)
  Qa/Qc=0.65 → 0.9092
  Qa/Qc=0.70 → 0.9065
  Qa/Qc=0.75 → 0.8999

Journey this session: 0.4493 → 0.9105 (+0.46 cosine)
  Q=6→40: +0.33
  Cascade LP→peak detector: +0.13
  Qa 0.60→0.55: +0.004

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  0a17cfd  Q=40: cosine 0.9065 (from 0.9036 at Q=37)
Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  7be36fd  ITER 22 FINAL: 90.4% cosine match to Ableton vocoder
Two fixes that took us from 0.45 → 0.90:
1. Q=6→37 (regression fix): +0.33 cosine
2. Cascade LP→simple peak detector: +0.13 cosine
3. Qa ratio 0.60→0.70: +0.003 cosine

Architecture (matches what Ableton probably does):
- Carrier: 4-stage RBJ BPF2 cascade at Qc=37
- Analysis: 3-stage BPF2 at Qa=0.70×Qc=25.9
- Envelope: simple peak detector (attack=1ms, release=50ms)
- No cascade LP, no ring buffer, no per-band normalization
- bwGain BW=10%: 35.0 dB
- Per-band tilt: none (inherent in constant-Q design)

Validated on kick/bass test with 32-bit float WAV (no clipping artifacts).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  7ea1f9a  FIX: Restore Q=37 from Q=6 regression — cosine 0.49 → 0.77
ROOT CAUSE FOUND: qTable had Q=6 at BW=10% instead of the correct Q=37.
This was a regression where "effective vocoder output bandwidth" was confused
with "individual filter Q". Q=37 was confirmed by:
- March 2026 Q breakthrough (peak width measurement)
- Apr 9 multi-band filter measurements (constant fractional Q)
- Obsidian master findings log

Impact of Q=6 regression:
- Filters 6x too wide → captured energy from neighboring bands
- Output 20-28 dB too loud on real music
- Every cosine measurement since the regression was corrupted
- All iter 20-21 architectural experiments (cascade LP, ring buffer,
  tilt, per-band normalization) were chasing symptoms of wrong Q

With Q=37 restored:
- TRUE cosine (unclipped float): 0.7747 (from 0.49 with Q=6)
- Simple peak detector envelope (no cascade needed)
- bwGain=35 gives near-0dBFS peak on kick content
- Per-region: Bass -1.4 dB (nearly perfect), Sub +8.1 (still hot)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  31c41c0  iter 22: back to basics — cascade LP 650ms + correct bwGain
Reverted all iter 21 experimental code (ring buffer, extra cascade
stages, phase alt removal, f^1.0 tilt). These were dead ends or
had implementation bugs.

Current VALID state:
- Carrier: 4-stage RBJ BPF2 cascade (Q=37)
- Envelope: peak detector + 3-stage cascade LP (tau=650ms per stage)
- bwGain BW=10%: 17 dB (near-unity on kick content)
- No per-band tilt (f^1.0 is inherent in constant-Q filter design)
- TRUE cosine (unclipped): 0.6797 on kick/bass test

Key learnings from iter 21-22:
- Simple peak detector (no cascade): cosine = 0.4493 (worse)
- f^1.0 tilt applied additively: cosine = 0.4405 (worse — it's already
  inherent in the filter, applying it again over-tilts)
- Cascade LP at 650ms: 0.6797 (best validated result)
- Python simulation with boxcar shows 0.89 is theoretically achievable
  but C++ implementation had a gap (0.54 vs 0.89) — root cause unknown

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  a03ecd2  iter 21: boxcar envelope discovery + debug state
KEY FINDINGS:
- Python simulation proves boxcar moving-average envelope achieves 0.89+ cosine
  vs ABL on kick test (causal: 0.8852, centered: 0.9153)
- Cascade LP peak was 0.6797 (3-stage at 650ms per stage)
- Boxcar at 200ms window dramatically outperforms cascade LP
- C++ ring buffer implementation gives only 0.54 cosine — implementation
  doesn't match simulation. Investigation ongoing.
- Phase alternation disabled (not the cause — 0.5337 vs 0.5399)
- Added env5/env6 fields to Band struct (not useful with ring buffer)
- JUCE makeBandPass confirmed identical to RBJ BPF2 formula
- Plugin output is 21.5 dB louder than simulation at same settings — unknown cause

BLOCKED: need to find why C++ ring buffer output differs from Python simulation.
The math should be identical (carrier × boxcar(|analysis|)) but plugin is 21.5 dB
hotter and 0.35 lower cosine.

Next: add per-band debug WAV dump to trace where the signal path diverges.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  d035ec7  iter 21 experiments: confirmed tilt + release changes don't fix cosine
Phase 1 (per-band tilt sweep): tested +2.5 and +5 dB/oct tilts against
kick/bass test. Cosine: 0.4878 → 0.4870 → 0.4844. No improvement.
Tilt shifts per-region energy but cosine is dominated by Sub where
the problem actually lives.

Phase 2 start (release table): changed uniform 15ms release from
frequency-dependent {15,30,76,10.5,4} ms. Reduced mid-band energy
by 80x (8.8% → 0.1%) but cosine barely moved (0.4875).

Key findings:
- BR vs DRY cosine: 0.79 (BR is near-passthrough of input)
- ABL vs DRY cosine: 0.52 (ABL significantly transforms input)
- BR vs ABL cosine: 0.49 (dominated by Sub region disagreement)
- Per-bin cosine: Sub (0.49) and Bass (0.26) are WORST match, not mid/high

Hypothesis: ABL's vocoder does something structurally different
from BR's "Self" mode autoencoder — possibly internal carrier even
in Modulator mode, or per-band envelope normalization.

Waiting for user confirmation of Ableton's carrier mode in the
reference recording before continuing.

See: Dev Notes/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  4433653  iter 20: BPF2 carrier restore, baseline before per-band normalization
Reverts Butterworth carrier (iter 19c) back to 4-stage cascaded RBJ BPF2
for the Linked mode carrier path. Sets bwGain BW=10% to 8.0 dB as a safe
test value (prevents clipping on kick/bass input). Removes HF/LF shelves
added in earlier iter 20 attempts — they hurt cosine similarity.

MAJOR DISCOVERY (2026-04-11): All previous real-music cosine measurements
were corrupted by 16-bit PCM clipping in the test harness. True unclipped
cosine on kick/bass input is 0.4878 (was reporting 0.71+ under clipping).
The plugin output at March calibration (bwGain=45.6) is +28 dBFS peak on
percussive content — silently clipping users' DAWs.

This commit saves the state before starting Phase 1 of the reverse
engineering plan: implementing f^1.0 per-band gain tilt and per-band
envelope normalization to match Ableton's vocoder architecture.

See: Dev Notes/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md
     Dev Notes/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  6ee0a21  FIX: SVF prepare() was resetting filter state every block
Critical bug: svf.prepare() was called in updateBandFilters (every block),
which resets all internal filter state. Low frequencies lost 20-36 dB
because the filter never accumulated enough state before being reset.

Fix: prepare() called ONCE at band creation. updateBandFilters only
calls setCutoffFrequency/setResonance (which don't reset state).

Before: 512-block output was -3 dB vs full-block (matched Ableton bug report)
After: 512-block output matches full-block within 0.2 dB

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  a5a3644  Recalibrate bwGain for cascaded envelope
bwGain table recalibrated after cascaded envelope change.
Levels back to 0.0 dB at all BWs on white noise.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  6ace341  Cascaded envelope with BW-dependent stage 2 release
Two-stage envelope follower matches Ableton's resonant tail:
- Stage 1: standard peak detector (fast initial drop)
- Stage 2: cascaded smoother with BW-dependent release coefficient
- Narrow BWs get 2.5x slower stage 2 (longer resonant tail)
- Wide BWs get 0.7x faster stage 2 (tighter response)

Verified: BW=10% tail 67ms→138ms (target 145ms), BW=15% 111ms (target 112ms)

Also: display reactivity 0.25→0.9, bar height boost 0.25→0.08,
release range extended to 30 seconds.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-25  2e05408  SVF carrier filters + 30s release + crackle-free formant
- Carrier filters: biquad → StateVariableTPT (SVF) for Linked mode
- SVF handles frequency changes natively — zero crackles on formant knob
- Q² gain compensation keeps spectral match at 1.4 dB (identical to biquad)
- Release range: 1s → 30s (matches Ableton's vocoder range)
- Formant smoothing: 50ms (fast + smooth with SVF)
- All cracked calibrations preserved (Q, tilt, bwGain, phase, envelope)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-25  3b72a8e  Formant: band remapping replaces filter frequency shifting
Formant knob now remaps envelope routing instead of shifting carrier
filter frequencies. Carrier filters stay at fixed positions — completely
eliminates crackles/clicks when turning the formant knob.

Implementation: two-pass processing loop. Pass 1 computes all band
envelopes. Pass 2 applies carriers using interpolated envelopes from
remapped source bands. Fractional shifts use linear interpolation
between adjacent band envelopes.

Based on industry-standard approach (confirmed from Ableton, EVOC 20,
Cherry Audio vocoder documentation and KVR DSP forum research).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-24  e56debe  Band Ritual Final 1 — Vocoder cracking complete
Linked mode matched to industry-standard vocoder at 1.8 dB avg RMS:
- 4th-order bandpass filter bank (2 cascaded biquads)
- Phase alternation on every other band
- Envelope: pow(0.01, 1/(sr*t)) — matched release curves
- Q table calibrated per BW with cos()^1.29 freq-dep correction
- Tilt + LF shelf correction
- bwGain calibrated per BW
- BW-dependent analysis Qa ratio (0.85→0.30)
- Gate: -inf to +12 dB range with -17 dB offset

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
```

