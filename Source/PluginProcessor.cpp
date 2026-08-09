#include "PluginProcessor.h"
#include "PluginEditor.h"
#if JUCE_WEB_BROWSER
#include "WebViewEditor.h"  // WebView-based GUI (Mac only)
#endif
#include <limits>
#include <map>

// Set to 1 to use WebView GUI, 0 for original C++ GUI
#define USE_WEBVIEW_GUI 0
#include <cmath>

// Band count is now a direct integer parameter (1-100)

//==============================================================================
// InputFilter Implementation
//==============================================================================

void InputFilter::prepare (double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    // Initialize all StateVariableTPT filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;

    for (auto& f : hpFilters)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        f.setResonance (1.0f / std::sqrt (2.0f));  // Butterworth Q
    }

    for (auto& f : lpFilters)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        f.setResonance (1.0f / std::sqrt (2.0f));  // Butterworth Q
    }

    update1PoleCoeffs();
    reset();
}

void InputFilter::reset()
{
    for (auto& f : hpFilters)
        f.reset();
    for (auto& f : lpFilters)
        f.reset();

    hp1PoleStateL = hp1PoleStateR = 0.0f;
    lp1PoleStateL = lp1PoleStateR = 0.0f;
}

void InputFilter::update1PoleCoeffs()
{
    // 1-pole HP coefficient: y = x - lpState, lpState = lpState + coeff * (x - lpState)
    float hpNorm = hpFreq / (float) sampleRate;
    hp1PoleCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hpNorm);

    // 1-pole LP coefficient
    float lpNorm = lpFreq / (float) sampleRate;
    lp1PoleCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * lpNorm);
}

void InputFilter::setHPFrequency (float freq)
{
    // Allow full audio range for complement mode support (inverse HP/LP)
    hpFreq = juce::jlimit (20.0f, 20000.0f, freq);

    for (auto& f : hpFilters)
        f.setCutoffFrequency (hpFreq);

    update1PoleCoeffs();
}

void InputFilter::setHPSlope (int dbPerOct)
{
    switch (dbPerOct)
    {
        case 6:  hpStages = 0; use1PoleHP = true;  break;
        case 12: hpStages = 1; use1PoleHP = false; break;
        case 24: hpStages = 2; use1PoleHP = false; break;
        case 48: hpStages = 4; use1PoleHP = false; break;
        case 72: hpStages = 6; use1PoleHP = false; break;
        case 96: hpStages = 8; use1PoleHP = false; break;
        default: hpStages = 1; use1PoleHP = false; break;
    }
}

void InputFilter::setLPFrequency (float freq)
{
    // Allow full audio range for complement mode support (inverse HP/LP)
    lpFreq = juce::jlimit (20.0f, 20000.0f, freq);

    for (auto& f : lpFilters)
        f.setCutoffFrequency (lpFreq);

    update1PoleCoeffs();
}

void InputFilter::setLPSlope (int dbPerOct)
{
    switch (dbPerOct)
    {
        case 6:  lpStages = 0; use1PoleLp = true;  break;
        case 12: lpStages = 1; use1PoleLp = false; break;
        case 24: lpStages = 2; use1PoleLp = false; break;
        case 48: lpStages = 4; use1PoleLp = false; break;
        case 72: lpStages = 6; use1PoleLp = false; break;
        case 96: lpStages = 8; use1PoleLp = false; break;
        default: lpStages = 1; use1PoleLp = false; break;
    }
}

void InputFilter::setHPResonance (float gainDb, float qWidth)
{
    // gainDb (-24 to +24dB) controls the HEIGHT of the peak/cut at cutoff
    // qWidth (0.5-10) controls the WIDTH of the peak/cut (higher Q = narrower)
    //
    // Positive gainDb = resonance boost (classic filter resonance)
    // Negative gainDb = cut at cutoff (we reduce Q below Butterworth)
    // Zero = Butterworth (flat passband, NO GAIN)
    const float butterworth = 1.0f / std::sqrt (2.0f);  // 0.707

    float safeDb = juce::jlimit (-24.0f, 24.0f, gainDb);
    float safeQ = juce::jlimit (0.5f, 10.0f, qWidth);

    // CRITICAL: Only allow Q > butterworth when user explicitly wants boost
    // Q > 1.0 causes gain at cutoff frequency!
    float q = butterworth;
    if (safeDb > 0.5f)  // Only boost when clearly positive (> 0.5dB)
    {
        // Map boost dB to Q: more boost = higher Q = narrower resonance peak
        // At +24dB boost, use the user's Q width setting
        float boostRatio = safeDb / 24.0f;
        q = butterworth + boostRatio * (safeQ - butterworth);
        q = juce::jlimit (butterworth, 10.0f, q);
    }
    else if (safeDb < -0.5f)
    {
        // Negative = softer knee, reduce Q below Butterworth
        q = butterworth * (1.0f - std::abs (safeDb) / 48.0f);
        q = juce::jlimit (0.1f, butterworth, q);
    }
    // else: keep q = butterworth (0.707) for flat response

    // IMPORTANT: Only apply resonance to FIRST stage!
    if (!hpFilters.empty())
        hpFilters[0].setResonance (q);

    // Remaining stages stay at Butterworth (flat) to maintain slope
    for (size_t i = 1; i < hpFilters.size(); ++i)
        hpFilters[i].setResonance (butterworth);
}

void InputFilter::setLPResonance (float gainDb, float qWidth)
{
    const float butterworth = 1.0f / std::sqrt (2.0f);  // 0.707

    float safeDb = juce::jlimit (-24.0f, 24.0f, gainDb);
    float safeQ = juce::jlimit (0.5f, 10.0f, qWidth);

    // CRITICAL: Only allow Q > butterworth when user explicitly wants boost
    float q = butterworth;
    if (safeDb > 0.5f)  // Only boost when clearly positive
    {
        float boostRatio = safeDb / 24.0f;
        q = butterworth + boostRatio * (safeQ - butterworth);
        q = juce::jlimit (butterworth, 10.0f, q);
    }
    else if (safeDb < -0.5f)
    {
        q = butterworth * (1.0f - std::abs (safeDb) / 48.0f);
        q = juce::jlimit (0.1f, butterworth, q);
    }

    if (!lpFilters.empty())
        lpFilters[0].setResonance (q);

    for (size_t i = 1; i < lpFilters.size(); ++i)
        lpFilters[i].setResonance (butterworth);
}

void InputFilter::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        float& hp1State = (ch == 0) ? hp1PoleStateL : hp1PoleStateR;
        float& lp1State = (ch == 0) ? lp1PoleStateL : lp1PoleStateR;

        for (int n = 0; n < numSamples; ++n)
        {
            float sample = data[n];

            // High-pass filtering
            if (hpFreq > 21.0f)  // Only filter if not at minimum
            {
                if (use1PoleHP)
                {
                    // 1-pole HP: output = input - lowpassed
                    hp1State = hp1State + (1.0f - hp1PoleCoeff) * (sample - hp1State);
                    sample = sample - hp1State;
                }
                else
                {
                    for (int s = 0; s < hpStages; ++s)
                        sample = hpFilters[(size_t) s].processSample (ch, sample);
                }
            }

            // Low-pass filtering
            if (lpFreq < 19999.0f)  // Only filter if not at maximum
            {
                if (use1PoleLp)
                {
                    // 1-pole LP
                    lp1State = lp1State + lp1PoleCoeff * (sample - lp1State);
                    sample = lp1State;
                }
                else
                {
                    for (int s = 0; s < lpStages; ++s)
                        sample = lpFilters[(size_t) s].processSample (ch, sample);
                }
            }

            data[n] = sample;
        }
    }
}

//==============================================================================

ColorVocoderAudioProcessor::ColorVocoderAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)  // External carrier input
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Initialize all band gains to 100% (1.0)
    bandGains.fill (1.0f);

    // Initialize band distribution curve to linear (default)
    bandDistributionCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bandDistributionCurve[(size_t) i] = (float) i / 100.0f;

    // Initialize band offset curve to linear 1:1 mapping (default)
    bandOffsetCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bandOffsetCurve[(size_t) i] = (float) i / 100.0f;

    // Initialize bank glide curve to linear diagonal (= plain linear portamento)
    bankGlideCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bankGlideCurve[(size_t) i] = (float) i / 100.0f;

    // Initialize per-band Q multipliers to 1.0 (no offset from global)
    perBandQMultiplier.fill (1.0f);
    perBandQLocked.fill (false);

    // Load any license stored on disk - wrapped in try-catch to prevent
    // constructor crash if license file is corrupted or I/O error occurs
    try {
        licenseManager.loadLicenseFromDisk();
        // Check if periodic re-verification is needed (every 7 days)
        // This runs silently in background - only revokes if Gumroad explicitly rejects
        licenseManager.checkPeriodicVerification();
    } catch (...) {
        // License failed to load - continue in demo mode
    }
}

ColorVocoderAudioProcessor::~ColorVocoderAudioProcessor()
{
    // Mark FFT as uninitialized FIRST to prevent audio thread from using it during destruction
    fftInitialized = false;

    // Give audio thread time to finish any in-progress FFT operations
    // This is a safety measure during plugin unload
    // 100ms is safer for large buffer sizes (was 10ms which was too short)
    juce::Thread::sleep (100);

    // Now safe to destroy FFT objects
    window.reset();
    fft.reset();
}


//==============================================================================
// PARAMETERS
//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
ColorVocoderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("bypass", "Bypass", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix", 0.0f, 1.0f, 1.0f));

    // Mod Level: Gain on modulator (0-200%, default 100%)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "modLevel", "Mod Level",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

    // Depth: 0-200%, default 160%
    // 0% = no modulation (flat carrier), 100% = standard vocoder, 200% = exaggerated
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "depth", "Depth",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

    // Harmony
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("key", "Key",
        juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("scaleType", "Scale Type",
        juce::StringArray {
            "Major",           // 0 - Ionian
            "Minor",           // 1 - Aeolian (Natural Minor)
            "Dorian",          // 2
            "Phrygian",        // 3
            "Lydian",          // 4
            "Mixolydian",      // 5
            "Locrian",         // 6
            "Major Penta",     // 7 - Major Pentatonic
            "Minor Penta",     // 8 - Minor Pentatonic
            "Blues",           // 9
            "Harm Minor",      // 10 - Harmonic Minor
            "Mel Minor",       // 11 - Melodic Minor
            "Whole Tone",      // 12
            "Diminished",      // 13 - Half-Whole Diminished
            "Chromatic",       // 14
            "LOG",             // 15 - free LOG distribution (non-scale-locked vocoder)
            "LIN"              // 16 - free LINEAR (Hz) distribution
        }, 15));   // default LOG (free distribution; enables keytrack out of the box)

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("scaleAmount", "Scale Amount",
        0.0f, 1.0f, 1.0f)); // 0 = normal vocoder, 1 = fully snapped to scale

    // ===== MICROTUNING PARAMETERS =====
    // Reference pitch (A4 frequency) - default 440Hz, range 420-460Hz
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("referencePitch", "Reference A4",
        juce::NormalisableRange<float> (420.0f, 460.0f, 0.1f), 440.0f));

    // Per-note cent offsets (±100 cents for each of the 12 pitch classes)
    const juce::StringArray noteNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
    {
        juce::String paramId = "centOffset" + juce::String (i);
        juce::String paramName = noteNames[i] + " Cents";
        params.push_back (std::make_unique<juce::AudioParameterFloat> (paramId, paramName,
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    }

    // ===== CHORD SECTION =====
    // Select which chord degree within the scale (I through vii)
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("chordDegree", "Chord",
        juce::StringArray { "I", "ii", "iii", "IV", "V", "vi", "vii" }, 0));

    // Chord emphasis amount: 0 = all scale tones, 1 = only chord tones
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("chordAmount", "Chord Amt",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // Chord variation: Triad, Power, 7th, 9th, sus2, sus4, add9, 6th, dim, Custom
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("chordVariation", "Variation",
        juce::StringArray { "Triad", "Power", "7th", "9th", "sus2", "sus4", "add9", "6th", "dim", "Custom" }, 0));

    // ===== MIDI FOLLOW MODE =====
    // When enabled, bands snap to held MIDI notes instead of chord degrees
    params.push_back (std::make_unique<juce::AudioParameterBool> ("midiFollow", "MIDI Follow", false));

    // ===== BANK KEYTRACK =====
    // Transposes the WHOLE free-mode band grid (LOG/LIN) like a synth filter cutoff.
    // POSITION = manual bank transpose in semitones (works alone, like a cutoff sweep).
    // When KEYTRACK is on, the incoming MIDI note ADDS to POSITION (bank follows the keyboard,
    // last-note priority). GLIDE = portamento time for bank moves (shaped by an editable curve).
    // Only active in free mode (scaleType >= 15); scale-locked is untouched. Off + POSITION 0
    // => bit-identical to legacy behaviour.
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bankKeytrack", "Bank Keytrack", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bankPosition", "Bank Position",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "st"));   // semitones, bipolar
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bankGlide", "Bank Glide",
        juce::NormalisableRange<float> (0.0f, 2000.0f, 1.0f, 0.3f), 20.0f, "ms")); // ms

    // ===== MIDI CHORD TRIGGER =====
    // When enabled, MIDI keys in octave 2/3 select chord degree and variation
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "midiChordTrigger", "MIDI Chord Trigger", false));

    // ===== MIDI BAND CONTROL =====
    // When enabled, MIDI notes D3-C#4 control which pitch classes are active/muted
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "midiBandControl", "MIDI Band Control", false));

    // Formant / band offset (in semitones)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant", "Formant",
        -24.0f, 24.0f, 0.0f));

    // Harmonic shift macro (continuous float for smooth shifting)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "harmonicShift", "Harm Shift", -12.0f, 12.0f, 0.0f));

    // Bands - Continuous 1-100 range (draggable)
    params.push_back (std::make_unique<juce::AudioParameterInt> ("bandCount", "Band Count",
        1, 100, 16));  // Default to 16 bands

    // Additive Mode: OFF = deduplicate merged bands (consistent volume)
    //                ON = all bands process independently (stacking at shared frequencies)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "additiveMode", "Additive Mode", false));

    // Wide ranges so LO and HIGH can be brought very close together anywhere in the
    // spectrum (razor-thin, super-resonant band cluster). A min-gap guard (kMinRangeRatio)
    // in the band-building code keeps the range ascending. Defaults unchanged (20 / 18000)
    // so full-range behaviour and the Iter-22 Ableton match are preserved.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lowFreq", "Low Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f),
        20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("highFreq", "High Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f),
        20000.0f)); // Default 20Hz-20kHz (full audio / Vocodex reach)

    // Envelope for bands
    // Attack: 0..100 ms, skewed so low values have more resolution
    // Default 1ms for crisp transients and full volume (like Ableton's Vocoder)
    // Note: 30ms (Vocodex-style) sounds quieter because slow attack misses peaks
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attackMs", "Attack (ms)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f, 0.5f),
        1.0f));

    // Release: skewed (0.3) so low values are finer — matches Ableton's log-ish release feel.
    // Max lowered 30000->5500 ms so 50% knob ~= 546 ms (was ~2.97 s); Ableton ~550 ms at 50%.
    // Mapping: 25%->54 ms, 50%->546 ms, 75%->2.1 s, 100%->5.5 s. Default 30 ms.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("releaseMs", "Release (ms)",
        juce::NormalisableRange<float> (0.0f, 5500.0f, 0.1f, 0.3f),
        30.0f));

    // TIGHTNESS (2026-06-14): per-band release shaping in Linked mode for the mid/high transient decay.
    // 0.5 (default) = MATCH ABLETON exactly (measured: BR is slightly tighter than Ableton in 320-5k,
    // so default lengthens BR by ~x1.06..x1.27 to land on Ableton's decay). >0.5 = tighter (shorter
    // decay), <0.5 = softer (longer). Time-only change -> steady-state level/spectrum unchanged.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tightness", "Tightness",
        0.0f, 1.0f, 0.5f));

    // Band shape (this is our "Bandwidth" UI)
    // Range 0.1-2.0, default 0.3
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bandWidth", "Band Width",
        0.1f, 2.0f, 0.3f)); // low = narrow, high = wide

    // ===== ALGORITHM MODE PARAMETERS =====
    // Algorithm Mode: 0=RITUAL (fixed modulator Q), 1=LINKED (same Q for both), 2=SPLIT (separate Q controls)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "algorithmMode", "Algorithm Mode",
        juce::StringArray { "Ritual", "Linked", "Split" }, 1));   // default LINKED

    // Modulator Q (only used in SPLIT mode) - 1.0 to 60.0
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "modulatorQ", "Modulator Q",
        juce::NormalisableRange<float> (1.0f, 60.0f, 0.1f, 0.4f), 2.0f));

    // MOD BW (2026-06-14): modulator (analysis) bandwidth MULTIPLIER, Vocodex-style.
    // Used in LINKED/RITUAL (the auto-derived-Q modes). DEFAULT 1.0 = no change = Ableton match.
    // >1 = wider analysis band (lower Q) = breathy/raspy; <1 = narrower (higher Q) = resonant.
    // BW ~ 1/Q, so it is applied as Qanalysis /= modBw. Not used in SPLIT (which has modulatorQ).
    // Range [0.5,1.5] LINEAR so the knob MIDPOINT (50%) == 1.0 == neutral == Ableton (which has no
    // separate modulator BW). 0% = 0.5 (narrower analysis = resonant), 100% = 1.5 (wider = breathy/Vocodex).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "modBw", "Mod BW",
        juce::NormalisableRange<float> (0.5f, 1.5f, 0.001f), 1.0f));

    // SHAPE (2026-06-14): carrier band-shape / bandwidth MULTIPLIER for the active dCar bank (Linked).
    // DEFAULT 1.25 = MATCH ABLETON (measured): widens BR's too-narrow carrier so band ring (tail) and
    // inter-band valleys (low-mid) match Ableton's gentler band shape. 1.0 = original BR (narrower,
    // more band separation/detail, longer tail). Applied as Qcarrier_dcar /= shape. bwCorr is
    // re-calibrated for the 1.25 default; moving SHAPE drifts level slightly (like any tone control).
    // Range [1.0,1.3] LINEAR so the knob MIDPOINT (50%) == 1.15 == measured Ableton filterbank match.
    // From clean noise-fingerprint exports (AB_noise_bw10/20/35/65): best fixed shape over the usable BW
    // range (35-65%) is 1.15 (err 2.38 dB) vs 1.5 (3.16). The earlier 1.5 was tuned on ONE music file
    // (mild overfit). 0% = 1.0 (original BR, narrower), 50% = 1.15 (Ableton), 100% = 1.3 (wider).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "carrierBw", "Shape",
        juce::NormalisableRange<float> (1.0f, 1.3f, 0.001f), 1.15f));


    // ===== NOISE SECTION (Combined System) =====
    // Better than Vocodex OR Ableton: combines constant noise blend + smart consonant detection
    // Base noise (35%): Always-on texture like Vocodex noise slider
    // Consonant boost (65%): Automatically increases during S, F, T, CH sounds

    // Noise Color - tilt from pink (-1) to white (+1)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("noiseColor", "Noise Color",
        -1.0f, 1.0f, 0.0f));

    // HP Blend - mix in high-passed modulator for sibilant clarity
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpBlend", "HP Blend",
        0.0f, 1.0f, 0.0f));

    // HP Blend Slope - steepness of the high-pass filter
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("hpBlendSlope", "HP Slope",
        juce::StringArray { "12dB", "24dB", "48dB" }, 0));

    // ===== INPUT FILTER SECTION =====

    // Input filter enable/bypass
    params.push_back (std::make_unique<juce::AudioParameterBool> ("inputFilterEnabled", "Input Filter On", false));

    // Low-cut frequency (default 20Hz = flat, no filtering)
    // Use 180Hz when in Complement mode for bass separation
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputHPFreq", "Low Cut Freq",
        juce::NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.5f), 20.0f));

    // Low-cut slope: 0=6dB, 1=12dB, 2=24dB, 3=48dB, 4=72dB, 5=96dB (default 48dB)
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("inputHPSlope", "Low Cut Slope",
        juce::StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" }, 3));

    // High-cut frequency (20kHz = effectively off)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputLPFreq", "High Cut Freq",
        juce::NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));

    // High-cut slope: 0=6dB, 1=12dB, 2=24dB, 3=48dB, 4=72dB, 5=96dB (default 48dB)
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("inputLPSlope", "High Cut Slope",
        juce::StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" }, 3));

    // ===== NEW PHASE 1 PARAMETERS =====

    // Carrier Mode: 0=Self, 1=External, 2=Noise, 3=Saw, 4=Square, 5=PitchTrack
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "carrierMode", "Carrier Mode",
        juce::StringArray { "Self", "External", "Noise", "Saw", "Square", "Pitch Track" }, 0));

    // Carrier pitch (for internal oscillators) - 20 Hz to 2000 Hz
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "carrierPitch", "Carrier Pitch",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f), 220.0f));

    // Noise level - combined system: base noise (Vocodex-style) + consonant boost (smart detection)
    // Adds texture/breathiness AND improves intelligibility of S, F, T, CH sounds
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "unvoiced", "Noise", 0.0f, 1.0f, 0.0f));  // ID kept as "unvoiced" for preset compatibility

    // Sib Focus: frequency of the consonant DETECTOR's high band (the "is this a consonant?" sensor).
    // Higher = noise triggers only on true sibilants (S/SH) -> cleaner vowels; lower = broader consonants.
    // Range floored at 2kHz (must stay above the fixed 1kHz low band or the HP/LP ratio breaks); 12kHz max.
    // Default 3000 = previous fixed value (no change in default behaviour).
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sibFreq", "Sib Focus",
        juce::NormalisableRange<float> (2000.0f, 12000.0f, 1.0f, 0.3f), 3000.0f));

    // Noise HP: high-pass on the injected noise itself (my own design, NOT a Vocodex clone) so the
    // noise can be reduced to high-frequency hiss only (no low rumble). 20 Hz = off (full-range, = now).
    // NOTE: slope is a single-biquad 12 dB/oct Butterworth — MY guess. Vocodex's actual noise-HP
    // slope is undocumented/unmeasurable; we may have the WRONG slope. Revisit if a reference appears.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("noiseHp", "Noise HP",
        juce::NormalisableRange<float> (20.0f, 12000.0f, 1.0f, 0.3f), 20.0f));

    // AIR Delay: delays the AIR (HP-blend / dry-voice high-pass) passthrough -> slap/smear on the
    // sibilants of the voice. 0 ms = off (= now). My design.
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("airDelay", "AIR Delay",
        juce::NormalisableRange<float> (0.0f, 150.0f, 0.1f, 0.4f), 0.0f));

    // Noise Mode: Auto (carrier-aware), Vocal (high reactivity), Synth (constant grit)
    // Auto: External=Vocal ratios, Saw/Square/Noise=Synth ratios
    // Vocal: 35% base + 65% reactive (optimized for speech intelligibility)
    // Synth: 70% base + 30% reactive (constant grit for colour bass)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "noiseMode", "Noise Mode",
        juce::StringArray { "Auto", "Vocal", "Synth" }, 0));  // Default: Auto

    // ===== MASTER GATE =====
    // Gate threshold: 0% = off, 100% = maximum gating
    // Internally maps to -50dB (sensitive) to -10dB (aggressive)
    // Gate threshold in dB, matching Ableton's vocoder range (-inf to +12 dB).
    // -60 dB = effectively off (displays as "-inf"), +12 dB = maximum gating.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("gate", 1), "Gate",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), -60.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) {
                return v <= -59.9f ? juce::String ("-inf") : juce::String (v, 1) + " dB";
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.containsIgnoreCase ("inf") ? -60.0f : s.getFloatValue();
            })));

    // ===== PER-BAND DYNAMIC THRESHOLD (Pro-Q 3-style downward compressor) =====
    // Draggable horizontal line on the band display. Bands whose envelope is ABOVE
    // the threshold get compressed (soft-knee, 4:1, 6 dB knee).
    // 0 dB = line at top = off (nothing exceeds 0 dB). Drag down to engage.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("dynThreshold", 1), "Dyn Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) {
                return v >= -0.1f ? juce::String ("off") : juce::String (v, 1) + " dB";
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return (s.containsIgnoreCase ("off") || s.containsIgnoreCase ("inf"))
                    ? 0.0f : s.getFloatValue();
            })));

    // Gate range - how much attenuation when gate is closed (0 = no gate, 60 = full silence)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gateRange", "Gate Range",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 40.0f));  // Default 40dB attenuation

    // ===== BELL EQ PARAMETERS =====
    // Bell 1
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bell1Enabled", "Bell 1 On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell1Freq", "Bell 1 Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell1Gain", "Bell 1 Gain",
        -18.0f, 18.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell1Q", "Bell 1 Q",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.25f));  // Min 0.1 for wide bells

    // Bell 2
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bell2Enabled", "Bell 2 On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell2Freq", "Bell 2 Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell2Gain", "Bell 2 Gain",
        -18.0f, 18.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bell2Q", "Bell 2 Q",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.25f));  // Min 0.1 for wide bells

    // HP/LP Gain in dB (-24 to +24dB) - controlled by Y-axis drag
    // Positive = resonance boost at cutoff, Negative = shelf cut
    // This ensures same Y position = same dB for both HP/LP and Bell nodes
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputHPQ", "HP Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputLPQ", "LP Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f, 1.0f), 0.0f));

    // HP/LP Q WIDTH (0.5-10) - controlled by Q knob, determines how wide/narrow the resonance peak is
    // Low Q = wide peak, High Q = narrow peak (like Bell EQ)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputHPQWidth", "HP Q Width",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f), 1.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("inputLPQWidth", "LP Q Width",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f), 1.25f));

    // Complement mode - when ON, enables frequency-dependent wet/dry control
    params.push_back (std::make_unique<juce::AudioParameterBool> ("filterComplement", "Complement Mode", false));

    // ===== COMPLEMENT EQ LAYER =====
    // When complement mode is ON, this layer controls wet/dry ratio per frequency
    // Boost (+dB) = more WET at that frequency
    // Cut (-dB) = more DRY at that frequency (protecting it)

    // Complement Bell 1 - controls wet/dry ratio at specific frequency
    params.push_back (std::make_unique<juce::AudioParameterBool> ("compBell1Enabled", "Comp Bell 1 On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell1Freq", "Comp Bell 1 Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell1Gain", "Comp Bell 1 Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell1Q", "Comp Bell 1 Q",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

    // Complement Bell 2 - controls wet/dry ratio at specific frequency
    params.push_back (std::make_unique<juce::AudioParameterBool> ("compBell2Enabled", "Comp Bell 2 On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell2Freq", "Comp Bell 2 Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 4000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell2Gain", "Comp Bell 2 Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("compBell2Q", "Comp Bell 2 Q",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

    // Sub complement level - controls how much clean sub is added (0dB = unity)
    // Sub is "fixed" and ignores global mix when complement is ON
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("subComplementLevel", "Sub Comp Level",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    // High complement level - controls how much clean highs are added
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("highComplementLevel", "High Comp Level",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    // ===== SPECTRAL X/Y PAD PARAMETERS =====
    // X = Spectral Tilt (0=low bias, 0.5=flat, 1=high bias)
    // Y = Intensity (0=flat/no effect, 1=maximum tilt strength)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spectralTilt", "Spectral Tilt",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spectralIntensity", "Spectral Intensity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // ===== INPUT / OUTPUT GAIN =====
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "inputGain", "Input",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "outputGain", "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================

void ColorVocoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // Ableton tonal-match tilt (Self mode): 3 low-shelves subtract BR's +6dB-sub->0 tilt.
    // Tuned vs Ableton renders (Self, white noise). Corners/gains fit the measured tilt.
    {
        // ABLETON TONAL MATCH — MEASURED & OPTIMIZED (2026-06-14) vs Ableton-Precise white-noise at
        // BW10/50/100. BR's real deviation (absolute, level-matched at 1-4kHz) is a GENTLE +~4 dB low
        // excess (20-160Hz) and a ~-2/-3 dB dark high end (5-10k). Best fit (RMS err 0.68 dB, all
        // regions within ±1.4 dB): low-shelf -3 dB @ 110 Hz + high-shelf +2.5 dB @ 4 kHz. NO steep HP
        // (the earlier HP overcut the sub by ~21 dB). [0]=low-shelf, [1]=high-shelf, [2]=neutral.
        const double q = 0.707;
        auto lo = juce::IIRCoefficients::makeLowShelf  (sampleRate, 110.0,  q, (float) juce::Decibels::decibelsToGain (-3.0));
        auto hi = juce::IIRCoefficients::makeHighShelf (sampleRate, 4000.0, q, (float) juce::Decibels::decibelsToGain (+2.5));
        auto neutral = juce::IIRCoefficients::makeLowShelf (sampleRate, 1000.0, q, 1.0f);
        ablTiltL[0].setCoefficients (lo);      ablTiltR[0].setCoefficients (lo);
        ablTiltL[1].setCoefficients (hi);      ablTiltR[1].setCoefficients (hi);
        ablTiltL[2].setCoefficients (neutral); ablTiltR[2].setCoefficients (neutral);
        for (int s = 0; s < 3; ++s) { ablTiltL[s].reset(); ablTiltR[s].reset(); }
    }

    updateEnvelopeCoeffs();
    rebuildBands();

    // ===== STFT VOCODER (Linked mode) =====
    stftVocoder.prepare (sampleRate, samplesPerBlock);

    // ===== INPUT FILTER =====
    inputFilter.prepare (sampleRate, samplesPerBlock);
    dryFilter.prepare (sampleRate, samplesPerBlock);
    complementFilter.prepare (sampleRate, samplesPerBlock);

    // ===== HP BLEND FILTERS (for sibilant passthrough) =====
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    // Initialize all 4 HP blend filter stages (for up to 48dB/oct)
    for (int i = 0; i < 4; ++i)
    {
        hpBlendFiltersL[i].prepare (spec);
        hpBlendFiltersR[i].prepare (spec);
        hpBlendFiltersL[i].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpBlendFiltersR[i].setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpBlendFiltersL[i].setCutoffFrequency (4000.0f);  // Fixed HP for sibilants
        hpBlendFiltersR[i].setCutoffFrequency (4000.0f);
        hpBlendFiltersL[i].setResonance (1.0f / std::sqrt (2.0f));  // Butterworth
        hpBlendFiltersR[i].setResonance (1.0f / std::sqrt (2.0f));
    }

    // ===== PINK NOISE FILTER COEFFICIENT =====
    // Simple 1-pole lowpass at ~1kHz for pink noise approximation
    float pinkCutoff = 1000.0f / (float) sampleRate;
    pinkFilterCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * pinkCutoff);
    pinkFilterStateL = 0.0f;
    pinkFilterStateR = 0.0f;

    // ===== PARAMETER SMOOTHING INITIALIZATION =====
    // ~10ms for most params - natural feel, no zipper noise, responsive automation
    double smoothingTimeMs = 10.0;
    smoothedSpectralTilt.reset (sampleRate, smoothingTimeMs / 1000.0);
    smoothedSpectralIntensity.reset (sampleRate, smoothingTimeMs / 1000.0);
    // Formant shift needs longer smoothing (~80ms) to avoid IIR filter crackling.
    // IIR filters produce transients when coefficients jump; spreading the change
    // across many blocks keeps each step small enough to be inaudible.
    // Fast formant smoothing — SVF carrier filters handle frequency changes natively.
    smoothedFormant.reset (sampleRate, 0.05);

    // Initialize to current parameter values
    if (auto* ptr = apvts.getRawParameterValue ("spectralTilt"))
        smoothedSpectralTilt.setCurrentAndTargetValue (ptr->load());
    if (auto* ptr = apvts.getRawParameterValue ("spectralIntensity"))
        smoothedSpectralIntensity.setCurrentAndTargetValue (ptr->load());
    if (auto* ptr = apvts.getRawParameterValue ("formant"))
        smoothedFormant.setCurrentAndTargetValue (ptr->load());

    // ===== BANK KEYTRACK / GLIDE: settle to current target, no glide on load =====
    {
        float pos = 0.0f;
        if (auto* ptr = apvts.getRawParameterValue ("bankPosition")) pos = ptr->load();
        keytrackNote     = 60;               // neutral until a note arrives
        keytrackHeldStack.clear();
        glideTargetShift = pos;              // keytrack adds 0 at neutral note 60
        glideStartShift  = pos;
        glideProgress    = 1.0f;             // already settled
        currentBankShift.store (pos);
    }

    // ===== CPU OPTIMIZATION: Pre-allocate ALL processBlock buffers =====
    bandEnvBuffer.resize ((size_t) samplesPerBlock);
    allBandEnvs.resize (100);  // max 100 bands
    for (auto& v : allBandEnvs) v.resize ((size_t) samplesPerBlock, 0.0f);

    dryBuf_.setSize (2, samplesPerBlock);
    wetBuf_.setSize (2, samplesPerBlock);
    wetOldBuf_.setSize (2, samplesPerBlock);
    modBuf_.setSize (2, samplesPerBlock);
    noiseBuf_.setSize (2, samplesPerBlock);
    bandCarBuf_.setSize (2, samplesPerBlock);
    complementBuf_.setSize (2, samplesPerBlock);
    bellComplementBuf_.setSize (2, samplesPerBlock);
    wetEqBuf_.setSize (2, samplesPerBlock);
    dryEqBuf_.setSize (2, samplesPerBlock);

    noiseEnvBuf_.resize ((size_t) samplesPerBlock);
    unvoicedScoreBuf_.resize ((size_t) samplesPerBlock);
    lowBandEnvBuf_.resize ((size_t) samplesPerBlock);
    highBandEnvBuf_.resize ((size_t) samplesPerBlock);
    monoModBuf_.resize ((size_t) samplesPerBlock);  // CPU OPT: pre-allocate mono mod buffer
    sidechainBuf_.setSize (2, samplesPerBlock);     // CPU OPT: pre-allocate sidechain buffer

    // ===== PHASE 1: Initialize carrier generator =====
    carrierGenerator.prepare (sampleRate, samplesPerBlock);


    // Allocate carrier buffer
    carrierBuffer.setSize (2, samplesPerBlock);

    // Initialize unvoiced detection filters
    // LPF at 1000 Hz - captures voiced (vowel) content
    // HPF at sibFreq (user-tunable "Sib Focus", default 3000 Hz) - captures consonant content (S, F, T)
    lastSibFreq = apvts.getRawParameterValue ("sibFreq") ? apvts.getRawParameterValue ("sibFreq")->load() : 3000.0f;
    auto lpCoeffs = juce::IIRCoefficients::makeLowPass (sampleRate, 1000.0);
    auto hpCoeffs = juce::IIRCoefficients::makeHighPass (sampleRate, (double) lastSibFreq);

    // AIR delay line prepare (mono per channel)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
        airDelayL.prepare (spec); airDelayR.prepare (spec);
        airDelayL.reset();        airDelayR.reset();
    }
    unvoicedLPF.setCoefficients (lpCoeffs);
    unvoicedHPF.setCoefficients (hpCoeffs);
    unvoicedLPF.reset();
    unvoicedHPF.reset();
    unvoicedEnvLP = 0.0f;
    unvoicedEnvHP = 0.0f;

    // ===== BELL EQ FILTERS =====
    juce::dsp::ProcessSpec bellSpec;
    bellSpec.sampleRate = sampleRate;
    bellSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    bellSpec.numChannels = 1;

    bell1FilterL.prepare (bellSpec);
    bell1FilterR.prepare (bellSpec);
    bell2FilterL.prepare (bellSpec);
    bell2FilterR.prepare (bellSpec);

    // Initialize with default coefficients
    bell1Coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 500.0f, 1.0f, 1.0f);
    bell2Coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 2000.0f, 1.0f, 1.0f);

    // Assign coefficients to filters (use the pointer, don't dereference)
    bell1FilterL.coefficients = bell1Coeffs;
    bell1FilterR.coefficients = bell1Coeffs;
    bell2FilterL.coefficients = bell2Coeffs;
    bell2FilterR.coefficients = bell2Coeffs;

    bell1FilterL.reset();
    bell1FilterR.reset();
    bell2FilterL.reset();
    bell2FilterR.reset();

    // ===== COMPLEMENT BELL FILTERS (for dry signal in complement mode) =====
    bell1CompL.prepare (bellSpec);
    bell1CompR.prepare (bellSpec);
    bell2CompL.prepare (bellSpec);
    bell2CompR.prepare (bellSpec);

    bell1CompCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 500.0f, 1.0f, 1.0f);
    bell2CompCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 2000.0f, 1.0f, 1.0f);

    bell1CompL.coefficients = bell1CompCoeffs;
    bell1CompR.coefficients = bell1CompCoeffs;
    bell2CompL.coefficients = bell2CompCoeffs;
    bell2CompR.coefficients = bell2CompCoeffs;

    bell1CompL.reset();
    bell1CompR.reset();
    bell2CompL.reset();
    bell2CompR.reset();

    // ===== DRY BELL FILTERS (for master EQ behavior - EQ on both wet and dry) =====
    bell1DryL.prepare (bellSpec);
    bell1DryR.prepare (bellSpec);
    bell2DryL.prepare (bellSpec);
    bell2DryR.prepare (bellSpec);

    bell1DryL.coefficients = bell1Coeffs;
    bell1DryR.coefficients = bell1Coeffs;
    bell2DryL.coefficients = bell2Coeffs;
    bell2DryR.coefficients = bell2Coeffs;

    bell1DryL.reset();
    bell1DryR.reset();
    bell2DryL.reset();
    bell2DryR.reset();

    // ===== COMPLEMENT EQ LAYER (frequency-dependent wet/dry ratio control) =====
    // Separate wet and dry filter instances to avoid state corruption
    compBell1WetL.prepare (bellSpec);
    compBell1WetR.prepare (bellSpec);
    compBell1DryL.prepare (bellSpec);
    compBell1DryR.prepare (bellSpec);
    compBell2WetL.prepare (bellSpec);
    compBell2WetR.prepare (bellSpec);
    compBell2DryL.prepare (bellSpec);
    compBell2DryR.prepare (bellSpec);

    compBell1WetCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 1000.0f, 1.0f, 1.0f);
    compBell1DryCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 1000.0f, 1.0f, 1.0f);
    compBell2WetCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 4000.0f, 1.0f, 1.0f);
    compBell2DryCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 4000.0f, 1.0f, 1.0f);

    compBell1WetL.coefficients = compBell1WetCoeffs;
    compBell1WetR.coefficients = compBell1WetCoeffs;
    compBell1DryL.coefficients = compBell1DryCoeffs;
    compBell1DryR.coefficients = compBell1DryCoeffs;
    compBell2WetL.coefficients = compBell2WetCoeffs;
    compBell2WetR.coefficients = compBell2WetCoeffs;
    compBell2DryL.coefficients = compBell2DryCoeffs;
    compBell2DryR.coefficients = compBell2DryCoeffs;

    compBell1WetL.reset();
    compBell1WetR.reset();
    compBell1DryL.reset();
    compBell1DryR.reset();
    compBell2WetL.reset();
    compBell2WetR.reset();
    compBell2DryL.reset();
    compBell2DryR.reset();

    // Sub and high complement filters (for isolating sub and high frequencies)
    subComplementFilter.prepare (sampleRate, samplesPerBlock);
    highComplementFilter.prepare (sampleRate, samplesPerBlock);

    // ===== SPECTRUM ANALYZER FFT (safely initialized after all other setup) =====
    // Use unique_ptr to avoid crashes during plugin scan when constructed inline
    fftInitialized = false;  // Reset flag before allocation attempt
    try
    {
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        window = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

        // Only set initialized if both pointers are valid
        if (fft != nullptr && window != nullptr)
        {
            fftBuffer.fill (0.0f);
            spectrumWriteBuffer.fill (0.0f);
            spectrumReadBuffer.fill (0.0f);
            fftWriteIndex = 0;
            spectrumDataReady.store (false);
            fftInitialized = true;
        }
    }
    catch (...)
    {
        // FFT allocation failed - plugin will work without spectrum analyzer
        fft = nullptr;
        window = nullptr;
        fftInitialized = false;
    }

    // Dual-color spectrum buffers (wet vs dry visualization)
    spectrumWetBuffer.fill (0.0f);
    spectrumDryBuffer.fill (0.0f);
    fftBufferDry.fill (0.0f);
    fftWriteIndexDry = 0;

    // ===== TRIAL NAG SYSTEM =====
    nagIntervalSamples = static_cast<int> (NAG_INTERVAL_SECONDS * sampleRate);
    nagDurationSamples = static_cast<int> (NAG_DURATION_SECONDS * sampleRate);
    nagSampleCounter = 0;
    nagActiveSamples = 0;
}

