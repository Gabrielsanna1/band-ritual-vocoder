# Band Ritual — First Prototype (reference only)

> ## 👋 Gabriel — start here
>
> **What this is:** the first version of Band Ritual, which I built between March and June 2026.
> It works, and it hit its goal. It is also messy, because I wrote it fast while figuring things out.
>
> **What this is NOT:** the thing we're building. **We are rebuilding from scratch, step by step, in the
> other repo.** Nothing here should be copy-pasted forward. This repo exists purely so you can *read* it
> and understand what I already learned, so we don't spend months relearning it.
>
> **Do these three things, in order:**
>
> 1. **Read [`docs/CONTEXT.md`](docs/CONTEXT.md)** — 5 minutes. It's the whole project distilled: what the
>    plugin does, the algorithm I reverse-engineered out of Ableton, the exact measured constants, and a
>    table of everything I tried that **didn't** work.
>    Then **[`docs/FILTERS.md`](docs/FILTERS.md)** — every filter topology I tried for the band
>    filterbank, which ones I rejected and the measurement that killed each one, and why LINKED
>    (the default mode, the one where I matched Ableton) ended up as a 4-stage cascaded RBJ bandpass.
> 2. **Skim the [trust table](#trust-table--read-this-before-believing-any-file) below** before opening any
>    C++. Roughly half the files in `Source/` are dead experiments I never deleted. The table says which is which.
> 3. **Clone it and point an AI agent at it** (see below). That's the fastest way in.
>
> **The notes matter more than the code.** `docs/` has all 59 development notes I wrote while building this, plus CONTEXT, FILTERS and UNCERTAINTY on top.
> The code only tells you *what* I did; the notes tell you *why*, and what I ruled out. That second part is
> the expensive part — it's ~3 months of measurements.
>
> Anything unclear, ask me. — Enrico

---

## What the project actually was

Band Ritual is a **vocoder plugin** (JUCE, VST3/AU). The goal wasn't "make a vocoder" — it was to
**reverse-engineer Ableton Live's stock Vocoder** until the output matched measurably, then build a
plugin around that engine with its own character mode and GUI.

"Matched measurably" means a number: **cosine similarity** between my output and Ableton's, on the same
input and settings. It went from 0.45 → **0.927 average across 5 inputs**. That metric drove every
decision for three months. Full story in [`docs/CONTEXT.md`](docs/CONTEXT.md).

---

## How to use this with an AI — two ways

**Option A — clone it and use a coding agent** (Claude Code, Cursor, Copilot). Best option: the agent
can grep, follow references, and open files on demand.

**Option B — upload one file to any chatbot** (Gemini, ChatGPT, Claude, Mistral, DeepSeek…). No install,
no git. See [the bundles section](#option-b--one-file-you-can-upload-to-any-chatbot) below.

---

### Option A — clone it and use a coding agent

```bash
gh repo clone eqenrico-hub/band-ritual-prototype
cd band-ritual-prototype
claude          # or cursor, or whatever you use
```

Then ask it things like:

- *"Read docs/CONTEXT.md, then explain how the carrier filter works and why it's 4 cascaded stages."*
- *"What was tried for the envelope follower, and what won?"*
- *"List every dead end that was tested and rejected, so I don't repeat them."*
- *"How does depth above 100% behave, and where did that formula come from?"*
- *"Walk me through PluginProcessor.cpp — what are the main stages of the audio path?"*

Because the code and the reasoning sit in the same repo, the agent can answer **why**, not just **what**.
Code on its own can't do that.

⚠️ **One warning when using an agent here:** the notes were written over 3 months and contradict each
other, because my understanding kept changing. **Later notes beat earlier ones**, and anything measured
before `docs/dsp/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md` was measured on clipped audio
and is simply wrong. Tell your agent this, or it will quote outdated numbers with total confidence.

---

### Option B — one file you can upload to any chatbot

If you'd rather use Gemini, ChatGPT, or anything else with a file-upload box, the whole project has been
flattened into **single self-contained `.md` files** in [`bundles/`](bundles/). Download one, drag it into
the chat, done. Each one already has the "what this is / what to trust" warnings baked into its header,
so the model gets the caveats without you having to explain them.

| File | Size | ~Tokens | What's in it |
|---|---:|---:|---|
| [`1-BAND-RITUAL-notes-only.md`](bundles/1-BAND-RITUAL-notes-only.md) | 420 KB | ~107k | **← start with this one.** All 59 notes + CONTEXT + FILTERS + UNCERTAINTY + git history. No code. |
| [`2-BAND-RITUAL-notes-and-code.md`](bundles/2-BAND-RITUAL-notes-and-code.md) | 1.3 MB | ~329k | Bundle 1 + all live C++ source |
| [`3-BAND-RITUAL-everything.md`](bundles/3-BAND-RITUAL-everything.md) | 2.2 MB | ~568k | Bundle 2 + the ~934 KB of abandoned/dead source |

**Which one?** Bundle **1** for almost everything — the notes *are* the valuable part, and it fits in every
model's context window. Bundle **2** when you want it to read actual implementation. Bundle **3** only if
you specifically want to study the roads not taken; otherwise it just burns context on code that was
deleted for good reason.

**Rough fit by model** (check current limits, these move):

- **Gemini** (1M–2M context) — any of the three, including bundle 3
- **Claude** (200k–1M) — bundle 1 comfortably; bundle 2 on the larger-context models
- **ChatGPT** — bundle 1 comfortably; bundle 2 may need the file-upload/retrieval path rather than one paste

**To download a single file from GitHub:** open it, then use the download (⤓) button in the file toolbar.
Don't copy from the rendered page — you'll lose the formatting.

A good first prompt:

> This is the complete development record of a vocoder plugin called Band Ritual, which
> reverse-engineered Ableton Live's stock Vocoder. Read all of it, then summarise the final
> architecture and list everything that was tried and rejected.

**Want literally everything instead — images, renders, moodboards, the lot?** Use GitHub's own
**Code → Download ZIP** on the repo homepage. That gives you all 75 MB. Be warned it's mostly `.pptx`
moodboards and PNG assets that no chatbot can read — it's an archive, not something to upload to an AI.
For feeding a model, the `.md` bundles above are strictly better.

*The bundles are generated, not hand-written. If the notes or code ever change, run
`bash bundles/make-bundles.sh` from the repo root to rebuild all three.*

---

## Layout

```
Source/              the prototype C++ (JUCE). See trust table below.
docs/
  CONTEXT.md         START HERE — the cracked algorithm + what was learned
  FILTERS.md         every filter topology tried, and the measurement that killed each
  UNCERTAINTY.md     what is NOT certain, ranked — read before trusting a constant
  00-INDEX.md        map of all 59 notes by topic
  dsp/               reverse-engineering logs, Ableton measurements, iteration history
  gui/               layout spec, Blender/chassis sessions, moodboard assets
  process/           checklists, build/install gotchas, validation spec
  PROTOTYPE_GIT_HISTORY.md    commit log of the original working repo
reference-renders/   what the thing looked like
CMakeLists.txt
```

`build/` is intentionally excluded (it was 1.4 GB). The original repo's `.git` was 316 MB and was
**not** carried over — its history is preserved as readable text in `docs/PROTOTYPE_GIT_HISTORY.md`.

---

## Trust table — read this before believing any file

The prototype has several parallel attempts sitting side by side. An agent reading cold **cannot
tell them apart** and will happily quote abandoned code as if it were live. So:

| File | Lines | Status |
|---|---:|---|
| `PluginProcessor.cpp` / `.h` | 5864 / 708 | ✅ **CURRENT** — the shipped DSP. This is the real one. |
| `PluginEditor.cpp` / `.h` | 4300 / 7444 | ✅ **CURRENT** — the shipped GUI. |
| `CarrierGenerator.cpp` / `.h` | 296 / 93 | ✅ CURRENT — carrier modes (self/external/noise/saw/square/sine/pitch-track) |
| `PresetManager.h` | 302 | ✅ CURRENT |
| `FilmstripLookAndFeel.cpp` / `.h` | 286 / 119 | ✅ CURRENT — filmstrip knob rendering |
| `LicenseManager.cpp` / `.h` | 972 / 114 | ✅ CURRENT — Gumroad licensing. Irrelevant to the rebuild. |
| `STFTVocoder.cpp` / `.h` | 498 / 147 | ⚠️ **EXPERIMENT, NOT WIRED IN** — FFT vocoder attempt (2048/512, 75% overlap). Abandoned; the IIR path won. See `docs/dsp/Band Ritual - STFT Integration Attempt 2026-04-10.md`. |
| `WebViewEditor.cpp` / `.h` + `Source/webui/` | 259 / 59 | ⚠️ **DISABLED** — HTML/CSS GUI experiment. `PluginProcessor.cpp` has `#define USE_WEBVIEW_GUI 0`, so this code never runs. |
| `PluginProcessor_stft_backup.cpp` / `.h` | 4364 / 592 | ❌ **DEAD** — snapshot from the STFT era |
| `PluginEditor_stft_backup.cpp` / `.h` | 4169 / 7063 | ❌ **DEAD** |
| `PluginEditor_backup.cpp` / `.h` | 731 / 1757 | ❌ **DEAD** — older GUI |
| `PluginProcessor_v1_calibrated_backup.cpp` | 4641 | ❌ **DEAD** — but historically interesting: the first calibrated match |

**Known weaknesses, stated plainly** (these are the reasons for the rebuild):

- `PluginProcessor.cpp` at 5864 lines and `PluginEditor.h` at 7444 lines are monoliths. DSP, parameter
  handling, and state are not separated.
- ~21k of the ~45k lines are dead backup files.
- No test suite. Validation was done externally with Python scripts against Ableton exports
  (see `docs/process/Band Ritual - Validation Suite Spec.md`).
- GUI layout is hardcoded, not driven by a layout system.

---

## Note on credentials

`LicenseManager.cpp` contains a Gumroad **product ID** and a Cloudflare Worker URL for trial checks.
Neither is a private secret — both ship inside the compiled binary anyway. There are no API keys,
tokens, or private keys in this repo. It is private regardless.
