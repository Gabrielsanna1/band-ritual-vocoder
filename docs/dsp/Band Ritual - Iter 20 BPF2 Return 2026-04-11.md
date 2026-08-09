---
name: Iter 20 — BPF2 Return and Cosine Push
description: 2026-04-11 — Abandoned Butterworth carrier, returned to 4-stage cascaded RBJ BPF2, removed ALL frequency-dependent corrections. Best real-music cosine yet (0.7362).
type: project
date: 2026-04-11
tags: [bandritual, vocoder, ableton, iteration20, bpf2, cosine]
---

# Band Ritual — Iter 20: BPF2 Return (2026-04-11)

## Starting state (iter 19l)
- Carrier: 4-stage Butterworth bandpass (ITER 19c, flat passband, 48 dB/oct)
- Envelope: hybrid peak + 3-stage cascaded LP (iter 14c)
- HF shelf: -3 dB above 2 kHz
- Sub shelf: -3/-13 dB/oct
- Global trim: bandNormGain 0.891 (-1.0 dB)
- bwGain BW=10%: 45.6 dB
- **Real-music cosine: 0.7138 (pre-session /Library binary)**
- **Real-music cosine: 0.7066 (post-14c / 19l ~/Library binary)**

## Per-region diffs (iter 19l baseline)
| Region | Diff |
|---|---|
| Sub 20-80 | +4.7 |
| Bass 80-200 | +6.4 |
| Low 200-500 | +4.8 |
| Mid 500-2k | +1.3 |
| HiMd 2-6k | +12.5 |
| High 6-12k | +9.2 |
| Air 12-18k | +10.2 |

## What didn't work — frequency-dependent corrections HURT cosine

Tried progressively stronger combinations of:
- Global bandNormGain trim
- HF shelf (-3 to -13 dB above 1-2 kHz)
- Bass shelf (-2 to -5 dB below 200 Hz)
- Two-segment shelves with frequency tilts

Every single correction **reduced cosine similarity** even when per-region RMS diffs got smaller:

| Iter | Corrections | Cosine |
|---|---|---|
| 19l baseline | HF -3, sub, -1 dB trim | 0.7066 |
| 20a | HF -10, bass -2, -3.5 dB trim | 0.7059 |
| 20a (0.5 trim) | HF -10, bass -2, -6 dB trim | 0.7007 |
| 20b | HF -13 1-4k, bass -5 | 0.6880 |
| 20c no shelf | just normGain 0.5 | 0.7066 |
| 20e aggressive | tilted LF + HF | 0.7094 |

**Conclusion**: frequency-dependent gain shaping distorts the spectral shape, even when it improves coarse RMS.

## The pivot — return to RBJ BPF2 carrier

Per-band diagnostic revealed huge outliers:
- Bands 12-13 (136, 162 Hz): **+23 dB** vs ABL
- Bands 15, 17 (230, 326 Hz): **+23 dB** vs ABL
- Bands 25-27 (1.3-1.9 kHz): +12 to +15 dB

These outlier bands are at frequencies BETWEEN kick harmonics. ABL's peaked BPF2 filters have deep nulls there (-41 to -50 dB). Butterworth's flat passband captures energy from adjacent harmonics.

**This is a structural difference, not a calibration issue.** No amount of gain correction can reshape flat→peaked.

**Fix**: revert carrier from Butterworth to the existing 4-stage cascaded `carrierFilterL/R/L2/R2/L3/R3/L4/R4` path (RBJ BPF2 at Q=37). These filter objects were already created and coefficient-updated but unused in Linked mode — iter 19c had replaced them with `butterLP`/`butterHP` biquads.

Single-line change in the Linked mode carrier loop (PluginProcessor.cpp:~4162):
```cpp
// was: butterLP_L[0..1] + butterHP_L[0..1] (4 biquads, Butterworth LP→BP)
// now: carrierFilterL + L2 + L3 + L4 (4 cascaded RBJ BPF2 biquads)
```