bool ColorVocoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn = layouts.getMainInputChannelSet();

    // Main in/out must be stereo or mono
    if (mainOut != juce::AudioChannelSet::mono()
     && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Main in/out must match
    if (mainOut != mainIn)
        return false;

    // Sidechain input (bus index 1) is optional
    // Can be disabled, mono, or stereo
    if (layouts.getNumChannels (true, 1) > 0)  // true = input bus, index 1 = sidechain
    {
        auto sidechainSet = layouts.getChannelSet (true, 1);
        if (sidechainSet != juce::AudioChannelSet::disabled()
         && sidechainSet != juce::AudioChannelSet::mono()
         && sidechainSet != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

//==============================================================================
// ENVELOPE COEFFS
//==============================================================================

void ColorVocoderAudioProcessor::updateEnvelopeCoeffs()
{
    auto attackMs  = apvts.getRawParameterValue ("attackMs")->load();
    auto releaseMs = apvts.getRawParameterValue ("releaseMs")->load();

    // CPU OPT: Skip expensive exp() calls when parameters haven't changed
    if (attackMs == lastEnvAttackMs && releaseMs == lastEnvReleaseMs)
        return;
    lastEnvAttackMs = attackMs;
    lastEnvReleaseMs = releaseMs;

    auto attackSec  = juce::jmax (attackMs  * 0.001f, 0.0001f);
    auto releaseSec = juce::jmax (releaseMs * 0.001f, 0.0001f);

    // Matched to Ableton: release time = time to reach 1% (-40 dB).
    // Verified: Ableton at release=200ms → T_1%=191ms (matches pow(0.01) → T_1%=200ms).
    // Our old exp(-1) formula gave T_37%=136ms at 200ms — 4.6× too slow.
    // ITER 22 FINAL: pow(0.01) is correct (NOT exp(-1)).
    // pow(0.01) = "reach -40 dB in release_ms" — matches ABL.
    // exp(-1) killed vocal matching (0.87 → 0.92 on vocal with pow).
    attackCoeff  = std::pow (0.01f, 1.0f / (currentSampleRate * attackSec));
    releaseCoeff = std::pow (0.01f, 1.0f / (currentSampleRate * releaseSec));

    // ITER 14/15 (2026-04-09/10): Voclib-style cascaded LP coefficient for Linked mode.
    // ITER 15 makes the actual cascade coefficient PER-BAND (frequency-dependent),
    // computed in the audio loop from b->currentFreq. This global value is kept as
    // a fallback. lastReleaseSec is cached so the audio loop can compute per-band tau.
    lastReleaseSec = releaseSec;
    float cascadeReactSec = releaseSec * 4.0f;
    cascadeEnvCoef = std::pow (0.01f, 1.0f / (currentSampleRate * cascadeReactSec));

    // Noise uses same release as bands (Ableton "Fast" style)
    // No minimum floor - follows user's release setting exactly
    noiseReleaseCoeff = releaseCoeff;

    // ===== GATE SMOOTHING COEFFICIENTS =====
    // These are FIXED times (not user-adjustable) based on standard gate behavior:
    // - Attack 2ms: Fast open, lets transients through without distortion
    // - Release 30ms: Slow close, prevents audible crackle when gate closes
    // Using proper sample-rate-dependent formula from musicdsp.org
    const float gateAttackMs  = 2.0f;   // 2ms - standard gate attack
    const float gateReleaseMs = 30.0f;  // 30ms - prevents crackle

    float gateAttackSec  = gateAttackMs  * 0.001f;
    float gateReleaseSec = gateReleaseMs * 0.001f;

    gateAttackCoeff  = std::exp (-1.0f / (float(currentSampleRate) * gateAttackSec));
    gateReleaseCoeff = std::exp (-1.0f / (float(currentSampleRate) * gateReleaseSec));
}

//==============================================================================
// SCALE FREQUENCIES + SNAP
//==============================================================================

void ColorVocoderAudioProcessor::buildScaleNoteFrequencies (std::vector<float>& outNotesHz,
                                                            int key, int scaleType,
                                                            float lowHz, float highHz) const
{
    outNotesHz.clear();

    // Scale intervals (semitones from root)
    std::vector<int> scaleIntervals;
    switch (scaleType)
    {
        case 0:  scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Major (Ionian)
        case 1:  scaleIntervals = { 0,2,3,5,7,8,10 };       break; // Minor (Aeolian)
        case 2:  scaleIntervals = { 0,2,3,5,7,9,10 };       break; // Dorian
        case 3:  scaleIntervals = { 0,1,3,5,7,8,10 };       break; // Phrygian
        case 4:  scaleIntervals = { 0,2,4,6,7,9,11 };       break; // Lydian
        case 5:  scaleIntervals = { 0,2,4,5,7,9,10 };       break; // Mixolydian
        case 6:  scaleIntervals = { 0,1,3,5,6,8,10 };       break; // Locrian
        case 7:  scaleIntervals = { 0,2,4,7,9 };            break; // Major Pentatonic
        case 8:  scaleIntervals = { 0,3,5,7,10 };           break; // Minor Pentatonic
        case 9:  scaleIntervals = { 0,3,5,6,7,10 };         break; // Blues
        case 10: scaleIntervals = { 0,2,3,5,7,8,11 };       break; // Harmonic Minor
        case 11: scaleIntervals = { 0,2,3,5,7,9,11 };       break; // Melodic Minor
        case 12: scaleIntervals = { 0,2,4,6,8,10 };         break; // Whole Tone
        case 13: scaleIntervals = { 0,1,3,4,6,7,9,10 };     break; // Diminished (HW)
        case 14: scaleIntervals = { 0,1,2,3,4,5,6,7,8,9,10,11 }; break; // Chromatic
        default: scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Default to Major
    }

    // Notes across full audible range, clamped by lowHz / highHz
    for (int midiNote = 0; midiNote <= 150; ++midiNote)
    {
        int pc    = midiNote % 12;
        bool inScale = false;

        for (int semis : scaleIntervals)
        {
            int scalePc = (key + semis) % 12;
            if (scalePc == pc)
            {
                inScale = true;
                break;
            }
        }

        if (! inScale)
            continue;

        float freq = getMicrotunedFrequency (midiNote);
        if (freq >= lowHz && freq <= highHz)
            outNotesHz.push_back (freq);
    }

    if (outNotesHz.empty())
    {
        outNotesHz.push_back (lowHz);
        outNotesHz.push_back (highHz);
    }
}

float ColorVocoderAudioProcessor::snapToScale (float freqHz) const
{
    if (scaleNoteFreqs.empty())
        return freqHz;

    float bestF = scaleNoteFreqs[0];
    float bestD = std::abs (freqHz - bestF);

    for (auto noteF : scaleNoteFreqs)
    {
        float d = std::abs (freqHz - noteF);
        if (d < bestD)
        {
            bestD = d;
            bestF = noteF;
        }
    }

    return bestF;
}

//==============================================================================
// BAND BUILD – with monotonic mapping across scale notes
//==============================================================================

int ColorVocoderAudioProcessor::findClosestScaleNoteIndex (float freqHz) const
{
    if (scaleNoteFreqs.empty())
        return -1;

    float bestDiff = std::numeric_limits<float>::max();
    int bestIdx = -1;

    for (int i = 0; i < (int) scaleNoteFreqs.size(); ++i)
    {
        float d = std::abs (std::log (freqHz) - std::log (scaleNoteFreqs[i]));
        if (d < bestDiff)
        {
            bestDiff = d;
            bestIdx = i;
        }
    }

    return bestIdx;
}

//==============================================================================
// CHORD FUNCTIONS – build chord tones and snap to them
//==============================================================================

std::vector<int> ColorVocoderAudioProcessor::getChordIntervals (int degree, int variation, int scaleType) const
{
    // Determine chord quality based on scale type and degree
    bool isMinorChord = false;
    bool isDiminished = false;

    // Chord qualities for each scale type
    switch (scaleType)
    {
        case 0:  // Major (Ionian): I=maj, ii=min, iii=min, IV=maj, V=maj, vi=min, vii=dim
            isMinorChord = (degree == 1 || degree == 2 || degree == 5);
            isDiminished = (degree == 6);
            break;
        case 1:  // Minor (Aeolian): i=min, ii=dim, III=maj, iv=min, v=min, VI=maj, VII=maj
            isMinorChord = (degree == 0 || degree == 3 || degree == 4);
            isDiminished = (degree == 1);
            break;
        case 2:  // Dorian: i=min, ii=min, III=maj, IV=maj, v=min, vi=dim, VII=maj
            isMinorChord = (degree == 0 || degree == 1 || degree == 4);
            isDiminished = (degree == 5);
            break;
        case 3:  // Phrygian: i=min, II=maj, III=maj, iv=min, v=dim, VI=maj, vii=min
            isMinorChord = (degree == 0 || degree == 3 || degree == 6);
            isDiminished = (degree == 4);
            break;
        case 4:  // Lydian: I=maj, II=maj, iii=min, iv=dim, V=maj, vi=min, VII=maj
            isMinorChord = (degree == 2 || degree == 5);
            isDiminished = (degree == 3);
            break;
        case 5:  // Mixolydian: I=maj, ii=min, iii=dim, IV=maj, v=min, VI=maj, VII=min
            isMinorChord = (degree == 1 || degree == 4 || degree == 6);
            isDiminished = (degree == 2);
            break;
        case 6:  // Locrian: i=dim, II=maj, iii=min, IV=min, V=maj, VI=maj, vii=min
            isMinorChord = (degree == 2 || degree == 3 || degree == 6);
            isDiminished = (degree == 0);
            break;
        case 7:  // Major Pentatonic: Use major triads for all degrees
        case 12: // Whole Tone: All augmented
            isMinorChord = false;
            isDiminished = false;
            break;
        case 8:  // Minor Pentatonic: Use minor triads for all degrees
        case 9:  // Blues: Use minor/dominant feel
            isMinorChord = true;
            isDiminished = false;
            break;
        case 10: // Harmonic Minor: i=min, ii=dim, III+=aug, iv=min, V=maj, VI=maj, vii=dim
            isMinorChord = (degree == 0 || degree == 3);
            isDiminished = (degree == 1 || degree == 6);
            break;
        case 11: // Melodic Minor: i=min, ii=min, III+=aug, IV=maj, V=maj, vi=dim, vii=dim
            isMinorChord = (degree == 0 || degree == 1);
            isDiminished = (degree == 5 || degree == 6);
            break;
        case 13: // Diminished: All diminished chords
            isDiminished = true;
            break;
        case 14: // Chromatic: Default to major
        default:
            isMinorChord = false;
            isDiminished = false;
            break;
    }

    switch (variation)
    {
        case 0:  // Triad
            if (isDiminished)
                return { 0, 3, 6 };   // dim triad
            else if (isMinorChord)
                return { 0, 3, 7 };   // minor triad
            else
                return { 0, 4, 7 };   // major triad

        case 1:  // Power (root + 5th only)
            return { 0, 7 };

        case 2:  // 7th
            if (isDiminished)
                return { 0, 3, 6, 9 };    // dim7
            else if (isMinorChord)
                return { 0, 3, 7, 10 };   // min7
            else if (degree == 4)         // V chord = dominant 7
                return { 0, 4, 7, 10 };   // dom7
            else
                return { 0, 4, 7, 11 };   // maj7

        case 3:  // 9th
            if (isMinorChord)
                return { 0, 3, 7, 10, 14 };  // min9
            else
                return { 0, 4, 7, 10, 14 };  // dom9/maj9

        case 4:  // sus2
            return { 0, 2, 7 };

        case 5:  // sus4
            return { 0, 5, 7 };

        case 6:  // add9
            if (isMinorChord)
                return { 0, 3, 7, 14 };   // min(add9)
            else
                return { 0, 4, 7, 14 };   // maj(add9)

        case 7:  // 6th
            if (isMinorChord)
                return { 0, 3, 7, 9 };    // min6
            else
                return { 0, 4, 7, 9 };    // maj6

        case 8:  // dim (always diminished regardless of degree quality)
            return { 0, 3, 6 };

        // case 9 (Custom) is handled before reaching getChordIntervals

        default:
            return { 0, 4, 7 };
    }
}

void ColorVocoderAudioProcessor::buildChordFrequencies (std::vector<float>& outChordHz,
                                                         int key, int scaleType, int chordDegree, int chordVariation,
                                                         float lowHz, float highHz) const
{
    outChordHz.clear();

    // Get scale intervals (must match buildScaleNoteFrequencies)
    std::vector<int> scaleIntervals;
    switch (scaleType)
    {
        case 0:  scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Major (Ionian)
        case 1:  scaleIntervals = { 0,2,3,5,7,8,10 };       break; // Minor (Aeolian)
        case 2:  scaleIntervals = { 0,2,3,5,7,9,10 };       break; // Dorian
        case 3:  scaleIntervals = { 0,1,3,5,7,8,10 };       break; // Phrygian
        case 4:  scaleIntervals = { 0,2,4,6,7,9,11 };       break; // Lydian
        case 5:  scaleIntervals = { 0,2,4,5,7,9,10 };       break; // Mixolydian
        case 6:  scaleIntervals = { 0,1,3,5,6,8,10 };       break; // Locrian
        case 7:  scaleIntervals = { 0,2,4,7,9 };            break; // Major Pentatonic
        case 8:  scaleIntervals = { 0,3,5,7,10 };           break; // Minor Pentatonic
        case 9:  scaleIntervals = { 0,3,5,6,7,10 };         break; // Blues
        case 10: scaleIntervals = { 0,2,3,5,7,8,11 };       break; // Harmonic Minor
        case 11: scaleIntervals = { 0,2,3,5,7,9,11 };       break; // Melodic Minor
        case 12: scaleIntervals = { 0,2,4,6,8,10 };         break; // Whole Tone
        case 13: scaleIntervals = { 0,1,3,4,6,7,9,10 };     break; // Diminished (HW)
        case 14: scaleIntervals = { 0,1,2,3,4,5,6,7,8,9,10,11 }; break; // Chromatic
        default: scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Default to Major
    }

    // Safety check - limit degree to available scale notes
    if (chordDegree < 0 || chordDegree >= (int) scaleIntervals.size())
        return;

    // Get chord intervals based on degree and variation
    std::vector<int> chordIntervals = getChordIntervals (chordDegree, chordVariation, scaleType);

    // Convert to absolute pitch classes (0-11)
    std::vector<int> chordPitchClasses;
    int rootOffset = scaleIntervals[(size_t) chordDegree];

    for (int interval : chordIntervals)
    {
        int pitchClass = (key + rootOffset + interval) % 12;
        chordPitchClasses.push_back (pitchClass);
    }

    // Generate all matching frequencies in range
    for (int midiNote = 0; midiNote <= 150; ++midiNote)
    {
        int pc = midiNote % 12;
        bool inChord = std::find (chordPitchClasses.begin(), chordPitchClasses.end(), pc) != chordPitchClasses.end();

        if (!inChord)
            continue;

        float freq = (float) getMicrotunedFrequency (midiNote);
        if (freq >= lowHz && freq <= highHz)
            outChordHz.push_back (freq);
    }
}

float ColorVocoderAudioProcessor::snapToChord (float freqHz) const
{
    if (chordNoteFreqs.empty())
        return freqHz;

    float bestF = chordNoteFreqs[0];
    float bestD = std::abs (std::log (freqHz) - std::log (bestF));

    for (auto noteF : chordNoteFreqs)
    {
        float d = std::abs (std::log (freqHz) - std::log (noteF));
        if (d < bestD)
        {
            bestD = d;
            bestF = noteF;
        }
    }

    return bestF;
}

float ColorVocoderAudioProcessor::snapToMidi (float freqHz) const
{
    if (midiNoteFreqs.empty())
        return freqHz;

    float bestF = midiNoteFreqs[0];
    float bestD = std::abs (std::log (freqHz) - std::log (bestF));

    for (auto noteF : midiNoteFreqs)
    {
        float d = std::abs (std::log (freqHz) - std::log (noteF));
        if (d < bestD)
        {
            bestD = d;
            bestF = noteF;
        }
    }

    return bestF;
}

//==============================================================================
// MICROTUNING FUNCTIONS
//==============================================================================

float ColorVocoderAudioProcessor::getMicrotunedFrequency (int midiNote) const
{
    // Get pitch class (0-11) and octave
    int pitchClass = midiNote % 12;

    // Get cent offset for this pitch class from parameters
    float centOffset = centOffsets[(size_t) pitchClass];

    // Calculate frequency with reference pitch and cent offset
    // Standard formula: freq = refPitch * 2^((midiNote - 69 + centOffset/100) / 12)
    float semitonesFromA4 = (float) (midiNote - 69) + centOffset / 100.0f;
    float freq = referencePitch * std::pow (2.0f, semitonesFromA4 / 12.0f);

    return freq;
}

void ColorVocoderAudioProcessor::loadScalaFile (const juce::File& scalaFile)
{
    // Parse Scala (.scl) file format
    // Format: Lines starting with ! are comments
    // First non-comment line is description
    // Second non-comment line is number of notes
    // Following lines are cent values or ratios

    if (!scalaFile.existsAsFile())
        return;

    juce::StringArray lines;
    scalaFile.readLines (lines);

    std::vector<float> centValues;
    bool foundDescription = false;
    bool foundNoteCount = false;
    int expectedNotes = 0;

    for (const auto& line : lines)
    {
        juce::String trimmed = line.trim();

        // Skip empty lines and comments
        if (trimmed.isEmpty() || trimmed.startsWithChar ('!'))
            continue;

        if (!foundDescription)
        {
            currentScalaName = trimmed;
            foundDescription = true;
        }
        else if (!foundNoteCount)
        {
            expectedNotes = trimmed.getIntValue();
            foundNoteCount = true;
        }
        else
        {
            // Parse cent value or ratio
            float cents = 0.0f;

            if (trimmed.containsChar ('/'))
            {
                // Ratio format (e.g., "3/2" or "5/4")
                int slashPos = trimmed.indexOf ("/");
                float num = trimmed.substring (0, slashPos).getFloatValue();
                float den = trimmed.substring (slashPos + 1).getFloatValue();
                if (den > 0.0f)
                    cents = 1200.0f * std::log2 (num / den);
            }
            else
            {
                // Cent value (e.g., "100.0" or "700.0")
                cents = trimmed.getFloatValue();
            }

            centValues.push_back (cents);

            if ((int) centValues.size() >= expectedNotes)
                break;
        }
    }

    // Apply to cent offsets (normalize to 12-note scale)
    if (centValues.size() >= 12)
    {
        // Full 12-note scale from Scala file
        for (int i = 0; i < 12; ++i)
        {
            // Scala files give intervals from the root, we need offsets from equal temperament
            float equalTempCents = i * 100.0f;
            float scalaCents = (i < (int) centValues.size()) ? centValues[(size_t) i] : equalTempCents;
            centOffsets[(size_t) i] = scalaCents - equalTempCents;
        }
        scalaFileLoaded = true;
    }
    else if (!centValues.empty())
    {
        // Partial scale - distribute across octave
        scalaFileLoaded = true;
        // For now, just mark as loaded - full implementation would interpolate
    }
}

void ColorVocoderAudioProcessor::resetMicrotuning()
{
    referencePitch = 440.0f;
    centOffsets.fill (0.0f);
    scalaFileLoaded = false;
    currentScalaName.clear();
}

void ColorVocoderAudioProcessor::rebuildBands()
{
    const juce::SpinLock::ScopedLockType lock (bandsLock);

    // ===== SMOOTH BAND TRANSITIONS =====
    // Instead of instant clear, move current bands to bandsOld for crossfade
    // This prevents clicks/pops when band count or other rebuild-triggering params change

    // Only start crossfade if we have existing bands to fade from
    if (bands.size() > 0 && currentSampleRate > 0)
    {
        // Move current bands to old bank
        bandsOld.clear (true);
        for (int i = 0; i < bands.size(); ++i)
        {
            bandsOld.add (bands.getUnchecked (i));
        }
        bands.clearQuick (false);  // Don't delete, we moved pointers to bandsOld

        // Start crossfade (~50ms)
        crossfadeSamplesTotal = (int) (currentSampleRate * 0.05);
        crossfadeSamplesRemaining = crossfadeSamplesTotal;
        isCrossfading = true;
    }
    else
    {
        bands.clear (true);
    }

    auto bandCount     = (int) apvts.getRawParameterValue ("bandCount")->load();
    auto lowFreq       =        apvts.getRawParameterValue ("lowFreq")->load();
    auto highFreq      =        apvts.getRawParameterValue ("highFreq")->load();
    auto key           = (int)  apvts.getRawParameterValue ("key")->load();
    auto scaleType     = (int)  apvts.getRawParameterValue ("scaleType")->load();
    auto scaleAmt      =        apvts.getRawParameterValue ("scaleAmount")->load();

    // Microtuning parameters
    referencePitch = apvts.getRawParameterValue ("referencePitch")->load();
    for (int i = 0; i < 12; ++i)
        centOffsets[(size_t) i] = apvts.getRawParameterValue ("centOffset" + juce::String (i))->load();

    auto harmonicShift = apvts.getRawParameterValue ("harmonicShift")->load();

    lastBandCount   = bandCount;  // Store band count for comparison
    lastLowFreq     = lowFreq;
    lastHighFreq    = highFreq;
    lastKey         = key;
    lastScaleType   = scaleType;
    lastScaleAmt    = scaleAmt;
    lastBandWidth   = apvts.getRawParameterValue ("bandWidth")->load();
    lastHarmShift   = harmonicShift;

    const int nBands = bandCount;

    // RANGE GUARD: with the widened LO/HI param ranges the user can bring them very close
    // (or even cross). Force an ascending range with a min gap so band layout stays valid.
    // lastLowFreq/lastHighFreq above keep the RAW param values for change-detection.
    constexpr float kMinRangeRatio = 1.1f;   // ~0.14 octave minimum span
    highFreq = juce::jmax (highFreq, lowFreq * kMinRangeRatio);

    // Distribution mode: scaleType 15=LOG, 16=LIN -> free (non-scale-locked) vocoder.
    // In free mode the bands ignore key/scale/scaleAmount and spread log- or linearly over [low,high].
    const bool freeDist = (scaleType >= 15);
    const bool linDist  = (scaleType == 16);

    // Build scale notes for this range (only needed in scale-locked modes)
    if (! freeDist)
        buildScaleNoteFrequencies (scaleNoteFreqs, key, scaleType, lowFreq, highFreq);
    else
        scaleNoteFreqs.clear();
    const int nNotes = freeDist ? 0 : (int) scaleNoteFreqs.size();

    std::vector<float> baseFreqs  ((size_t) nBands);
    std::vector<float> scaleFreqs ((size_t) nBands);

    // Base distribution: LIN = linear in Hz, otherwise (LOG/KEY) = logarithmic.
    float ratio = highFreq / juce::jmax (lowFreq, 20.0f);
    for (int i = 0; i < nBands; ++i)
    {
        float t = (nBands > 1 ? (float) i / (float) (nBands - 1) : 0.0f);
        baseFreqs[(size_t) i] = linDist ? (lowFreq + (highFreq - lowFreq) * t)
                                        : (lowFreq * std::pow (ratio, t));
    }

    // Map bands across scale notes monotonically (avoid piling many bands on one note).
    // In free mode (LOG/LIN) there is no snapping: scaleFreqs == baseFreqs.
    for (int i = 0; i < nBands; ++i)
    {
        float bandPos = (nBands > 1 ? (float) i / (float) (nBands - 1) : 0.0f);

        float fBase = baseFreqs[(size_t) i];
        float fOut  = fBase;

        if (nNotes > 0)
        {
            // Base note index for this band: spread from lowest to highest scale note
            int baseNoteIndex = 0;
            if (nNotes > 1)
                baseNoteIndex = (int) std::round (bandPos * (float) (nNotes - 1));
            baseNoteIndex = juce::jlimit (0, nNotes - 1, baseNoteIndex);

            // Band-dependent harmonic shift in note-domain
            float shiftShape = std::pow (bandPos, 1.5f);          // low bands less shift, highs more
            float effShiftF  = (float) harmonicShift * shiftShape;
            int   effShift   = (int) std::round (effShiftF);

            int idx = baseNoteIndex + effShift;

            while (idx < 0)       idx += nNotes;
            while (idx >= nNotes) idx -= nNotes;

            fOut = scaleNoteFreqs[(size_t) idx];
        }

        scaleFreqs[(size_t) i] = fOut;
    }

    bands.ensureStorageAllocated (nBands);

    for (int i = 0; i < nBands; ++i)
    {
        auto* b = new Band();
        b->centerFreqBase  = baseFreqs [(size_t) i];
        b->centerFreqScale = scaleFreqs[(size_t) i];
        // Initialize currentFreq respecting scaleAmount (0% = pure base freq, 100% = scale-snapped)
        float scaleAmt = apvts.getRawParameterValue ("scaleAmount")->load();
        b->currentFreq     = baseFreqs[(size_t) i] * (1.0f - scaleAmt) + scaleFreqs[(size_t) i] * scaleAmt;
        b->cachedPitchClass = frequencyToPitchClass (b->currentFreq);
        b->env             = 0.0f;
        b->env2            = 0.0f;
        b->env3            = 0.0f;
        b->env4            = 0.0f;
        b->env5            = 0.0f;
        b->env6            = 0.0f;
        // ITER 21: reset ring buffer moving average
        b->envRingSize = juce::jmin ((int) (0.200f * (float) currentSampleRate),
                                      (int) Band::kEnvRingMaxSize);
        std::fill_n (b->envRingBuf, Band::kEnvRingMaxSize, 0.0f);
        b->envRingHead = 0;
        b->envRingSum = 0.0f;
        // ITER 19: reset Butterworth BP filter state
        for (int s = 0; s < 2; ++s) {
            b->butterLP_L[s].reset();
            b->butterLP_R[s].reset();
            b->butterHP_L[s].reset();
            b->butterHP_R[s].reset();
        }

        b->analysisFilter.reset();
        b->analysisFilter2.reset();
        b->analysisFilter3.reset();
        b->carrierFilterL.reset();
        b->carrierFilterR.reset();
        b->carrierFilterL2.reset();
        b->carrierFilterR2.reset();
        b->carrierFilterL3.reset();
        b->carrierFilterL4.reset();
        b->carrierFilterR3.reset();
        b->carrierFilterR4.reset();
        b->carrierFilterL5.reset();
        b->carrierFilterR5.reset();
        for (int s = 0; s < 4; ++s) { b->dCarL[s].reset(); b->dCarR[s].reset(); }
        for (int s = 0; s < 3; ++s) b->dAna[s].reset();

        // Initialize SVF carrier filters ONCE (prepare must only be called here, not per-block)
        {
            double safeSR = (currentSampleRate > 0.0) ? currentSampleRate : 44100.0;
            juce::dsp::ProcessSpec spec { safeSR, (juce::uint32) getBlockSize(), 1 };
            b->svfCarrierL1.prepare (spec); b->svfCarrierR1.prepare (spec);
            b->svfCarrierL2.prepare (spec); b->svfCarrierR2.prepare (spec);
            b->svfCarrierL3.prepare (spec); b->svfCarrierR3.prepare (spec);
            b->svfCarrierL1.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            b->svfCarrierR1.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            b->svfCarrierL2.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            b->svfCarrierR2.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            b->svfCarrierL3.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            b->svfCarrierR3.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        }

        // ====== CRITICAL: Initialize filter coefficients to prevent NaN/crash ======
        // Filters MUST have coefficients before processSample() is called.
        // Use a safe default - updateBandFilters() will set proper values later.
        double safeSampleRate = (currentSampleRate > 0.0) ? currentSampleRate : 44100.0;
        auto defaultCoeffs = juce::IIRCoefficients::makeLowPass(safeSampleRate, 1000.0);
        b->analysisFilter.setCoefficients(defaultCoeffs);
        b->analysisFilter2.setCoefficients(defaultCoeffs);
        b->analysisFilter3.setCoefficients(defaultCoeffs);
        b->carrierFilterL.setCoefficients(defaultCoeffs);
        b->carrierFilterR.setCoefficients(defaultCoeffs);
        b->carrierFilterL2.setCoefficients(defaultCoeffs);
        b->carrierFilterR2.setCoefficients(defaultCoeffs);
        b->carrierFilterL3.setCoefficients(defaultCoeffs);
        b->carrierFilterL4.setCoefficients(defaultCoeffs);
        b->carrierFilterR3.setCoefficients(defaultCoeffs);
        b->carrierFilterR4.setCoefficients(defaultCoeffs);
        b->carrierFilterL5.setCoefficients(defaultCoeffs);
        b->carrierFilterR5.setCoefficients(defaultCoeffs);
        // ===========================================================================

        b->bandQ           = 2.0f;
        bands.add (b);
    }

    updateBandFilters();
    updateNoiseFilters();
}

//==============================================================================
// BAND FILTERS / Q MAPPING – more aggressive + stacked filters
//==============================================================================

void ColorVocoderAudioProcessor::updateBandFilters()
{
    auto scaleAmt     = apvts.getRawParameterValue ("scaleAmount")->load();
    auto bandWidth    = apvts.getRawParameterValue ("bandWidth")->load();
    // Use SMOOTHED formant to prevent crackling during automation
    auto formantSemis = smoothedFormant.getCurrentValue();
    auto chordAmt     = apvts.getRawParameterValue ("chordAmount")->load();

    // ===== ALGORITHM MODE PARAMETERS =====
    int algorithmMode = (int) apvts.getRawParameterValue ("algorithmMode")->load();
    float modulatorQParam = apvts.getRawParameterValue ("modulatorQ")->load();
    float modBwParam = apvts.getRawParameterValue ("modBw") ? apvts.getRawParameterValue ("modBw")->load() : 1.0f;
    float carrierBwParam = apvts.getRawParameterValue ("carrierBw") ? apvts.getRawParameterValue ("carrierBw")->load() : 1.0f;


    // Normalise BandWidth (0.1..2.0) -> norm 0..1
    // norm = 0   -> narrowest
    // norm = 1   -> widest
    float norm = (bandWidth - 0.1f) / (2.0f - 0.1f);
    norm = juce::jlimit (0.0f, 1.0f, norm);

    // Ableton vocoder cracking (2026-03-22/23):
    // 2 carrier stages, Q=62 combined at BW=10%, cos()^1.29 freq-dep correction.
    // Wide BWs (70-200%) match within 2-4 dB. Narrow BWs inherently harder.
    float bwPercent = bandWidth * 100.0f;  // 0.1..2.0 -> 10..200
    bwPercent = juce::jlimit (10.0f, 200.0f, bwPercent);

    const int nBands = bands.size();
    // BW-dependent Q (combined Q for 2 cascaded stages).
    // cos()^1.29 frequency-dependent correction applied per-band below.
    // ITER 16/19 (2026-04-09/10): Q=37 per stage = March 2026 measured value.
    // Locked in as the final state of this session.
    // Cascading 4 stages of Q=37 gives effective Q ≈ 85 at BW=10%.
    // Tested Q=22 (too wide), Q=30 (middle), Q=37 (current best).
    // The 1 kHz off-band-center -10 dB issue is a BAND POSITIONING problem,
    // not a Q problem (BR bands at 990, 1077; 1000 Hz lands between).
    // ITER 20a (2026-04-10): BW=10% Q revised from 37 → 6.0 based on
    // Q=37 at BW=10% — confirmed by March Q breakthrough, Apr 9 multi-band
    // Two Q tables: Linked mode uses ABL-matched Q=40 at BW=10%.
    // Ritual/Split modes keep the original wider Q (different character).
    static const float qTableLinked[][2] = {
        {10.0f, 40.0f}, {15.0f, 27.0f}, {20.0f, 19.0f}, {30.0f, 13.0f},
        {40.0f, 9.0f}, {50.0f, 7.0f}, {70.0f, 5.0f},
        {100.0f, 3.5f}, {130.0f, 2.8f}, {170.0f, 2.2f}, {200.0f, 2.0f}
    };
    static const float qTableRitual[][2] = {
        {10.0f, 6.0f}, {15.0f, 5.0f}, {20.0f, 4.5f}, {30.0f, 3.5f},
        {40.0f, 3.0f}, {50.0f, 2.7f}, {70.0f, 2.4f},
        {100.0f, 2.0f}, {130.0f, 1.8f}, {170.0f, 1.7f}, {200.0f, 1.7f}
    };
    static const int qTableSize = 11;
    const auto* qTable = (algorithmMode == 1) ? qTableLinked : qTableRitual;
    float QcarrierBase;
    if (bwPercent <= qTable[0][0])
        QcarrierBase = qTable[0][1];
    else if (bwPercent >= qTable[qTableSize - 1][0])
        QcarrierBase = qTable[qTableSize - 1][1];
    else
    {
        QcarrierBase = qTable[qTableSize - 1][1];
        for (int qi = 0; qi < qTableSize - 1; ++qi)
        {
            if (bwPercent >= qTable[qi][0] && bwPercent <= qTable[qi + 1][0])
            {
                float t = (bwPercent - qTable[qi][0]) / (qTable[qi + 1][0] - qTable[qi][0]);
                QcarrierBase = qTable[qi][1] * (1.0f - t) + qTable[qi + 1][1] * t;
                break;
            }
        }
    }
    QcarrierBase = juce::jmax (0.5f, QcarrierBase);

    // ===== ABLETON CONSTANT-FRACTIONAL-Q (Linked mode) =====
    // Ableton's vocoder Q ~ N / (n_octaves * BW). Our qTable was calibrated only at the
    // reference point (N=40, 20-18kHz = 9.81 oct), so range/band-count did NOT affect Q -
    // narrowing the range left the bands at the same Q (no Ableton-style resonance sharpening).
    // Multiply the qTable Q by a scale that is 1.0 at the reference (preserves Iter-22 match)
    // and rises as the range narrows or band count grows. Ref: Ableton Filter Measurements 2026-04-09.
    if (algorithmMode == 1)
    {
        constexpr float kRefBands   = 40.0f;
        constexpr float kRefOctaves = 9.81f;            // log2(18000/20)
        float loF  = apvts.getRawParameterValue ("lowFreq")->load();
        float hiF  = apvts.getRawParameterValue ("highFreq")->load();
        hiF = juce::jmax (hiF, loF * 1.1f);             // same min-gap guard as band layout
        float nOct = std::log2 (juce::jmax (1.0001f, hiF / juce::jmax (20.0f, loF)));
        nOct = juce::jmax (0.1f, nOct);                 // low floor: let Q keep rising as LO/HI close
        float qScale = ((float) nBands / kRefBands) * (kRefOctaves / nOct);
        QcarrierBase = juce::jlimit (0.5f, 200.0f, QcarrierBase * qScale);
    }

    // Sync globalBandQ so carousel/GUI reads the current knob-derived Q
    globalBandQ = QcarrierBase;

    // V3 CRACKED FORMANT: band index offset for Ableton-matching mode
    // Formant semis range: -24 to +24. Direct mapping: 1 semi ≈ 1/3 band position
    float formantBandOffset = formantSemis / 3.0f;  // 3 semi per band
    float formantFactor = std::pow (2.0f, formantSemis / 12.0f);  // legacy pitch shift

    // CPU OPT: Hoist parameter lookup outside band loop
    auto* midiFollowParam = apvts.getRawParameterValue ("midiFollow");
    bool midiFollowEnabled = midiFollowParam ? (midiFollowParam->load() > 0.5f) : false;

    // ===== BANK KEYTRACK factor =====
    // Rigidly transpose the whole grid by the glided shift (POSITION + keytracked note), applied
    // equally to analysis + carrier so the comb interaction is preserved across the keyboard.
    // Free mode only (scaleType >= 15 = LOG/LIN); scale-locked is left untouched. Shift 0 => 1.0
    // => bit-identical to legacy.
    auto* scaleTypeParam = apvts.getRawParameterValue ("scaleType");
    const bool  bankFreeDist = scaleTypeParam ? ((int) scaleTypeParam->load() >= 15) : false;
    const float bankFactor   = bankFreeDist ? std::pow (2.0f, currentBankShift.load() / 12.0f) : 1.0f;

    // TEMP diagnostic: publish live gate/factor for the on-GUI readout (remove after verification).
    dbgBankFreeMode.store (bankFreeDist);
    dbgBankFactor.store (bankFactor);

    for (int i = 0; i < nBands; ++i)
    {
        auto* b = bands[i];
        if (b == nullptr) continue;  // Safety: skip null band pointers

        float fBase  = b->centerFreqBase;
        float fScale = b->centerFreqScale;

        // ANALYSIS FOLLOWS THE SAME SCALE BLEND AS THE CARRIER.
        // This used to be `fBase * bankFactor` -- the RAW log grid, with the scale blend applied to the
        // carrier only (see fRaw below). At scaleAmount 0 that was harmless, because fRaw collapses to
        // fBase and the two coincide, which is what a vocoder band IS: one frequency, analysed and
        // resynthesised in the same place. But turning the scale up moved ONLY the carrier, so the band
        // that resynthesises A4 was still listening at 417.9 Hz (89 cents low, and not a note at all).
        // With wide bands the mismatch is masked -- each filter is broad enough to catch what is around
        // it. With a narrow WIDTH it is fatal: at WIDTH 0.10 the analysis Q is 40 over 3 cascaded stages,
        // so 89 cents off centre is -37.7 dB. A sine played dead on a scale note produced no envelope at
        // all, and which notes worked was pure luck of where the log grid fell.
        // Scale mode is a band DISTRIBUTION, exactly like LOG or LIN -- it moves the whole band, not half
        // of it. rebuildBands already agreed: it seeds currentFreq with this same blend (see the
        // `baseFreqs * (1-scaleAmt) + scaleFreqs * scaleAmt` line), which the old fAnalysis then
        // overwrote a moment later.
        const float fBlend = fBase * (1.0f - scaleAmt) + fScale * scaleAmt;
        float fAnalysis = juce::jlimit (20.0f, (float) (0.45 * currentSampleRate), fBlend * bankFactor);

        // ===== BAND OFFSET: Remap carrier frequency =====
        // Analysis stays at band i's frequency; carrier gets offset-mapped frequency
        float fCarrierBase, fCarrierScale;

        if (bandOffsetCurve.size() >= 2 && nBands > 1)
        {
            // Normalized position of this band (0..1)
            float normI = (float) i / (float) (nBands - 1);

            // Look up offset curve (interpolated)
            float curveIdx = normI * (float) (bandOffsetCurve.size() - 1);
            int idx0 = (int) curveIdx;
            int idx1 = juce::jmin (idx0 + 1, (int) bandOffsetCurve.size() - 1);
            float frac = curveIdx - (float) idx0;
            float offsetNorm = bandOffsetCurve[(size_t) idx0] * (1.0f - frac)
                             + bandOffsetCurve[(size_t) idx1] * frac;

            // Add global offset (in band units, normalized)
            offsetNorm += globalBandOffset / (float) (nBands - 1);
            offsetNorm = juce::jlimit (0.0f, 1.0f, offsetNorm);

            // Map to fractional carrier band index
            float carrierBandF = offsetNorm * (float) (nBands - 1);

            // SCALE-PROPORTIONAL SNAP: when scaleAmt is high, snap to nearest integer band
            // (preserves scale). When scaleAmt is low, allow fractional interpolation
            // (textural smearing between bands). Uses round-to-nearest when scale is strict.
            int cb0;
            int cb1;
            float cbFrac;
            if (scaleAmt > 0.99f)
            {
                // Pure scale mode: snap to nearest integer band, no interpolation
                cb0 = juce::jlimit (0, nBands - 1, (int) std::round (carrierBandF));
                cb1 = cb0;
                cbFrac = 0.0f;
            }
            else
            {
                cb0 = juce::jlimit (0, nBands - 1, (int) carrierBandF);
                cb1 = juce::jlimit (0, nBands - 1, cb0 + 1);
                // Attenuate fractional portion by (1-scaleAmt): more scale lock = less interpolation
                cbFrac = (carrierBandF - (float) cb0) * (1.0f - scaleAmt);
            }

            fCarrierBase  = bands[cb0]->centerFreqBase  * (1.0f - cbFrac)
                          + bands[cb1]->centerFreqBase  * cbFrac;
            fCarrierScale = bands[cb0]->centerFreqScale * (1.0f - cbFrac)
                          + bands[cb1]->centerFreqScale * cbFrac;
        }
        else
        {
            fCarrierBase  = fBase;
            fCarrierScale = fScale;
        }

        float fRaw     = fCarrierBase * (1.0f - scaleAmt) + fCarrierScale * scaleAmt;
        // FORMANT (scale-aware, Option B — unified across all modes):
        //   Scale-locked: modulator→carrier band remapping (true vowel/formant shift
        //     on scale — complements harmonic shift which moves in scale-note-DIFFERENT units).
        //   Scale-free: frequency multiplication (classic pitch-shift formant).
        //   In between: blends the two approaches.
        float fCarrier;
        if (std::abs(formantBandOffset) > 0.01f)
        {
            // BAND REMAPPING COMPONENT (scale-locked behavior)
            float fRemappedScale = fCarrierScale;  // default to self if out of range
            float srcBand = (float)i + formantBandOffset;
            bool formantOutOfRange = (srcBand < 0.0f || srcBand >= (float)nBands);

            if (!formantOutOfRange)
            {
                int sb0, sb1;
                float sbFrac;
                if (scaleAmt > 0.99f)
                {
                    sb0 = juce::jlimit (0, nBands - 1, (int) std::round (srcBand));
                    sb1 = sb0;
                    sbFrac = 0.0f;
                }
                else
                {
                    sb0 = juce::jlimit (0, nBands - 1, (int) srcBand);
                    sb1 = juce::jlimit (0, nBands - 1, sb0 + 1);
                    sbFrac = (srcBand - (float) sb0) * (1.0f - scaleAmt);
                }
                float fScl0 = bands[sb0]->centerFreqScale;
                float fScl1 = bands[sb1]->centerFreqScale;
                fRemappedScale = fScl0 * (1.0f - sbFrac) + fScl1 * sbFrac;
            }

            // FREQUENCY-MULTIPLICATION COMPONENT (scale-free behavior)
            float fShiftedBase = fCarrierBase * formantFactor;

            // Blend based on scaleAmt: scale-locked → remap, scale-free → multiply
            fCarrier = fShiftedBase * (1.0f - scaleAmt) + fRemappedScale * scaleAmt;
            fCarrier = juce::jlimit (20.0f, (float)(0.45 * currentSampleRate), fCarrier);

            // Only mute in strict Linked scale-locked mode when out of range
            b->formantMute = (formantOutOfRange && algorithmMode == 1 && scaleAmt > 0.99f);
        }
        else
        {
            fCarrier = fRaw;
            b->formantMute = false;
        }

        // (debug logging moved below Q computation)

        if (midiFollowEnabled && !midiNoteFreqs.empty())
        {
            // MIDI Follow mode - snap to held MIDI notes with HYBRID gating
            float fMidi = snapToMidi (fCarrier);
            float midiAmt = juce::jmax (chordAmt, 0.5f);  // At least 50% snap when MIDI Follow is on

            // Calculate distance in semitones for gating
            float distanceSemitones = std::abs (12.0f * std::log2 (fCarrier / fMidi));
            float gateThreshold = 1.0f;
            float gateFalloff = 2.0f;
            float midiGate = 1.0f - juce::jlimit (0.0f, 1.0f, (distanceSemitones - gateThreshold) / gateFalloff);
            b->chordGate = 1.0f * (1.0f - midiAmt) + midiGate * midiAmt;

            if (b->chordGate > 0.01f)
                fCarrier = fCarrier * (1.0f - midiAmt) + fMidi * midiAmt;
        }
        else if (chordAmt > 0.01f && !chordNoteFreqs.empty())
        {
            // HYBRID chord mode: snap nearby bands, fade out distant bands
            // (threshold avoids snapping from tiny automation interpolation values)
            float fChord = snapToChord (fCarrier);

            // Calculate distance in semitones (12 semitones = 1 octave = 2x frequency)
            float distanceSemitones = std::abs (12.0f * std::log2 (fCarrier / fChord));

            // Gate curve:
            // - Within 1 semitone: full volume (gate = 1.0)
            // - 1-3 semitones: fade out
            // - Beyond 3 semitones: silent (gate = 0.0)
            float gateThreshold = 1.0f;   // Start fading at 1 semitone
            float gateFalloff = 2.0f;     // Fully muted at 3 semitones (1 + 2)
            float chordGate = 1.0f - juce::jlimit (0.0f, 1.0f, (distanceSemitones - gateThreshold) / gateFalloff);

            // Apply chordAmt: 0% = no gating (all pass), 100% = full gating
            b->chordGate = 1.0f * (1.0f - chordAmt) + chordGate * chordAmt;

            // Snap frequency (only for bands that will be audible)
            if (b->chordGate > 0.01f)
                fCarrier = fCarrier * (1.0f - chordAmt) + fChord * chordAmt;
        }
        else
        {
            // No chord mode - full volume
            b->chordGate = 1.0f;
        }

        fCarrier *= bankFactor;   // BANK KEYTRACK: rigid transpose (matches fAnalysis above)
        fCarrier = juce::jlimit (20.0f, (float) (0.45 * currentSampleRate), fCarrier);

        // Use per-band Q: locked = global × multiplier, unlocked = global (multiplier stays 1.0)
        float Qcarrier;
        if (perBandQLocked[(size_t) i])
        {
            Qcarrier = QcarrierBase * perBandQMultiplier[(size_t) i];
        }
        else
        {
            Qcarrier = QcarrierBase;
            perBandQMultiplier[(size_t) i] = 1.0f;  // Keep unlocked at ×1.0
        }
        // Frequency-dependent Q: Q(f) = Q_base × cos(π×f/fs)^exp
        // Compensates bilinear transform BW compression at HF.
        // ITER 12 (2026-04-09): Linked mode uses exp=0.65 (gentler than 1.29).
        // Full 1.29 over-widens HF bands; zero leaves Air -4 dB. 0.65 splits.
        {
            int amode = (int) apvts.getRawParameterValue ("algorithmMode")->load();
            float cosExp = (amode == 1) ? 0.65f : 1.29f;
            float w = juce::MathConstants<float>::pi * fCarrier / (float) currentSampleRate;
            float cosW = std::cos (w);
            Qcarrier *= std::pow (juce::jmax (0.01f, cosW), cosExp);
        }

        Qcarrier = juce::jlimit (0.5f, 200.0f, Qcarrier);

        b->bandQ = Qcarrier;

        // ===== ALGORITHM MODE: Calculate modulator (analysis) Q =====
        // RITUAL (0): Fixed Q at 2.0 - original Band Ritual behavior
        // LINKED (1): Same Q for both modulator and carrier (Ableton-style)
        // SPLIT (2): Separate modulator Q control (Vocodex-style)
        float Qanalysis;
        switch (algorithmMode)
        {
            case 0:  Qanalysis = 2.0f; break;           // RITUAL: fixed at 2.0
            case 1:
            {
                // ITER 9 (2026-04-09): Restored March 2026 Qa = 0.60 × Qc.
                // April code drifted to 0.85 × Qc which made analysis bands too narrow.
                float qaRatio = 0.52f - (0.52f - 0.26f) * norm;  // ITER 22: peak cosine 0.9128 at 0.52 (Q=40)
                Qanalysis = Qcarrier * qaRatio;
                break;
            }
            case 2:  Qanalysis = modulatorQParam; break; // SPLIT: user-controlled
            default: Qanalysis = 2.0f; break;
        }

        // MOD BW (2026-06-14): Vocodex-style modulator bandwidth multiplier on the ANALYSIS bank only.
        // Applied in LINKED(1) and RITUAL(0) where Qanalysis is auto-derived. NOT in SPLIT(2) (has modulatorQ).
        // BW ~ 1/Q -> divide Q by modBw. modBw=1.0 (default) is a pure no-op (= Ableton behavior).
        if (algorithmMode == 1 || algorithmMode == 0)
            Qanalysis = juce::jlimit (0.3f, 200.0f, Qanalysis / juce::jmax (0.01f, modBwParam));

        juce::IIRCoefficients bpA = juce::IIRCoefficients::makeBandPass (currentSampleRate, fAnalysis, Qanalysis);
        juce::IIRCoefficients bpC = juce::IIRCoefficients::makeBandPass (currentSampleRate, fCarrier, Qcarrier);

        b->analysisFilter.setCoefficients (bpA);
        b->analysisFilter2.setCoefficients (bpA);
        b->analysisFilter3.setCoefficients (bpA);  // V3: 3-stage analysis
        for (int s = 0; s < 3; ++s)
            b->dAna[s].setBandPass (currentSampleRate, (double) fAnalysis, (double) Qanalysis);

        // ITER 19c (2026-04-10): True Butterworth bandpass via LP→BP frequency transform.
        // Mathematically equivalent to scipy.signal.butter(4, [lo,hi], 'band', output='sos').
        // Verified against scipy: 0.00 dB difference at all test frequencies (1 kHz, 100 Hz, 5 kHz, 50 Hz).
        // Replaces RBJ BPF2 cascade (which had narrow peaked response). Butterworth has
        // flat passband (peak = 0 dB) and -48 dB/oct skirts.
        if (algorithmMode == 1)
        {
            // ITER 19c FINAL: Use qTable Q directly (Q=37 at BW=10%).
            // Tested with multipliers: Q=37 → best real music (A-weighted -0.83 dBA, Mid -1.7).
            // Q=50 better balanced but worse real music. Q=85 fixes noise but breaks tonal.
            // Real music match is the gold standard → Q=37 is the answer.
            float bandwidthHz = juce::jmax (1.0f, fCarrier / juce::jmax (0.5f, Qcarrier));
            float f_lo_d = juce::jmax (1.0f, fCarrier - 0.5f * bandwidthHz);
            float f_hi_d = juce::jmin ((float) (0.49 * currentSampleRate), fCarrier + 0.5f * bandwidthHz);

            // === 4th-order Butterworth bandpass via LP→BP transform ===
            using cd = std::complex<double>;
            const int N = 4;
            const double pi = juce::MathConstants<double>::pi;
            const double fs = currentSampleRate;

            // 1. Butterworth LP prototype poles (analog, fc=1)
            cd lp_poles[N];
            for (int k = 0; k < N; ++k) {
                double theta = pi * (2*k + N + 1) / (2.0 * N);
                lp_poles[k] = std::polar (1.0, theta);
            }

            // 2. Pre-warp frequencies for bilinear transform
            double wa_lo = 2.0 * fs * std::tan (pi * f_lo_d / fs);
            double wa_hi = 2.0 * fs * std::tan (pi * f_hi_d / fs);
            double wa_bw = wa_hi - wa_lo;
            double wa_0 = std::sqrt (wa_lo * wa_hi);

            // 3. LP → BP: each LP pole p becomes 2 BP poles
            //    s² - p*wa_bw*s + wa_0² = 0
            cd bp_poles[2*N];
            for (int k = 0; k < N; ++k) {
                cd half = lp_poles[k] * wa_bw / 2.0;
                cd disc = std::sqrt (half*half - cd(wa_0*wa_0));
                bp_poles[2*k]     = half + disc;
                bp_poles[2*k + 1] = half - disc;
            }

            // 4. Bilinear transform: pole_z = (2fs + s) / (2fs - s)
            cd dig_poles[2*N];
            for (int k = 0; k < 2*N; ++k) {
                dig_poles[k] = (2.0*fs + bp_poles[k]) / (2.0*fs - bp_poles[k]);
            }

            // 5. Group conjugate pairs into 4 biquad sections
            // Each biquad: 1 zero at z=1 + 1 zero at z=-1 → b0=1, b1=0, b2=-1
            // Denominator from conjugate pole pair: 1 + a1*z^-1 + a2*z^-2
            //   a1 = -2*Re(p), a2 = |p|²
            float sec_b0[N] = {1, 1, 1, 1};
            float sec_b1[N] = {0, 0, 0, 0};
            float sec_b2[N] = {-1, -1, -1, -1};
            float sec_a1[N], sec_a2[N];
            bool used[2*N] = {};
            int sec_idx = 0;
            for (int i = 0; i < 2*N && sec_idx < N; ++i) {
                if (used[i]) continue;
                cd p = dig_poles[i];
                used[i] = true;
                // Find conjugate
                int conj_idx = -1;
                for (int j = i+1; j < 2*N; ++j) {
                    if (!used[j] && std::abs (dig_poles[j] - std::conj(p)) < 1e-9) {
                        conj_idx = j; break;
                    }
                }
                if (conj_idx >= 0) {
                    used[conj_idx] = true;
                    sec_a1[sec_idx] = (float)(-2.0 * p.real());
                    sec_a2[sec_idx] = (float)(p.real()*p.real() + p.imag()*p.imag());
                } else {
                    // Real pole — use alone (rare)
                    sec_a1[sec_idx] = (float)(-p.real());
                    sec_a2[sec_idx] = 0.0f;
                }
                sec_idx++;
            }

            // 6. Normalize cascade gain so peak at fCarrier is 0 dB (N sections only)
            cd zf = std::polar (1.0, 2.0*pi*fCarrier/fs);
            cd cascadeResp = 1.0;
            for (int k = 0; k < N; ++k) {
                cd num = (double)sec_b0[k] + (double)sec_b1[k]/zf + (double)sec_b2[k]/(zf*zf);
                cd den = 1.0 + (double)sec_a1[k]/zf + (double)sec_a2[k]/(zf*zf);
                cascadeResp *= num / den;
            }
            double peakMag = std::abs (cascadeResp);
            double perSectionGain = (peakMag > 1e-15) ? std::pow (1.0 / peakMag, 1.0 / N) : 1.0;
            for (int k = 0; k < N; ++k) {
                sec_b0[k] *= (float) perSectionGain;
                sec_b1[k] *= (float) perSectionGain;
                sec_b2[k] *= (float) perSectionGain;
            }

            // 7. Apply to filters (reusing butterLP_L/R[2] and butterHP_L/R[2] = 4 biquads total)
            juce::IIRFilter* filtersL[4] = {
                &b->butterLP_L[0], &b->butterLP_L[1], &b->butterHP_L[0], &b->butterHP_L[1]
            };
            juce::IIRFilter* filtersR[4] = {
                &b->butterLP_R[0], &b->butterLP_R[1], &b->butterHP_R[0], &b->butterHP_R[1]
            };
            for (int k = 0; k < N; ++k) {
                juce::IIRCoefficients coef (sec_b0[k], sec_b1[k], sec_b2[k],
                                            1.0f, sec_a1[k], sec_a2[k]);
                filtersL[k]->setCoefficients (coef);
                filtersR[k]->setCoefficients (coef);
            }
        }

        // Stacked carrier filters (IIR — for Ritual/Split modes)
        b->carrierFilterL .setCoefficients (bpC);
        b->carrierFilterR .setCoefficients (bpC);
        b->carrierFilterL2.setCoefficients (bpC);
        b->carrierFilterR2.setCoefficients (bpC);

        // SVF carrier filters: update frequency and Q only (NO prepare — that resets state!)
        // prepare() is called once in prepareToPlay/band creation, not per-block.
        {
            auto updateSvf = [&] (juce::dsp::StateVariableTPTFilter<float>& svf) {
                svf.setCutoffFrequency (fCarrier);
                svf.setResonance (Qcarrier);
            };
            updateSvf (b->svfCarrierL1); updateSvf (b->svfCarrierR1);
            updateSvf (b->svfCarrierL2); updateSvf (b->svfCarrierR2);
            updateSvf (b->svfCarrierL3); updateSvf (b->svfCarrierR3);
        }
        b->carrierFilterL3.setCoefficients (bpC);
        b->carrierFilterL4.setCoefficients (bpC);
        b->carrierFilterR3.setCoefficients (bpC);
        b->carrierFilterR4.setCoefficients (bpC);
        b->carrierFilterL5.setCoefficients (bpC);
        b->carrierFilterR5.setCoefficients (bpC);

        // Double-precision carrier biquads (active 4-stage path) — same fCarrier/Qcarrier as bpC.
        // CARRIER BW (Linked only): widen the carrier band (lower per-stage Q) to match Ableton's
        // gentler band shape -> shorter ring (tail) + fuller low-mid. carrierBw=1 => unchanged.
        float QcarDcar = (algorithmMode == 1 && carrierBwParam > 1.0001f)
                       ? juce::jlimit (0.3f, 200.0f, Qcarrier / carrierBwParam)
                       : Qcarrier;
        for (int s = 0; s < 4; ++s)
        {
            b->dCarL[s].setBandPass (currentSampleRate, (double) fCarrier, (double) QcarDcar);
            b->dCarR[s].setBandPass (currentSampleRate, (double) fCarrier, (double) QcarDcar);
        }

        // NOTE: tested per-band peak normalization here (carrierNorm) on the theory that the carrier
        // bandpass was unnormalized (peak ∝ Q) — DISPROVEN by measurement: the DBP is already 0 dB
        // peak for every band, so it was a no-op and did NOT cause the low tilt. The low-end diff is
        // Ableton producing LESS sub (its lowest band rolls off), matched by the output shelf+HP below.

        b->currentFreq = fAnalysis;  // Analysis frequency (where envelope is detected)
        b->carrierFreq = fCarrier;   // Carrier frequency (can be pitch-shifted)

        // PERSISTENT DEBUG: write every 1000th block
        static int debugCounter = 0;
        if (i == 0) debugCounter++;
        if (debugCounter == 1000 && (i == 0 || i == 9 || i == 19 || i == 29 || i == nBands-1))
        {
            juce::File df (juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                              .getChildFile ("band_ritual_debug_freqs.txt"));
            juce::String s;
            if (i == 0)
                s += "--- nBands=" + juce::String(nBands)
                   + " scaleAmt=" + juce::String(scaleAmt, 4)
                   + " formant=" + juce::String(formantFactor, 6)
                   + " offset=" + juce::String(globalBandOffset, 4) + "\n";
            s += "  Band " + juce::String(i+1) + ": fBase=" + juce::String(fBase, 2)
               + " fA=" + juce::String(fAnalysis, 2)
               + " fC=" + juce::String(fCarrier, 2)
               + " Qc=" + juce::String(Qcarrier, 2) + "\n";
            df.appendText (s);
            if (i == nBands-1) debugCounter = 0;
        }
        b->analysisQ = Qanalysis;  // Store for envelope compensation in processBlock
        b->cachedPitchClass = frequencyToPitchClass (fAnalysis);  // CPU OPT: cache pitch class

        // ===== V3 CLEAN: No per-band normalization =====
        // Ableton lets each band output its natural level.
        // BW-dependent and band-count gains are applied GLOBALLY (not per-band).
        // Per-band tilt (f^1.0) handles spectral shape only.
        // ITER 19i (2026-04-10): Linked mode Butterworth ENBW correction.
        // True Butterworth BP has a flat passband that integrates more broadband energy
        // per band than ABL's filters (which appear to have a peaky/narrow response).
        // Measured from realmusic_test.py iter 19i: broadband diff +2.1 dB, average
        // per-region (excl Mid and Air which are already matched) ~+2 dB.
        // Applying -1.5 dB global output trim for Linked mode only.
        // ITER 20a (2026-04-10): Increased from -1.0 dB to -3.5 dB.
        // realmusic_test.py iter 19l: broadband +4.8 dB, Sub +4.7, Bass +6.4, Low +4.8.
        // Butterworth flat passband captures ~3-5 dB more broadband energy than
        // ABL's peaked BPF2 cascade. -3.5 dB brings low/mid regions within ±2 dB.
        // ITER 20f: No per-band trim. Broadband level handled by bwGain.
        b->bandNormGain = 1.0f;
    }

    // ===== V3 CRACKED: Per-band gain ∝ f (linear) =====
    // Measured from isolated single-band recordings (BW=10%, 4 bands):
    //   Band 1 (38 Hz):    -56.2 dBFS
    //   Band 2 (318 Hz):   -35.0 dBFS
    //   Band 3 (2369 Hz):  -16.8 dBFS
    //   Band 4 (17200 Hz):  -2.8 dBFS
    // Best fit: gain ∝ f^1.01 = +6.1 dB/oct per band.
    // This is the ACTUAL per-band gain law from Ableton's vocoder.
    if (algorithmMode == 1)
    {
        // ===== LINKED MODE: Per-band correction LUT (measured from EXPORT 1) =====
        // Calibrated against Ableton Vocoder using sub-rich input.
        // Smoothed and clamped to ±5dB to prevent ringing/artifacts.
        // Index = band position (0..39 for 40 bands logspace 20-18000Hz).
        // PHASE A1 DIAGNOSTIC: LUT bypassed (all zeros) to measure baseline zigzag
        static constexpr float linkedBandCorrectionDb[40] = {
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
        };

        for (int i = 0; i < nBands; ++i)
        {
            // Map band index to LUT (handles nBands != 40 by interpolation)
            float pos = (nBands > 1) ? (float) i / (float) (nBands - 1) * 39.0f : 0.0f;
            int idx0 = juce::jlimit (0, 39, (int) pos);
            int idx1 = juce::jlimit (0, 39, idx0 + 1);
            float frac = pos - (float) idx0;
            float corrDb = linkedBandCorrectionDb[idx0] * (1.0f - frac)
                         + linkedBandCorrectionDb[idx1] * frac;
            bands[i]->tiltCorr = std::pow (10.0f, corrDb / 20.0f);

            // tiltCorr = 1.0 — no per-band correction needed.
            // f^1.0 gain is inherent in constant-Q filter design (BW ∝ fc).

            bands[i]->overlapComp = 1.0f;
            bands[i]->bwComp = 1.0f;
        }

        // Feed band config to STFT vocoder (for STFT Linked mode path)
        if (useSTFTLinked)
        {
            std::vector<STFTVocoder::BandInfo> stftBands (nBands);
            for (int i = 0; i < nBands; ++i)
            {
                auto& sb = stftBands[i];
                sb.centerFreq = bands[i]->currentFreq;
                // Band edges at geometric midpoints to neighbors
                float ratio = (nBands > 1)
                    ? std::pow (bands[nBands-1]->currentFreq / bands[0]->currentFreq,
                                1.0f / (float)(nBands - 1))
                    : 2.0f;
                sb.lowFreq  = sb.centerFreq / std::sqrt (ratio);
                sb.highFreq = sb.centerFreq * std::sqrt (ratio);
                sb.gain     = 1.0f;  // STFT has its own internal tilt calibration
                sb.analysisQ = bands[i]->analysisQ;
                sb.chordGate = bands[i]->chordGate;
                sb.isExcluded = false;
                sb.isSecondary = false;
            }
            stftVocoder.updateBands (stftBands.data(), nBands);
            stftVocoder.setEnvelopeParams (attackCoeff, releaseCoeff);
        }
    }
    else
    {
      {
        // Tilt v8: power-law tilt + LF shelf boost.
        // The power law handles overall slope. The LF shelf boosts bands below ~200 Hz
        // to match Ableton's vocoder which has more LF energy at mid BWs (70-130%).
        constexpr float fRef = 1000.0f;
        float tiltAlpha = -0.003f * norm - 0.090f;
        // LF shelf: boost below fKnee, amount depends on BW (strongest at mid BWs)
        constexpr float fKnee = 250.0f;
        float lfBoostDb = 3.0f * (1.0f - std::abs (norm - 0.35f) * 2.5f);  // peaks at norm≈0.35 (BW≈77%)
        lfBoostDb = juce::jmax (0.0f, lfBoostDb);  // never cut
        for (int i = 0; i < nBands; ++i)
        {
            float fi = juce::jmax (40.0f, bands[i]->currentFreq);
            float tilt = std::pow (fi / fRef, tiltAlpha);
            // Smooth LF shelf: full boost below fKnee/2, tapers to 0 at fKnee
            float lfGain = 1.0f;
            if (fi < fKnee && lfBoostDb > 0.0f)
            {
                float t = fi / fKnee;  // 0..1
                float shelfDb = lfBoostDb * (1.0f - t * t);  // quadratic taper
                lfGain = std::pow (10.0f, shelfDb / 20.0f);
            }
            bands[i]->tiltCorr = tilt * lfGain;
        }
    }

    // ===== NOISE SPECTRAL SHAPE + BW COMPRESSION CORRECTION =====
    // Two separate effects are corrected:
    // 1) Spectral shape: IIR carrier BW ∝ f → mid-frequency hump in noise response.
    //    Fixed with asymmetric parabolic correction (normalized to avg=1.0).
    // 2) Bilinear transform BW compression: actual BW = designed × sin(2πf/fs)/(2πf/fs).
    //    At 12kHz/Q=60: actual BW is 58% of designed → HF envelope deficit.
    //    Fixed with sinc correction (NOT normalized — absolute HF boost).
    {
        constexpr float kShapeCenter    = 5000.0f;
        constexpr float kShapeAlphaLow  = 0.022f;
        constexpr float kShapeAlphaHigh = 0.045f;
        constexpr float kBwCompPower    = 0.80f;  // Compensates for combined analysis+carrier BW compression
        float compSum = 0.0f;

        for (int i = 0; i < nBands; ++i)
        {
            float fi = bands[i]->currentFreq;
            float Qi = bands[i]->analysisQ;

            // --- Parabolic spectral shape correction ---
            float logDist = std::log2 (fi / kShapeCenter);
            float qScale = std::pow (juce::jmax (1.0f, Qi / 6.0f), 0.65f);
            // Q-dependent alpha for low side: at high Q (narrow BW), the IIR
            // filter bank produces stronger low-frequency deficit vs Ableton.
            // Scale alphaLow by +30% at Q≥27 (gentle correction).
            float qAlphaScale = juce::jlimit (0.0f, 1.0f, (Qi - 10.0f) / 17.0f);
            float alphaLow = kShapeAlphaLow * (1.0f + 0.3f * qAlphaScale);
            float alpha = (logDist > 0.0f) ? kShapeAlphaHigh : alphaLow;
            float parabolic = 1.0f + alpha * qScale * logDist * logDist;
            compSum += parabolic;

            // --- Bilinear transform BW compression correction ---
            // BW_actual/BW_designed = sin(2πf/fs) / (2πf/fs)
            // Only significant above ~7 kHz; applying below that worsens upper-mid.
            float bwCorr = 1.0f;
            if (fi > 7000.0f)
            {
                float w = 2.0f * juce::MathConstants<float>::pi * fi / (float) currentSampleRate;
                float sinc = (w > 0.001f) ? (std::sin (w) / w) : 1.0f;
                bwCorr = std::pow (1.0f / juce::jmax (0.01f, sinc), kBwCompPower);
            }

            // Store parabolic in overlapComp (will be normalized), bwCorr separately
            bands[i]->overlapComp = parabolic;
            bands[i]->bwComp = bwCorr;  // NOT normalized — absolute HF correction
        }

        // Normalize ONLY the parabolic part (shape correction, no level shift)
        float avgComp = (nBands > 0) ? (compSum / (float) nBands) : 1.0f;
        if (avgComp > 0.01f)
        {
            for (int i = 0; i < nBands; ++i)
            {
                bands[i]->overlapComp /= avgComp;
                // Combine: normalized parabolic × unnormalized sinc
                bands[i]->overlapComp *= bands[i]->bwComp;
            }
        }
    }
    } // end else (non-Linked tilt/overlap corrections)

    // ===== MERGE DETECTION =====
    // Group bands within 0.5 semitones of each other to prevent volume stacking
    // when Scale Amount or Chord Amount causes bands to converge to the same frequency.
    // One "primary" band processes the filter; others are marked as secondary.
    // Output is normalized by group size to maintain consistent volume.

    // Reset merge state
    for (int i = 0; i < nBands; ++i)
    {
        auto* b = bands[i];
        b->mergeGroupId = -1;
        b->mergeGroupSize = 1;
        b->isMergedSecondary = false;
    }

    // Group bands within 0.5 semitones
    int currentGroupId = 0;
    std::vector<bool> assigned ((size_t) nBands, false);

    for (int i = 0; i < nBands; ++i)
    {
        if (assigned[(size_t) i])
            continue;

        auto* bandI = bands[i];

        // Start new group with band i as primary
        bandI->mergeGroupId = currentGroupId;
        bandI->isMergedSecondary = false;
        assigned[(size_t) i] = true;
        int groupSize = 1;

        // Find all bands within 0.5 semitones of band i
        for (int j = i + 1; j < nBands; ++j)
        {
            if (assigned[(size_t) j])
                continue;

            auto* bandJ = bands[j];

            // Calculate semitone distance using carrier (scale-snapped) frequency
            float freqRatio = bandJ->carrierFreq / bandI->carrierFreq;
            if (freqRatio < 0.001f)
                freqRatio = 0.001f; // Prevent log(0)

            float semitoneDistance = std::abs (12.0f * std::log2 (freqRatio));

            if (semitoneDistance < 0.5f)
            {
                bandJ->mergeGroupId = currentGroupId;
                bandJ->isMergedSecondary = true;
                assigned[(size_t) j] = true;
                groupSize++;
            }
        }

        bandI->mergeGroupSize = groupSize;
        currentGroupId++;
    }
}

//==============================================================================
// Noise filters (legacy - no longer used, kept for compatibility)
//==============================================================================

void ColorVocoderAudioProcessor::updateNoiseFilters()
{
    // Noise filtering now handled by Noise Color parameter (pink/white blend)
    // No per-frequency noise shaping - cleaner approach

    // Reset the legacy filters
    noiseFilterL.reset();
    noiseFilterR.reset();
}

//==============================================================================
// BAND SNAPSHOT FOR GUI
//==============================================================================

void ColorVocoderAudioProcessor::getBandSnapshot (std::vector<float>& freqs,
                                                  std::vector<float>& envs,
                                                  float& lowHz,
                                                  float& highHz,
                                                  std::vector<int>* outPrimaryIndices)
{
    // NOTE: freqs, envs, and outPrimaryIndices are NOT cleared before the lock.
    // On lock failure, the previous cached values are preserved so the display
    // remains stable and drag handlers still map to correct band indices.

    // Try to acquire lock - if audio thread is rebuilding bands, skip this frame
    const juce::SpinLock::ScopedTryLockType lock (bandsLock);
    if (! lock.isLocked())
        return;

    // Lock acquired — safe to clear and rebuild all output data
    freqs.clear();
    envs.clear();
    if (outPrimaryIndices)
        outPrimaryIndices->clear();

    // Get current parameter values (for display even without audio)
    // Guard against null pointers during early init
    auto* rawBandCount = apvts.getRawParameterValue ("bandCount");
    auto* rawLowFreq   = apvts.getRawParameterValue ("lowFreq");
    auto* rawHighFreq  = apvts.getRawParameterValue ("highFreq");
    if (rawBandCount == nullptr || rawLowFreq == nullptr || rawHighFreq == nullptr)
        return;

    auto paramBandCount = (int) rawBandCount->load();
    auto paramLowFreq   = rawLowFreq->load();
    auto paramHighFreq  = rawHighFreq->load();

    lowHz  = paramLowFreq;
    highHz = paramHighFreq;

    // RANGE GUARD for the preview LAYOUT math (lowHz/highHz above stay RAW so the GUI
    // drag-handles sit where the user put them). Mirrors the guard in rebuildBands().
    paramHighFreq = juce::jmax (paramHighFreq, paramLowFreq * 1.1f);

    // If bands array matches expected count, use actual band data
    if ((int) bands.size() == paramBandCount && paramBandCount > 0)
    {
        // Check if additive/STACK mode is ON
        auto* rawAdditiveMode = apvts.getRawParameterValue ("additiveMode");
        bool additiveMode = rawAdditiveMode ? rawAdditiveMode->load() > 0.5f : false;

        // ALWAYS show unique frequency positions only (primary bands)
        // When STACK is ON, combine envelopes from all bands in the same merge group
        // This way: 89 bands stacked at 20 scale notes = 20 visual bars with combined envelopes

        // First pass: count unique positions and build envelope sums per merge group
        std::map<int, float> groupEnvSums;  // mergeGroupId -> sum of envelopes

        for (auto* b : bands)
        {
            if (b == nullptr) continue;
            if (additiveMode)
            {
                // STACK ON: sum envelopes for all bands in same group
                groupEnvSums[b->mergeGroupId] += b->envDisplay;  // gated -> stacked display reflects gate
            }
            // STACK OFF: only primary bands matter (secondaries skipped in processing)
        }

        // Get scale amount for computing display frequency
        // (currentFreq is the analysis base freq, NOT scale-snapped)
        auto* rawScaleAmt = apvts.getRawParameterValue ("scaleAmount");
        float scaleAmt = rawScaleAmt ? rawScaleAmt->load() : 0.0f;

        freqs.reserve (bands.size());
        envs.reserve (bands.size());

        int bandArrayIdx = 0;
        for (auto* b : bands)
        {
            if (b == nullptr) { bandArrayIdx++; continue; }
            // Only output PRIMARY bands (unique frequency positions)
            if (b->isMergedSecondary) { bandArrayIdx++; continue; }

            // Use carrierFreq which already includes scale blend + formant shift + chord snap
            // Fallback to raw blend if carrierFreq hasn't been set yet (before first audio block)
            float displayFreq = (b->carrierFreq > 0.0f) ? b->carrierFreq
                : (b->centerFreqBase * (1.0f - scaleAmt) + b->centerFreqScale * scaleAmt);
            freqs.push_back (displayFreq);

            if (additiveMode)
            {
                // STACK ON: use combined envelope from all stacked bands
                envs.push_back (groupEnvSums[b->mergeGroupId]);
            }
            else
            {
                // STACK OFF: just this band's envelope
                envs.push_back (b->envDisplay);  // gated envelope so the display reflects the gate
            }

            if (outPrimaryIndices)
                outPrimaryIndices->push_back (bandArrayIdx);
            bandArrayIdx++;
        }
    }
    else if (paramBandCount > 0)
    {
        // Bands array doesn't match expected count (e.g., no audio yet)
        // Calculate expected band positions from parameters for display
        // INCLUDING scale AND chord snapping so bands visually move when parameters change!
        freqs.reserve ((size_t) paramBandCount);
        envs.reserve ((size_t) paramBandCount);

        // Get formant shift for visual preview
        auto* rawFormantShift = apvts.getRawParameterValue ("formantShift");
        float formantSemis = rawFormantShift ? rawFormantShift->load() : 0.0f;
        float formantFactor = std::pow (2.0f, formantSemis / 12.0f);

        // Get scale parameters for visual preview
        auto* keyPtr = apvts.getRawParameterValue ("key");
        auto* scaleTypePtr = apvts.getRawParameterValue ("scaleType");
        auto* scaleAmtPtr = apvts.getRawParameterValue ("scaleAmount");

        // Get chord parameters for visual preview
        auto* chordDegreePtr = apvts.getRawParameterValue ("chordDegree");
        auto* chordVariationPtr = apvts.getRawParameterValue ("chordVariation");
        auto* chordAmtPtr = apvts.getRawParameterValue ("chordAmount");

        int key = keyPtr ? (int) keyPtr->load() : 0;
        int scaleType = scaleTypePtr ? (int) scaleTypePtr->load() : 0;
        float scaleAmt = scaleAmtPtr ? scaleAmtPtr->load() : 0.0f;

        int chordDegree = chordDegreePtr ? (int) chordDegreePtr->load() : 0;
        int chordVariation = chordVariationPtr ? (int) chordVariationPtr->load() : 0;
        float chordAmt = chordAmtPtr ? chordAmtPtr->load() : 0.0f;

        // Distribution mode (mirror rebuildBands): 15=LOG, 16=LIN -> free (no scale/chord snap).
        const bool freeDistP = (scaleType >= 15);
        const bool linDistP  = (scaleType == 16);

        // Build scale notes for snapping (same as rebuildBands) — skipped in free mode
        std::vector<float> previewScaleNotes;
        if (! freeDistP)
            buildScaleNoteFrequencies (previewScaleNotes, key, scaleType, paramLowFreq, paramHighFreq);

        // Build chord notes for snapping (skipped in free mode)
        std::vector<float> previewChordNotes;
        if (freeDistP) { /* no chord snap in LOG/LIN */ }
        else if (chordVariation == 9)  // Custom
        {
            int letterKey = lastTriggeredLetterKey.load();
            juce::SpinLock::ScopedLockType lock (customChordLock);
            auto it = customChords.find (letterKey);
            if (it != customChords.end() && !it->second.empty())
            {
                std::set<int> pcs;
                for (int n : it->second) pcs.insert (n % 12);
                for (int m = 0; m <= 150; ++m)
                    if (pcs.count (m % 12))
                    {
                        float f = (float) getMicrotunedFrequency (m);
                        if (f >= paramLowFreq && f <= paramHighFreq)
                            previewChordNotes.push_back (f);
                    }
            }
            else
                buildChordFrequencies (previewChordNotes, key, scaleType, chordDegree, 0, paramLowFreq, paramHighFreq);
        }
        else
            buildChordFrequencies (previewChordNotes, key, scaleType, chordDegree, chordVariation, paramLowFreq, paramHighFreq);

        float ratio = paramHighFreq / juce::jmax (paramLowFreq, 20.0f);

        for (int i = 0; i < paramBandCount; ++i)
        {
            float t = (paramBandCount > 1 ? (float) i / (float) (paramBandCount - 1) : 0.0f);
            float baseFreq = linDistP ? (paramLowFreq + (paramHighFreq - paramLowFreq) * t)
                                      : (paramLowFreq * std::pow (ratio, t));

            // Apply scale snapping for visual preview (same logic as rebuildBands)
            float displayFreq = baseFreq;
            if (scaleAmt > 0.0f && !previewScaleNotes.empty())
            {
                // Find closest scale note
                float closest = previewScaleNotes[0];
                float minDist = std::abs (std::log (baseFreq) - std::log (closest));
                for (size_t n = 1; n < previewScaleNotes.size(); ++n)
                {
                    float d = std::abs (std::log (baseFreq) - std::log (previewScaleNotes[n]));
                    if (d < minDist)
                    {
                        minDist = d;
                        closest = previewScaleNotes[n];
                    }
                }
                // Blend between base and scale-snapped frequency
                displayFreq = baseFreq * std::pow (closest / baseFreq, scaleAmt);
            }

            // Apply chord snapping on top of scale snapping (HYBRID mode logic)
            if (chordAmt > 0.0f && !previewChordNotes.empty())
            {
                // Find closest chord note
                float closestChord = previewChordNotes[0];
                float minChordDist = std::abs (std::log (displayFreq) - std::log (closestChord));
                for (size_t c = 1; c < previewChordNotes.size(); ++c)
                {
                    float d = std::abs (std::log (displayFreq) - std::log (previewChordNotes[c]));
                    if (d < minChordDist)
                    {
                        minChordDist = d;
                        closestChord = previewChordNotes[c];
                    }
                }
                // Blend towards chord frequency
                displayFreq = displayFreq * std::pow (closestChord / displayFreq, chordAmt);
            }

            // Apply formant shift (same as updateBandFilters: fCarrier = fRaw * formantFactor)
            displayFreq *= formantFactor;

            freqs.push_back (displayFreq);
            envs.push_back (0.0f);  // No bars when no audio (gain cap lines still visible)

            if (outPrimaryIndices)
                outPrimaryIndices->push_back (i);
        }
    }
}

//==============================================================================
void ColorVocoderAudioProcessor::getBandMergeGroupSizes (std::vector<int>& sizes)
{
    sizes.clear();

    // Only report group sizes when STACK mode is active
    bool additiveMode = apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;
    if (! additiveMode)
        return;

    const juce::SpinLock::ScopedTryLockType lock (bandsLock);
    if (! lock.isLocked()) return;
    for (auto* b : bands)
    {
        if (b == nullptr) continue;
        if (b->isMergedSecondary)
            continue;
        sizes.push_back (b->mergeGroupSize);
    }
}

//==============================================================================
int ColorVocoderAudioProcessor::getUniqueBandCount() const
{
    // Always return unique frequency count (for display)
    // Stacked mode affects processing, not display
    int count = 0;
    for (auto* b : bands)
    {
        if (!b->isMergedSecondary)
            count++;
    }
    return count;
}

int ColorVocoderAudioProcessor::calculateMaxUsefulBands() const
{
    // In additive mode, ALL bands are useful (they stack, no deduplication)
    bool additiveMode = apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;
    if (additiveMode)
        return maxBands;

    // Get current parameters
    auto lowFreq  = apvts.getRawParameterValue ("lowFreq")->load();
    auto highFreq = apvts.getRawParameterValue ("highFreq")->load();
    auto key      = (int) apvts.getRawParameterValue ("key")->load();
    auto scaleType = (int) apvts.getRawParameterValue ("scaleType")->load();
    auto scaleAmt  = apvts.getRawParameterValue ("scaleAmount")->load();
    auto harmonicShift = apvts.getRawParameterValue ("harmonicShift")->load();

    // Chord + formant parameters (must match updateBandFilters logic)
    auto* chordDegreePtr    = apvts.getRawParameterValue ("chordDegree");
    auto* chordVariationPtr = apvts.getRawParameterValue ("chordVariation");
    auto* chordAmtPtr       = apvts.getRawParameterValue ("chordAmount");
    auto* formantPtr        = apvts.getRawParameterValue ("formant");

    int   chordDegree    = chordDegreePtr    ? (int) chordDegreePtr->load()    : 0;
    int   chordVariation = chordVariationPtr ? (int) chordVariationPtr->load() : 0;
    float chordAmt       = chordAmtPtr       ? chordAmtPtr->load()             : 0.0f;
    float formantSemis   = formantPtr        ? formantPtr->load()              : 0.0f;
    float formantFactor  = std::pow (2.0f, formantSemis / 12.0f);

    // If scale amount is 0 and chord amount is 0, all 100 bands are useful (no snapping)
    if (scaleAmt < 0.01f && chordAmt < 0.01f)
        return maxBands;

    // Build scale notes for this range (temporary copy)
    std::vector<float> testScaleNotes;
    buildScaleNoteFrequencies (testScaleNotes, key, scaleType, lowFreq, highFreq);
    const int nNotes = (int) testScaleNotes.size();

    if (nNotes == 0 && scaleAmt > 0.01f)
        return maxBands;

    // Build chord notes for snapping (same as updateBandFilters uses)
    std::vector<float> testChordNotes;
    if (chordAmt > 0.01f)
    {
        if (chordVariation == 9)  // Custom — fall back to triad for max-band calculation
            buildChordFrequencies (testChordNotes, key, scaleType, chordDegree, 0, lowFreq, highFreq);
        else
            buildChordFrequencies (testChordNotes, key, scaleType, chordDegree, chordVariation, lowFreq, highFreq);
    }

    // Simulate 100 bands and calculate their frequencies
    const int testBandCount = maxBands;
    std::vector<float> testFreqs ((size_t) testBandCount);

    float ratio = highFreq / juce::jmax (lowFreq, 20.0f);

    for (int i = 0; i < testBandCount; ++i)
    {
        float t = (testBandCount > 1 ? (float) i / (float) (testBandCount - 1) : 0.0f);
        float baseFreq = lowFreq * std::pow (ratio, t);

        // Calculate scale-snapped frequency (same logic as rebuildBands)
        float bandPos = t;
        float fOut = baseFreq;

        if (nNotes > 0)
        {
            int baseNoteIndex = 0;
            if (nNotes > 1)
                baseNoteIndex = (int) std::round (bandPos * (float) (nNotes - 1));
            baseNoteIndex = juce::jlimit (0, nNotes - 1, baseNoteIndex);

            float shiftShape = std::pow (bandPos, 1.5f);
            float effShiftF  = (float) harmonicShift * shiftShape;
            int   effShift   = (int) std::round (effShiftF);

            int idx = baseNoteIndex + effShift;
            while (idx < 0)       idx += nNotes;
            while (idx >= nNotes) idx -= nNotes;

            fOut = testScaleNotes[(size_t) idx];
        }

        // Interpolate based on scale amount (same as rebuildBands)
        float fRaw = baseFreq + scaleAmt * (fOut - baseFreq);

        // Apply formant shift (same as updateBandFilters)
        float fCarrier = fRaw * formantFactor;

        // Apply chord snapping (same as updateBandFilters)
        if (chordAmt > 0.01f && !testChordNotes.empty())
        {
            // Find closest chord note
            float bestF = testChordNotes[0];
            float bestD = std::abs (std::log (fCarrier) - std::log (bestF));
            for (size_t c = 1; c < testChordNotes.size(); ++c)
            {
                float d = std::abs (std::log (fCarrier) - std::log (testChordNotes[c]));
                if (d < bestD)
                {
                    bestD = d;
                    bestF = testChordNotes[c];
                }
            }
            // Blend toward chord tone (same as updateBandFilters line 1573)
            fCarrier = fCarrier * (1.0f - chordAmt) + bestF * chordAmt;
        }

        testFreqs[(size_t) i] = fCarrier;
    }

    // Count unique frequencies using 0.5 semitone merge threshold
    std::vector<bool> merged ((size_t) testBandCount, false);
    int uniqueCount = 0;

    for (int i = 0; i < testBandCount; ++i)
    {
        if (merged[(size_t) i])
            continue;

        uniqueCount++;

        // Mark all bands within 0.5 semitones as merged
        for (int j = i + 1; j < testBandCount; ++j)
        {
            if (merged[(size_t) j])
                continue;

            float freqRatio = testFreqs[(size_t) j] / testFreqs[(size_t) i];
            if (freqRatio < 0.001f)
                freqRatio = 0.001f;

            float semitoneDistance = std::abs (12.0f * std::log2 (freqRatio));

            if (semitoneDistance < 0.5f)
                merged[(size_t) j] = true;
        }
    }

    return uniqueCount;
}

// PER-BAND GAIN CONTROL
//==============================================================================

float ColorVocoderAudioProcessor::getBandGain (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < maxBands)
        return bandGains[(size_t) bandIndex];
    return 1.0f;
}

void ColorVocoderAudioProcessor::setBandGain (int bandIndex, float gain)
{
    if (bandIndex >= 0 && bandIndex < maxBands)
        bandGains[(size_t) bandIndex] = juce::jlimit (0.0f, 1.0f, gain);
}

void ColorVocoderAudioProcessor::resetAllBandGains()
{
    bandGains.fill (1.0f);
}

//==============================================================================
// PITCH CLASS NOTE EXCLUSION
//==============================================================================

int ColorVocoderAudioProcessor::frequencyToPitchClass (float freq) const
{
    if (freq <= 0.0f)
        return 0;

    // Convert frequency to MIDI note number
    // MIDI note = 12 * log2(freq/440) + 69
    float midiNote = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
    int roundedNote = (int) std::round (midiNote);

    // Get pitch class (0-11) - always positive via modulo
    int pitchClass = ((roundedNote % 12) + 12) % 12;
    return pitchClass;
}

void ColorVocoderAudioProcessor::setNoteExclusion (int pitchClass, bool excluded)
{
    if (pitchClass >= 0 && pitchClass < 12)
        noteExclusions[(size_t) pitchClass] = excluded;
}

bool ColorVocoderAudioProcessor::getNoteExclusion (int pitchClass) const
{
    if (pitchClass >= 0 && pitchClass < 12)
        return noteExclusions[(size_t) pitchClass];
    return false;
}

void ColorVocoderAudioProcessor::toggleNoteExclusion (int pitchClass)
{
    if (pitchClass >= 0 && pitchClass < 12)
        noteExclusions[(size_t) pitchClass] = !noteExclusions[(size_t) pitchClass];
}

void ColorVocoderAudioProcessor::resetAllNoteExclusions()
{
    noteExclusions.fill (false);
}

bool ColorVocoderAudioProcessor::isMidiBandControlActive() const
{
    auto* param = apvts.getRawParameterValue ("midiBandControl");
    return param ? (param->load() > 0.5f) : false;
}

std::array<bool, 12> ColorVocoderAudioProcessor::getScaleNoteFlags() const
{
    std::array<bool, 12> flags = { false };

    auto* keyParam = apvts.getRawParameterValue ("key");
    auto* scaleTypeParam = apvts.getRawParameterValue ("scaleType");

    if (!keyParam || !scaleTypeParam)
        return flags;

    int key = (int) keyParam->load();
    int scaleType = (int) scaleTypeParam->load();

    // Get scale intervals (must match buildScaleNoteFrequencies)
    std::vector<int> scaleIntervals;
    switch (scaleType)
    {
        case 0:  scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Major (Ionian)
        case 1:  scaleIntervals = { 0,2,3,5,7,8,10 };       break; // Minor (Aeolian)
        case 2:  scaleIntervals = { 0,2,3,5,7,9,10 };       break; // Dorian
        case 3:  scaleIntervals = { 0,1,3,5,7,8,10 };       break; // Phrygian
        case 4:  scaleIntervals = { 0,2,4,6,7,9,11 };       break; // Lydian
        case 5:  scaleIntervals = { 0,2,4,5,7,9,10 };       break; // Mixolydian
        case 6:  scaleIntervals = { 0,1,3,5,6,8,10 };       break; // Locrian
        case 7:  scaleIntervals = { 0,2,3,5,7,9,11 };       break; // Melodic Minor
        case 8:  scaleIntervals = { 0,2,3,5,7,8,11 };       break; // Harmonic Minor
        case 9:  scaleIntervals = { 0,2,4,5,7,8,11 };       break; // Harmonic Major
        case 10: scaleIntervals = { 0,1,4,5,7,8,11 };       break; // Double Harmonic
        case 11: scaleIntervals = { 0,2,4,6,8,10 };         break; // Whole Tone
        case 12: scaleIntervals = { 0,2,3,5,6,8,9,11 };     break; // Diminished (HW)
        case 13: scaleIntervals = { 0,1,3,4,6,7,9,10 };     break; // Diminished (WH)
        case 14: scaleIntervals = { 0,3,5,6,7,10 };         break; // Blues
        case 15: scaleIntervals = { 0,2,4,7,9 };            break; // Major Pentatonic
        case 16: scaleIntervals = { 0,3,5,7,10 };           break; // Minor Pentatonic
        case 17: scaleIntervals = { 0,1,2,3,4,5,6,7,8,9,10,11 }; break; // Chromatic
        default: scaleIntervals = { 0,2,4,5,7,9,11 };       break; // Default to Major
    }

    // Mark which pitch classes are in scale
    for (int interval : scaleIntervals)
    {
        int pitchClass = (key + interval) % 12;
        flags[(size_t) pitchClass] = true;
    }

    return flags;
}

std::array<bool, 12> ColorVocoderAudioProcessor::getChordNoteFlags() const
{
    std::array<bool, 12> flags = { false };

    auto* keyParam = apvts.getRawParameterValue ("key");
    auto* scaleTypeParam = apvts.getRawParameterValue ("scaleType");
    auto* chordDegreeParam = apvts.getRawParameterValue ("chordDegree");
    auto* chordVariationParam = apvts.getRawParameterValue ("chordVariation");

    if (!keyParam || !scaleTypeParam || !chordDegreeParam || !chordVariationParam)
        return flags;

    int key = (int) keyParam->load();
    int scaleType = (int) scaleTypeParam->load();
    int chordDegree = (int) chordDegreeParam->load();
    int chordVariation = (int) chordVariationParam->load();

    // Get scale intervals
    std::vector<int> scaleIntervals;
    switch (scaleType)
    {
        case 0:  scaleIntervals = { 0,2,4,5,7,9,11 };       break;
        case 1:  scaleIntervals = { 0,2,3,5,7,8,10 };       break;
        case 2:  scaleIntervals = { 0,2,3,5,7,9,10 };       break;
        case 3:  scaleIntervals = { 0,1,3,5,7,8,10 };       break;
        case 4:  scaleIntervals = { 0,2,4,6,7,9,11 };       break;
        case 5:  scaleIntervals = { 0,2,4,5,7,9,10 };       break;
        case 6:  scaleIntervals = { 0,1,3,5,6,8,10 };       break;
        case 7:  scaleIntervals = { 0,2,3,5,7,9,11 };       break;
        case 8:  scaleIntervals = { 0,2,3,5,7,8,11 };       break;
        case 9:  scaleIntervals = { 0,2,4,5,7,8,11 };       break;
        case 10: scaleIntervals = { 0,1,4,5,7,8,11 };       break;
        case 11: scaleIntervals = { 0,2,4,6,8,10 };         break;
        case 12: scaleIntervals = { 0,2,3,5,6,8,9,11 };     break;
        case 13: scaleIntervals = { 0,1,3,4,6,7,9,10 };     break;
        case 14: scaleIntervals = { 0,3,5,6,7,10 };         break;
        case 15: scaleIntervals = { 0,2,4,7,9 };            break;
        case 16: scaleIntervals = { 0,3,5,7,10 };           break;
        case 17: scaleIntervals = { 0,1,2,3,4,5,6,7,8,9,10,11 }; break;
        default: scaleIntervals = { 0,2,4,5,7,9,11 };       break;
    }

    if (chordVariation == 9)  // Custom variation
    {
        int letterKey = lastTriggeredLetterKey.load();
        juce::SpinLock::ScopedLockType lock (customChordLock);
        auto it = customChords.find (letterKey);
        if (it != customChords.end())
            for (int note : it->second)
                flags[(size_t) (note % 12)] = true;
        return flags;
    }

    if (chordDegree < 0 || chordDegree >= (int) scaleIntervals.size())
        return flags;

    // Get chord intervals based on degree and variation
    std::vector<int> chordIntervals = getChordIntervals (chordDegree, chordVariation, scaleType);

    // Convert to absolute pitch classes
    int rootOffset = scaleIntervals[(size_t) chordDegree];
    for (int interval : chordIntervals)
    {
        int pitchClass = (key + rootOffset + interval) % 12;
        flags[(size_t) pitchClass] = true;
    }

    return flags;
}

//==============================================================================
// BAND DISTRIBUTION CURVE
//==============================================================================

void ColorVocoderAudioProcessor::setBandDistribution (const std::vector<float>& curve)
{
    if (curve.size() >= 2)
    {
        bandDistributionCurve = curve;
        // Signal audio thread to rebuild bands (thread-safe)
        // This prevents crashes from GUI thread calling rebuildBands() while audio thread reads bands
        bandDistributionDirty.store (true);
    }
}

void ColorVocoderAudioProcessor::resetBandDistribution()
{
    bandDistributionCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bandDistributionCurve[(size_t) i] = (float) i / 100.0f;
}

//==============================================================================
// BAND OFFSET CURVE
//==============================================================================

void ColorVocoderAudioProcessor::setBandOffsetCurve (const std::vector<float>& curve)
{
    if (curve.size() >= 2)
    {
        bandOffsetCurve = curve;
        // Signal audio thread to rebuild bands (thread-safe)
        bandDistributionDirty.store (true);
    }
}

void ColorVocoderAudioProcessor::resetBandOffsetCurve()
{
    bandOffsetCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bandOffsetCurve[(size_t) i] = (float) i / 100.0f;  // Linear 1:1 mapping
}

//==============================================================================
// BANK GLIDE CURVE
//==============================================================================

void ColorVocoderAudioProcessor::setBankGlideCurve (const std::vector<float>& curve)
{
    // Size stays fixed at 101, so operator= copies element-wise without reallocating -
    // the audio thread's evalGlideCurve() keeps valid pointers (same accepted model as the
    // band curves above). No band rebuild needed: the curve only shapes the live glide.
    if (curve.size() >= 2)
        bankGlideCurve = curve;
}

void ColorVocoderAudioProcessor::resetBankGlideCurve()
{
    bankGlideCurve.resize (101);
    for (int i = 0; i < 101; ++i)
        bankGlideCurve[(size_t) i] = (float) i / 100.0f;  // Linear diagonal = plain linear glide
}

float ColorVocoderAudioProcessor::evalGlideCurve (float t) const
{
    const int n = (int) bankGlideCurve.size();
    if (n < 2)
        return juce::jlimit (0.0f, 1.0f, t);   // safety: identity if uninitialised

    t = juce::jlimit (0.0f, 1.0f, t);
    const float fpos = t * (float) (n - 1);
    const int   i0   = juce::jlimit (0, n - 1, (int) fpos);
    const int   i1   = juce::jmin (n - 1, i0 + 1);
    const float frac = fpos - (float) i0;
    return bankGlideCurve[(size_t) i0] * (1.0f - frac) + bankGlideCurve[(size_t) i1] * frac;
}

void ColorVocoderAudioProcessor::setGlobalBandOffset (float offset)
{
    globalBandOffset = juce::jlimit (-12.0f, 12.0f, offset);
    bandDistributionDirty.store (true);
}

//==============================================================================
// PER-BAND Q CONTROL
//==============================================================================

float ColorVocoderAudioProcessor::getPerBandQ (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < (int) perBandQMultiplier.size())
        return juce::jlimit (1.0f, 60.0f, globalBandQ * perBandQMultiplier[(size_t) bandIndex]);
    return globalBandQ;
}

float ColorVocoderAudioProcessor::getPerBandQMultiplier (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < (int) perBandQMultiplier.size())
        return perBandQMultiplier[(size_t) bandIndex];
    return 1.0f;
}

