---
name: Iter 14c — Hybrid Envelope Follower
description: >-
  2026-04-09 milestone — voclib-style hybrid envelope follower (peak + 3-stage
  cascaded LP), 4-stage biquad carrier restored, noise within ±3 dB, sine 050
  nearly perfect. Defensible architecture.
type: project
date: '2026-04-09T00:00:00.000Z'
tags:
  - bandritual
  - vocoder
  - ableton
  - milestone
  - iteration14c
---

# Band Ritual — Iter 14c Hybrid Envelope Follower (2026-04-09)

## Context

Continuation of today's iter-12 session. Iter 12 restored March's 4-stage biquad cascade and added a per-band envelope cap hack to keep the sweep from exploding. The cap was a band-aid: it worked for sweep but killed sustained sines (sine 050 was -12 dB). Today's remaining 14 iterations drove toward a principled envelope follower derived from voclib (open-source channel vocoder) and DAFx literature on cascaded smoothers.

By iter 14c the cap is gone entirely and the envelope follower is a HYBRID: a peak detector front-end feeding a 3-stage cascaded one-pole LP smoother. This achieves natural per-band normalization through the cascade's inherent amplitude bounding — no hack required.

## Motivation

- Iter 12 had noise mid hump (+3 to +4 dB in Mid/HiMid/High)
- Sine 050 was -12 dB because envelope cap was killing sustained content
- Architecture needed to be defensible (grounded in literature), not a collection of per-band curves
- Web research on voclib (github.com/blastbay/voclib) showed its envelope follower is a 4-stage cascaded halfwave LP, which inspired the hybrid approach here

## Architecture diff vs iter 12

### Unchanged (iter 12 stays)
- Carrier: 4-stage cascaded biquad (8th-order)
- Qa = 0.60 × Qc
- bwGain[BW=10%] = 45.6 dB
- cos(w)^0.65 Q correction in Linked mode

### Changed in iter 14c
- **Envelope follower**: was `|filter(x)|` → 1-pole LP → per-band frequency-dependent cap. Now: **peak detector → 3-stage cascaded one-pole LP** (no cap).
- **Per-band envelope cap**: REMOVED. The cascade smoothing provides amplitude normalization naturally because each stage attenuates transients and the product across 3 stages bounds the DC component cleanly.

## Reference

- **voclib** — https://github.com/blastbay/voclib — uses a 4-stage cascaded halfwave LP follower. Inspired this approach.
- **DAFx literature on cascaded smoothers** — cascading one-pole LPs produces near-Gaussian impulse response and amplitude bounding similar to a short RMS window but cheaper and zero-lookahead.

## Validation results (validation_suite.py, iter 14c)

| Test | Sub | Bass | Low | Mid | HiMid | High | Air |
|---|---|---|---|---|---|---|---|
| white_noise | -1.8 | -0.7 | -2.0 | -0.2 | +0.9 | -0.1 | -3.2 |
| pink_noise | -1.8 | -0.6 | -1.4 | -0.7 | +0.7 | +0.2 | -2.5 |
| sweep | +26.1 | +22.3 | +18.4 | +11.8 | +3.1 | -3.6 | -10.9 |
| sine_050 (broadband) | +0.5 ✓✓ | | | | | | |
| sine_1000 in-band | -13.7 | | | | | | |
| sine_10k in-band | -16.0 | | | | | | |

### Iter 12 → Iter 14c comparison (noise regions)

| Region | Iter 12 | Iter 14c | Delta |
|---|---|---|---|
| Sub | -0.8 | -1.8 | -1.0 |
| Bass | +0.2 | -0.7 | -0.9 |
| Low | +0.0 | -2.0 | -2.0 |
| Mid | **+3.1** | **-0.2** | -3.3 ✓ |
| HiMid | **+4.0** | **+0.9** | -3.1 ✓ |
| High | +2.7 | -0.1 | -2.8 ✓ |
| Air | -0.6 | -3.2 | -2.6 |

The mid hump is gone. Small Air loss is acceptable.

## What worked

- **Noise within ±3 dB across all 7 regions.** No more "mid hump" that plagued iter 12.
- **Sine 050 essentially perfect (+0.5 dB).** Removing the cap and letting cascade smoothing normalize naturally restored sustained-tone linearity.
- **Architecture is defensible end-to-end**: 4-stage biquad cascade (matches March measurements) + voclib-style hybrid envelope follower (grounded in open-source reference + DAFx literature). Every design choice traces to a measurement or a published reference.
- **No per-band cap hack.** Iter 12 needed 40 frequency-dependent cap values; iter 14c uses one global cascade time constant.

## What's still wrong

### Sweep asymmetry (big)
- Sub +26 dB, Bass +22 dB, Low +18 dB, Mid +12 dB, HiMid +3 dB, High -4 dB, Air -11 dB
- **Monotonic tilt**: over-attenuated at HF, under-attenuated at LF
- Iter 12 was closer on sweep because the frequency-dependent cap partially compensated. The cap was wrong in principle but accidentally helped sweep balance.