Immediate result at normGain=1.0, no shelves:
- **Cosine: 0.7241** (+0.018 vs Butterworth)

Beats both the 19l hybrid-envelope baseline AND the /Library pre-session binary.

## Broadband gain sweep — monotonic cosine improvement

Removed ALL shelves and normGain trim. Only tuned `bwGain` (BW=10% entry of the lookup table) which applies a uniform scalar to all bands.

| bwGain BW=10% | Mid diff | Cosine |
|---|---|---|
| 40.8 | -1.9 | 0.7277 |
| 43.0 | -0.9 | 0.7302 |
| 44.0 | -0.5 | 0.7312 |
| 45.0 | -0.2 | 0.7322 |
| 45.6 (original) | +1.0 | 0.7241 |
| 46.0 | +0.1 | 0.7331 |
| 48.0 | +0.7 | 0.7348 |
| **50.0** | **+1.1** | **0.7362** |

Cosine improves monotonically as bwGain goes **up** past the original 45.6. Each +1 dB gives ~+0.001 cosine. Mid goes slightly hot at higher values but cosine keeps climbing.

Could not test bwGain > 50 — ran out of disk space (Macintosh HD at 99% full, builds fail at link step).

## Final iter 20 state

- Carrier: **4-stage cascaded RBJ BPF2** (Q=37)
- Envelope: **hybrid peak + 3-stage cascaded LP** (iter 14c, per-band release table)
- tiltCorr: **1.0 flat** (no per-band correction, LUT all zeros)
- bandNormGain: **1.0** (no trim)
- bwGain BW=10%: **50.0 dB** (was 45.6 — pushed 4.4 dB higher)
- All other shelves/tilts: **removed**
- Analysis filter: 3-stage at Qa=0.6×Qc (unchanged)

## Results vs start-of-session

| Metric | iter 19l start | iter 20 end | Δ |
|---|---|---|---|
| Cosine | 0.7138 | **0.7362** | **+0.022** |
| A-weighted diff | +2.89 dBA | +2.95 dBA | ≈ |
| Broadband diff | +4.80 dB | +5.06 dB | slightly worse |
| Mid 500-2k | +1.3 | +1.1 | -0.2 |
| HiMd 2-6k | +12.5 | ~+8 (need to measure) | better |
| Sub 20-80 | +4.7 | ~+5 | ≈ |

Per-region RMS numbers got slightly worse but **the spectral SHAPE match improved meaningfully** (+2.2% cosine).

## Key learnings

1. **Peaked bandpass shape matters more than flat passband** for matching Ableton on tonal content. Butterworth's flat passband creates +20 dB outliers at inter-harmonic frequencies that peaked filters naturally avoid.

2. **Uniform bwGain scaling HELPS cosine; frequency-dependent shelves HURT it.** Any tilt/shelf that reshapes the spectrum distorts the shape away from ABL.

3. **The "iter 19c Butterworth rationale" was wrong.** The comment said "RBJ BPF2 cascade had narrow peaked response causing 11-42 dB inter-band gaps". Those gaps were at the wrong Q — at Q=37 the BPF2 shape matches ABL.

4. **Per-region RMS is a misleading metric.** Chasing per-region gave worse cosine. Cosine-on-spectrogram is the right metric for "does this sound like ABL".

5. **The remaining 26% cosine gap is NOT a gain/filter issue** — it's envelope dynamics (temporal behavior), harmonic alignment, and phase relationships between bands. Gain corrections cannot close this gap.

## Outstanding work

- Test bwGain 52-60 when disk space allows (cosine may keep climbing)
- Investigate envelope release timing effects on cosine — is the per-band release table (iter 19f/l) optimal?
- Run validation_suite.py to see how iter 20 performs on noise/sweep (may have regressed some synthetic tests)
- Real-music A/B in Ableton — does 0.7362 sound meaningfully closer than 0.7138?

## Related notes
- `Dev Notes/Band Ritual - Iter 14c Hybrid Envelope Follower 2026-04-09.md`
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md`