void ColorVocoderAudioProcessor::setPerBandQ (int bandIndex, float q)
{
    if (bandIndex >= 0 && bandIndex < (int) perBandQMultiplier.size())
    {
        float clampedQ = juce::jlimit (1.0f, 60.0f, q);
        // Store as multiplier relative to current globalBandQ
        if (globalBandQ > 0.01f)
            perBandQMultiplier[(size_t) bandIndex] = clampedQ / globalBandQ;
        else
            perBandQMultiplier[(size_t) bandIndex] = 1.0f;
        // No need to set bandDistributionDirty — updateBandFilters() runs
        // every audio block and picks up the multiplier change automatically
    }
}

bool ColorVocoderAudioProcessor::isPerBandQLocked (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < (int) perBandQLocked.size())
        return perBandQLocked[(size_t) bandIndex];
    return false;
}

void ColorVocoderAudioProcessor::setPerBandQLocked (int bandIndex, bool locked)
{
    if (bandIndex >= 0 && bandIndex < (int) perBandQLocked.size())
    {
        perBandQLocked[(size_t) bandIndex] = locked;
        // On unlock, reset multiplier to 1.0
        if (! locked)
            perBandQMultiplier[(size_t) bandIndex] = 1.0f;
    }
}

void ColorVocoderAudioProcessor::setGlobalBandQ (float q)
{
    globalBandQ = juce::jlimit (1.0f, 60.0f, q);
    // Multipliers auto-apply — no need to update individual bands
    bandDistributionDirty.store (true);
}

