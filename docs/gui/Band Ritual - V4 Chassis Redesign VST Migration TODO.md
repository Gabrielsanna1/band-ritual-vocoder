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
