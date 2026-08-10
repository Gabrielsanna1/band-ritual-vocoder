# FILTERS — every topology tried, and why LINKED ended up where it did

*The complete filter-design record for **LINKED mode** — the default algorithm, and the one in which
Ableton's Vocoder was reverse-engineered. If you only read one technical document before rebuilding
the filterbank, read this one.*

Everything below is cross-checked against the actual shipped source (`Source/PluginProcessor.cpp`),
not just the notes. Where a note and the code disagree, the code is quoted.

---

## 0. The answer, up front

| | Chosen | Value |
|---|---|---|
| **Carrier filter** | RBJ bandpass biquad, **4 stages cascaded** (8th-order) | `juce::IIRCoefficients::makeBandPass` ×4 |
| **Analysis filter** | RBJ bandpass biquad, **3 stages cascaded** (6th-order) | same call, 3 stages |
| **Carrier Q** @ BW=10% | **40** | `qTableLinked[0] = {10.0f, 40.0f}` |
| **Analysis Q** | **0.52 × carrier Q** = 20.8 | `qaRatio = 0.52f` at BW=10% |
| **Q law** | constant fractional Q: `Q ≈ N / (n_octaves × BW)` | measured from Ableton |
| **Result** | **0.927 cosine** vs Ableton, avg over 5 inputs | |

**Why this and not something else:** Ableton's filters were *measured* at **47–52 dB/octave skirt slope**
and **43–49 dB peak-to-valley**. Only a 4-stage cascaded peaked biquad reproduces both numbers.
Everything with a flatter passband or a shallower skirt failed for a *structural* reason that no amount
of gain compensation could fix. That sentence is the whole document — the rest is the evidence.

---

## 1. Why LINKED is the mode that matters

The plugin has three modes. Only one is a reverse-engineering target:

| Mode | Carrier Q @ BW=10% | Purpose |
|---|---:|---|
| **LINKED** (default) | **40** | The Ableton match. Everything in this document. |
| Ritual | 6 | The original, wider, more characterful Band Ritual sound |
| Split | 6 | Same wide Q as Ritual |

They are genuinely different filterbanks, held in two separate tables in the source:

```cpp
static const float qTableLinked[][2] = {   // {BW%, Q}
    {10.0f, 40.0f}, {15.0f, 27.0f}, {20.0f, 19.0f}, {30.0f, 13.0f},
    {40.0f,  9.0f}, {50.0f,  7.0f}, {70.0f,  5.0f},
    {100.0f, 3.5f}, {130.0f, 2.8f}, {170.0f, 2.2f}, {200.0f, 2.0f}
};
static const float qTableRitual[][2] = {
    {10.0f,  6.0f}, {15.0f,  5.0f}, {20.0f, 4.5f}, {30.0f, 3.5f},
    {40.0f,  3.0f}, {50.0f,  2.7f}, {70.0f, 2.4f},
    {100.0f, 2.0f}, {130.0f, 1.8f}, {170.0f, 1.7f}, {200.0f, 1.7f}
};
```

The split exists because forcing Q=40 on Ritual/Split **broke them** — 40 is far too narrow for the sound
those modes are meant to make. Envelope compression (`e^0.75`) is likewise LINKED-only; Ritual and Split
use a linear envelope. Keep this separation in the rebuild: *the Ableton match and the house sound are
two different filterbanks and should not be forced to share constants.*

---

## 2. The measured target (what Ableton actually does)

Before any topology can be judged, here's the ground truth it was judged against. From
`dsp/Ableton Vocoder - Complete Cracked Algorithm.md` (31 recordings) and
`dsp/Band Ritual - Ableton Filter Measurements 2026-04-09.md` (5 band-count exports):

