# Prototype git history

Full commit log of the original `BandRitualV3` working repo, preserved as text.
The 316 MB `.git` of that repo was **not** carried over — this is the decision trail.

```
2026-04-13  05333ea  Fix depth >100%: remove attenuation, match ABL's linear scaling
ABL at depth=200% outputs at -0.48 dB RMS (loud, no limiting).
BR was at -13.92 dB because pow(0.5, (depth-1)*1.35) attenuated.
New formula: envGain = floor*(1-d) + envComp*d — pure linear scaling.
Now BR at -5.70 dB (still 5 dB off but 8 dB closer).

Depth=100% cosine unchanged at 0.9161.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-13  e4d70d5  Separate Q tables: Linked (Q=40, ABL match) vs Ritual (Q=6, original)
Linked mode broke Ritual/Split because Q=40 was too narrow for those modes.
Now each mode uses its own Q table:
- Linked: Q=40 at BW=10% (ABL-matched, 92.7% cosine)
- Ritual/Split: Q=6 at BW=10% (original wider character)

Also: envelope compression e^0.75 only in Linked mode.
Ritual/Split use linear envelope (original BR character).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f979b61  FINAL: 92.7% avg cosine across 5 inputs — architecture locked
5-input validation with fresh ABL exports (confirmed settings):
  KICK:  0.9160    PAD:   0.9857
  BASS:  0.9559    MIX:   0.8550
  VOCAL: 0.9221    AVG:   0.9269

Architecture:
  Q=40, Qa=0.52×Qc, 4-stage carrier, 3-stage analysis
  Peak detector pow(0.01), e^0.75 compression, phase alt ON

Session journey: 0.4493 → 0.9269 (+107% improvement)
Root cause was Q=6 regression. Rest was calibration.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f9bb5e4  92.7% avg cosine: pow(0.01) formula restored, VOCAL +5.1%
Reverting exp(-1) back to pow(0.01) for attack/release coefficients.
The exp(-1) change was a mistake — it only helped kick by +0.0003 but
KILLED vocal matching (0.87 → 0.92 with pow).

pow(0.01) means "reach -40 dB in release_ms" — matches ABL's documented
behavior. exp(-1) means "reach -8.7 dB" — too slow.

5-input results:
  KICK:  0.9160  PAD:   0.9857
  BASS:  0.9559  MIX:   0.8550
  VOCAL: 0.9221  AVG:   0.9269

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  96e2d38  Depth formula + gate analysis from ABL exports
Depth: floor 0.005→0.065, >100% exponent 1.5→1.35.
Measured from AB_DEPTH_{0,25,100,150,200} exports:
  ABL depth=0%: -6.9 dB vs 100% (was -34.8 dB in BR)
  ABL depth=200%: -2.3 dB vs 100% (was -6.1 dB in BR)

Gate: ABL gate=-inf same as gate=-40. Gate starts
affecting at -20 dB. Gate=0 is full mute.

Core vocoder cosine unchanged at 0.9138 (depth=100%).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  6b1a131  92.1% sim / 91.4% plugin: envelope compression e^0.75
ABL applies soft envelope compression — e^0.75 instead of linear e^1.0.
Reduces dynamic range of per-band envelope, giving more balanced output.
Standard vocoder technique.

Simulation (float64): 0.9159 → 0.9209 (+0.005)
Plugin (float32):     0.9131 → 0.9138 (+0.0007)

Remaining gap: ~8% is from float32 precision loss (~0.3%), input-specific
harmonic patterns (~5%), and irreducible implementation differences (~3%).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  f38b468  91.3% cosine: exp coefficient formula (+0.0003)
Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  c32b628  91.1% cosine: Qa=0.55, Q=40, simple peak detector
Final optimized parameters:
- Carrier Q: 40 (from formula Q = N/(n_octaves × BW), was 37 empirical)
- Analysis Qa: 0.55 × Qc = 22.0 (swept 0.3-1.0, peak at 0.55)
- Envelope: simple peak detector (attack 1ms, release 50ms)
- Carrier: 4-stage BPF2 cascade (3 and 5 stages tested, 4 is optimal)
- Analysis: 3-stage BPF2 (2 stages tested, 3 is better)
- Phase alternation: ON (marginal +0.0005 benefit)
- bwGain BW=10%: 35.0 dB

Sweep results at Q=40:
  Qa/Qc=0.55 → 0.9105 (BEST)
  Qa/Qc=0.65 → 0.9092
  Qa/Qc=0.70 → 0.9065
  Qa/Qc=0.75 → 0.8999

Journey this session: 0.4493 → 0.9105 (+0.46 cosine)
  Q=6→40: +0.33
  Cascade LP→peak detector: +0.13
  Qa 0.60→0.55: +0.004

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  0a17cfd  Q=40: cosine 0.9065 (from 0.9036 at Q=37)
Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  7be36fd  ITER 22 FINAL: 90.4% cosine match to Ableton vocoder
Two fixes that took us from 0.45 → 0.90:
1. Q=6→37 (regression fix): +0.33 cosine
2. Cascade LP→simple peak detector: +0.13 cosine
3. Qa ratio 0.60→0.70: +0.003 cosine

Architecture (matches what Ableton probably does):
- Carrier: 4-stage RBJ BPF2 cascade at Qc=37
- Analysis: 3-stage BPF2 at Qa=0.70×Qc=25.9
- Envelope: simple peak detector (attack=1ms, release=50ms)
- No cascade LP, no ring buffer, no per-band normalization
- bwGain BW=10%: 35.0 dB
- Per-band tilt: none (inherent in constant-Q design)

Validated on kick/bass test with 32-bit float WAV (no clipping artifacts).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  7ea1f9a  FIX: Restore Q=37 from Q=6 regression — cosine 0.49 → 0.77
ROOT CAUSE FOUND: qTable had Q=6 at BW=10% instead of the correct Q=37.
This was a regression where "effective vocoder output bandwidth" was confused
with "individual filter Q". Q=37 was confirmed by:
- March 2026 Q breakthrough (peak width measurement)
- Apr 9 multi-band filter measurements (constant fractional Q)
- Obsidian master findings log

Impact of Q=6 regression:
- Filters 6x too wide → captured energy from neighboring bands
- Output 20-28 dB too loud on real music
- Every cosine measurement since the regression was corrupted
- All iter 20-21 architectural experiments (cascade LP, ring buffer,
  tilt, per-band normalization) were chasing symptoms of wrong Q

With Q=37 restored:
- TRUE cosine (unclipped float): 0.7747 (from 0.49 with Q=6)
- Simple peak detector envelope (no cascade needed)
- bwGain=35 gives near-0dBFS peak on kick content
- Per-region: Bass -1.4 dB (nearly perfect), Sub +8.1 (still hot)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  31c41c0  iter 22: back to basics — cascade LP 650ms + correct bwGain
Reverted all iter 21 experimental code (ring buffer, extra cascade
stages, phase alt removal, f^1.0 tilt). These were dead ends or
had implementation bugs.

Current VALID state:
- Carrier: 4-stage RBJ BPF2 cascade (Q=37)
- Envelope: peak detector + 3-stage cascade LP (tau=650ms per stage)
- bwGain BW=10%: 17 dB (near-unity on kick content)
- No per-band tilt (f^1.0 is inherent in constant-Q filter design)
- TRUE cosine (unclipped): 0.6797 on kick/bass test

Key learnings from iter 21-22:
- Simple peak detector (no cascade): cosine = 0.4493 (worse)
- f^1.0 tilt applied additively: cosine = 0.4405 (worse — it's already
  inherent in the filter, applying it again over-tilts)
- Cascade LP at 650ms: 0.6797 (best validated result)
- Python simulation with boxcar shows 0.89 is theoretically achievable
  but C++ implementation had a gap (0.54 vs 0.89) — root cause unknown

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  a03ecd2  iter 21: boxcar envelope discovery + debug state
KEY FINDINGS:
- Python simulation proves boxcar moving-average envelope achieves 0.89+ cosine
  vs ABL on kick test (causal: 0.8852, centered: 0.9153)
- Cascade LP peak was 0.6797 (3-stage at 650ms per stage)
- Boxcar at 200ms window dramatically outperforms cascade LP
- C++ ring buffer implementation gives only 0.54 cosine — implementation
  doesn't match simulation. Investigation ongoing.
- Phase alternation disabled (not the cause — 0.5337 vs 0.5399)
- Added env5/env6 fields to Band struct (not useful with ring buffer)
- JUCE makeBandPass confirmed identical to RBJ BPF2 formula
- Plugin output is 21.5 dB louder than simulation at same settings — unknown cause

BLOCKED: need to find why C++ ring buffer output differs from Python simulation.
The math should be identical (carrier × boxcar(|analysis|)) but plugin is 21.5 dB
hotter and 0.35 lower cosine.

Next: add per-band debug WAV dump to trace where the signal path diverges.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  d035ec7  iter 21 experiments: confirmed tilt + release changes don't fix cosine
Phase 1 (per-band tilt sweep): tested +2.5 and +5 dB/oct tilts against
kick/bass test. Cosine: 0.4878 → 0.4870 → 0.4844. No improvement.
Tilt shifts per-region energy but cosine is dominated by Sub where
the problem actually lives.

Phase 2 start (release table): changed uniform 15ms release from
frequency-dependent {15,30,76,10.5,4} ms. Reduced mid-band energy
by 80x (8.8% → 0.1%) but cosine barely moved (0.4875).

Key findings:
- BR vs DRY cosine: 0.79 (BR is near-passthrough of input)
- ABL vs DRY cosine: 0.52 (ABL significantly transforms input)
- BR vs ABL cosine: 0.49 (dominated by Sub region disagreement)
- Per-bin cosine: Sub (0.49) and Bass (0.26) are WORST match, not mid/high

Hypothesis: ABL's vocoder does something structurally different
from BR's "Self" mode autoencoder — possibly internal carrier even
in Modulator mode, or per-band envelope normalization.

Waiting for user confirmation of Ableton's carrier mode in the
reference recording before continuing.

See: Dev Notes/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-04-12  4433653  iter 20: BPF2 carrier restore, baseline before per-band normalization
Reverts Butterworth carrier (iter 19c) back to 4-stage cascaded RBJ BPF2
for the Linked mode carrier path. Sets bwGain BW=10% to 8.0 dB as a safe
test value (prevents clipping on kick/bass input). Removes HF/LF shelves
added in earlier iter 20 attempts — they hurt cosine similarity.

MAJOR DISCOVERY (2026-04-11): All previous real-music cosine measurements
were corrupted by 16-bit PCM clipping in the test harness. True unclipped
cosine on kick/bass input is 0.4878 (was reporting 0.71+ under clipping).
The plugin output at March calibration (bwGain=45.6) is +28 dBFS peak on
percussive content — silently clipping users' DAWs.

This commit saves the state before starting Phase 1 of the reverse
engineering plan: implementing f^1.0 per-band gain tilt and per-band
envelope normalization to match Ableton's vocoder architecture.

See: Dev Notes/Band Ritual - WAV Clipping Bug Discovery 2026-04-11.md
     Dev Notes/Band Ritual - Iter 20 BPF2 Return 2026-04-11.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  6ee0a21  FIX: SVF prepare() was resetting filter state every block
Critical bug: svf.prepare() was called in updateBandFilters (every block),
which resets all internal filter state. Low frequencies lost 20-36 dB
because the filter never accumulated enough state before being reset.

Fix: prepare() called ONCE at band creation. updateBandFilters only
calls setCutoffFrequency/setResonance (which don't reset state).

Before: 512-block output was -3 dB vs full-block (matched Ableton bug report)
After: 512-block output matches full-block within 0.2 dB

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  a5a3644  Recalibrate bwGain for cascaded envelope
bwGain table recalibrated after cascaded envelope change.
Levels back to 0.0 dB at all BWs on white noise.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-26  6ace341  Cascaded envelope with BW-dependent stage 2 release
Two-stage envelope follower matches Ableton's resonant tail:
- Stage 1: standard peak detector (fast initial drop)
- Stage 2: cascaded smoother with BW-dependent release coefficient
- Narrow BWs get 2.5x slower stage 2 (longer resonant tail)
- Wide BWs get 0.7x faster stage 2 (tighter response)

Verified: BW=10% tail 67ms→138ms (target 145ms), BW=15% 111ms (target 112ms)

Also: display reactivity 0.25→0.9, bar height boost 0.25→0.08,
release range extended to 30 seconds.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-25  2e05408  SVF carrier filters + 30s release + crackle-free formant
- Carrier filters: biquad → StateVariableTPT (SVF) for Linked mode
- SVF handles frequency changes natively — zero crackles on formant knob
- Q² gain compensation keeps spectral match at 1.4 dB (identical to biquad)
- Release range: 1s → 30s (matches Ableton's vocoder range)
- Formant smoothing: 50ms (fast + smooth with SVF)
- All cracked calibrations preserved (Q, tilt, bwGain, phase, envelope)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-25  3b72a8e  Formant: band remapping replaces filter frequency shifting
Formant knob now remaps envelope routing instead of shifting carrier
filter frequencies. Carrier filters stay at fixed positions — completely
eliminates crackles/clicks when turning the formant knob.

Implementation: two-pass processing loop. Pass 1 computes all band
envelopes. Pass 2 applies carriers using interpolated envelopes from
remapped source bands. Fractional shifts use linear interpolation
between adjacent band envelopes.

Based on industry-standard approach (confirmed from Ableton, EVOC 20,
Cherry Audio vocoder documentation and KVR DSP forum research).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>

2026-03-24  e56debe  Band Ritual Final 1 — Vocoder cracking complete
Linked mode matched to industry-standard vocoder at 1.8 dB avg RMS:
- 4th-order bandpass filter bank (2 cascaded biquads)
- Phase alternation on every other band
- Envelope: pow(0.01, 1/(sr*t)) — matched release curves
- Q table calibrated per BW with cos()^1.29 freq-dep correction
- Tilt + LF shelf correction
- bwGain calibrated per BW
- BW-dependent analysis Qa ratio (0.85→0.30)
- Gate: -inf to +12 dB range with -17 dB offset

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
```
