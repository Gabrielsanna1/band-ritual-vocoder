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

> **Which measurement these figures come from.** The values in this table are from the **float64
> simulation**, whose baseline was 0.9159 and whose final was **0.9209**. The 0.9138 quoted above is the
> **float32 plugin**, whose baseline was 0.9131. The two environments differ by about 0.007, so a figure
> here is only meaningful against the float64 baseline — compare within an environment, never across.
> This is why `RMS envelope 0.9199` and `2x oversampling 0.9159` read as higher than 0.9138 without
> being better: the first is 0.001 below the float64 final, the second *is* the float64 baseline.
> See [`UNCERTAINTY.md`](UNCERTAINTY.md) §2.

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

- **What is *not* certain → [`UNCERTAINTY.md`](UNCERTAINTY.md)** — the eight places where the record is
  thin, extrapolated, or resolved-but-unlabelled, ranked by how much each matters for a rebuild
- **Filters, in full → [`FILTERS.md`](FILTERS.md)** — every topology tried and rejected for the LINKED
  filterbank (single biquad, 2/3/4/5-stage cascades, SVF, Butterworth, STFT), the measurement that
  killed each, the constant-fractional-Q law, and why the analysis filter differs from the carrier
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
