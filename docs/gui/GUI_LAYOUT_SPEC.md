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
