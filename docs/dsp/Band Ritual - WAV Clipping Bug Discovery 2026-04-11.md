---
name: WAV Clipping Bug — All Cosine Measurements Were Invalid
description: 2026-04-11 discovery — the plugin outputs +18-22 dB RMS / +28-32 dBFS peak on real-music input. 16-bit PCM WAV writes clip at ±1.0, hiding the true level. All cosine measurements in realmusic_test.py from March V3 onward were on clipped signal and are invalid.
type: project
date: 2026-04-11
tags: [bandritual, vocoder, bug, measurement, cosine, clipping]
---

# Band Ritual — WAV Clipping Bug (2026-04-11)

## The bug

`realmusic_test.py` used `sf.write(BR_OUT, out, sr)` without specifying subtype. **soundfile's default is 16-bit PCM**, which hard-clips samples to ±1.0 on write.

The plugin output on `DRY-EXPORT 1.wav` (kick/bass loop) is:

| Setting | BR RMS | BR peak | Clipped samples |
|---|---|---|---|
| bwGain=45.6 (March) | **+22.5 dB** | **+31.8 dBFS** | 82.8% |
| bwGain=100 | ~+27 dB | ~+36 dBFS | ~90% |
| OLD /Library binary | **+18.5 dB** | **+28.2 dBFS** | 78.9% |

**Ableton reference: RMS -5.95 dB, peak +4.32 dBFS.**

BR is **~28 dB** louder than Ableton RMS-wise, **~27 dB** louder peak-wise. The 16-bit WAV silently clamped everything above ±1.0, so readback showed a wildly distorted signal with ~half the samples stuck at ±1.0.

## Why all cosine measurements are invalid

Clipping is nonlinear. It flattens transients and adds harmonic distortion. The spectrogram of a heavily clipped signal looks nothing like the same signal unclipped.

Every cosine similarity we've been measuring was BR_clipped vs ABL_unclipped. We were essentially measuring "how similar a square wave is to a kick", not "how similar BR is to ABL".

All the "iter 20 cosine progression" from our session is garbage:

| Iter | Cosine | Status |
|---|---|---|
| 19l baseline | 0.7066 | CLIPPED |
| 20c BPF2 no-corr | 0.7241 | CLIPPED |
| 20d +HF shelf | 0.7227 | CLIPPED |
| 20f bwGain=40.8 | 0.7277 | CLIPPED |
| 20g bwGain=44 | 0.7312 | CLIPPED |
| 20h bwGain=50 | 0.7362 | CLIPPED |
| 20i bwGain=100 | 0.7494 | HEAVILY CLIPPED |

The monotonic "improvement" as bwGain went up was pure clipping artifact — more clipping flattens the spectrum, which coincidentally increases the dot product with ABL's flatter-than-expected spectrum.

## The fix

### Fix the test immediately
```python
# OLD (16-bit PCM, clips at ±1.0)
sf.write(BR_OUT, out, sr)

# NEW (32-bit float, no clipping)
sf.write(BR_OUT, out, sr, subtype='FLOAT')
```

### Add clipping warning
```python
n_clip = int(np.sum(np.abs(br) >= 0.9999))
if n_clip > 0:
    print(f"CLIPPING: {n_clip} samples ({100*n_clip/len(br):.1f}%) — cosine INVALID")
```

### Fix the plugin calibration
The plugin broadband gain is ~28 dB too high on kick/bass content. Need to recalibrate `bwGain` on REAL MUSIC (not just noise) to produce output within 0 dBFS headroom.

At bwGain=17, BR broadband RMS matches ABL (~-6 dB), but per-region profile becomes:
- Sub: matched (+0.5)
- Bass/Low/Mid/HiMd/High/Air: all -10 to -17 dB below ABL

This means the plugin's TRUE frequency response (at matched broadband) is massively sub-weighted on kick content. The March calibration (tiltCorr=1.0, bwGain=45.6) was done on noise which has a flat-enough spectrum that the problem was masked — noise RMS matched because the sub dominance didn't show up.

## Why this was never caught

1. **validation_suite.py uses noise/sine/sweep**, not kick/bass. Noise has flat spectrum → per-region matches were easy to hit without realizing sub-dominance.
2. **Earlier manual tests** used short sine tones or WAV comparisons — tone levels were below clipping threshold.
3. **realmusic_test.py is recent** and was the first test to use heavy-transient real-music input. It immediately hit the clipping limit, but the 16-bit WAV hid the issue.
4. **The "cosine 0.7138" baseline from the /Library binary** (before this session) was ALSO clipped. The March "cracked" state has never been validated on real-music unclipped output.

## Implications

- **All per-region diffs** from realmusic_test.py are also corrupted by clipping.
- **All validation_suite.py noise results** are still valid (noise is flat and levels were chosen to not clip).
- **The Band Ritual plugin, when loaded in Ableton with gain=0 dB input, will silently clip on transient content.** Users would hear distortion on kicks, snares, and any percussive material.
- **March calibration "working"** was measured on content that didn't trigger clipping.

## Next steps

1. ✅ Fix realmusic_test.py (32-bit float, clipping warning) — DONE
2. Measure TRUE output level on the DRY test input at various bwGain values
3. Find bwGain such that peak stays below 0 dBFS on real music → ~14-17 dB for this input
4. Re-measure cosine on UNCLIPPED output
5. Decide: do we want BR to match ABL loudness exactly on every input type, or do we want BR to have a fixed "unity gain" calibration that matches on average?
6. Warn users via GUI if output peak hits 0 dBFS? Or add internal soft clip?
7. Re-run validation_suite.py with float WAVs to verify synthetic tests weren't also hiding clipping

## Related notes
- `Dev Notes/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md` (invalidated)
- `Dev Notes/Ableton Vocoder - Master Findings Log.md` (cosine 0.714 "cracked" state was also clipped)
- `Dev Notes/Band Ritual - Validation Suite Spec.md`