void ColorVocoderAudioProcessor::resetAllPerBandQLocks()
{
    perBandQLocked.fill (false);
    perBandQMultiplier.fill (1.0f);
    bandDistributionDirty.store (true);
}

void ColorVocoderAudioProcessor::getSpectrumData (std::array<float, fftSize / 2>& spectrum) const
{
    // Thread-safe read: if new data is available, copy from read buffer
    // The atomic flag ensures we don't read while audio thread is writing
    if (spectrumDataReady.load (std::memory_order_acquire))
    {
        // Safe to read from spectrumReadBuffer - audio thread writes to spectrumWriteBuffer
        spectrum = spectrumReadBuffer;
    }
    else
    {
        // Return last known good data (or zeros if never written)
        spectrum = spectrumReadBuffer;
    }
}

void ColorVocoderAudioProcessor::getDualSpectrumData (std::array<float, fftSize / 2>& wetSpectrum,
                                                       std::array<float, fftSize / 2>& drySpectrum) const
{
    // Thread-safe read of dual spectrum data for wet/dry visualization
    if (spectrumDataReady.load (std::memory_order_acquire))
    {
        wetSpectrum = spectrumWetBuffer;
        drySpectrum = spectrumDryBuffer;
    }
    else
    {
        wetSpectrum = spectrumWetBuffer;
        drySpectrum = spectrumDryBuffer;
    }
}