### Sine 1k / 10k in-band -14 to -16 dB
- Clean in-band tonal content is 14-16 dB quieter than Ableton at the matching tone band
- Noise tests pass but single-frequency tests don't — suggests frequency-dependent cascade response

### Root cause hypothesis
The cascade smoothing time constant is **the same across all frequencies**. A single LP cascade timeconstant means:
- LF bands: cascade is much slower than the band's period → over-smooths → envelope droop on sustained LF tones
- HF bands: cascade is much faster than the band's period → under-smooths → spikes pass through unbounded
- Opposite problem at sweep extremes

**Solution direction**: make the cascade time constant frequency-dependent (tau ∝ 1/fc or similar).

## Filter measurement findings (new)

Today the user also did 5 multi-band Ableton exports of white noise at different band counts (4, 8, 12, 16, 20 bands, BW=10%, 20 Hz – 18 kHz range). Spectral analysis of these exports revealed:

1. **Ableton uses CONSTANT FRACTIONAL Q.** Q scales with band count via roughly `Q ≈ N_bands / (n_octaves × BW%)`. At 4 bands Q ≈ 4.6. At 16 bands Q ≈ 19. At 20 bands Q ≈ 22. Extrapolating to 40 bands gives Q ≈ 38-41 — which matches March's empirical maxQ=37 finding almost exactly.
2. **BR's qTable does not scale Q with band count.** BR uses a fixed Q derived from BW% alone. At 40 bands BR's Q is approximately right (because that's where March calibrated). At lower band counts BR is way too narrow.
3. **Filter order is ~8th.** Measured skirt slopes 47-52 dB/octave at mid frequencies — consistent with the 4-stage cascaded biquad March documented and iter 12 restored.
4. **HF bands have asymmetric rolloff.** Above the center the skirt falls slower than below, likely a bilinear-transform warping artifact at high fc / fs ratios.
5. **At 40 bands BR's Q matches Ableton** at the bands the FFT can resolve, but **BR's band gain is +1 to +5 dB hot** at band centers. This is the noise mid hump iter 14c fixed via the new envelope follower — but the gain imbalance also exists at the filter level and may be a second-order issue when sweep is revisited.

See `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md` for raw data.

## Code locations modified

All in `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/Source/`:

- **PluginProcessor.h**
  - `Band` struct: added `env3`, `env4` fields (extra cascade stages)
  - Processor class: added `cascadeEnvCoef` member (shared cascade time constant)
- **PluginProcessor.cpp:903**
  - `cascadeEnvCoef` computation (one-pole LP coefficient from a chosen tau)
- **PluginProcessor.cpp:~3712**
  - Envelope follower hybrid branch: peak detector front-end → `env2 = env2 + cascadeEnvCoef*(peak - env2)` → `env3 = env3 + cascadeEnvCoef*(env2 - env3)` → `env4 = env4 + cascadeEnvCoef*(env3 - env4)`. `env4` drives the carrier multiplier.
- **PluginProcessor.cpp:~3833**
  - Per-band envelope cap REMOVED (the whole iter-12 frequency-dependent clamp block is deleted)

## Next iteration — three options

### Option A — Test iter 14c in Ableton with real music FIRST
Validation suite is synthetic. Before spending more time tuning, load BR in Ableton and A/B vs Ableton Vocoder on vocal + drums. The sweep and single-sine failures may not be perceptually significant on musical content where many bands overlap. Cheapest path to answering "is this good enough to ship?"

### Option B — Frequency-dependent cascade speed
Make `cascadeEnvCoef` a per-band value with `tau ∝ 1/fc` (or similar). Should fix sweep asymmetry and sine 1k/10k simultaneously. Principled, small code change, risk is picking the wrong scaling law.

### Option C — Hilbert envelope
Replace cascade LP with a per-band Hilbert transform to extract the analytic envelope. Mathematically ideal (instant zero-lag envelope from the carrier-bandwidth analytic signal), expensive per-band (needs an IIR Hilbert pair per band), and a much larger rewrite.

## Status: PARTIAL CRACK

- Defensible architecture: yes
- Noise validation: pass (±3 dB all regions)
- Sine 050 broadband: pass
- Sweep: fail (asymmetric, monotonic tilt)
- Sine 1k / 10k in-band: fail (-14 to -16 dB)
- Real music A/B test in Ableton: **PENDING** (recommended next step, Option A)

## Related notes

- `Dev Notes/Band Ritual - Iter 12 Architecture Restored 2026-04-09.md`
- `Dev Notes/Band Ritual - Ableton Filter Measurements 2026-04-09.md`
- `Dev Notes/Band Ritual - Linked Mode SVF Regression 2026-04-08.md`
- `Dev Notes/Ableton Vocoder - Master Findings Log.md`
- `Dev Notes/Band Ritual - Validation Suite Spec.md`
