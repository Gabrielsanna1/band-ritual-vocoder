# Index — all 59 development notes

Every note written during Band Ritual's development, exported from Obsidian. Nothing was filtered
out, including dead ends and superseded conclusions — the wrong turns are part of the record.

⚠️ **Chronology matters.** Notes contradict each other, because understanding changed. When two
notes disagree, **the later one wins**, with one hard exception: anything measured before
`Band Ritual - WAV Clipping Bug Discovery 2026-04-11` was measured on clipped audio and is invalid.

**Read `CONTEXT.md` first** — it's the distilled version. Come here for depth.
- [`UNCERTAINTY.md`](UNCERTAINTY.md) — what these notes are NOT sure about; read after CONTEXT and FILTERS

---

## Suggested reading order (the 7 that matter most)

0. [`FILTERS.md`](FILTERS.md) — ⭐⭐ **the filterbank decision, start to finish.** Every topology tried
   and rejected for LINKED mode, with the measurement that killed each one. Synthesised from the notes
   below and verified against the source.
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
