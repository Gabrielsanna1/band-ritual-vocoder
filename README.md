# Band Ritual — Vocoder Plugin (V0 prototype)

**Band Ritual** is a vocoder audio plugin (C++ / JUCE, VST3 & AU) built by Ritual Audio Labs. The core
of the project was **reverse-engineering Ableton Live's stock Vocoder** to a *measurable* match, then
building a plugin around that engine with its own character modes, preset system and custom GUI.

This repository is the first working prototype (V0): source code, the full development record, and
GUI renders.

---

## Highlights

- **Matched Ableton's Vocoder to 0.927 cosine similarity** (average over 5 test inputs), up from 0.45
  at the start. Cosine similarity between the plugin's output and Ableton's, same input and same
  settings, was the metric behind every design decision.
- **Data-driven DSP.** Filter topologies, Q laws, envelope followers and gain curves were chosen by
  rendering both engines and measuring the result, not by ear. This ruled out more than a dozen
  textbook techniques that measured worse.
- **Recovered the algorithm** behind Ableton's vocoder: cascaded RBJ bandpass filterbanks, a
  full-wave peak-detector envelope with `e^0.75` compression, and a constant-fractional-Q law
  `Q ≈ N / (n_octaves × BW)`, plus measured depth and gain constants.
- **Shipped as a full product prototype**: three processing modes, multiple carrier sources, presets,
  a filmstrip-rendered skeuomorphic GUI designed in Blender, and licensing integration.

---

## The DSP engine

```
For each band i:
  1. analysis  = cascaded_bandpass(modulator, fc[i], Qa)   // 3 stages, RBJ BPF2 (6th order)
  2. envelope  = peak_detect(|analysis|)                    // full-wave rectify, peak follower
  3. envelope  = envelope ^ 0.75                            // slight compression
  4. carrier_f = cascaded_bandpass(carrier, fc[i], Qc)      // 4 stages, RBJ BPF2 (8th order)
  5. band_out  = carrier_f * envelope * band_gain * depth_factor
Sum all bands -> output
```

| Component | Final value | How it was established |
|---|---|---|
| Carrier filter | 4-stage cascaded RBJ bandpass | swept 3 / 4 / 5 stages, matched Ableton's measured 47–52 dB/oct skirts |
| Carrier Q (BW = 10%) | 40 | derived from the Q law, confirmed by a 37–43 sweep |
| Analysis Q | 0.52 × carrier Q | fine sweep 0.40–0.85 |
| Envelope follower | simple peak detector | beat LP, RMS and power-detection followers |
| Envelope compression | `e^0.75` | swept 0.5–1.0 |
| Depth | floor 0.065, exponent 1.35 above 100% | measured from Ableton depth exports |

**Modes**
- **Linked**: the Ableton-matched filterbank (Q = 40).
- **Ritual / Split**: a wider (Q = 6), more characterful house sound, kept as a separate Q table.

**Carrier sources**: internal (self), external sidechain, noise, saw, square, sine and pitch-tracked.

### Approaches evaluated and rejected (measured)

| Technique | Result |
|---|---|
| Textbook low-pass envelope (10–100 Hz) | 0.84–0.92, worse than a plain peak detector |
| RMS / power (y²) detection | 0.920 / 0.811 |
| Carrier / envelope normalization | 0.733 / 0.855 |
| Per-band spectral tilt | 0.44, actively harmful because constant-Q already handles tilt |
| 2× oversampling | no improvement |
| SVF, Butterworth and single-biquad filterbanks | structurally unable to reproduce Ableton's skirt slope |
| STFT / FFT vocoder (2048 / 512, 75% overlap) | prototyped and abandoned in favour of the IIR path |

### Engineering lessons

- A single wrong constant (a Q = 6 regression) caused symptoms that looked like deep algorithmic
  problems. **Validating the basic parameters first** became a core rule.
- 16-bit WAV clipping had been quietly corrupting the measurement pipeline. Catching it led to
  re-validating every earlier result.
- The target turned out to be simple. Frequency-dependent "corrections" made the match worse, not better.

---

## Tech stack

- **C++17, JUCE**, CMake: VST3 and AU plugin formats
- **Python**: offline validation suite (rendering, cosine-similarity scoring against Ableton exports)
- **Blender + Photoshop**: GUI chassis and knob renders, exported as filmstrips
- Experimental **WebView (HTML/CSS/JS) GUI** and **STFT** engine branches (kept for reference)

---

## Repository layout

```
Source/                 plugin source (JUCE)
  PluginProcessor.*     DSP engine: filterbank, envelopes, modes, parameters
  PluginEditor.*        GUI
  CarrierGenerator.*    carrier sources
  PresetManager.h       presets
  FilmstripLookAndFeel.* filmstrip knob rendering
  STFTVocoder.*         experimental FFT engine (not wired in)
  webui/                experimental HTML GUI (disabled)
  *_backup.*            historical snapshots of earlier iterations
docs/
  CONTEXT.md            project overview, final algorithm, lessons learned
  FILTERS.md            every filter topology tried and the measurement that ruled each out
  UNCERTAINTY.md        known open questions, ranked
  dsp/  gui/  process/  59 development notes: measurements, iterations, GUI and build process
bundles/                the whole record flattened into single Markdown files
reference-renders/      GUI chassis and knob renders
CMakeLists.txt
```

Good places to start: [`docs/CONTEXT.md`](docs/CONTEXT.md) and [`docs/FILTERS.md`](docs/FILTERS.md).

---

## Status

V0 is a working prototype that reached its goal. A cleaner, test-driven rebuild is in progress. It
reuses the measured constants and the validation method, and replaces the monolithic code structure.