//==============================================================================
// REBUILD / UPDATE BANDS
//==============================================================================

void ColorVocoderAudioProcessor::updateBandsIfNeeded()
{
    // Safe parameter access with null checks
    auto* bandCountPtr = apvts.getRawParameterValue ("bandCount");
    auto* lowFreqPtr = apvts.getRawParameterValue ("lowFreq");
    auto* highFreqPtr = apvts.getRawParameterValue ("highFreq");
    auto* keyPtr = apvts.getRawParameterValue ("key");
    auto* scaleTypePtr = apvts.getRawParameterValue ("scaleType");
    auto* scaleAmtPtr = apvts.getRawParameterValue ("scaleAmount");
    auto* bandWidthPtr = apvts.getRawParameterValue ("bandWidth");
    auto* harmonicShiftPtr = apvts.getRawParameterValue ("harmonicShift");
    auto* chordDegreePtr = apvts.getRawParameterValue ("chordDegree");
    auto* chordVariationPtr = apvts.getRawParameterValue ("chordVariation");

    // Return early if any critical parameters are missing
    if (bandCountPtr == nullptr || lowFreqPtr == nullptr || highFreqPtr == nullptr ||
        keyPtr == nullptr || scaleTypePtr == nullptr || scaleAmtPtr == nullptr ||
        bandWidthPtr == nullptr || harmonicShiftPtr == nullptr ||
        chordDegreePtr == nullptr || chordVariationPtr == nullptr)
        return;

    auto bandCount     = (int) bandCountPtr->load();  // Direct band count (1-100)
    auto lowFreq       = lowFreqPtr->load();
    auto highFreq      = highFreqPtr->load();
    auto key           = (int) keyPtr->load();
    auto scaleType     = (int) scaleTypePtr->load();
    auto scaleAmt      = scaleAmtPtr->load();
    auto bandWidth     = bandWidthPtr->load();
    auto harmonicShift = harmonicShiftPtr->load();

    // ===== CHORD PARAMETERS =====
    auto chordDegree    = (int) chordDegreePtr->load();
    auto chordVariation = (int) chordVariationPtr->load();

    bool needRebuild =
        bandCount != lastBandCount  ||  // Compare band count
        lowFreq       != lastLowFreq    ||
        highFreq      != lastHighFreq   ||
        key           != lastKey        ||
        scaleType     != lastScaleType  ||
        harmonicShift != lastHarmShift;

    // ===== CHORD FREQUENCY REBUILD =====
    // Rebuild chord frequencies when chord params or key/scale/range changes
    bool needChordRebuild =
        chordDegree    != lastChordDegree    ||
        chordVariation != lastChordVariation ||
        key            != lastKey            ||
        scaleType      != lastScaleType      ||
        lowFreq        != lastLowFreq        ||
        highFreq       != lastHighFreq;

    if (needChordRebuild)
    {
        if (chordVariation == 9)  // Custom variation
        {
            // Use custom chord for the last triggered letter key
            int letterKey = lastTriggeredLetterKey.load();
            juce::SpinLock::ScopedLockType lock (customChordLock);
            auto it = customChords.find (letterKey);
            if (it != customChords.end() && !it->second.empty())
            {
                // Convert custom MIDI notes to pitch classes, then expand across frequency range
                std::set<int> pitchClasses;
                for (int note : it->second)
                    pitchClasses.insert (note % 12);

                chordNoteFreqs.clear();
                for (int midiNote = 0; midiNote <= 150; ++midiNote)
                {
                    if (pitchClasses.count (midiNote % 12) > 0)
                    {
                        float freq = (float) getMicrotunedFrequency (midiNote);
                        if (freq >= lowFreq && freq <= highFreq)
                            chordNoteFreqs.push_back (freq);
                    }
                }
            }
            else
            {
                // No custom chord defined — fall back to triad
                buildChordFrequencies (chordNoteFreqs, key, scaleType, chordDegree, 0 /*Triad*/, lowFreq, highFreq);
            }
        }
        else
        {
            buildChordFrequencies (chordNoteFreqs, key, scaleType, chordDegree, chordVariation, lowFreq, highFreq);
        }
        lastChordDegree = chordDegree;
        lastChordVariation = chordVariation;
    }

    if (needRebuild)
    {
        // Reset per-band Q locks when scale/key/band structure changes
        // (scale is usually set once per song, so locks become meaningless)
        bool scaleChanged = key != lastKey || scaleType != lastScaleType || bandCount != lastBandCount;
        if (scaleChanged)
            resetAllPerBandQLocks();
        // Reset per-band GAINS only when the band COUNT changes (band indices change). Changing
        // key/scale keeps the user's drawn gains. Combined with the cache-sync in setStateInformation,
        // this stops the per-band gains being wiped to MAX when the editor/window is reopened.
        if (bandCount != lastBandCount)
            resetAllBandGains();
        rebuildBands();
    }
    else
    {
        lastScaleAmt   = scaleAmt;
        lastBandWidth  = bandWidth;
        updateBandFilters();
        // CPU OPT: Removed updateNoiseFilters() — legacy no-op that reset unused filters every block
    }

    updateEnvelopeCoeffs();
}

//==============================================================================
// PROCESSBLOCK – smoother envelopes + per-band noise emphasis
//==============================================================================

void ColorVocoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // ===== DEFERRED BAND DISTRIBUTION REBUILD (Thread-Safe) =====
    // GUI thread sets dirty flag, audio thread consumes it here safely
    if (bandDistributionDirty.exchange (false))
    {
        rebuildBands();
    }

    // ===== MIDI BAND CONTROL: Use MIDI notes D3-C#4 to control pitch class exclusions =====
    auto* midiBandControlParam = apvts.getRawParameterValue ("midiBandControl");
    bool midiBandControlEnabled = midiBandControlParam ? (midiBandControlParam->load() > 0.5f) : false;

    if (midiBandControlEnabled)
    {
        // On activation: save user's manual exclusions, then exclude ALL pitch classes
        if (!lastMidiBandControlState)
        {
            savedNoteExclusions = noteExclusions;
            midiBandControlHeld.fill (false);
            noteExclusions.fill (true);  // All muted until MIDI notes arrive
            dbgMidiNoteCount.store (0);
            dbgLastMidiNote.store (-1);
            dbgLastMidiChannel.store (-1);
        }

        // DEBUG: Accept ALL channels and ALL note ranges to diagnose routing
        // Will be narrowed down once we confirm MIDI is arriving
        juce::MidiBuffer filteredMidi;
        for (const auto& metadata : midi)
        {
            auto msg = metadata.getMessage();

            if (msg.isNoteOn() || msg.isNoteOff())
            {
                int note = msg.getNoteNumber();
                int ch = msg.getChannel();

                // Log every note for debug display
                if (msg.isNoteOn())
                {
                    dbgLastMidiNote.store (note);
                    dbgLastMidiChannel.store (ch);
                    dbgMidiNoteCount.fetch_add (1);
                }

                // Map: use note modulo 12 as pitch class (any octave works for now)
                int pitchClass = note % 12;

                if (msg.isNoteOn())
                {
                    midiBandControlHeld[(size_t) pitchClass] = true;
                    noteExclusions[(size_t) pitchClass] = false;  // Un-exclude → bands play
                }
                else
                {
                    midiBandControlHeld[(size_t) pitchClass] = false;
                    noteExclusions[(size_t) pitchClass] = true;   // Exclude → bands mute
                }

                continue;  // Consume this note
            }

            // Pass through all other MIDI events
            filteredMidi.addEvent (msg, metadata.samplePosition);
        }
        midi.swapWith (filteredMidi);
    }
    else if (lastMidiBandControlState)
    {
        // Mode just deactivated: restore user's saved exclusions
        noteExclusions = savedNoteExclusions;
        midiBandControlHeld.fill (false);
    }
    lastMidiBandControlState = midiBandControlEnabled;

    // ===== MIDI CHORD TRIGGER: Consume trigger notes before MIDI Follow =====
    auto* midiChordTriggerParam = apvts.getRawParameterValue ("midiChordTrigger");
    bool midiChordTriggerEnabled = midiChordTriggerParam ? (midiChordTriggerParam->load() > 0.5f) : false;

    if (midiChordTriggerEnabled)
    {
        // White key lookup: maps MIDI note within octave to chord index (0-6), -1 = black key
        static constexpr int whiteKeyMap[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

        juce::MidiBuffer filteredMidi;

        for (const auto& metadata : midi)
        {
            auto msg = metadata.getMessage();

            if (msg.isNoteOn())
            {
                int note = msg.getNoteNumber();

                // Octave 2 white keys (MIDI 36-47) = chord degree
                if (note >= 36 && note <= 47)
                {
                    int idx = whiteKeyMap[note - 36];
                    if (idx >= 0)
                    {
                        // Save current variation for the OLD degree
                        if (auto* curDegreeParam = apvts.getRawParameterValue ("chordDegree"))
                        {
                            int oldDegree = (int) curDegreeParam->load();
                            if (auto* curVarParam = apvts.getRawParameterValue ("chordVariation"))
                                lastVariationPerDegree[(size_t) oldDegree] = (int) curVarParam->load();
                        }

                        // Set new chord degree
                        if (auto* param = apvts.getParameter ("chordDegree"))
                            param->setValueNotifyingHost ((float) idx / 6.0f);

                        // Restore remembered variation for this degree
                        if (auto* param = apvts.getParameter ("chordVariation"))
                            param->setValueNotifyingHost ((float) lastVariationPerDegree[(size_t) idx] / 9.0f);

                        // Auto-set chordAmount to 100% on first press
                        if (midiChordFirstPress)
                        {
                            if (auto* param = apvts.getParameter ("chordAmount"))
                                param->setValueNotifyingHost (1.0f);
                            midiChordFirstPress = false;
                        }

                        continue;  // consume this note
                    }
                }
                // Octave 3 white keys (MIDI 48-59) = variation (first 7 of 10)
                else if (note >= 48 && note <= 59)
                {
                    int idx = whiteKeyMap[note - 48];
                    if (idx >= 0)
                    {
                        // Set variation (indices 0-6 via MIDI, 7-9 via UI only)
                        if (auto* param = apvts.getParameter ("chordVariation"))
                            param->setValueNotifyingHost ((float) idx / 9.0f);

                        // Update memory for current degree
                        if (auto* curDegreeParam = apvts.getRawParameterValue ("chordDegree"))
                            lastVariationPerDegree[(size_t) (int) curDegreeParam->load()] = idx;

                        continue;  // consume this note
                    }
                }
            }
            // Also consume note-off for trigger octaves (so MIDI Follow doesn't see ghost offs)
            else if (msg.isNoteOff())
            {
                int note = msg.getNoteNumber();
                if ((note >= 36 && note <= 47 && whiteKeyMap[note - 36] >= 0) ||
                    (note >= 48 && note <= 59 && whiteKeyMap[note - 48] >= 0))
                    continue;  // consume matching note-off
            }

            // Pass through everything else
            filteredMidi.addEvent (msg, metadata.samplePosition);
        }

        // Replace original buffer with filtered one
        midi.swapWith (filteredMidi);
    }
    else
    {
        // Reset first-press flag when disabled
        midiChordFirstPress = true;
    }

    // ===== MIDI LEARN: Capture chord from MIDI input =====
    if (midiLearnActive.load())
    {
        std::vector<int> learnedNotes;
        for (const auto& metadata : midi)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
                learnedNotes.push_back (msg.getNoteNumber());
        }

        if (!learnedNotes.empty())
        {
            int targetKey = midiLearnTargetKey.load();
            if (targetKey > 0)
            {
                juce::SpinLock::ScopedLockType lock (customChordLock);
                customChords[targetKey] = learnedNotes;
            }
            midiLearnActive.store (false);
        }
    }

    // ===== MIDI FOLLOW MODE: Process MIDI messages =====
    // Track held notes to generate frequencies for band snapping
    auto* midiFollowParam = apvts.getRawParameterValue ("midiFollow");
    bool midiFollowEnabled = midiFollowParam ? (midiFollowParam->load() > 0.5f) : false;

    if (midiFollowEnabled)
    {
        for (const auto& metadata : midi)
        {
            auto msg = metadata.getMessage();

            if (msg.isNoteOn())
            {
                heldMidiNotes.insert (msg.getNoteNumber());
            }
            else if (msg.isNoteOff())
            {
                heldMidiNotes.erase (msg.getNoteNumber());
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                heldMidiNotes.clear();
            }
        }

        // Convert held MIDI notes to frequencies (with microtuning)
        midiNoteFreqs.clear();
        for (int noteNum : heldMidiNotes)
        {
            float freq = getMicrotunedFrequency (noteNum);
            midiNoteFreqs.push_back (freq);
        }
    }
    else
    {
        // MIDI Follow disabled - clear held notes
        if (!heldMidiNotes.empty())
        {
            heldMidiNotes.clear();
            midiNoteFreqs.clear();
        }
    }

    // ===== BANK KEYTRACK / GLIDE =====
    // Transpose the whole free-mode band grid by a glided semitone shift. POSITION is a manual
    // bank transpose (works alone); when KEYTRACK is on the most-recently-played MIDI note ADDS
    // to POSITION (last-note priority). The shift glides through an editable easing curve. The
    // actual factor is applied per-band in updateBandFilters(), gated on free mode.
    {
        auto* ktParam    = apvts.getRawParameterValue ("bankKeytrack");
        auto* posParam   = apvts.getRawParameterValue ("bankPosition");
        auto* glideParam = apvts.getRawParameterValue ("bankGlide");
        const bool  ktOn     = ktParam    ? (ktParam->load()  > 0.5f) : false;
        const float posSemis = posParam   ? posParam->load()   : 0.0f;
        const float glideMs  = glideParam ? glideParam->load() : 0.0f;

        // Last-note recency (heldMidiNotes is pitch-ordered, no recency). keytrackNote is
        // RETAINED after release (hold: the bank stays put until the next note).
        if (ktOn)
        {
            auto removeNote = [this] (int n)
            {
                for (size_t k = 0; k < keytrackHeldStack.size(); ++k)
                    if (keytrackHeldStack[k] == n)
                    {
                        keytrackHeldStack.erase (keytrackHeldStack.begin() + (long) k);
                        break;
                    }
            };

            for (const auto& metadata : midi)
            {
                auto msg = metadata.getMessage();
                if (msg.isNoteOn())
                {
                    const int n = msg.getNoteNumber();
                    removeNote (n);
                    keytrackHeldStack.push_back (n);
                    keytrackNote = n;
                }
                else if (msg.isNoteOff())
                {
                    removeNote (msg.getNoteNumber());
                    if (! keytrackHeldStack.empty())
                        keytrackNote = keytrackHeldStack.back();   // fall back to prior held note
                    // else: retain keytrackNote (hold)
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    keytrackHeldStack.clear();   // retain keytrackNote (hold)
                }
            }
        }

        // Destination shift in semitones.
        const float target = ktOn ? (posSemis + (float) (keytrackNote - 60)) : posSemis;

        // Retarget the glide when the destination moves.
        if (std::abs (target - glideTargetShift) > 1.0e-6f)
        {
            glideStartShift  = currentBankShift.load();
            glideTargetShift = target;
            glideProgress    = 0.0f;
        }

        // Advance linear time progress, then shape it through the editable glide curve.
        const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
        const float  glideSamples = (float) (glideMs * 0.001 * sr);
        if (glideSamples < 1.0f)
            glideProgress = 1.0f;   // 0 ms -> instant snap
        else
            glideProgress = juce::jmin (1.0f,
                                        glideProgress + (float) buffer.getNumSamples() / glideSamples);

        const float shaped = evalGlideCurve (glideProgress);
        currentBankShift.store (glideStartShift + (glideTargetShift - glideStartShift) * shaped);
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // ====== LICENSE / TRIAL BLOCK ======
    if (! licenseManager.canProduceAudio())
    {
        // Trial expired and no license - mute completely
        buffer.clear();
        return;
    }
    // =====================================

    // ====== SAFETY: Ensure prepareToPlay was called ======
    // Ableton's plugin scanner may call processBlock before prepareToPlay!
    // If filters aren't initialized, processing will crash with NaN/garbage.
    if (currentSampleRate <= 0.0 || currentBlockSize <= 0)
    {
        buffer.clear();
        return;
    }
    // =====================================================

    // ===== INPUT GAIN =====
    // Only apply to main bus channels (not sidechain)
    {
        const int mainChCount = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
        auto* inputGainPtr = apvts.getRawParameterValue ("inputGain");
        if (inputGainPtr != nullptr)
        {
            float gainDb = inputGainPtr->load();
            if (std::abs (gainDb) > 0.01f)
            {
                float linearGain = juce::Decibels::decibelsToGain (gainDb);
                for (int ch = 0; ch < mainChCount; ++ch)
                    buffer.applyGain (ch, 0, buffer.getNumSamples(), linearGain);
            }
        }
        // Compute input RMS
        float rms = 0.0f;
        for (int ch = 0; ch < mainChCount; ++ch)
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
        rms /= juce::jmax (1, mainChCount);
        inputRmsLevel.store (rms);
    }

    // Safe parameter access - check bypass first
    auto* bypassPtr = apvts.getRawParameterValue ("bypass");
    if ((bypassPtr != nullptr && bypassPtr->load() > 0.5f) || bands.isEmpty())
        return;

    // ===== FORMANT SMOOTHING (prevents crackling during automation) =====
    if (auto* ptr = apvts.getRawParameterValue ("formant"))
        smoothedFormant.setTargetValue (ptr->load());
    smoothedFormant.skip (buffer.getNumSamples());

    updateBandsIfNeeded();

    // ===== INPUT FILTER: Read parameters and update =====
    auto* inputHPFreqPtr = apvts.getRawParameterValue ("inputHPFreq");
    auto* inputHPSlopePtr = apvts.getRawParameterValue ("inputHPSlope");
    auto* inputLPFreqPtr = apvts.getRawParameterValue ("inputLPFreq");
    auto* inputLPSlopePtr = apvts.getRawParameterValue ("inputLPSlope");

    // Return early if critical filter parameters are missing
    if (inputHPFreqPtr == nullptr || inputHPSlopePtr == nullptr ||
        inputLPFreqPtr == nullptr || inputLPSlopePtr == nullptr)
        return;

    auto inputHPFreq  = inputHPFreqPtr->load();
    auto inputHPSlope = (int) inputHPSlopePtr->load();
    auto* hpQPtr = apvts.getRawParameterValue ("inputHPQ");
    auto inputHPQ     = hpQPtr ? hpQPtr->load() : 0.707f;
    auto inputLPFreq  = inputLPFreqPtr->load();
    auto inputLPSlope = (int) inputLPSlopePtr->load();
    auto* lpQPtr = apvts.getRawParameterValue ("inputLPQ");
    auto inputLPQ     = lpQPtr ? lpQPtr->load() : 0.0f;  // dB boost

    // Get Q width parameters (for peak width control)
    auto* hpQWidthPtr = apvts.getRawParameterValue ("inputHPQWidth");
    auto* lpQWidthPtr = apvts.getRawParameterValue ("inputLPQWidth");
    auto inputHPQWidth = hpQWidthPtr ? hpQWidthPtr->load() : 1.0f;
    auto inputLPQWidth = lpQWidthPtr ? lpQWidthPtr->load() : 1.0f;

    // Get input filter enabled state
    auto* inputFilterEnabledPtr = apvts.getRawParameterValue ("inputFilterEnabled");
    bool inputFilterEnabled = inputFilterEnabledPtr ? (inputFilterEnabledPtr->load() > 0.5f) : true;

    // Convert slope index to dB/oct: 0=6, 1=12, 2=24, 3=48, 4=72, 5=96
    static const int slopeValues[] = { 6, 12, 24, 48, 72, 96 };
    inputFilter.setHPFrequency (inputHPFreq);
    inputFilter.setHPSlope (slopeValues[juce::jlimit (0, 5, inputHPSlope)]);
    inputFilter.setHPResonance (inputHPQ, inputHPQWidth);
    inputFilter.setLPFrequency (inputLPFreq);
    inputFilter.setLPSlope (slopeValues[juce::jlimit (0, 5, inputLPSlope)]);
    inputFilter.setLPResonance (inputLPQ, inputLPQWidth);

    // Same settings for dry filter (separate state so wet doesn't affect dry)
    dryFilter.setHPFrequency (inputHPFreq);
    dryFilter.setHPSlope (slopeValues[juce::jlimit (0, 5, inputHPSlope)]);
    dryFilter.setHPResonance (inputHPQ, inputHPQWidth);
    dryFilter.setLPFrequency (inputLPFreq);
    dryFilter.setLPSlope (slopeValues[juce::jlimit (0, 5, inputLPSlope)]);
    dryFilter.setLPResonance (inputLPQ, inputLPQWidth);

    // Safe parameter access with null checks and defaults
    auto* mixPtr = apvts.getRawParameterValue ("mix");
    auto* compPtr = apvts.getRawParameterValue ("filterComplement");
    auto* depthPtr = apvts.getRawParameterValue ("depth");
    auto* noiseColorPtr = apvts.getRawParameterValue ("noiseColor");
    auto* hpBlendPtr = apvts.getRawParameterValue ("hpBlend");
    auto* hpBlendSlopePtr = apvts.getRawParameterValue ("hpBlendSlope");
    auto* carrierModePtr = apvts.getRawParameterValue ("carrierMode");
    auto* carrierPitchPtr = apvts.getRawParameterValue ("carrierPitch");
    auto* unvoicedPtr = apvts.getRawParameterValue ("unvoiced");
    auto* noiseModePtr = apvts.getRawParameterValue ("noiseMode");
    auto* gatePtr = apvts.getRawParameterValue ("gate");
    auto* gateRangePtr = apvts.getRawParameterValue ("gateRange");

    auto mix = mixPtr ? mixPtr->load() : 1.0f;
    auto* modLevelPtr = apvts.getRawParameterValue ("modLevel");
    auto modLevel = modLevelPtr ? modLevelPtr->load() : 1.0f;  // 0-2 (0-200%)
    auto complementMode = compPtr ? (compPtr->load() > 0.5f) : false;

    // ===== COMPLEMENT MODE EQ MEMORY SWAP =====
    // When toggling complement mode, swap ALL EQ points to saved values for that mode
    if (complementMode != lastComplementModeState)
    {
        // Helper lambda to swap a parameter
        auto swapParam = [&](const char* paramId, float& savedNormal, float& savedComplement)
        {
            auto* param = apvts.getParameter (paramId);
            auto* rawPtr = apvts.getRawParameterValue (paramId);
            if (param != nullptr && rawPtr != nullptr)
            {
                float current = rawPtr->load();
                if (complementMode)
                {
                    savedNormal = current;
                    param->setValueNotifyingHost (param->convertTo0to1 (savedComplement));
                }
                else
                {
                    savedComplement = current;
                    param->setValueNotifyingHost (param->convertTo0to1 (savedNormal));
                }
            }
        };

        // Swap HP filter
        swapParam ("inputHPFreq", savedHPFreqNormal, savedHPFreqComplement);

        // Swap LP filter
        swapParam ("inputLPFreq", savedLPFreqNormal, savedLPFreqComplement);

        // Swap Bell 1
        swapParam ("bell1Freq", savedBell1FreqNormal, savedBell1FreqComplement);
        swapParam ("bell1Gain", savedBell1GainNormal, savedBell1GainComplement);

        // Swap Bell 2
        swapParam ("bell2Freq", savedBell2FreqNormal, savedBell2FreqComplement);
        swapParam ("bell2Gain", savedBell2GainNormal, savedBell2GainComplement);

        lastComplementModeState = complementMode;
    }

    auto depth = depthPtr ? depthPtr->load() : 0.0f;
    auto noiseColor = noiseColorPtr ? noiseColorPtr->load() : 0.0f;
    auto hpBlend = hpBlendPtr ? hpBlendPtr->load() : 0.0f;
    auto hpBlendSlope = hpBlendSlopePtr ? (int) hpBlendSlopePtr->load() : 0;

    // ===== PHASE 1: Get carrier/unvoiced parameters =====
    auto carrierModeInt = carrierModePtr ? (int) carrierModePtr->load() : 0;
    auto carrierPitch = carrierPitchPtr ? carrierPitchPtr->load() : 220.0f;
    auto unvoicedLevel = unvoicedPtr ? unvoicedPtr->load() : 0.0f;
    auto noiseModeInt = noiseModePtr ? (int) noiseModePtr->load() : 0;  // 0=Auto, 1=Vocal, 2=Synth

    // ===== Gate parameters =====
    auto gateAmount = gatePtr ? gatePtr->load() : 0.0f;
    auto gateRangeDb = gateRangePtr ? gateRangePtr->load() : 40.0f;

    // ===== Spectral X/Y pad parameters (SMOOTHED to prevent zipper noise) =====
    // Set target values from parameters, smoothing handles the interpolation
    if (auto* ptr = apvts.getRawParameterValue ("spectralTilt"))
        smoothedSpectralTilt.setTargetValue (ptr->load());
    if (auto* ptr = apvts.getRawParameterValue ("spectralIntensity"))
        smoothedSpectralIntensity.setTargetValue (ptr->load());

    // Advance smoothing and get current smoothed values for this block
    smoothedSpectralTilt.skip (buffer.getNumSamples());
    smoothedSpectralIntensity.skip (buffer.getNumSamples());
    float spectralTilt = smoothedSpectralTilt.getCurrentValue();
    float spectralIntensity = smoothedSpectralIntensity.getCurrentValue();

    // Pre-calculate gate threshold (for per-band spectral gating)
    // CRACKED from Ableton (2026-03-24): gate knob is now in dB (-60 to +12).
    // Ableton's actual cutoff ≈ knob_value - 18 dB.
    // At -60 dB (default): no gating. At 0 dB: cutoff at -18 dB. At +12 dB: cutoff at -6 dB.
    // Gate knob is in dB (-60 to +12). Per-band envelope is ~16 dB below broadband
    // input because each of 40 bands gets 1/40th of energy (10*log10(1/40) = -16).
    // Ableton's gate compares against per-band envelope with this offset baked in.
    // Sweep verified: offset=-17 makes our gate=+1 match Ableton gate=0 (within 0.3s).
    float gateThresholdDb = gateAmount - 27.0f;  // V3 CRACKED: offset -27, hard mute gate

    // CPU OPTIMIZATION: Pre-calculate linear gate values ONCE per block
    // Avoids expensive log() calls per sample per band (51,200 calls saved!)
    // When gate is at -60 (off), set threshold impossibly low so nothing triggers
    float effectiveGateDb = (gateAmount <= -59.0f) ? -200.0f : gateThresholdDb;
    float gateThresholdLinear = juce::Decibels::decibelsToGain (effectiveGateDb);
    // Soft gate: attenuate by gateRange dB when closed (smoother than hard cut)
    float gateGainLinear = juce::jmax (0.001f, juce::Decibels::decibelsToGain (-gateRangeDb));

    // ===== PER-BAND DYNAMIC THRESHOLD (Pro-Q 3-style downward compressor) =====
    // When a band's envelope exceeds the threshold line, attenuate proportionally
    // (4:1 ratio, 6 dB soft knee). Line at top of display = off (no band exceeds 0 dB).
    // Drag line down into the bars → bars above line get compressed → stronger effect.
    auto* dynThreshParam = apvts.getRawParameterValue ("dynThreshold");
    float dynThresholdDb = dynThreshParam ? dynThreshParam->load() : 0.0f;
    bool  isDynEnabled   = (dynThresholdDb < -0.1f);
    static constexpr float kDynRatioCoef = 0.75f;  // (ratio - 1) / ratio  for 4:1
    static constexpr float kDynKneeDb    = 6.0f;
    static constexpr float kDynHalfKneeDb = kDynKneeDb * 0.5f;
    static constexpr float kDynMaxReduceDb = 40.0f;

    const int numSamples = buffer.getNumSamples();
    // CRITICAL: Use main output bus channels only — buffer may include sidechain channels
    // which would cause out-of-bounds access on 2-channel internal buffers
    const int numCh      = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());

    // Safety check - if buffer is empty, nothing to do
    if (numSamples == 0 || numCh == 0)
        return;

    // ===== COMPLEMENT BUFFERS (copied BEFORE Bell EQ = raw original) =====
    auto& complement = complementBuf_;
    auto& bellComplement = bellComplementBuf_;

    if (complementMode)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            complement.copyFrom (ch, 0, buffer, ch, 0, numSamples);
            bellComplement.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }
    }

    // ===== INPUT EQ (Bell1/Bell2) =====
    // Applied to input BEFORE wet/dry split - affects what vocoder "hears" AND dry signal
    // This is the master tonal shaping EQ
    {
        auto* bell1EnabledPtr = apvts.getRawParameterValue ("bell1Enabled");
        auto* bell2EnabledPtr = apvts.getRawParameterValue ("bell2Enabled");

        bool bell1Enabled = bell1EnabledPtr ? (bell1EnabledPtr->load() > 0.5f) : false;
        bool bell2Enabled = bell2EnabledPtr ? (bell2EnabledPtr->load() > 0.5f) : false;

        if (bell1Enabled)
        {
            auto* freqPtr = apvts.getRawParameterValue ("bell1Freq");
            auto* gainPtr = apvts.getRawParameterValue ("bell1Gain");
            auto* qPtr = apvts.getRawParameterValue ("bell1Q");

            if (freqPtr && gainPtr && qPtr && currentSampleRate > 0)
            {
                float bell1Freq = juce::jlimit (20.0f, 20000.0f, freqPtr->load());
                float bell1Gain = gainPtr->load();
                float bell1Q = juce::jlimit (0.1f, 10.0f, qPtr->load());

                float gainLinear = juce::Decibels::decibelsToGain (bell1Gain);
                bell1Coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, bell1Freq, bell1Q, gainLinear);

                if (bell1Coeffs != nullptr)
                {
                    bell1FilterL.coefficients = bell1Coeffs;
                    bell1FilterR.coefficients = bell1Coeffs;

                    // Apply to input buffer
                    for (int n = 0; n < numSamples; ++n)
                    {
                        buffer.setSample (0, n, bell1FilterL.processSample (buffer.getSample (0, n)));
                        if (numCh > 1)
                            buffer.setSample (1, n, bell1FilterR.processSample (buffer.getSample (1, n)));
                    }
                }
            }
        }

        if (bell2Enabled)
        {
            auto* freqPtr = apvts.getRawParameterValue ("bell2Freq");
            auto* gainPtr = apvts.getRawParameterValue ("bell2Gain");
            auto* qPtr = apvts.getRawParameterValue ("bell2Q");

            if (freqPtr && gainPtr && qPtr && currentSampleRate > 0)
            {
                float bell2Freq = juce::jlimit (20.0f, 20000.0f, freqPtr->load());
                float bell2Gain = gainPtr->load();
                float bell2Q = juce::jlimit (0.1f, 10.0f, qPtr->load());

                float gainLinear = juce::Decibels::decibelsToGain (bell2Gain);
                bell2Coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, bell2Freq, bell2Q, gainLinear);

                if (bell2Coeffs != nullptr)
                {
                    bell2FilterL.coefficients = bell2Coeffs;
                    bell2FilterR.coefficients = bell2Coeffs;

                    // Apply to input buffer
                    for (int n = 0; n < numSamples; ++n)
                    {
                        buffer.setSample (0, n, bell2FilterL.processSample (buffer.getSample (0, n)));
                        if (numCh > 1)
                            buffer.setSample (1, n, bell2FilterR.processSample (buffer.getSample (1, n)));
                    }
                }
            }
        }
    }

    // (Debug WAV input recording removed — file I/O on audio thread causes heap corruption)

    // ===== Copy INPUT signal (after Bell EQ) for dry path =====
    auto& dry = dryBuf_;
    for (int ch = 0; ch < numCh; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ===== EQ BEHAVIOR =====
    // Bell1/Bell2 (Input EQ): Applied BEFORE wet/dry split - shapes what vocoder analyzes
    // W1/W2 (Complement Bells): Applied AFTER vocoder - controls wet/dry ratio per frequency
    //
    // Normal mode (complement OFF):
    //   Bell1/Bell2 shape the input (affects vocoder + dry)
    //   Standard wet/dry mix
    //
    // Complement mode (complement ON):
    //   Bell1/Bell2 shape the input (affects vocoder + dry)
    //   HP/LP filters shape wet and dry
    //   W1/W2 apply INVERSE gains (wet boost = dry cut) for frequency-selective ratio control
    //   Sub complement: clean sub at fixed level (below HP)
    //   High complement: clean highs following mix (above LP)

    auto& wet = wetBuf_;
    wet.clear();

    // Buffer for old bands during crossfade
    auto& wetOld = wetOldBuf_;

    const int nBands = bands.size();

    // EFFECTIVE band count = how many bands actually make sound.
    // In scale-locked modes several requested bands snap onto the SAME scale note; the extras are marked
    // isMergedSecondary and are skipped outright in the band loops below (unless STACK is on, where they
    // deliberately stack). Any level compensation therefore has to divide by THIS number, not by the number
    // the user asked for -- otherwise dragging the band slider up in a scale attenuates the output for bands
    // that contribute nothing, which is exactly the "the sound changes but the -> X readout doesn't" report.
    int nPrimaryBands = 0;
    for (auto* b : bands)
        if (b != nullptr && ! b->isMergedSecondary)
            ++nPrimaryBands;

    // Modulator uses the FILTERED buffer (input filter applied)
    // Create modulator buffer with modLevel gain applied
    auto& modBuffer = modBuf_;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        float* dst = modBuffer.getWritePointer (ch);
        for (int n = 0; n < numSamples; ++n)
            dst[n] = src[n] * modLevel;
    }

    const float* modL = modBuffer.getReadPointer (0);
    const float* modR = (numCh > 1 ? modBuffer.getReadPointer (1) : nullptr);

    // ===== PHASE 1: Configure and generate carrier =====
    // Map carrierMode parameter to CarrierGenerator::Type
    CarrierGenerator::Type carrierType;
    switch (carrierModeInt)
    {
        case 0:  carrierType = CarrierGenerator::Type::Self;       break;
        case 1:  carrierType = CarrierGenerator::Type::External;   break;
        case 2:  carrierType = CarrierGenerator::Type::Noise;      break;
        case 3:  carrierType = CarrierGenerator::Type::Saw;        break;
        case 4:  carrierType = CarrierGenerator::Type::Square;     break;
        case 5:  carrierType = CarrierGenerator::Type::PitchTrack; break;
        default: carrierType = CarrierGenerator::Type::Self;       break;
    }

    carrierGenerator.setType (carrierType);
    carrierGenerator.setFrequency (carrierPitch);

    // Pass MIDI notes to carrier for Saw/Square polyphonic modes
    if ((carrierType == CarrierGenerator::Type::Saw || carrierType == CarrierGenerator::Type::Square)
        && !midiNoteFreqs.empty())
    {
        carrierGenerator.setMidiFrequencies (midiNoteFreqs);
    }
    else
    {
        carrierGenerator.clearMidiFrequencies();
    }

    // Ensure carrier buffer is sized correctly
    if (carrierBuffer.getNumSamples() < numSamples)
        carrierBuffer.setSize (2, numSamples);

    carrierBuffer.clear();

    // ===== EXTERNAL SIDECHAIN CARRIER =====
    // Get sidechain buffer from input bus 1 (if available and External mode is selected)
    juce::AudioBuffer<float>* sidechainPtr = nullptr;

    if (carrierType == CarrierGenerator::Type::External)
    {
        // Safety: ensure pre-allocated sidechain buffer is ready
        if (sidechainBuf_.getNumChannels() > 0 && sidechainBuf_.getNumSamples() > 0)
        {
            auto sidechainBus = getBus (true, 1);  // true = input, 1 = sidechain bus index
            if (sidechainBus != nullptr && sidechainBus->isEnabled())
            {
                int sidechainChannels = sidechainBus->getNumberOfChannels();
                if (sidechainChannels > 0)
                {
                    // Get buffer for sidechain bus — defensive during host routing changes
                    auto sidechainRange = getBusBuffer (buffer, true, 1);
                    int scChannels = juce::jmin (sidechainRange.getNumChannels(), sidechainBuf_.getNumChannels());
                    int scSamples  = juce::jmin (sidechainRange.getNumSamples(), sidechainBuf_.getNumSamples());

                    if (scChannels > 0 && scSamples > 0)
                    {
                        for (int ch = 0; ch < scChannels; ++ch)
                            sidechainBuf_.copyFrom (ch, 0, sidechainRange, ch, 0, scSamples);
                        sidechainPtr = &sidechainBuf_;
                    }
                }
            }
        }
    }

    // Generate carrier signal (pass sidechain buffer if External mode)
    carrierGenerator.generateCarrier (carrierBuffer, sidechainPtr, &dry);

    // Carrier pointers for use in band loop
    const float* carrierL = carrierBuffer.getReadPointer (0);
    const float* carrierR = (carrierBuffer.getNumChannels() > 1) ? carrierBuffer.getReadPointer (1) : carrierL;

    //====================================================
    // Noise generation with Noise Color (pink to white blend)
    //====================================================
    // NOISE GENERATION (for unvoiced/consonant sounds)
    // Noise is added during detected consonants (Ableton-style)
    //====================================================
    auto& noiseBuf = noiseBuf_;

    // V3 CRACKED: When unvoiced > 0, noise is used as carrier replacement
    // and MUST be pure white noise (Ableton confirmed slope ~0 dB/oct).
    // The noiseColor parameter only affects the old post-band noise path (now disabled).
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* nb = noiseBuf.getWritePointer (ch);
        for (int n = 0; n < numSamples; ++n)
            nb[n] = noiseRandom.nextFloat() * 2.0f - 1.0f;  // Pure white noise
    }

    // NOISE HP (my design): high-pass the noise so it can be hiss-only (no low rumble).
    // 20 Hz = off. Retune coeffs only on change (no per-sample cost / zipper).
    if (auto* nhpPtr = apvts.getRawParameterValue ("noiseHp"))
    {
        float nhp = nhpPtr->load();
        if (nhp > 25.0f)
        {
            if (std::abs (nhp - lastNoiseHp) > 0.5f)
            {
                lastNoiseHp = nhp;
                auto c = juce::IIRCoefficients::makeHighPass (currentSampleRate, (double) nhp);
                noiseHpL.setCoefficients (c);
                noiseHpR.setCoefficients (c);
            }
            for (int n = 0; n < numSamples; ++n)
                noiseBuf.getWritePointer (0)[n] = noiseHpL.processSingleSampleRaw (noiseBuf.getWritePointer (0)[n]);
            if (numCh > 1)
                for (int n = 0; n < numSamples; ++n)
                    noiseBuf.getWritePointer (1)[n] = noiseHpR.processSingleSampleRaw (noiseBuf.getWritePointer (1)[n]);
        }
        else
        {
            lastNoiseHp = nhp;  // keep in sync so re-enabling retunes
        }
    }

    //====================================================
    // SIBILANT DETECTION – proper vocoder unvoiced detection
    // ===== ADVANCED UNVOICED DETECTION (HP/LP Ratio + Spectral Centroid) =====
    // State-of-the-art vocoder technique:
    // 1. HP/LP ratio: high ratio = unvoiced (sibilants), low ratio = voiced (vowels/bass)
    // 2. Spectral centroid: high = bright/unvoiced, low = dark/voiced
    // This is more accurate than simple HP envelope detection
    //====================================================
    auto& noiseEnvBuf = noiseEnvBuf_;
    auto& unvoicedScoreBuf = unvoicedScoreBuf_;
    std::fill (noiseEnvBuf.begin(), noiseEnvBuf.begin() + numSamples, 0.0f);
    std::fill (unvoicedScoreBuf.begin(), unvoicedScoreBuf.begin() + numSamples, 0.0f);

    if (unvoicedLevel > 0.0f)
    {
        // Sib Focus: retune the detector's high band if the user moved the knob (on-change only,
        // no per-sample cost; envelope state preserved so no click).
        if (auto* sibPtr = apvts.getRawParameterValue ("sibFreq"))
        {
            float sibF = sibPtr->load();
            if (std::abs (sibF - lastSibFreq) > 0.5f)
            {
                lastSibFreq = sibF;
                unvoicedHPF.setCoefficients (juce::IIRCoefficients::makeHighPass (currentSampleRate, (double) sibF));
            }
        }

        for (int n = 0; n < numSamples; ++n)
        {
            float mono = modL[n];
            if (modR != nullptr)
                mono = 0.5f * (mono + modR[n]);

            // ===== LP ENVELOPE (below 1000Hz) - voiced content =====
            float lpSample = unvoicedLPF.processSingleSampleRaw (mono);
            float lpAbs = std::abs (lpSample);

            if (lpAbs > unvoicedEnvLP)
                unvoicedEnvLP = attackCoeff * unvoicedEnvLP + (1.0f - attackCoeff) * lpAbs;
            else
                unvoicedEnvLP = releaseCoeff * unvoicedEnvLP + (1.0f - releaseCoeff) * lpAbs;

            // ===== HP ENVELOPE (above 3000Hz) - unvoiced/sibilant content =====
            float hpSample = unvoicedHPF.processSingleSampleRaw (mono);
            float hpAbs = std::abs (hpSample);

            // Faster attack for crisp consonant response
            float sibilantAttack = attackCoeff * 0.5f;
            if (hpAbs > unvoicedEnvHP)
                unvoicedEnvHP = sibilantAttack * unvoicedEnvHP + (1.0f - sibilantAttack) * hpAbs;
            else
                unvoicedEnvHP = releaseCoeff * unvoicedEnvHP + (1.0f - releaseCoeff) * hpAbs;

            noiseEnvBuf[(size_t) n] = unvoicedEnvHP;

            // ===== HP/LP RATIO (core unvoiced detection) =====
            // High ratio = sibilants (S, F, T, CH) = add noise
            // Low ratio = vowels/bass = less noise
            float hpLpRatio = unvoicedEnvHP / (unvoicedEnvLP + 1e-6f);

            // Normalize ratio: 0-1 range, where 1 = strongly unvoiced
            // Ratio of 2.0+ is definitely unvoiced, 0.5 or less is voiced
            float unvoicedScore = juce::jlimit (0.0f, 1.0f, hpLpRatio * 0.5f);

            // Apply soft curve for natural response
            unvoicedScore = fastTanh (unvoicedScore * 2.0f);

            unvoicedScoreBuf[(size_t) n] = unvoicedScore;
        }
    }

    //====================================================
    // BAND LOOP – per-sample envelope processing
    //====================================================

    // Accumulators for SPECTRAL CENTROID approximation
    // Low bands (bottom 30%) vs High bands (top 30%) gives brightness estimate
    auto& lowBandEnvBuf = lowBandEnvBuf_;
    auto& highBandEnvBuf = highBandEnvBuf_;
    std::fill (lowBandEnvBuf.begin(), lowBandEnvBuf.begin() + numSamples, 0.0f);
    std::fill (highBandEnvBuf.begin(), highBandEnvBuf.begin() + numSamples, 0.0f);
    const int lowFreqBandEnd = nBands * 3 / 10;            // Bottom 30% of bands
    const int highFreqBandStart = nBands * 7 / 10;         // Top 30% of bands
    const int lowFreqBandCount = lowFreqBandEnd;
    const int highFreqBandCount = nBands - highFreqBandStart;

    // Additive mode: all bands process independently (stack at shared frequencies)
    // Normal mode: skip merged secondary bands, normalize primary band output
    bool additiveMode = apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;

    // CPU OPT: Pre-compute mono modulator once instead of per-band
    auto& monoMod = monoModBuf_;
    if (modR != nullptr)
    {
        for (int n = 0; n < numSamples; ++n)
            monoMod[(size_t) n] = 0.5f * (modL[n] + modR[n]);
    }
    else
    {
        for (int n = 0; n < numSamples; ++n)
            monoMod[(size_t) n] = modL[n];
    }

    // ===== V3: NO GLOBAL NOISINESS DETECTION NEEDED =====
    // Energy-normalized filter bank handles tonal and noise signals identically.
    // smoothedGlobalNoisiness_ kept at 0 for any code that still references it.
    smoothedGlobalNoisiness_ = 0.0f;

    // Pre-compute stage 2 release coefficient (BW-dependent, outside band loop)
    float envBandWidth = apvts.getRawParameterValue ("bandWidth")->load();
    float envWidthNorm = juce::jlimit (0.0f, 1.0f, (envBandWidth - 0.1f) / 1.9f);
    float slowFactor = juce::jlimit (0.7f, 2.5f, 2.5f - 15.0f * envWidthNorm);
    float releaseCoeff2 = std::pow (releaseCoeff, 1.0f / slowFactor);

    // TIGHTNESS knob (0.5 = match Ableton); read once per block, used in the Linked envelope path.
    float tightnessParam = apvts.getRawParameterValue ("tightness")
                         ? apvts.getRawParameterValue ("tightness")->load() : 0.5f;

    for (int bi = 0; bi < nBands; ++bi)
    {
        auto* b = bands[bi];

        if (!additiveMode && b->isMergedSecondary)
            continue;

        // Merge normalization. The 1/groupSize form compensated for N bands SUMMING at one position -- but
        // that only happens with STACK ON, and STACK ON is precisely the branch that skips it. With STACK OFF
        // the secondaries `continue` a few lines up, so the primary is the only band sounding at that note and
        // dividing it by the group size just made scale-collapsed notes quieter the more bands you asked for.
        // Both branches are 1.0: STACK ON stacks on purpose, STACK OFF has nothing to normalize against.
        float mergeNorm = 1.0f;

        // Note exclusion: check if this band's pitch class is excluded by user
        // SKIP the entire band if its note is excluded (affects all octaves of that pitch class)
        int bandPitchClass = b->cachedPitchClass;  // CPU OPT: use cached value
        bool isNoteExcluded = noteExclusions[(size_t) bandPitchClass];

        // OPTIMIZATION: Skip band processing entirely if note is excluded
        // This guarantees zero output from excluded notes
        if (isNoteExcluded)
        {
            b->env = 0.0f;  // Reset envelope so display bars drop to zero
            continue;
        }

        float noteExclusionGain = 1.0f;  // If we get here, note is NOT excluded

        // Per-sample envelope buffer (CPU OPTIMIZATION: reuse pre-allocated buffer)
        auto& envBuf = bandEnvBuffer;

        float env = b->env;    // Stage 1
        float env2 = b->env2;  // Stage 2
        float env3 = b->env3;  // Stage 3 (Linked mode only)
        float env4 = b->env4;  // Stage 4 (Linked mode only)
        float env5 = b->env5;  // Stage 5 (iter 21)
        float env6 = b->env6;  // Stage 6 (iter 21)

        // ===== V3: ENERGY-NORMALIZED ENVELOPE =====
        float normGain = b->bandNormGain;

        // ITER 19f (2026-04-10): Sub release shortened from 200 → 95 ms (= 100 Hz value).
        // The +3.8 dB sub hot was caused by the over-extrapolated 200 ms release at 20 Hz
        // keeping kick attack energy in the envelope too long. Setting sub = 100 Hz value
        // is the conservative interpretation (Ableton's release shouldn't get LONGER below
        // 100 Hz than at 100 Hz unless there's a specific reason).
        // Source: extract_ableton_taus.py output (measured values at user release=50 ms).
        //   freq Hz | decay_50 ms
        //   20      | 95   (set = 100 Hz value, was 200 ms over-extrapolated)
        //   100     | 95   (measured)
        //   1000    | 76   (measured)
        //   5000    | 10.5 (measured)
        //   18000   | 4    (extrapolated)
        const bool useCascadeLP = ((int) apvts.getRawParameterValue ("algorithmMode")->load() == 1);
        static constexpr int kTauTblN = 5;
        static constexpr float kTauFreqs[kTauTblN] = { 20.0f, 100.0f, 1000.0f, 5000.0f, 18000.0f };
        // ITER 19l (2026-04-10): Sub release 95 → 15 ms. worst_frames.py showed
        // post-kick frames where BR Sub is +25-35 dB louder than ABL — BR's sub
        // envelope sustains while ABL's has already decayed. ABL decays 30 dB in
        // ~23 ms at sub → effective release ~7 ms. The probe's 95 ms was steady-
        // state sine decay; transient sub content releases much faster in ABL.
        // Changed sub entries from 95 to 15 ms; 100 Hz from 95 → 30 ms (transition).
        // ITER 21 PHASE 2h: 1500 ms cascade.
        static constexpr float kTauReleaseMs[kTauTblN] = { 650.0f, 650.0f, 650.0f, 650.0f, 650.0f };

        float fc = juce::jlimit (kTauFreqs[0], kTauFreqs[kTauTblN-1],
                                 juce::jmax (20.0f, b->currentFreq));
        float bandReleaseMs;
        {
            int idx = 0;
            for (int k = 0; k < kTauTblN - 1; ++k)
                if (fc >= kTauFreqs[k] && fc <= kTauFreqs[k+1]) { idx = k; break; }
            float lf0 = std::log10 (kTauFreqs[idx]);
            float lf1 = std::log10 (kTauFreqs[idx+1]);
            float lt0 = std::log10 (kTauReleaseMs[idx]);
            float lt1 = std::log10 (kTauReleaseMs[idx+1]);
            float t = (std::log10 (fc) - lf0) / (lf1 - lf0);
            bandReleaseMs = std::pow (10.0f, lt0 + t * (lt1 - lt0));
        }
        // Scale by user release setting (table is at user_release = 50 ms)
        float bandTauSec = bandReleaseMs * 0.001f * (lastReleaseSec / 0.05f);
        bandTauSec = juce::jlimit (0.001f, 10.0f, bandTauSec);  // ITER 21: raised clamp for sweep
        const float c  = std::pow (0.01f, 1.0f / (bandTauSec * (float) currentSampleRate));
        const float ic = 1.0f - c;

        // TIGHTNESS (2026-06-14): per-band release shaping in Linked mode for the mid/high decay.
        // Measured DIRECT (time-domain, decay-to-30dB on bursts) at tightness 0.5 baseline: BR is slightly
        // TIGHTER than Ableton in 320-5k, so to MATCH Ableton we LENGTHEN BR per band by these factors:
        //   320-640 x1.27 | 640-1280 x1.17 | 1.3-2.5k x1.13 | 2.5-5k x1.16 | 5k+ x1.06.
        // ablMatchScale(fc) interpolates that curve in log-freq. The TIGHTNESS knob (default 0.5 = match)
        // scales it: >0.5 = tighter (shorter), <0.5 = softer. +-1 octave of range at the extremes.
        float linkedReleaseCoeff = releaseCoeff;
        if (useCascadeLP && fc >= 320.0f)
        {
            // log-freq match curve: nodes at 450, 900, 1800, 3500, 7000 Hz -> 1.27,1.17,1.13,1.16,1.06
            float lfHz = juce::jlimit (450.0f, 7000.0f, fc);
            const float nF[5] = { 450.0f, 900.0f, 1800.0f, 3500.0f, 7000.0f };
            const float nS[5] = { 1.27f, 1.17f, 1.13f, 1.16f, 1.06f };
            float ablMatchScale = nS[0];
            for (int s = 0; s < 4; ++s)
                if (lfHz >= nF[s] && lfHz <= nF[s + 1])
                {
                    float u = (std::log10 (lfHz) - std::log10 (nF[s])) / (std::log10 (nF[s + 1]) - std::log10 (nF[s]));
                    ablMatchScale = nS[s] + (nS[s + 1] - nS[s]) * u;
                    break;
                }
            if (lfHz >= nF[4]) ablMatchScale = nS[4];
            // knob: 0.5 -> x1 (Ableton match), 1.0 -> x0.5 (tighter), 0.0 -> x2 (softer)
            float knob = std::pow (2.0f, (0.5f - tightnessParam) * 2.0f);
            float relScale = ablMatchScale * knob;
            float relSecBand = juce::jmax (0.002f, lastReleaseSec * relScale);
            linkedReleaseCoeff = std::pow (0.01f, 1.0f / (relSecBand * (float) currentSampleRate));
        }

        for (int n = 0; n < numSamples; ++n)
        {
            double ya = b->dAna[0].process ((double) monoMod[(size_t) n]);
            ya = b->dAna[1].process (ya);
            float y = (float) b->dAna[2].process (ya);
            float xAbs = std::abs (y);

            float compensatedEnv;
            if (useCascadeLP)
            {
                // ITER 22b: Simple peak detector only (no cascade LP).
                // TIGHT: use per-band linkedReleaseCoeff (shortens mid/high decay to match Ableton).
                // (Detector type peak/RMS/mean-abs tested 2026-06-15 -> no effect on the BR-AB comb; kept peak.)
                if (xAbs > env)
                    env = attackCoeff  * env + (1.0f - attackCoeff)  * xAbs;
                else
                    env = linkedReleaseCoeff * env + (1.0f - linkedReleaseCoeff) * xAbs;
                compensatedEnv = env;
            }
            else
            {
                // Original 2-stage peak detector for Ritual/Split modes
                if (xAbs > env)
                    env = attackCoeff  * env + (1.0f - attackCoeff)  * xAbs;
                else
                    env = releaseCoeff * env + (1.0f - releaseCoeff) * xAbs;

                if (env > env2)
                    env2 = attackCoeff    * env2 + (1.0f - attackCoeff)    * env;
                else
                    env2 = releaseCoeff2  * env2 + (1.0f - releaseCoeff2)  * env;

                compensatedEnv = env2;
            }

            // ===== PER-BAND SPECTRAL GATE (Ableton-style with SMOOTH transitions) =====
            // Smoothed gate gain to prevent crackle/clicks
            float gatedEnv = compensatedEnv;
            if (gateAmount > -59.0f)  // Gate is active (above -inf)
            {
                // V3 CRACKED GATE: hard mute below threshold (like Ableton)
                float targetGateGain = (compensatedEnv < gateThresholdLinear) ? 0.0f : 1.0f;

                // Smooth the gate gain using sample-rate-dependent coefficients
                // (calculated in updateEnvelopeCoeffs: 2ms attack, 30ms release)
                if (targetGateGain > b->gateGainSmoothed)
                    b->gateGainSmoothed = gateAttackCoeff * b->gateGainSmoothed
                                        + (1.0f - gateAttackCoeff) * targetGateGain;
                else
                    b->gateGainSmoothed = gateReleaseCoeff * b->gateGainSmoothed
                                        + (1.0f - gateReleaseCoeff) * targetGateGain;

                gatedEnv = compensatedEnv * b->gateGainSmoothed;
            }
            else
            {
                b->gateGainSmoothed = 1.0f;  // Reset when gate is off
            }

            // ===== PER-BAND DYNAMIC THRESHOLD (soft expander) =====
            // Attenuates band when envelope falls below user-dragged threshold line.
            // Ratio 4:1, soft knee 6 dB. Output gain stored in b->dynGainSmoothed
            // (applied later to bandMult so this factor is visible in the final band gain).
            if (isDynEnabled)
            {
                float envDb = juce::Decibels::gainToDecibels (compensatedEnv, -120.0f);
                float diff  = envDb - dynThresholdDb;  // positive when env is ABOVE threshold
                // Soft knee: ramp attenuation over ±halfKnee dB around threshold
                float kneeBlend = juce::jlimit (0.0f, 1.0f,
                                                (diff + kDynHalfKneeDb) / kDynKneeDb);
                float reductionDb = juce::jlimit (0.0f, kDynMaxReduceDb,
                                                  diff * kDynRatioCoef * kneeBlend);
                float targetGain = juce::Decibels::decibelsToGain (-reductionDb);

                // Asymmetric smoothing: fast attack when reducing, slower release when restoring
                if (targetGain < b->dynGainSmoothed)
                    b->dynGainSmoothed = gateAttackCoeff * b->dynGainSmoothed
                                       + (1.0f - gateAttackCoeff) * targetGain;
                else
                    b->dynGainSmoothed = gateReleaseCoeff * b->dynGainSmoothed
                                       + (1.0f - gateReleaseCoeff) * targetGain;
            }
            else
            {
                b->dynGainSmoothed = 1.0f;
            }

            // Apply HYBRID chord gate (bands far from chord notes fade out)
            gatedEnv *= b->chordGate;
            // V3 CRACKED FORMANT: mute bands shifted outside range
            if (b->formantMute) gatedEnv = 0.0f;

            envBuf[(size_t) n] = gatedEnv;

            // Accumulate band envelopes for SPECTRAL CENTROID approximation
            // Low bands (bottom 30%) = bass/voiced content
            // High bands (top 30%) = treble/sibilant content
            if (bi < lowFreqBandEnd)
                lowBandEnvBuf[(size_t) n] += gatedEnv;
            else if (bi >= highFreqBandStart)
                highBandEnvBuf[(size_t) n] += gatedEnv;
        }

        // (Debug band 10 envelope dump removed — file I/O on audio thread causes heap corruption)

        // Persist envelope state across blocks
        b->env  = env;   // Stage 1
        b->env2 = env2;  // Stage 2
        b->env3 = env3;  // Stage 3 (Linked mode)
        b->env4 = env4;  // Stage 4 (Linked mode)
        b->env5 = env5;  // Stage 5 (iter 21)
        b->env6 = env6;  // Stage 6 (iter 21)

        // Visualization-only gated envelope (does NOT feed the follower). Mirrors the per-sample
        // gate chain (gate/chord/formant) so the band display reflects the GATE like Ableton.
        // Gate OFF => gateGainSmoothed==1, chordGate==1, formantMute==false => envDisplay == env
        // (pixel-identical to the previous display).
        {
            float disp = env;
            if (gateAmount > -59.0f) disp *= b->gateGainSmoothed;  // same factor as the audio gate
            disp *= b->chordGate;
            if (b->formantMute) disp = 0.0f;
            b->envDisplay = disp;
        }

        // Store this band's envelope for formant remapping
        // ALL band envelopes must be computed before carrier processing starts,
        // because formant shift can map to higher-numbered bands.
        if (bi < (int) allBandEnvs.size())
        {
            for (int n = 0; n < numSamples; ++n)
                allBandEnvs[(size_t) bi][(size_t) n] = envBuf[(size_t) n];
        }
    }  // END of envelope pass (all bands)

    // ===== VOCODEX CONTOUR: "FROM CONSONANTS -> NOISE" =====
    // Vocodex drives the NOISE level from a consonant detector on the modulator (noise rises
    // on S/T/F, falls on vowels). BR already has the detector (unvoicedScoreBuf, HP/LP @3kHz)
    // and the per-mode base/reactive contour, but they were computed and thrown away.
    // Compute the base/reactive split here (param-only, before the STFT goto/IIR loop) so the
    // noise injection below can gate the noise gain by the consonant score. noiseMode picks the
    // amount: Vocal = strongly consonant-driven, Synth = mostly flat, Auto = carrier-aware.
    float noiseBaseRatio = 1.0f, noiseReactiveRatio = 0.0f;
    if (noiseModeInt == 1)        { noiseBaseRatio = 0.35f; noiseReactiveRatio = 0.65f; }   // Vocal
    else if (noiseModeInt == 2)   { noiseBaseRatio = 0.70f; noiseReactiveRatio = 0.30f; }   // Synth (flatter)
    else                          // Auto - carrier-aware (0=Self,1=External,2=Noise,3=Saw,4=Square,5=PitchTrack)
    {
        switch (carrierModeInt)
        {
            case 1:  noiseBaseRatio = 0.35f; noiseReactiveRatio = 0.65f; break;  // External -> vocal
            case 2:  noiseBaseRatio = 0.80f; noiseReactiveRatio = 0.20f; break;  // Noise carrier -> mostly constant
            case 3:
            case 4:  noiseBaseRatio = 0.65f; noiseReactiveRatio = 0.35f; break;  // Saw/Square -> synth grit
            case 5:  noiseBaseRatio = 0.50f; noiseReactiveRatio = 0.50f; break;  // Pitch Track
            case 0:
            default: noiseBaseRatio = 0.50f; noiseReactiveRatio = 0.50f; break;  // Self
        }
    }

    // ===== PASS 2: CARRIER PROCESSING (uses remapped envelopes) =====
    // STFT path for Linked mode: bypass the IIR per-band carrier loop entirely.
    // The STFTVocoder handles band-splitting, envelope, gain, and synthesis internally
    // via FFT bin grouping — architecturally closer to Ableton's likely implementation.
    int algoMode = (int) apvts.getRawParameterValue ("algorithmMode")->load();
    if (useSTFTLinked && algoMode == 1)
    {
        // Modulator = mono sum of input (already computed above as monoMod)
        // Carrier = input signal (Self mode: same as modulator, in stereo)
        const float* modMono = monoMod.data();
        const float* carL = carrierL;
        const float* carR = carrierR;

        // Clear wet buffer — STFT process() ADDS to output
        wet.clear();

        stftVocoder.process (modMono, carL, carR,
                             wet.getWritePointer (0), wet.getWritePointer (1),
                             numSamples,
                             depth,
                             gateThresholdLinear, gateGainLinear,
                             0.0f, 0.0f,   // gate attack/release (not used with gate=-inf)
                             1.0f);        // globalNoisiness = 1.0 (assume noise-like input)

        goto stftDone;  // Skip IIR carrier loop
    }

    // Separated from envelope computation so formant remapping can access any band's envelope.
    for (int bi = 0; bi < nBands; ++bi)
    {
        auto* b = bands[bi];

        if (!additiveMode && b->isMergedSecondary)
            continue;

        int bandPitchClass = b->cachedPitchClass;
        if (noteExclusions[(size_t) bandPitchClass])
            continue;

        float mergeNorm = 1.0f;   // see the note at the first mergeNorm: secondaries are skipped, not summed
        float normGain = b->bandNormGain;
        float noteExclusionGain2 = 1.0f;

        // ===== V3 CRACKED: Ableton-style carrier path =====
        // Formant shift via BAND REMAPPING: carrier band i uses envelope from band (i - offset).
        // Filters stay at fixed frequencies — completely artifact-free.
        auto& bandCar = bandCarBuf_;

        float bandPos = (nBands > 1) ? (float) bi / (float) (nBands - 1) : 0.5f;
        float spectralMult = calculateSpectralGain (bandPos, spectralTilt, spectralIntensity);

        float bandMult = bandGains[(size_t) bi] * spectralMult * mergeNorm * noteExclusionGain2 * normGain * b->tiltCorr * b->dynGainSmoothed;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* carr = (ch == 0) ? carrierL : carrierR;
            auto* bc  = bandCar.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                float e = allBandEnvs[(size_t) bi][(size_t) n];
                // ITER 22: Envelope compression e^0.75 for Linked mode (ABL match).
                // Ritual/Split: linear envelope (original BR character).
                float envComp = (algoMode == 1)
                    ? std::pow (e + 1e-20f, 0.75f)
                    : e;
                // Depth formula: floor=0.065 at 0%, linear scaling above 100%.
                // ABL does NOT attenuate at depth>100% — it just scales linearly.
                float envGain = 0.065f * juce::jmax (0.0f, 1.0f - depth)
                              + envComp * depth;

                // ITER 14c (2026-04-09): No envelope cap. The hybrid envelope
                // follower (peak detector + 3-stage cascaded LP smoothing) above
                // bounds per-band output naturally for swept content.

                // V3 CRACKED NOISE: carrier replacement (same as IIR path)
                // VOCODEX CONTOUR: gate the noise amount by the consonant detector
                // (unvoicedScoreBuf, pre-loop HP/LP score). Vowels -> base, consonants -> full.
                float in = carr[n];
                if (unvoicedLevel > 0.0f)
                {
                    float contour = noiseBaseRatio + noiseReactiveRatio * unvoicedScoreBuf[(size_t) n];
                    float ng = unvoicedLevel * unvoicedLevel * unvoicedLevel * contour;  // k^3 * contour
                    // NOISE FOLLOWS THE INPUT, NOT DEPTH. The shared *envGain below flattens to a
                    // constant floor (0.065) at depth 0, which would SUSTAIN the noise (the bug).
                    // Pre-scale the noise term by (envComp/envGain) so after *envGain it nets
                    // *envComp = the modulator envelope: the noise ducks with the signal at ANY
                    // depth, like Ableton's Unvoiced path. No-op at depth=100% (envGain==envComp).
                    float noiseFollow = (envGain > 1.0e-4f) ? (envComp / envGain) : (envComp * 1.0e4f);
                    in = in * (1.0f - ng) + noiseBuf.getSample(ch, n) * ng * 2.66f * noiseFollow;  // +8.5 dB
                }

                // ITER 20c (2026-04-10): Switched back to 4-stage cascaded RBJ BPF2.
                // Butterworth's flat passband captured harmonic energy from adjacent bands,
                // creating +22 dB outliers vs ABL at inter-harmonic frequencies. ABL uses
                // peaked BPF filters that naturally have inter-band valleys matching the
                // harmonic structure. RBJ BPF2 cascade at Q=37 reproduces this peaked shape.
                // The "11-42 dB inter-band gaps" from iter 19c were likely at wrong Q.
                // 4-stage carrier bandpass in DOUBLE precision (clean low-freq resonances).
                auto* dc = (ch == 0) ? b->dCarL : b->dCarR;
                double yd = (double) in;
                yd = dc[0].process (yd); yd = dc[1].process (yd);
                yd = dc[2].process (yd); yd = dc[3].process (yd);
                float f4 = (float) yd;
                bc[n] = f4 * envGain * bandMult;
            }
        }

        float phaseSign = (bi % 2 == 0) ? 1.0f : -1.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            if (phaseSign < 0.0f)
                bandCar.applyGain (ch, 0, numSamples, -1.0f);
            wet.addFrom (ch, 0, bandCar, ch, 0, numSamples);
        }
    }

    stftDone:  // STFT path jumps here after bypassing the IIR carrier loop

    // ===== COMPUTE SPECTRAL CENTROID from band envelopes =====
    // Centroid = highBandEnergy / (lowBandEnergy + highBandEnergy)
    // High centroid = bright/sibilant, Low centroid = dark/bass
    // Combine with HP/LP ratio for robust unvoiced detection
    if (lowFreqBandCount > 0 && highFreqBandCount > 0)
    {
        const float invLow = 1.0f / (float) lowFreqBandCount;
        const float invHigh = 1.0f / (float) highFreqBandCount;

        for (int n = 0; n < numSamples; ++n)
        {
            // Average the band energies
            float lowEnergy = lowBandEnvBuf[(size_t) n] * invLow;
            float highEnergy = highBandEnvBuf[(size_t) n] * invHigh;

            // Spectral centroid approximation: 0 = all bass, 1 = all treble
            float totalEnergy = lowEnergy + highEnergy + 1e-10f;
            float centroid = highEnergy / totalEnergy;

            // Combine HP/LP ratio (from direct filters) with spectral centroid (from bands)
            // This gives robust unvoiced detection:
            // - HP/LP ratio detects sibilants at 3kHz+
            // - Centroid confirms overall brightness from bands
            float hpLpScore = unvoicedScoreBuf[(size_t) n];
            float combinedScore = 0.6f * hpLpScore + 0.4f * centroid;

            // Store for noise section
            unvoicedScoreBuf[(size_t) n] = combinedScore;

            // Also store high band envelope for noise timing
            highBandEnvBuf[(size_t) n] = highEnergy;
        }
    }

    // ===== CROSSFADE: Process old bands during transition =====
    // This prevents clicks when band count or other params change
    if (isCrossfading && bandsOld.size() > 0)
    {
        wetOld.clear();

        const int nBandsOld = bandsOld.size();

        // Use same additive mode setting for crossfade consistency
        bool additiveModeXfade = apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;

        for (int bi = 0; bi < nBandsOld; ++bi)
        {
            auto* b = bandsOld[bi];

            // Skip merged secondary bands in crossfade too (unless additive mode)
            if (!additiveModeXfade && b->isMergedSecondary)
                continue;

            // Merge normalization: see the note at the first mergeNorm. Kept identical on the crossfade path
            // so a band-count change does not step in level halfway through the fade.
            float mergeNorm = 1.0f;
            juce::ignoreUnused (additiveModeXfade);

            // Note exclusion for crossfade bands too - SKIP entirely if excluded
            int bandPitchClassXf = b->cachedPitchClass;  // CPU OPT: use cached value
            bool isNoteExcludedXf = noteExclusions[(size_t) bandPitchClassXf];

            // Skip excluded bands in crossfade too
            if (isNoteExcludedXf)
            {
                b->env = 0.0f;
                continue;
            }

            float noteExclusionGainXf = 1.0f;  // If we get here, note is NOT excluded

            auto& envBuf = bandEnvBuffer;
            std::fill (envBuf.begin(), envBuf.end(), 0.0f);
            float env = b->env;

            // V3: Energy-normalized crossfade path (mirrors main V3 path)
            float normGainXf = b->bandNormGain;

            for (int n = 0; n < numSamples; ++n)
            {
                double yaxf = b->dAna[0].process ((double) monoMod[(size_t) n]);
                yaxf = b->dAna[1].process (yaxf);
                float y = (float) b->dAna[2].process (yaxf);
                float xAbs = std::abs (y);

                if (xAbs > env)
                    env = attackCoeff * env + (1.0f - attackCoeff) * xAbs;
                else
                    env = releaseCoeff * env + (1.0f - releaseCoeff) * xAbs;

                float compensatedEnvXf = env;  // V3: raw envelope, norm applied to bandMult

                // Apply spectral gate to old bands too (Ableton-style with SMOOTH transitions)
                float gatedEnv = compensatedEnvXf;
                if (gateAmount > -59.0f)
                {
                    float targetGateGain = (compensatedEnvXf < gateThresholdLinear) ? gateGainLinear : 1.0f;

                    // Use same sample-rate-dependent coefficients as main bands
                    if (targetGateGain > b->gateGainSmoothed)
                        b->gateGainSmoothed = gateAttackCoeff * b->gateGainSmoothed
                                            + (1.0f - gateAttackCoeff) * targetGateGain;
                    else
                        b->gateGainSmoothed = gateReleaseCoeff * b->gateGainSmoothed
                                            + (1.0f - gateReleaseCoeff) * targetGateGain;

                    gatedEnv = compensatedEnvXf * b->gateGainSmoothed;
                }
                else
                {
                    b->gateGainSmoothed = 1.0f;
                }

                // Apply HYBRID chord gate (bands far from chord notes fade out)
                gatedEnv *= b->chordGate;

                envBuf[(size_t) n] = gatedEnv;
            }

            b->env = env;

            auto& bandCar = bandCarBuf_;
            bandCar.clear();

            // V3: Clean crossfade carrier path
            float bandPosXf = (nBandsOld > 1) ? (float) bi / (float) (nBandsOld - 1) : 0.5f;
            float spectralMultXf = calculateSpectralGain (bandPosXf, spectralTilt, spectralIntensity);
            float bandMultXf = bandGains[(size_t) bi] * spectralMultXf * mergeNorm * noteExclusionGainXf * normGainXf * b->tiltCorr * b->dynGainSmoothed;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* carr = (ch == 0) ? carrierL : carrierR;
                auto* bc = bandCar.getWritePointer (ch);

                for (int n = 0; n < numSamples; ++n)
                {
                    float e = envBuf[(size_t) n];
                    // V3 CRACKED DEPTH: pow(envelope, depth)
                    float envGain = std::pow(juce::jmax(e, 1e-6f), depth);

                    // V3 CRACKED NOISE: Ableton-style carrier replacement
                    // White noise crossfaded into carrier BEFORE band filters
                    // Power curve k^6 matches Ableton's Unvoiced Level scaling
                    // Uses pre-generated noiseBuf (same noise for all bands = correct)
                    float in = carr[n];
                    if (unvoicedLevel > 0.0f)
                    {
                        // VOCODEX CONTOUR (crossfade path): same consonant gating as main loop
                        float contour = noiseBaseRatio + noiseReactiveRatio * unvoicedScoreBuf[(size_t) n];
                        float ng = unvoicedLevel * unvoicedLevel * unvoicedLevel * contour;  // k^3 * contour
                        // NOISE FOLLOWS THE INPUT, NOT DEPTH (see main loop). Here envGain=pow(e,depth),
                        // so e/envGain = e^(1-depth): noise rides e (the modulator envelope) regardless
                        // of depth. No-op at depth=100%.
                        float noiseFollow = (envGain > 1.0e-4f) ? (e / envGain) : (e * 1.0e4f);
                        in = in * (1.0f - ng) + noiseBuf.getSample(ch, n) * ng * 2.66f * noiseFollow;  // +8.5 dB gain to match Ableton
                    }

                    // 4-stage carrier bandpass in DOUBLE precision (crossfade/old-band path)
                    auto* dcx = (ch == 0) ? b->dCarL : b->dCarR;
                    double ydx = (double) in;
                    ydx = dcx[0].process (ydx); ydx = dcx[1].process (ydx);
                    ydx = dcx[2].process (ydx); ydx = dcx[3].process (ydx);
                    float filtered4 = (float) ydx;
                    bc[n] = filtered4 * envGain * bandMultXf;
                }
            }

            for (int ch = 0; ch < numCh; ++ch)
                wetOld.addFrom (ch, 0, bandCar, ch, 0, numSamples);
        }

        // Apply same scaling to wetOld (crossfade path)
        if (nBandsOld > 0)
        {
            float bcGainOld = 1.0f / std::sqrt ((float) juce::jmax (1, nBandsOld) / 4.0f);
            wetOld.applyGain (bcGainOld);
            // Note: bwGain is applied below to the combined wet signal
        }
    }

    // ===== V3 CLEAN: BW-dependent gain + Band count gain =====
    // Two global corrections (NOT per-band), calibrated from Ableton Phase 3+4 recordings.
    //
    // 1) BW gain: at narrow BW, less energy per band → need more global boost.
    //    Derived from Phase 4: Ableton noise output is -6.5 dBFS for BW 30-100%,
    //    and -7.2 dBFS at BW=10%. Our raw output varies much more.
    //    Formula: widthGain_dB = maxBoost × (1 - widthNorm)^2
    //    Calibrated so noise output matches Ableton across BW range.
    //
    // 2) Band count gain: Phase 3 showed Ableton output is ~constant from 4→40 bands.
    //    Scale by 1/sqrt(N/4) to match.
    {
        auto bandWidth = apvts.getRawParameterValue ("bandWidth")->load();
        float widthNorm = (bandWidth - 0.1f) / (2.0f - 0.1f);
        widthNorm = juce::jlimit (0.0f, 1.0f, widthNorm);

        // BW gain: measured from Ableton Phase 4 (6 points, full 10-200% range).
        // Ableton output peaks at BW=100% and drops on BOTH sides.
        // Use piecewise linear interpolation from measured corrections.
        // Correction = Ableton_dB - V3_raw_dB at each BW.
        // V3_raw was measured without BW gain.
        //   BW=10% (bw=0.1): +0.4 dB needed
        //   BW=50% (bw=0.5): +0.5 dB needed
        //   BW=100% (bw=1.0): +1.3 dB needed
        //   BW=130% (bw=1.3): +0.9 dB needed
        //   BW=170% (bw=1.7): +5.2 dB needed
        //   BW=200% (bw=2.0): +14.6 dB needed
        // These are ON TOP of the current BW gain. So the total BW gain is:
        // current_gain + correction.
        // Current gain at each BW: 40*(1-wN)^0.70
        //   BW=10%: 40*1^0.70 = 40.0 → total needed = 40.0+0.4 = 40.4
        //   BW=50%: 40*0.79^0.70 = 33.4 → total = 33.9
        //   BW=100%: 40*0.53^0.70 = 25.1 → total = 26.4
        //   BW=130%: 40*0.37^0.70 = 18.8 → total = 19.7
        //   BW=170%: 40*0.16^0.70 = 9.5 → total = 14.7
        //   BW=200%: 40*0^0.70 = 0 → total = 14.6
        // bwGain v4 (2026-03-23): calibrated via pedalboard against Ableton vocoder.
        // Lookup table: {widthNorm, bwGainDb}
        // ITER 9 (2026-04-09): Restored BW=10% to March's 45.6 dB
        // (was 41.9, drifted in April with the SVF regression).
        // ITER 20g: Fine-tuned bwGain. 40.8 made Mid -1.9 (overcorrected).
        // 45.6 gave Mid +1.0. Linear interp → 43.9 for Mid ≈ 0.
        // Using 44.0 dB, -1.6 dB offset for all entries.
        static const float bwGainTable[][2] = {
            {0.000f, 35.0f},   // BW=10% — reduced to avoid >0dBFS on kicks
            {0.105f, 32.5f},   // BW=30%
            {0.211f, 32.2f},   // BW=50%
            {0.316f, 31.5f},   // BW=70%
            {0.474f, 28.0f},   // BW=100%
            {0.632f, 26.4f},   // BW=130%
            {0.842f, 18.9f},   // BW=170%
            {1.000f, 14.6f}    // BW=200%
        };
        static const int bwGainTableSize = 8;
        float bwGainDb;
        if (widthNorm <= bwGainTable[0][0])
            bwGainDb = bwGainTable[0][1];
        else if (widthNorm >= bwGainTable[bwGainTableSize - 1][0])
            bwGainDb = bwGainTable[bwGainTableSize - 1][1];
        else
        {
            bwGainDb = bwGainTable[bwGainTableSize - 1][1];
            for (int gi = 0; gi < bwGainTableSize - 1; ++gi)
            {
                if (widthNorm >= bwGainTable[gi][0] && widthNorm <= bwGainTable[gi + 1][0])
                {
                    float t = (widthNorm - bwGainTable[gi][0]) / (bwGainTable[gi + 1][0] - bwGainTable[gi][0]);
                    bwGainDb = bwGainTable[gi][1] * (1.0f - t) + bwGainTable[gi + 1][1] * t;
                    break;
                }
            }
        }
        float bwGain = juce::Decibels::decibelsToGain (bwGainDb);

        // Band count gain: 1/sqrt(N/4) — Phase 3 showed ~constant output.
        // N is the count of bands that actually SOUND. With STACK off the merged secondaries are skipped in
        // the band loop, so counting them here quietened the output every time a scale collapsed more bands
        // onto the same notes (60 requested / 12 unique used to cost ~5 dB against 20 requested / 12 unique).
        const int nGainBands = additiveMode ? nBands : nPrimaryBands;
        float bandCountGain = 1.0f / std::sqrt ((float) juce::jmax (1, nGainBands) / 4.0f);

        wet.applyGain (bwGain * bandCountGain);
    }

    // ===== NOISE CONTOUR (Carrier-Mode-Aware) =====
    // The base/reactive contour is now computed ABOVE the band loop (noiseBaseRatio /
    // noiseReactiveRatio) and applied to the pre-band carrier-replacement noise, so the
    // noise follows the modulator's consonants (Vocodex "FROM CONSONANTS -> NOISE").
    // The old disabled post-band noise block lived here and has been removed.

    // ===== HP BLEND: High-passed modulator passthrough for sibilant clarity =====
    // This is the "clarity trick" from Vocodex - passing through high-frequency
    // content (S, F, T, CH sounds) directly to improve intelligibility
    // Slope: 0=12dB (1 stage), 1=24dB (2 stages), 2=48dB (4 stages)
    if (hpBlend > 0.0f)
    {
        int numStages = (hpBlendSlope == 0) ? 1 : (hpBlendSlope == 1) ? 2 : 4;
        bool hpGateActive = (gateAmount > 0.0f);

        // AIR delay: delay the gated AIR contribution (0 ms = off, returns current sample).
        float airMs = apvts.getRawParameterValue ("airDelay") ? apvts.getRawParameterValue ("airDelay")->load() : 0.0f;
        float airDelaySamples = juce::jlimit (0.0f, 16000.0f, airMs * 0.001f * (float) currentSampleRate);

        for (int n = 0; n < numSamples; ++n)
        {
            // Process left channel through cascaded HP filters
            float hpL = dry.getSample (0, n);
            for (int s = 0; s < numStages; ++s)
                hpL = hpBlendFiltersL[s].processSample (0, hpL);

            // Gate the HP blend using same gate parameters as vocoder bands
            float hpGateGain = 1.0f;
            if (hpGateActive)
            {
                float absHp = std::abs (hpL);
                if (numCh > 1)
                    absHp = juce::jmax (absHp, std::abs (dry.getSample (1, n)));

                // Envelope follower (same attack/release as band envelopes)
                if (absHp > hpBlendEnvelope)
                    hpBlendEnvelope = attackCoeff * hpBlendEnvelope + (1.0f - attackCoeff) * absHp;
                else
                    hpBlendEnvelope = releaseCoeff * hpBlendEnvelope + (1.0f - releaseCoeff) * absHp;

                // Gate decision (same threshold/range as band gate)
                float targetGate = (hpBlendEnvelope < gateThresholdLinear) ? gateGainLinear : 1.0f;

                // Smooth the gate gain (same attack/release as band gate)
                if (targetGate > hpBlendGateSmoothed)
                    hpBlendGateSmoothed = gateAttackCoeff * hpBlendGateSmoothed
                                        + (1.0f - gateAttackCoeff) * targetGate;
                else
                    hpBlendGateSmoothed = gateReleaseCoeff * hpBlendGateSmoothed
                                        + (1.0f - gateReleaseCoeff) * targetGate;

                hpGateGain = hpBlendGateSmoothed;
            }
            else
            {
                hpBlendGateSmoothed = 1.0f;
            }

            float airContribL = hpL * hpBlend * hpGateGain;
            airDelayL.pushSample (0, airContribL);
            wet.getWritePointer (0)[n] += airDelayL.popSample (0, airDelaySamples);

            // Process right channel if stereo
            if (numCh > 1)
            {
                float hpR = dry.getSample (1, n);
                for (int s = 0; s < numStages; ++s)
                    hpR = hpBlendFiltersR[s].processSample (0, hpR);

                float airContribR = hpR * hpBlend * hpGateGain;
                airDelayR.pushSample (0, airContribR);
                wet.getWritePointer (1)[n] += airDelayR.popSample (0, airDelaySamples);
            }
        }
    }

    // ===== ABLETON LEVEL MATCH (MEASURED 2026-06-13 vs Ableton stock Vocoder) =====
    // Measured offset BR-vs-Ableton (40 bands, LOG, Linked, white noise) was NOT a flat +16.5 dB
    // (that was an ear estimate) — it is BAND-WIDTH-DEPENDENT: BR was -2.8 dB @BW10, +6.2 @BW50,
    // +6.8 @BW100. Apply the inverse correction, interpolated by BW%, to the WET bus.
    // (>100% extrapolated flat — no Ableton render above 100% yet; refine when available.)
    {
        float bwPctTrim = (apvts.getRawParameterValue ("bandWidth")
                           ? apvts.getRawParameterValue ("bandWidth")->load() : 1.0f) * 100.0f;
        // Measured BR-vs-Ableton level offset per BW (white noise, 40 bands, LOG, Linked, rel 30ms),
        // inverted -> correction dB. Exact at these 6 points; linear interp between; held at ends.
        // (10/50/100 measured 2026-06-13; 130/170/200 added from NOISE renders — curve is non-monotonic.)
        // Re-calibrated 2026-06-14 for the new default SHAPE (carrierBw=1.25): widening the carrier
        // adds +1..+2 dB per BW, so each point = old - delta(BW) (measured BR cBw1.25 vs cBw1.0).
        static const float bwCorr[][2] = {
            { 10.0f,  2.05f }, { 15.0f,  2.10f }, { 20.0f,  0.22f }, { 30.0f, -1.05f },
            { 40.0f, -3.28f }, { 50.0f, -6.70f }, { 100.0f, -8.18f },
            { 130.0f, -9.84f }, { 170.0f, -6.38f }, { 200.0f, -4.54f }
        };
        constexpr int NC = 10;
        float corrDb;
        if      (bwPctTrim <= bwCorr[0][0])      corrDb = bwCorr[0][1];
        else if (bwPctTrim >= bwCorr[NC-1][0])   corrDb = bwCorr[NC-1][1];
        else
        {
            corrDb = bwCorr[NC-1][1];
            for (int k = 0; k < NC - 1; ++k)
                if (bwPctTrim >= bwCorr[k][0] && bwPctTrim <= bwCorr[k+1][0])
                {
                    float t = (bwPctTrim - bwCorr[k][0]) / (bwCorr[k+1][0] - bwCorr[k][0]);
                    corrDb = bwCorr[k][1] * (1.0f - t) + bwCorr[k+1][1] * t;
                    break;
                }
        }
        wet.applyGain (juce::Decibels::decibelsToGain (corrDb));
    }

    // ===== ABLETON TONAL MATCH — REMOVED 2026-06-14 =====
    // An ear A/B (BR EQ-on vs EQ-off vs Ableton, RMS-matched) showed EQ-OFF is closer to Ableton:
    // my low-shelf(-3@110)+high-shelf(+2.5@4k) was derived from leakage-contaminated measurements
    // and was thinning the musical bass / over-brightening. Removed. Volume stays on bwCorr alone.
    // (ablTilt/ablHp members + their prepareToPlay init are now unused, harmless.)

    // ===== POST-VOCODER EQ =====
    // Normal mode (complement OFF): EQ affects ONLY wet, dry stays clean
    // Complement mode (complement ON): EQ affects BOTH wet AND dry (same EQ)
    //   - This prevents dry from interfering with complement's frequencies
    //   - Complement buffer gets inverse EQ and is added to wet
    if (numSamples > 0 && numCh > 0 && currentSampleRate > 0.0 &&
        wet.getNumSamples() > 0 && wet.getNumChannels() > 0)
    {
        // Check if HP/LP is doing something (not at neutral positions) AND filter is enabled
        bool hpActive = inputFilterEnabled && inputHPFreq > 21.0f;
        bool lpActive = inputFilterEnabled && inputLPFreq < 19500.0f;

        // DEBUG: Log filter state (remove after debugging)
        // DBG("hpActive=" << hpActive << " lpActive=" << lpActive << " hpFreq=" << inputHPFreq << " lpFreq=" << inputLPFreq);

        // ===== HP/LP EQ (Master EQ - applies to both wet and dry) =====
        // This ensures the EQ curve matches what you hear
        if (hpActive || lpActive)
        {
            inputFilter.process (wet);
            dryFilter.process (dry);  // Always apply to dry too (master EQ behavior)
        }

        // If complement mode, create inverse-EQ'd signal (will be added to wet AFTER Bell EQ)
        // This fills in the frequencies cut by EQ with clean original
        if (complementMode && complement.getNumSamples() > 0 && (hpActive || lpActive))
        {
            if (hpActive)
            {
                // HP on wet cuts lows → complement buffer gets LP (clean sub)
                // This applies whether LP is also active or not
                // (High complement uses bellComplement, processed separately below)
                complementFilter.setLPFrequency (inputHPFreq);
                complementFilter.setLPSlope (slopeValues[juce::jlimit (0, 5, inputHPSlope)]);
                complementFilter.setLPResonance (inputHPQ, inputHPQWidth);
                complementFilter.setHPFrequency (20.0f);  // No HP filtering
                complementFilter.setHPResonance (0.0f, 1.0f);
                complementFilter.process (complement);
            }
            // Note: When only LP active (no HP), we don't process complement buffer
            // High complement uses bellComplement which is handled below
        }

        // ===== HIGH COMPLEMENT FILTER =====
        // HP filter bellComplement at LP frequency to isolate highs for high complement
        if (lpActive && bellComplement.getNumSamples() > 0)
        {
            highComplementFilter.setHPFrequency (inputLPFreq);
            highComplementFilter.setHPSlope (slopeValues[juce::jlimit (0, 5, inputLPSlope)]);
            highComplementFilter.setLPFrequency (20000.0f);  // No LP filtering
            highComplementFilter.process (bellComplement);
        }

        // NOTE: Bell1/Bell2 are now INPUT EQ (applied before wet/dry split)
        // W1/W2 (compBell) handle frequency-selective wet/dry ratio control

    }

    // ===== CROSSFADE BLENDING =====
    // Blend old and new band outputs during transition
    if (isCrossfading && wetOld.getNumSamples() > 0)
    {
        // Calculate crossfade position (1.0 = all old, 0.0 = all new)
        for (int n = 0; n < numSamples; ++n)
        {
            float fadeProgress = (float) crossfadeSamplesRemaining / (float) crossfadeSamplesTotal;
            fadeProgress = juce::jlimit (0.0f, 1.0f, fadeProgress);

            // Smooth crossfade curve (equal power)
            float oldGain = std::sqrt (fadeProgress);
            float newGain = std::sqrt (1.0f - fadeProgress);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float oldSample = wetOld.getSample (ch, n);
                float newSample = wet.getSample (ch, n);
                wet.setSample (ch, n, oldSample * oldGain + newSample * newGain);
            }

            crossfadeSamplesRemaining--;
            if (crossfadeSamplesRemaining <= 0)
            {
                isCrossfading = false;
                bandsOld.clear (true);
                break;
            }
        }
    }

    // ===== DRY / WET MIX + COMPLEMENT SYSTEM =====
    // Complement bells (compBell1/2) ALWAYS process - they shape wet/dry ratio per frequency
    // Sub/High complement extraction is gated by complementMode toggle
    // This makes the toggle a "view switch" - compBells stay active regardless

    // ===== COMPLEMENT BELLS (ALWAYS ACTIVE) =====
    // Get complement bell parameters - these process regardless of complementMode
    auto* compB1EnPtr = apvts.getRawParameterValue ("compBell1Enabled");
    auto* compB1FreqPtr = apvts.getRawParameterValue ("compBell1Freq");
    auto* compB1GainPtr = apvts.getRawParameterValue ("compBell1Gain");
    auto* compB1QPtr = apvts.getRawParameterValue ("compBell1Q");

    auto* compB2EnPtr = apvts.getRawParameterValue ("compBell2Enabled");
    auto* compB2FreqPtr = apvts.getRawParameterValue ("compBell2Freq");
    auto* compB2GainPtr = apvts.getRawParameterValue ("compBell2Gain");
    auto* compB2QPtr = apvts.getRawParameterValue ("compBell2Q");

    bool compBell1Enabled = compB1EnPtr && (compB1EnPtr->load() > 0.5f);
    bool compBell2Enabled = compB2EnPtr && (compB2EnPtr->load() > 0.5f);

    // Use pre-allocated working buffers for EQ'd wet and dry
    auto& wetEQ = wetEqBuf_;
    auto& dryEQ = dryEqBuf_;
    if (compBell1Enabled || compBell2Enabled)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            wetEQ.copyFrom (ch, 0, wet, ch, 0, numSamples);
            dryEQ.copyFrom (ch, 0, dry, ch, 0, numSamples);
        }
    }

    // Complement bells control how much dry vs wet at specific frequencies
    // Boost (+dB) = more wet at that freq, Cut (-dB) = more dry (protecting it)
    // CORRECT APPROACH: Apply EQ to wet and dry signals BEFORE mixing
    // - Positive gain: BOOST wet, CUT dry → more wet at that frequency
    // - Negative gain: CUT wet, BOOST dry → more dry (protected) at that frequency

    if (compBell1Enabled && compB1GainPtr && currentSampleRate > 0)
    {
        float compGain1 = compB1GainPtr->load();  // -18 to +18 dB
        float freq1 = compB1FreqPtr ? compB1FreqPtr->load() : 1000.0f;
        float q1 = compB1QPtr ? compB1QPtr->load() : 1.0f;

        // Safety: clamp frequency and Q to valid ranges
        freq1 = juce::jlimit (20.0f, (float) currentSampleRate * 0.45f, freq1);
        q1 = juce::jlimit (0.1f, 10.0f, q1);

        if (std::abs (compGain1) > 0.1f)
        {
            // Convert dB to linear gain
            float linearGain = juce::Decibels::decibelsToGain (compGain1);
            float inverseGain = 1.0f / linearGain;  // Inverse for the other signal
            // Limit inverse gain to prevent extreme phase distortion affecting other frequencies
            inverseGain = juce::jlimit (0.25f, 4.0f, inverseGain);  // Max ±12dB inverse

            // Wet signal: apply the gain as-is (positive = boost wet)
            auto wetCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, freq1, q1, linearGain);

            // Dry signal: apply INVERSE gain (positive compGain = cut dry)
            auto dryCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, freq1, q1, inverseGain);

            if (wetCoeffs != nullptr && dryCoeffs != nullptr)
            {
                // Apply to wet using dedicated wet filters
                compBell1WetL.coefficients = wetCoeffs;
                compBell1WetR.coefficients = wetCoeffs;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* wetPtr = wetEQ.getWritePointer (ch);
                    auto& filter = (ch == 0) ? compBell1WetL : compBell1WetR;
                    for (int n = 0; n < numSamples; ++n)
                        wetPtr[n] = filter.processSample (wetPtr[n]);
                }

                // Apply inverse to dry using dedicated dry filters (NO reset needed!)
                compBell1DryL.coefficients = dryCoeffs;
                compBell1DryR.coefficients = dryCoeffs;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* dryPtr = dryEQ.getWritePointer (ch);
                    auto& filter = (ch == 0) ? compBell1DryL : compBell1DryR;
                    for (int n = 0; n < numSamples; ++n)
                        dryPtr[n] = filter.processSample (dryPtr[n]);
                }
            }
        }
    }

    if (compBell2Enabled && compB2GainPtr && currentSampleRate > 0)
    {
        float compGain2 = compB2GainPtr->load();  // -18 to +18 dB
        float freq2 = compB2FreqPtr ? compB2FreqPtr->load() : 4000.0f;
        float q2 = compB2QPtr ? compB2QPtr->load() : 1.0f;

        freq2 = juce::jlimit (20.0f, (float) currentSampleRate * 0.45f, freq2);
        q2 = juce::jlimit (0.1f, 10.0f, q2);

        if (std::abs (compGain2) > 0.1f)
        {
            float linearGain = juce::Decibels::decibelsToGain (compGain2);
            float inverseGain = 1.0f / linearGain;
            // Limit inverse gain to prevent extreme phase distortion affecting other frequencies
            inverseGain = juce::jlimit (0.25f, 4.0f, inverseGain);  // Max ±12dB inverse

            auto wetCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, freq2, q2, linearGain);
            auto dryCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, freq2, q2, inverseGain);

            if (wetCoeffs != nullptr && dryCoeffs != nullptr)
            {
                // Apply to wet using dedicated wet filters
                compBell2WetL.coefficients = wetCoeffs;
                compBell2WetR.coefficients = wetCoeffs;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* wetPtr = wetEQ.getWritePointer (ch);
                    auto& filter = (ch == 0) ? compBell2WetL : compBell2WetR;
                    for (int n = 0; n < numSamples; ++n)
                        wetPtr[n] = filter.processSample (wetPtr[n]);
                }

                // Apply inverse to dry using dedicated dry filters (NO reset needed!)
                compBell2DryL.coefficients = dryCoeffs;
                compBell2DryR.coefficients = dryCoeffs;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* dryPtr = dryEQ.getWritePointer (ch);
                    auto& filter = (ch == 0) ? compBell2DryL : compBell2DryR;
                    for (int n = 0; n < numSamples; ++n)
                        dryPtr[n] = filter.processSample (dryPtr[n]);
                }
            }
        }
    }

    // ===== WET/DRY MIX =====
    // Use EQ'd buffers if compBells active, otherwise raw buffers
    if (compBell1Enabled || compBell2Enabled)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            auto* d = dryEQ.getReadPointer (ch);
            auto* w = wetEQ.getReadPointer (ch);

            for (int n = 0; n < numSamples; ++n)
                out[n] = d[n] * (1.0f - mix) + w[n] * mix;
        }
    }
    else
    {
        // No complement bells active - use standard wet/dry mix
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            auto* d = dry.getReadPointer (ch);
            auto* w = wet.getReadPointer (ch);

            for (int n = 0; n < numSamples; ++n)
                out[n] = d[n] * (1.0f - mix) + w[n] * mix;
        }
    }

    // ===== SUB/HIGH COMPLEMENT (GATED BY TOGGLE) =====
    // These add clean sub/highs from the original signal - only when complement mode is ON
    if (complementMode)
    {
        // Get filter frequencies to define band boundaries
        auto* filterEnPtr = apvts.getRawParameterValue ("inputFilterEnabled");
        auto* hpFrPtr = apvts.getRawParameterValue ("inputHPFreq");
        auto* lpFrPtr = apvts.getRawParameterValue ("inputLPFreq");
        auto* subLevelPtr = apvts.getRawParameterValue ("subComplementLevel");
        auto* highLevelPtr = apvts.getRawParameterValue ("highComplementLevel");

        bool filterEnabled = filterEnPtr ? (filterEnPtr->load() > 0.5f) : true;
        float hpFreq = hpFrPtr ? hpFrPtr->load() : 20.0f;
        float lpFreq = lpFrPtr ? lpFrPtr->load() : 20000.0f;
        float subLevel = subLevelPtr ? juce::Decibels::decibelsToGain (subLevelPtr->load()) : 1.0f;
        float highLevel = highLevelPtr ? juce::Decibels::decibelsToGain (highLevelPtr->load()) : 1.0f;

        bool hpActive = filterEnabled && (hpFreq > 25.0f);
        bool lpActive = filterEnabled && (lpFreq < 19500.0f);

        // ===== SUB COMPLEMENT (Fixed level, ignores mix) =====
        // Clean sub from original input, always at fixed level when complement is ON
        // MUST be added AFTER wet/dry mix so it doesn't get overwritten!
        if (hpActive && complement.getNumSamples() > 0 && complement.getNumChannels() >= numCh)
        {
            // Sub complement: LP-filtered original (below HP frequency)
            // This is already prepared in the complement buffer
            // Add at fixed level (ignoring global mix)
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* out = buffer.getWritePointer (ch);
                auto* sub = complement.getReadPointer (ch);
                for (int n = 0; n < numSamples; ++n)
                    out[n] = out[n] + sub[n] * subLevel;  // Add sub at fixed level
            }
        }

        // ===== HIGH COMPLEMENT (Follows mix knob) =====
        // Clean highs from original input (above LP frequency)
        // Unlike sub (fixed level), highs follow the mix knob for natural blending
        if (lpActive && complement.getNumSamples() > 0)
        {
            // High complement uses the high-passed portion of the original
            // Level scales with mix: at 100% wet you get full highs, at 0% wet you get none
            // (at 0% wet you have full dry signal anyway, so no complement needed)

            if (bellComplement.getNumSamples() > 0 && bellComplement.getNumChannels() >= numCh)
            {
                float highGain = highLevel * mix;  // Follow mix knob
                for (int ch = 0; ch < numCh; ++ch)
                    buffer.addFrom (ch, 0, bellComplement, ch, 0, numSamples, highGain);
            }
        }
    }

    // ===== NOTE: MASTER GATE DISABLED =====
    // Per-band spectral gating is now applied inside the band loop (see above).
    // Each band gates independently based on its own envelope level.
    // This eliminates the "drone" problem where silent vocal frequencies still output carrier.
    //
    // The master gate member variables are kept for potential future use but are not applied here.
    // If you need overall output gating, use a separate gate plugin in your DAW chain.
    juce::ignoreUnused (masterGateEnv, masterGateGainSmoothed);

    // ===== TRIAL NAG SYSTEM =====
    // Every 7 seconds: 0.7 seconds of silence (mute output)
    // Only applies during valid trial (not licensed, not expired)
    if (licenseManager.shouldApplyTrialNag())
    {
        for (int n = 0; n < numSamples; ++n)
        {
            nagSampleCounter++;

            // Check if we should start a new nag burst
            if (nagSampleCounter >= nagIntervalSamples)
            {
                nagSampleCounter = 0;
                nagActiveSamples = nagDurationSamples;
            }

            // If nag is active, mute output (silence)
            if (nagActiveSamples > 0)
            {
                nagActiveSamples--;

                // Silence all channels
                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* out = buffer.getWritePointer (ch);
                    out[n] = 0.0f;  // Silence
                }
            }
        }
    }
    // =============================

    // ===== SPECTRUM ANALYZER FFT - DISABLED FOR CPU OPTIMIZATION =====
    // Triple FFT was consuming ~30-40% CPU. Disabled for beta v1.3.
    // The spectrum visualization in the GUI will show flat/empty.
    // TODO: Re-enable with optimized single FFT in future release.
    #if 0
    if (fftInitialized && fft != nullptr && window != nullptr)
    {
        // ... FFT code disabled ...
    }
    #endif
    juce::ignoreUnused (fftInitialized, fft, window);

    // (Debug WAV output recording removed — file I/O on audio thread causes heap corruption)

    // ===== OUTPUT GAIN =====
    // Only apply to main bus channels (not sidechain)
    {
        const int mainChCount = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
        auto* outputGainPtr = apvts.getRawParameterValue ("outputGain");
        if (outputGainPtr != nullptr)
        {
            float gainDb = outputGainPtr->load();
            if (std::abs (gainDb) > 0.01f)
            {
                float linearGain = juce::Decibels::decibelsToGain (gainDb);
                for (int ch = 0; ch < mainChCount; ++ch)
                    buffer.applyGain (ch, 0, buffer.getNumSamples(), linearGain);
            }
        }
        // Compute output RMS
        float rms = 0.0f;
        for (int ch = 0; ch < mainChCount; ++ch)
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
        rms /= juce::jmax (1, mainChCount);
        outputRmsLevel.store (rms);
    }
}


