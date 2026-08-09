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
