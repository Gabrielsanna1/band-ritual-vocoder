# Band Ritual — First Prototype (reference only)

This repo is a **frozen snapshot of the first Band Ritual prototype**, built solo over ~3 months
(March–June 2026). It exists so it can be **read and interrogated**, not built on.

The actual rebuild happens in the *other* repo, from scratch, step by step. Nothing here is
meant to be copied forward as-is. The prototype was written fast, to answer one question:
**how does Ableton Live's Vocoder actually work?** It answered it — and accumulated a lot of
mess along the way.

**The most valuable thing in this repo is `docs/`, not `Source/`.** The code is the residue of
the experiments; the notes are the experiments. Read `docs/CONTEXT.md` first.

---

## How to use this with an AI agent

Clone it, open a terminal in the folder, and run your agent (Claude Code, Cursor, whatever) inside it.
Then ask things like:

- *"Why is the carrier filter 4 cascaded stages instead of one?"*
- *"What was tried for the envelope follower, and what won?"*
- *"Show me every dead end that was tested and rejected."*
- *"How does depth >100% behave, and where did that formula come from?"*

Because the code and the reasoning live in the same repo, the agent can answer **why**, not just **what**.
Code alone can't do that.

---

## Layout

```
Source/              the prototype C++ (JUCE). See trust table below.
docs/
  CONTEXT.md         START HERE — the cracked algorithm + what was learned
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