| Property | Measured on Ableton |
|---|---|
| Skirt slope | **47–52 dB/octave** at mid frequencies |
| Peak-to-valley | **43–49 dB** |
| Implied order | **8th–10th** (4–5 cascaded biquad stages) |
| Q behaviour | **constant fractional Q** — scales linearly with band count |
| Passband shape | **peaked**, not flat |
| HF bands (>6 kHz) | asymmetric rolloff (bilinear-transform warping — structural, reproduce it, don't correct it) |

Isolated-peak measurements from a 4-band export:

| Peak | Freq | Rolloff | Implied order | P/V | Q |
|---|---|---|---|---|---|
| 1 | 41.7 Hz | 49.6 dB/oct | ~8th | 43.0 dB | 4.4 |
| 2 | 317.6 Hz | 61.2 dB/oct | ~10th | 46.6 dB | 6.7 |
| 3 | 2304 Hz | 53.8 dB/oct | ~9th | 48.8 dB | — |

---

## 3. Every topology tried — the rejection table

Chronological. Each row is a real experiment with a real measurement behind it.

| # | Date | Topology | Verdict | Killed by |
|---|---|---|---|---|
| 1 | 03-14 | **Single 2nd-order biquad BPF** (12 dB/oct) | ❌ rejected | Per-band RMS went from ±1.0 dB to ±2–4 dB. Passband too wide. |
| 2 | 03-16 | **2-stage RBJ cascade** (4th-order, 24 dB/oct) | ❌ too shallow | P/V only **26.2 dB** vs Ableton's 45.9. Half of Ableton's slope. |
| 3 | 03-16 | **3-stage RBJ cascade** (6th-order) | ❌ compromise, lost | P/V 31.5 dB — still 14 dB short. Every 6th-order config overcorrected in the sweep. |
| 4 | 03-17 | **4-stage RBJ cascade** (8th-order) | ✅ **CHOSEN** | P/V **40.8 → 41.9 dB**, slope ~48 dB/oct. Matches the measurement. |
| 5 | 03-17 | **5-stage** (10th-order) | ⏭️ not pursued | Would close the last ~4 dB of P/V, but CPU: 5 biquads × 2 ch × 40 bands. Diminishing returns. |
| 6 | 04-08 | **2-stage SVF** (4th-order state-variable) | ❌ **regression** | At Q=37, 1 octave out: 4× `makeBandPass` attenuates ~140 dB, 2× SVF only ~7 dB. **130 dB skirt difference** — ungainable. |
| 7 | 04-10 | **STFT / FFT vocoder** (2048, 75% overlap) | ❌ abandoned | Never beat the IIR path. Code survives as `STFTVocoder.*`, **not wired in**. |
| 8 | 04-11 | **4-stage Butterworth bandpass** (flat passband, 48 dB/oct) | ❌ rejected | Right slope, **wrong shape**. See below — this is the most instructive failure. |
| 9 | 04-11 | **Return to 4-stage RBJ BPF2** | ✅ confirmed | Cosine +0.018 immediately on the revert. |

### The two failures worth understanding

**Butterworth (#8) — right slope, wrong shape.** Butterworth had the correct 48 dB/oct rolloff, so on
paper it should have matched. It didn't, and the per-band diagnostic showed why:

| Bands | Freq | BR (Butterworth) vs Ableton |
|---|---|---|
| 12–13 | 136, 162 Hz | **+23 dB** |
| 15, 17 | 230, 326 Hz | **+23 dB** |
| 25–27 | 1.3–1.9 kHz | +12 to +15 dB |

Those bands sit *between* kick harmonics. Ableton's peaked BPF2 has deep nulls there (−41 to −50 dB);
Butterworth's **flat passband** scoops up energy from adjacent harmonics instead.

> **This is a structural difference, not a calibration issue. No amount of gain correction can reshape
> flat → peaked.**

The prior note justifying Butterworth ("RBJ BPF2 cascade had narrow peaked response causing 11–42 dB
inter-band gaps") turned out to be **wrong** — those gaps were measured at the wrong Q. At Q=37–40 the
BPF2 shape matches Ableton.

**SVF (#6) — the silent regression.** The carrier was switched to a 2-stage SVF, probably to get smooth
`setCutoffFrequency` for formant shifting, and the calibration was never redone. Level compensation
(`1/Q²`) hid it in the RMS numbers while the **skirts stayed 130 dB too wide**. It went unnoticed for
weeks and produced a per-band correction LUT that zigzagged ±25 dB between adjacent bands — a
textbook overfit that was actually chasing a structural defect. Two lessons:

- If you need continuous modulation later, **don't** silently swap topology to get it. SVF and cascaded
  biquad are not interchangeable, and gain compensation will disguise the difference in exactly the
  metric you're watching.
- **A correction curve that zigzags is a bug report.** Real DSP corrections are smooth. Alternating
  ±25 dB means the optimizer is fighting an artifact.

*(A third contributor to that same episode: `phaseSign = (bi % 2 == 0) ? 1.0f : -1.0f` inverted every
odd band, causing destructive interference at every band boundary. Cited a "KVR tip" about smoothing
overlap; at BW=10% with 40 bands it created far more notches than it fixed.)*

---

## 4. Q — the part that was wrong the longest

### The law

Ableton uses **constant fractional Q**. Measured across 4/8/12/16/20-band exports:

```
Q ≈ N_bands / (n_octaves × BW_fraction)      n_octaves = log2(highFreq / lowFreq)
```

| N bands | Predicted | Measured |
|---|---:|---:|
| 4 | 4.1 | ~4.6 |
| 16 | 16 | ~19 |
| 20 | 20 | ~22 |
| 40 | 40 | **37–41** ✓ |

Which is just good filterbank design: each band covers a fixed fraction of an octave, so Q must scale
with band density.

### The bug this exposed (fixed 2026-06-12)

The `qTable` was keyed on the **Band Width knob only** — no band-count term, no range term. So narrowing
the LO↔HI range did **not** sharpen the bands, which is exactly the difference Enrico could hear against
Ableton. The fix multiplies the existing table by a scale factor:

```cpp
if (algorithmMode == 1) {               // LINKED only
    constexpr float kRefBands = 40.0f, kRefOctaves = 9.81f;   // log2(18000/20)
    float nOct   = jmax(0.1f, log2(jmax(1.0001f, hiF / jmax(20.0f, loF))));
    float qScale = (nBands / kRefBands) * (kRefOctaves / nOct);
    QcarrierBase = jlimit(0.5f, 200.0f, QcarrierBase * qScale);
}
```

Deliberately **identity at the reference point** (N=40, 20 Hz–18 kHz → `qScale` = 1.0 exactly), so the
0.927 cosine match is preserved bit-for-bit. Verified by measurement — Q doubles per octave-halving:

| Range | Octaves | Measured Qeff |
|---|---:|---:|
| 125–8000 | 6.00 | 7.4 |
| 354–2828 | 3.00 | 15.9 |
| 595–1682 | 1.50 | 28.3 |
| 794–1260 | 0.67 | 47.5 |
| 891–1122 | 0.33 | 62.7 |

**In the rebuild, derive Q from the law, not from a lookup table.** The table was a 40-band snapshot of
a formula, and keying it on one variable when it depends on three is what caused the bug.

---

## 5. The analysis filter is NOT the carrier filter

Easy to miss, and it matters:

| | Stages | Order | Q |
|---|---:|---:|---|
| Carrier | **4** | 8th | Qc = 40 @ BW=10% |
| Analysis | **3** | 6th | **0.52 × Qc** = 20.8 |

```cpp
float qaRatio = 0.52f - (0.52f - 0.26f) * norm;  // ITER 22: peak cosine 0.9128 at 0.52 (Q=40)
Qanalysis = Qcarrier * qaRatio;
```

The analysis filter is **deliberately wider and shallower** than the carrier. The ratio was swept
0.40–0.85 and peaked at 0.52. Note it drifted to 0.85 in April code and had to be caught and restored —
it's an easy value to lose.

Both use the same `juce::IIRCoefficients::makeBandPass` call; only stage count and Q differ.

---

## 6. Filter order vs transient response — the trap

Higher order is not free. At Q=60, 8th-order gives an effective bandwidth of ~8 Hz at 1 kHz, and a
**ring-up time of ~76 ms**. A sine sweep passes through in ~12 ms, so the filter never reaches full
amplitude → a −13 dB sine deficit that looks like a catastrophic bug.

Two things were learned the hard way:

1. **It mostly isn't a bug.** Ableton has the same ring-up at the same order, so on a relative comparison
   it largely cancels. Ableton's own sine/noise ratio (sine peak 4.31 vs noise 2.07) proves its filters
   ring up fine.
2. **Most of the apparent deficit was wrong compensation, not physics.** The fix was flipping the sign of
   the high-Q correction exponent — `hiQExp` from +0.7 to −0.35, i.e. *boost* sine at high Q instead of
   cutting it. That single sign flip closed a 12.6 dB gap to 0.41 dB.

Still, for real audio this matters less than the sweep suggests: vowel formants are stable enough to let
the filter ring up. **Sine-sweep tests over-penalise high-order filterbanks** — don't tune to them alone.

---

## 7. What to carry into the rebuild

**Keep:**
- 4-stage cascaded RBJ bandpass for the carrier, 3-stage for the analysis
- Qa = 0.52 × Qc
- Constant-fractional-Q law, computed — not a table
- Peaked, never flat
- Separate LINKED / Ritual filterbanks

**Don't re-test** (§3 — all measured and rejected): single 2nd-order, 2-stage, 3-stage, SVF, Butterworth, STFT.

**Do differently:**
- Derive Q from `N / (n_oct × BW)` from day one instead of a BW-keyed table
- No per-band correction LUT. If one appears necessary, something structural is broken — that's what
  the ±25 dB zigzag was telling us.
- Multi-input validation from the start. The LUT episode happened because everything was fitted to one
  kick/bass file.
- Judge on **cosine similarity of the spectrogram**, not per-region RMS. Chasing per-region RMS actively
  made cosine worse (see §8).

---

## 8. One more warning: the metric you optimise decides what you build

Frequency-dependent gain shaping was tried repeatedly to fix per-region RMS. It improved RMS and
**reduced cosine every single time**:

| Config | Corrections | Cosine |
|---|---|---:|
| 19l baseline | HF −3, sub, −1 dB trim | 0.7066 |
| 20a | HF −10, bass −2, −3.5 dB trim | 0.7059 |
| 20a (0.5 trim) | HF −10, bass −2, −6 dB trim | 0.7007 |
| 20b | HF −13 @1–4k, bass −5 | 0.6880 |
| 20c | no shelf, normGain 0.5 | 0.7066 |

Meanwhile a **uniform** broadband gain (`bwGain`) improved cosine monotonically. Shelves reshape the
spectrum away from the target even as the coarse numbers improve. **Per-region RMS is a misleading
metric; cosine on the spectrogram is the one that tracks "does this sound like Ableton".**

---

## 9. Sources

Every claim above traces to one of these:

- `dsp/Band Ritual - Filter Order Correction.md` — the 2/3/4-stage hunt, P/V measurements, ring-up analysis, `hiQExp` sign flip
- `dsp/Band Ritual - 2nd Order Carrier Filter Experiment.md` — single-biquad attempt and rejection
- `dsp/Band Ritual - Linked Mode SVF Regression 2026-04-08.md` — SVF regression, phase inversion, the overfit LUT
- `dsp/Band Ritual - Iter 12 Architecture Restored 2026-04-09.md` — restoring the 4-stage cascade
- `dsp/Band Ritual - Ableton Filter Measurements 2026-04-09.md` — constant-fractional-Q, skirt slopes
- `dsp/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md` — Butterworth rejection, shelves-hurt-cosine table
- `dsp/Band Ritual - Iter 22 Final Architecture 2026-04-12.md` — final locked constants
- `dsp/Band Ritual - Q-from-range + close LO-HI 2026-06-12.md` — the Q-scaling fix
- `dsp/Band Ritual - STFT Integration Attempt 2026-04-10.md` — the FFT path
- Code: `Source/PluginProcessor.cpp` (`updateBandFilters`, ~L1770–2230)

⚠️ Standard caveat: anything measured before **2026-04-11** was measured on clipped 16-bit audio
(`dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md`). The *relative* filter-order comparisons
in §3 survive this — they were shape/slope ratios, not absolute levels — but treat any absolute dB
figure from March with suspicion.
