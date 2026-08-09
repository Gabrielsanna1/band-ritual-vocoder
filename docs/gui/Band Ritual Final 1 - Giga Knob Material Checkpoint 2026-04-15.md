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

