---
name: Q-from-range + close LO/HI (Ableton constant-fractional-Q)
description: >-
  Linked-mode Q now scales with band-count AND range octaves (Q ~ N/(n_oct*BW));
  LO/HI param ranges widened so they can be brought very close (down to ~0.03
  oct) for super-resonant clusters. Validated via pedalboard: Q doubles per
  octave-halving, stable, Iter-22 preserved.
type: project
date: '2026-06-12T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - Q
  - range
  - implemented
---
# Band Ritual — Q-from-range + close LO/HI (2026-06-12)

Implements the **never-done action item** from `Band Ritual - Ableton Filter Measurements 2026-04-09`:
Ableton uses **constant fractional Q** `Q ≈ N / (n_octaves × BW)`, `n_octaves = log2(highFreq/lowFreq)`.
BR's `qTable` was keyed on the Band Width knob ONLY (no N, no range term), so narrowing LO↔HI did
**not** sharpen the bands — the thing ricosan noticed vs Ableton. Active engine is the **IIR filterbank**
(STFT off, `useSTFTLinked=false`).

## Change 1 — Q scales with band-count + range (Linked mode only)
`PluginProcessor.cpp` in `updateBandFilters()`, right after `QcarrierBase = jmax(0.5, QcarrierBase)` (~L1672):
```cpp
if (algorithmMode == 1) {               // Linked only; Ritual/Split untouched
    constexpr float kRefBands=40.0f, kRefOctaves=9.81f;   // log2(18000/20)
    float loF=lowFreq, hiF=jmax(highFreq, loF*1.1f);
    float nOct=jmax(0.1f, log2(jmax(1.0001f, hiF/jmax(20.0f,loF))));
    float qScale=(nBands/kRefBands)*(kRefOctaves/nOct);
    QcarrierBase=jlimit(0.5f,200.0f, QcarrierBase*qScale);
}
```
- **Identity at the Iter-22 reference** (N=40, full range 20–18k → qScale=1.0 exactly) → 91.4% cosine match preserved, no regression.
- Multiplies the existing qTable (keeps the fine BW-curve tuning) rather than replacing it.
- Per-band `Qcarrier` and `Qanalysis=Qcarrier*qaRatio` inherit the scaling downstream.
- `globalBandQ` (GUI/carousel) set AFTER scaling so the readout reflects the scaled Q.

## Change 2 — let LO and HIGH be brought very close
Before: `lowFreq 20–500`, `highFreq 2000–18000` → min gap **2 octaves**; band-graph handles also forced 1 octave.
- **Param ranges** (`PluginProcessor.cpp:405-411`): both → `NormalisableRange(20, 18000, 1.0, 0.25)`, defaults unchanged (20 / 18000). Presets back-compat (APVTS stores real Hz).
- **Range guard** (min gap `kMinRangeRatio = 1.1f` ≈ 0.14 oct) applied at the 3 band-layout sites: `rebuildBands()` (after raw values stored for change-detection), the Q-from-range block, and the `processBlock` preview block. `lastLowFreq/lastHighFreq` and GUI `lowHz/highHz` keep RAW values (no spurious rebuilds; handles sit where user put them).
- **Band-graph drag handles** (`PluginEditor.h:896,904`): `highHz*0.5`→`highHz/1.1f`, `lowHz*2.0`→`lowHz*1.1f` (handles approach to 1.1×, no crossing).
- **Hidden sliders** (`PluginEditor.cpp:2467,2471`): `setRange(20,18000,1)` for coherence.

## Verification (pedalboard 0.9.22, headless VST3, white noise, carrier=Self, BW0.3)

**Q tracks the range (the original bug), Linked vs Ritual — confirms scoping & no regression:**
| | Q full (20–18k) | Q narrow (500–2000) | ratio |
|---|---|---|---|
| Ritual | 7.24 | 7.93 | **1.09×** (flat — fix NOT applied, Ritual untouched) |
| Linked | 4.64 | 14.52 | **3.13×** (sharpens) |

**Single isolated band ~1 kHz, 5 bands, closing the range (1/96-oct measurement):**
| range | octaves | Qeff |
|---|---|---|
| 125–8000 | 6.00 | 7.4 |
| 354–2828 | 3.00 | 15.9 |
| 595–1682 | 1.50 | 28.3 |
| 794–1260 | 0.67 | 47.5 |
| 891–1122 | 0.33 | 62.7 |
→ **Q doubles per octave-halving** = exactly Q ∝ 1/n_octaves (Ableton's law).

**New sub-2-octave ranges (impossible before) tested down to 990–1010 Hz (0.03 oct): all outputs FINITE — stable** (Q clamp 200 + 1.1× guard, no NaN/inf).

## Notes / open
- At extreme-narrow range Q slams the 200 ceiling → very resonant "bell"/near-sine. Intended; soften by lowering the clamp or `pow(qScale, <1)` if ricosan finds it too aggressive (1 line). **Pending his ear.**
- Measurement caveat: summed filterbank Q understates per-band Q when many bands overlap/merge; isolate a band (few bands + fine smoothing) for clean numbers.
- NOT applied to Ritual mode by design; 1 line to extend if wanted.
- Build: `~/Desktop/BandRitualV3/build` (internal disk, normal cmake). Install `/Library` via `~/Desktop/INSTALL_BANDRITUAL_QRANGE.command`.

## Related
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09` (the source measurement + formula)
- `Dev Notes/Band Ritual - Iter 22 Final Architecture 2026-04-12` (the 91.4% reference being preserved)
- `Dev Notes/Band Ritual - Vocoder Cracking NEXT OBJECTIVE` (the "Ableton rings, BR doesn't" transient gap — same Q root)