//==============================================================================
// CUSTOM CHORD EDITOR
//==============================================================================

void ColorVocoderAudioProcessor::setCustomChord (int keyChar, const std::vector<int>& midiNotes)
{
    juce::SpinLock::ScopedLockType lock (customChordLock);
    if (midiNotes.empty())
        customChords.erase (keyChar);
    else
        customChords[keyChar] = midiNotes;
}

std::vector<int> ColorVocoderAudioProcessor::getCustomChord (int keyChar) const
{
    juce::SpinLock::ScopedLockType lock (customChordLock);
    auto it = customChords.find (keyChar);
    return (it != customChords.end()) ? it->second : std::vector<int>();
}

bool ColorVocoderAudioProcessor::hasCustomChord (int keyChar) const
{
    juce::SpinLock::ScopedLockType lock (customChordLock);
    auto it = customChords.find (keyChar);
    return it != customChords.end() && !it->second.empty();
}

void ColorVocoderAudioProcessor::clearCustomChord (int keyChar)
{
    juce::SpinLock::ScopedLockType lock (customChordLock);
    customChords.erase (keyChar);
}

void ColorVocoderAudioProcessor::clearAllCustomChords()
{
    juce::SpinLock::ScopedLockType lock (customChordLock);
    customChords.clear();
}

//==============================================================================
// STATE
//==============================================================================

void ColorVocoderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // Save per-band gains as a child element
    auto* bandGainsElement = xml->createNewChildElement ("BandGains");
    juce::String gainsStr;
    for (int i = 0; i < maxBands; ++i)
    {
        if (i > 0) gainsStr += ",";
        gainsStr += juce::String (bandGains[(size_t) i], 4);  // 4 decimal places
    }
    bandGainsElement->setAttribute ("values", gainsStr);

    // Save band distribution curve
    auto* distCurveElement = xml->createNewChildElement ("BandDistribution");
    juce::String curveStr;
    for (size_t i = 0; i < bandDistributionCurve.size(); ++i)
    {
        if (i > 0) curveStr += ",";
        curveStr += juce::String (bandDistributionCurve[i], 4);
    }
    distCurveElement->setAttribute ("values", curveStr);

    // Save note exclusions (piano keyboard mutes)
    auto* noteExclusionsElement = xml->createNewChildElement ("NoteExclusions");
    juce::String exclusionsStr;
    for (int i = 0; i < 12; ++i)
        exclusionsStr += (noteExclusions[(size_t) i] ? "1" : "0");
    noteExclusionsElement->setAttribute ("values", exclusionsStr);

    // Save per-band Q multipliers and lock states
    auto* perBandQElement = xml->createNewChildElement ("PerBandQ");
    juce::String qMultStr;
    for (int i = 0; i < maxBands; ++i)
    {
        if (i > 0) qMultStr += ",";
        qMultStr += juce::String (perBandQMultiplier[(size_t) i], 4);
    }
    perBandQElement->setAttribute ("multipliers", qMultStr);
    juce::String lockStr;
    for (int i = 0; i < maxBands; ++i)
        lockStr += (perBandQLocked[(size_t) i] ? "1" : "0");
    perBandQElement->setAttribute ("locks", lockStr);

    // Save band offset curve + global offset
    auto* offsetCurveElement = xml->createNewChildElement ("BandOffsetCurve");
    juce::String offsetCurveStr;
    for (size_t i = 0; i < bandOffsetCurve.size(); ++i)
    {
        if (i > 0) offsetCurveStr += ",";
        offsetCurveStr += juce::String (bandOffsetCurve[i], 4);
    }
    offsetCurveElement->setAttribute ("values", offsetCurveStr);
    offsetCurveElement->setAttribute ("globalOffset", (double) globalBandOffset);

    // Save bank glide curve (keytrack/position portamento easing)
    auto* bankGlideElement = xml->createNewChildElement ("BankGlideCurve");
    juce::String bankGlideStr;
    for (size_t i = 0; i < bankGlideCurve.size(); ++i)
    {
        if (i > 0) bankGlideStr += ",";
        bankGlideStr += juce::String (bankGlideCurve[i], 4);
    }
    bankGlideElement->setAttribute ("values", bankGlideStr);

    // Save MIDI Chord Trigger variation memory
    auto* midiChordVarElement = xml->createNewChildElement ("MidiChordVarMemory");
    juce::String varMemStr;
    for (int i = 0; i < 7; ++i)
    {
        if (i > 0) varMemStr += ",";
        varMemStr += juce::String (lastVariationPerDegree[(size_t) i]);
    }
    midiChordVarElement->setAttribute ("values", varMemStr);

    // Save custom chords
    {
        juce::SpinLock::ScopedLockType lock (customChordLock);
        if (!customChords.empty())
        {
            auto* customChordsElement = xml->createNewChildElement ("CustomChords");
            for (const auto& [keyChar, notes] : customChords)
            {
                auto* chordElement = customChordsElement->createNewChildElement ("Chord");
                chordElement->setAttribute ("key", keyChar);
                juce::String notesStr;
                for (size_t i = 0; i < notes.size(); ++i)
                {
                    if (i > 0) notesStr += ",";
                    notesStr += juce::String (notes[i]);
                }
                chordElement->setAttribute ("notes", notesStr);
            }
        }
    }

    copyXmlToBinary (*xml, destData);
}

void ColorVocoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
    {
        // Load per-band gains if present
        if (auto* bandGainsElement = xmlState->getChildByName ("BandGains"))
        {
            juce::String gainsStr = bandGainsElement->getStringAttribute ("values");
            juce::StringArray tokens;
            tokens.addTokens (gainsStr, ",", "");

            resetAllBandGains();  // Start with defaults
            for (int i = 0; i < juce::jmin (tokens.size(), maxBands); ++i)
                bandGains[(size_t) i] = juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue());
        }

        // Load band distribution curve if present
        if (auto* distCurveElement = xmlState->getChildByName ("BandDistribution"))
        {
            juce::String curveStr = distCurveElement->getStringAttribute ("values");
            juce::StringArray tokens;
            tokens.addTokens (curveStr, ",", "");

            if (tokens.size() >= 2)
            {
                bandDistributionCurve.resize ((size_t) tokens.size());
                for (int i = 0; i < tokens.size(); ++i)
                    bandDistributionCurve[(size_t) i] = juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue());
            }
        }

        // Load note exclusions if present
        if (auto* noteExclusionsElement = xmlState->getChildByName ("NoteExclusions"))
        {
            juce::String exclusionsStr = noteExclusionsElement->getStringAttribute ("values");
            // Reset all exclusions first
            noteExclusions.fill (false);
            // Load saved exclusions
            for (int i = 0; i < 12 && i < exclusionsStr.length(); ++i)
                noteExclusions[(size_t) i] = (exclusionsStr[i] == '1');
        }

        // Load per-band Q multipliers and lock states if present
        if (auto* perBandQElement = xmlState->getChildByName ("PerBandQ"))
        {
            juce::String qMultStr = perBandQElement->getStringAttribute ("multipliers");
            juce::StringArray tokens;
            tokens.addTokens (qMultStr, ",", "");

            perBandQMultiplier.fill (1.0f);  // Default to 1.0
            for (int i = 0; i < juce::jmin (tokens.size(), maxBands); ++i)
                perBandQMultiplier[(size_t) i] = juce::jlimit (0.01f, 100.0f, tokens[i].getFloatValue());

            juce::String lockStr = perBandQElement->getStringAttribute ("locks");
            perBandQLocked.fill (false);
            for (int i = 0; i < maxBands && i < lockStr.length(); ++i)
                perBandQLocked[(size_t) i] = (lockStr[i] == '1');
        }

        // Load band offset curve + global offset if present
        if (auto* offsetCurveElement = xmlState->getChildByName ("BandOffsetCurve"))
        {
            juce::String offsetCurveStr = offsetCurveElement->getStringAttribute ("values");
            juce::StringArray tokens;
            tokens.addTokens (offsetCurveStr, ",", "");

            if (tokens.size() >= 2)
            {
                bandOffsetCurve.resize ((size_t) tokens.size());
                for (int i = 0; i < tokens.size(); ++i)
                    bandOffsetCurve[(size_t) i] = juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue());
            }

            globalBandOffset = juce::jlimit (-12.0f, 12.0f, (float) offsetCurveElement->getDoubleAttribute ("globalOffset", 0.0));
        }
        else
        {
            // Older presets without offset curve — reset to defaults
            resetBandOffsetCurve();
            globalBandOffset = 0.0f;
        }

        // Load bank glide curve (keytrack/position portamento easing)
        if (auto* bankGlideElement = xmlState->getChildByName ("BankGlideCurve"))
        {
            juce::String bankGlideStr = bankGlideElement->getStringAttribute ("values");
            juce::StringArray tokens;
            tokens.addTokens (bankGlideStr, ",", "");

            if (tokens.size() >= 2)
            {
                bankGlideCurve.resize ((size_t) tokens.size());
                for (int i = 0; i < tokens.size(); ++i)
                    bankGlideCurve[(size_t) i] = juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue());
            }
        }
        else
        {
            // Older presets without a bank glide curve — reset to linear (plain linear glide)
            resetBankGlideCurve();
        }

        // Load MIDI Chord Trigger variation memory
        if (auto* midiChordVarElement = xmlState->getChildByName ("MidiChordVarMemory"))
        {
            juce::String varMemStr = midiChordVarElement->getStringAttribute ("values");
            juce::StringArray tokens;
            tokens.addTokens (varMemStr, ",", "");

            lastVariationPerDegree.fill (0);
            for (int i = 0; i < juce::jmin (tokens.size(), 7); ++i)
                lastVariationPerDegree[(size_t) i] = juce::jlimit (0, 9, tokens[i].getIntValue());
        }

        // Load custom chords
        {
            juce::SpinLock::ScopedLockType lock (customChordLock);
            customChords.clear();
            if (auto* customChordsElement = xmlState->getChildByName ("CustomChords"))
            {
                for (auto* chordElement = customChordsElement->getFirstChildElement();
                     chordElement != nullptr;
                     chordElement = chordElement->getNextElement())
                {
                    int keyChar = chordElement->getIntAttribute ("key", 0);
                    juce::String notesStr = chordElement->getStringAttribute ("notes");
                    juce::StringArray tokens;
                    tokens.addTokens (notesStr, ",", "");

                    std::vector<int> notes;
                    for (const auto& t : tokens)
                        notes.push_back (t.getIntValue());

                    if (keyChar > 0 && !notes.empty())
                        customChords[keyChar] = notes;
                }
            }
        }

        // Backward compatibility: remap chord variation for old presets (7 → 10 choices)
        // Old presets don't have CustomChords element and used 7-item normalization
        bool isOldFormat = (xmlState->getChildByName ("CustomChords") == nullptr);

        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

        // A restored project/preset already carries the user's bandCount — mark it initialised
        // so the editor does NOT auto-max over it when it opens.
        bandCountInitialized = true;

        // Sync the band-rebuild caches to the freshly-loaded params so the next processBlock does NOT
        // see a spurious "change" and wipe the band gains we just restored. Then request a clean,
        // gain-preserving rebuild via the dirty flag (consumed on the audio thread).
        if (auto* p = apvts.getRawParameterValue ("bandCount"))  lastBandCount = (int) p->load();
        if (auto* p = apvts.getRawParameterValue ("key"))        lastKey       = (int) p->load();
        if (auto* p = apvts.getRawParameterValue ("scaleType"))  lastScaleType = (int) p->load();
        if (auto* p = apvts.getRawParameterValue ("lowFreq"))    lastLowFreq   = p->load();
        if (auto* p = apvts.getRawParameterValue ("highFreq"))   lastHighFreq  = p->load();
        if (auto* p = apvts.getRawParameterValue ("harmonicShift")) lastHarmShift = p->load();
        if (auto* p = apvts.getRawParameterValue ("scaleAmount")) lastScaleAmt = p->load();
        if (auto* p = apvts.getRawParameterValue ("bandWidth"))  lastBandWidth = p->load();
        if (auto* p = apvts.getRawParameterValue ("chordDegree")) lastChordDegree = (int) p->load();
        if (auto* p = apvts.getRawParameterValue ("chordVariation")) lastChordVariation = (int) p->load();
        bandDistributionDirty.store (true);

        if (isOldFormat)
        {
            // Fix chord variation: old normalized as idx/6, new needs idx/9
            if (auto* varParam = apvts.getRawParameterValue ("chordVariation"))
            {
                int currentIdx = (int) varParam->load();
                // Reverse-map: old norm = oldIdx/6, loaded as round(oldNorm*9)
                // oldIdx: 0→0, 1→2, 2→3, 3→5, 4→6, 5→8, 6→9
                // Reverse: 0→0, 2→1, 3→2, 5→3, 6→4, 8→5, 9→6
                static const int reverseMap[10] = { 0, 0, 1, 2, 2, 3, 4, 4, 5, 6 };
                int fixedIdx = reverseMap[juce::jlimit (0, 9, currentIdx)];
                if (fixedIdx != currentIdx)
                {
                    if (auto* param = apvts.getParameter ("chordVariation"))
                        param->setValueNotifyingHost ((float) fixedIdx / 9.0f);
                }
            }
        }
    }
}

//==============================================================================
// EDITOR / ENTRY POINT
//==============================================================================

juce::AudioProcessorEditor* ColorVocoderAudioProcessor::createEditor()
{
#if USE_WEBVIEW_GUI && JUCE_WEB_BROWSER
    return new WebViewEditor(*this);
#else
    return new ColorVocoderAudioProcessorEditor (*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ColorVocoderAudioProcessor();
}
