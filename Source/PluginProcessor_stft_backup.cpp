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
        "mix", "Mix", 0.0f, 1.0f, 0.5f));

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
            "Chromatic"        // 14
        }, 0));

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

    // ===== MIDI CHORD TRIGGER =====
    // When enabled, MIDI keys in octave 2/3 select chord degree and variation
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "midiChordTrigger", "MIDI Chord Trigger", false));

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

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lowFreq", "Low Freq",
        juce::NormalisableRange<float> (20.0f, 500.0f, 0.01f, 0.5f),
        20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("highFreq", "High Freq",
        juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.5f),
        20000.0f)); // full spectrum by default

    // Envelope for bands
    // Attack: 0..100 ms, skewed so low values have more resolution
    // Default 1ms for crisp transients and full volume (like Ableton's Vocoder)
    // Note: 30ms (Vocodex-style) sounds quieter because slow attack misses peaks
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attackMs", "Attack (ms)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f, 0.5f),
        1.0f));

    // Release: 0..1000 ms, skewed so 0–100ms region is easier to fine-tune
    // Default 30ms (like FL Studio Vocodex)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("releaseMs", "Release (ms)",
        juce::NormalisableRange<float> (0.0f, 1000.0f, 0.1f, 0.5f),
        30.0f));

    // Adaptive Envelope: OFF = flat timing (zappy, Ableton-style), ON = per-band adaptive (smoother)
    params.push_back (std::make_unique<juce::AudioParameterBool> ("adaptiveEnv", "Adaptive Env", false));

    // Band shape (this is our "Bandwidth" UI)
    // Range 0.01-2.0, default 1.0
    // 0.01 = ultra-narrow (Q~200, great for scale-locked mode)
    // 0.1  = narrow (Q~60, matches Ableton's 10%)
    // 1.0  = normal (Q~1, matches Ableton's 100%)
    // 2.0  = very wide (Q~0.5, max overlap)
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bandWidth", "Band Width",
        0.01f, 2.0f, 1.0f)); // low = narrow, high = wide

    // ===== ALGORITHM MODE PARAMETERS =====
    // Algorithm Mode: 0=RITUAL (fixed modulator Q), 1=LINKED (same Q for both), 2=SPLIT (separate Q controls)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "algorithmMode", "Algorithm Mode",
        juce::StringArray { "Ritual", "Linked", "Split" }, 0));

    // Modulator Q (only used in SPLIT mode) - 1.0 to 60.0
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "modulatorQ", "Modulator Q",
        juce::NormalisableRange<float> (1.0f, 60.0f, 0.1f, 0.4f), 2.0f));


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
    params.push_back (std::make_unique<juce::AudioParameterBool> ("inputFilterEnabled", "Input Filter On", true));

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
    // Default 15 (light gating to reduce noise)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gate", "Gate",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f));

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

    updateEnvelopeCoeffs();
    rebuildBands();

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
    smoothedFormant.reset (sampleRate, 0.08);

    // Initialize to current parameter values
    if (auto* ptr = apvts.getRawParameterValue ("spectralTilt"))
        smoothedSpectralTilt.setCurrentAndTargetValue (ptr->load());
    if (auto* ptr = apvts.getRawParameterValue ("spectralIntensity"))
        smoothedSpectralIntensity.setCurrentAndTargetValue (ptr->load());
    if (auto* ptr = apvts.getRawParameterValue ("formant"))
        smoothedFormant.setCurrentAndTargetValue (ptr->load());

    // ===== CPU OPTIMIZATION: Pre-allocate ALL processBlock buffers =====
    bandEnvBuffer.resize ((size_t) samplesPerBlock);

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

    // ===== STFT VOCODER =====
    stftVocoder.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (stftVocoder.getLatencySamples());

    // ===== PHASE 1: Initialize carrier generator =====
    carrierGenerator.prepare (sampleRate, samplesPerBlock);

    // Allocate carrier buffer
    carrierBuffer.setSize (2, samplesPerBlock);

    // Initialize unvoiced detection filters
    // LPF at 1000 Hz - captures voiced (vowel) content
    // HPF at 3000 Hz - captures unvoiced (consonant) content like S, F, T
    auto lpCoeffs = juce::IIRCoefficients::makeLowPass (sampleRate, 1000.0);
    auto hpCoeffs = juce::IIRCoefficients::makeHighPass (sampleRate, 3000.0);
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

    // Global coefficients (used for noise, HP blend, and as fallback)
    attackCoeff  = std::exp (-1.0f / (currentSampleRate * attackSec));
    releaseCoeff = std::exp (-1.0f / (currentSampleRate * releaseSec));

    // Noise uses same release as bands (Ableton "Fast" style)
    // No minimum floor - follows user's release setting exactly
    noiseReleaseCoeff = releaseCoeff;

    // ===== PER-BAND ENVELOPE COEFFICIENTS =====
    bool adaptiveEnvEnabled = apvts.getRawParameterValue ("adaptiveEnv")->load() > 0.5f;

    {
        juce::SpinLock::ScopedLockType lock (bandsLock);

        // Find reference bandwidth for normalization (median band)
        float refBW = 500.0f;  // fallback
        if (! bands.isEmpty())
        {
            int midIdx = bands.size() / 2;
            float midFreq = juce::jmax (20.0f, bands[midIdx]->currentFreq);
            refBW = midFreq / juce::jmax (0.5f, bands[midIdx]->analysisQ);
        }

        for (auto* b : bands)
        {
            float freq = juce::jmax (20.0f, b->currentFreq);
            float bw   = freq / juce::jmax (0.5f, b->analysisQ);  // bandwidth in Hz

            // ===== BANDWIDTH NORMALIZATION =====
            // Log-spaced bands have wider bandwidth at higher frequencies,
            // capturing more total energy from broadband input.
            // Normalize so all bands read equal with flat-spectrum (white noise) input.
            // Exponent 0.35 calibrated via automated comparison with Ableton's vocoder
            // (0.5/sqrt was too aggressive, caused -0.88 dB/oct HF rolloff)
            b->bwNormGain = 1.0f;  // CALIBRATION: measuring raw uncompensated levels

            if (adaptiveEnvEnabled)
            {
                // ===== ADAPTIVE ENVELOPE TIMING =====
                // Per-band timing scaled by bandwidth. Smoother, prevents cycle-tracking
                // on low bands, but less reactive than flat timing.
                float adaptiveEnvCutoff = bw / 3.0f;
                float adaptiveReleaseMs = juce::jlimit (0.1f, 200.0f, 1000.0f / adaptiveEnvCutoff);

                float userScale = releaseMs / 30.0f;
                float finalReleaseMs = juce::jlimit (0.1f, 2000.0f, adaptiveReleaseMs * juce::jmax (0.01f, userScale));
                float finalReleaseSec = juce::jmax (finalReleaseMs * 0.001f, 0.0001f);

                float adaptiveAttackMs = juce::jlimit (0.05f, 50.0f, adaptiveReleaseMs * 0.1f);
                float attackScale = attackMs / 1.0f;
                float finalAttackMs = juce::jlimit (0.05f, 500.0f, adaptiveAttackMs * juce::jmax (0.01f, attackScale));
                float finalAttackSec = juce::jmax (finalAttackMs * 0.001f, 0.0001f);

                b->bandAttackCoeff  = std::exp (-1.0f / (float(currentSampleRate) * finalAttackSec));
                b->bandReleaseCoeff = std::exp (-1.0f / (float(currentSampleRate) * finalReleaseSec));
            }
            else
            {
                // ===== FLAT ENVELOPE TIMING (Ableton-style) =====
                // All bands use exact user-set attack/release. Zappy, reactive.
                b->bandAttackCoeff  = attackCoeff;
                b->bandReleaseCoeff = releaseCoeff;
            }
        }
    }

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

    // Build scale notes for this range
    buildScaleNoteFrequencies (scaleNoteFreqs, key, scaleType, lowFreq, highFreq);
    const int nNotes = (int) scaleNoteFreqs.size();

    std::vector<float> baseFreqs  ((size_t) nBands);
    std::vector<float> scaleFreqs ((size_t) nBands);

    // Base freqs distributed according to distribution curve (default = linear in log frequency)
    float ratio = highFreq / juce::jmax (lowFreq, 20.0f);
    for (int i = 0; i < nBands; ++i)
    {
        // Linear position 0-1 across bands
        float linearPos = (nBands > 1 ? (float) i / (float) (nBands - 1) : 0.0f);

        // Look up distribution curve to get actual frequency position
        // Curve has 101 points (0-100), interpolate for smooth results
        float t;
        if (bandDistributionCurve.size() >= 2)
        {
            float curveIdx = linearPos * (float) (bandDistributionCurve.size() - 1);
            int idx0 = (int) curveIdx;
            int idx1 = juce::jmin (idx0 + 1, (int) bandDistributionCurve.size() - 1);
            float frac = curveIdx - (float) idx0;
            t = bandDistributionCurve[(size_t) idx0] * (1.0f - frac) + bandDistributionCurve[(size_t) idx1] * frac;
        }
        else
        {
            t = linearPos;  // Fallback to linear if curve is empty
        }

        baseFreqs[(size_t) i] = lowFreq * std::pow (ratio, t);
    }

    // Map bands across scale notes monotonically (avoid piling many bands on one note)
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

        b->analysisFilter.reset();
        b->carrierFilterL.reset();
        b->carrierFilterR.reset();
        b->carrierFilterL2.reset();
        b->carrierFilterR2.reset();

        // ====== CRITICAL: Initialize filter coefficients to prevent NaN/crash ======
        // Filters MUST have coefficients before processSample() is called.
        // Use a safe default - updateBandFilters() will set proper values later.
        double safeSampleRate = (currentSampleRate > 0.0) ? currentSampleRate : 44100.0;
        auto defaultCoeffs = juce::IIRCoefficients::makeLowPass(safeSampleRate, 1000.0);
        b->analysisFilter.setCoefficients(defaultCoeffs);
        b->carrierFilterL.setCoefficients(defaultCoeffs);
        b->carrierFilterR.setCoefficients(defaultCoeffs);
        b->carrierFilterL2.setCoefficients(defaultCoeffs);
        b->carrierFilterR2.setCoefficients(defaultCoeffs);
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


    // Normalise BandWidth (0.01..2.0) -> norm 0..1
    // norm = 0   -> narrowest (Q~200, ultra-narrow for scale-locked)
    // norm = 1   -> widest (Q~0.5, maximum overlap)
    float norm = (bandWidth - 0.01f) / (2.0f - 0.01f);
    norm = juce::jlimit (0.0f, 1.0f, norm);

    // Log-style mapping calibrated to match Ableton's vocoder:
    //   BW 0.01 → Q~200 (ultra-narrow, below Ableton's range)
    //   BW 0.1  → Q~57  (matches Ableton 10%, measured Q=57.6)
    //   BW 1.0  → Q~10  (Ableton 100% appears Q≈0 but that's measurement artifact from overlapping bands)
    //   BW 2.0  → Q~0.5 (wider than Ableton, max overlap)
    const float minQ = 0.5f;    // very wide (Ableton ~200% equivalent)
    const float maxQ = 200.0f;  // ultra-narrow for scale-locked mode
    float QcarrierBase = minQ * std::pow (maxQ / minQ, 1.0f - norm);

    // Sync globalBandQ so carousel/GUI reads the current knob-derived Q
    globalBandQ = QcarrierBase;

    float formantFactor = std::pow (2.0f, formantSemis / 12.0f);

    const int nBands = bands.size();

    // CPU OPT: Hoist parameter lookup outside band loop
    auto* midiFollowParam = apvts.getRawParameterValue ("midiFollow");
    bool midiFollowEnabled = midiFollowParam ? (midiFollowParam->load() > 0.5f) : false;

    for (int i = 0; i < nBands; ++i)
    {
        auto* b = bands[i];
        if (b == nullptr) continue;  // Safety: skip null band pointers

        float fBase  = b->centerFreqBase;
        float fScale = b->centerFreqScale;

        float fAnalysis = juce::jlimit (20.0f, (float) (0.45 * currentSampleRate), fBase);

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

            // Map to fractional carrier band index and interpolate frequency
            float carrierBandF = offsetNorm * (float) (nBands - 1);
            int cb0 = juce::jlimit (0, nBands - 1, (int) carrierBandF);
            int cb1 = juce::jlimit (0, nBands - 1, cb0 + 1);
            float cbFrac = carrierBandF - (float) cb0;

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
        float fCarrier = fRaw * formantFactor;

        if (midiFollowEnabled && !midiNoteFreqs.empty())
        {
            // MIDI Follow mode - snap to held MIDI notes with FULL gating
            float fMidi = snapToMidi (fCarrier);

            // Calculate distance in semitones for gating
            float distanceSemitones = std::abs (12.0f * std::log2 (fCarrier / fMidi));
            float gateThreshold = 1.0f;   // Within 1 semitone: full volume
            float gateFalloff = 2.0f;     // 1-3 semitones: fade out, beyond 3: silent
            float midiGate = 1.0f - juce::jlimit (0.0f, 1.0f, (distanceSemitones - gateThreshold) / gateFalloff);
            b->chordGate = midiGate;      // Full gating: 1 = near MIDI note, 0 = silent

            if (b->chordGate > 0.01f)
                fCarrier = fMidi;         // Full snap to nearest MIDI note
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
                // LINKED: asymptotic soft-cap — matches carrier at low Q,
                // caps analysis at ~48 so modulator tracks carrier closely.
                const float maxAnalysisQ = 48.0f;
                Qanalysis = maxAnalysisQ * Qcarrier / (Qcarrier + maxAnalysisQ);
                break;
            }
            case 2:  Qanalysis = modulatorQParam; break; // SPLIT: user-controlled
            default: Qanalysis = 2.0f; break;
        }

        juce::IIRCoefficients bpA = juce::IIRCoefficients::makeBandPass (currentSampleRate, fAnalysis, Qanalysis);
        juce::IIRCoefficients bpC = juce::IIRCoefficients::makeBandPass (currentSampleRate, fCarrier, Qcarrier);

        b->analysisFilter.setCoefficients (bpA);

        // Stacked carrier filters (same coeffs) for 4th-order response
        b->carrierFilterL .setCoefficients (bpC);
        b->carrierFilterR .setCoefficients (bpC);
        b->carrierFilterL2.setCoefficients (bpC);
        b->carrierFilterR2.setCoefficients (bpC);

        b->currentFreq = fAnalysis;  // Analysis frequency (where envelope is detected)
        b->carrierFreq = fCarrier;   // Carrier frequency (can be pitch-shifted)
        b->analysisQ = Qanalysis;  // Store for envelope compensation in processBlock
        b->cachedPitchClass = frequencyToPitchClass (fAnalysis);  // CPU OPT: cache pitch class
    }

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
                // STACK ON: sum gated envelopes for all bands in same group
                groupEnvSums[b->mergeGroupId] += b->env * b->chordGate;
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
                // STACK ON: use combined envelope (already gated in sum above)
                envs.push_back (groupEnvSums[b->mergeGroupId]);
            }
            else
            {
                // STACK OFF: just this band's envelope, gated by chord/MIDI
                envs.push_back (b->env * b->chordGate);
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

        // Build scale notes for snapping (same as rebuildBands)
        std::vector<float> previewScaleNotes;
        buildScaleNoteFrequencies (previewScaleNotes, key, scaleType, paramLowFreq, paramHighFreq);

        // Build chord notes for snapping
        std::vector<float> previewChordNotes;
        if (chordVariation == 9)  // Custom
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
            float baseFreq = paramLowFreq * std::pow (ratio, t);

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
        {
            resetAllPerBandQLocks();
            resetAllBandGains();
        }
        rebuildBands();
        // Force per-band envelope recalculation (new band frequencies)
        lastEnvAttackMs = -1.0f;
        lastEnvReleaseMs = -1.0f;
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
        // Force per-band envelope recalculation (new band frequencies)
        lastEnvAttackMs = -1.0f;
        lastEnvReleaseMs = -1.0f;
    }

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
        heldMidiNoteCount.store (static_cast<int> (heldMidiNotes.size()));
    }
    else
    {
        // MIDI Follow disabled - clear held notes
        if (!heldMidiNotes.empty())
        {
            heldMidiNotes.clear();
            midiNoteFreqs.clear();
        }
        heldMidiNoteCount.store (0);
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

        // Reset complement-related filters to prevent IIR transient noise from
        // stale filter state built up with the old coefficient settings
        complementFilter.reset();
        highComplementFilter.reset();
        compBell1WetL.reset();
        compBell1WetR.reset();
        compBell1DryL.reset();
        compBell1DryR.reset();
        compBell2WetL.reset();
        compBell2WetR.reset();
        compBell2DryL.reset();
        compBell2DryR.reset();

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
    // Map gate knob (0-100%) to threshold (-60dB to 0dB)
    // Band envelopes for loud content are typically -10 to -20dB after splitting,
    // so we need the full range to make the knob usable across its sweep
    float gateThresholdDb = -60.0f + (gateAmount / 100.0f) * 60.0f;

    // CPU OPTIMIZATION: Pre-calculate linear gate values ONCE per block
    // Avoids expensive log() calls per sample per band (51,200 calls saved!)
    float gateThresholdLinear = juce::Decibels::decibelsToGain (gateThresholdDb);
    float gateGainLinear = juce::jmax (0.02f, juce::Decibels::decibelsToGain (-gateRangeDb));

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

    // ===== Copy INPUT signal (after Bell EQ) for dry path =====
    // Both wet and dry now have Bell1/Bell2 applied
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

    // CPU OPT: Hoist constant blend, swap to channel-outer/sample-inner for better cache + vectorization
    float colorBlend = (noiseColor + 1.0f) * 0.5f;  // Map -1..+1 to 0..1
    float invColorBlend = 1.0f - colorBlend;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* nb = noiseBuf.getWritePointer (ch);
        float& pinkState = (ch == 0) ? pinkFilterStateL : pinkFilterStateR;

        for (int n = 0; n < numSamples; ++n)
        {
            float white = noiseRandom.nextFloat() * 2.0f - 1.0f;
            pinkState = pinkState + pinkFilterCoeff * (white - pinkState);
            float pink = pinkState * 3.0f;
            nb[n] = pink * invColorBlend + white * colorBlend;
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
    // STFT VOCODER PROCESSING (replaces IIR per-band loop)
    //====================================================

    // Spectral centroid accumulators
    auto& lowBandEnvBuf = lowBandEnvBuf_;
    auto& highBandEnvBuf = highBandEnvBuf_;
    std::fill (lowBandEnvBuf.begin(), lowBandEnvBuf.begin() + numSamples, 0.0f);
    std::fill (highBandEnvBuf.begin(), highBandEnvBuf.begin() + numSamples, 0.0f);
    const int lowFreqBandEnd = nBands * 3 / 10;
    const int highFreqBandStart = nBands * 7 / 10;
    const int lowFreqBandCount = lowFreqBandEnd;
    const int highFreqBandCount = nBands - highFreqBandStart;

    bool additiveMode = apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;

    // Pre-compute mono modulator
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

    // ===== Build STFT band info from current band state =====
    {
        std::vector<STFTVocoder::BandInfo> bandInfos ((size_t) nBands);

        for (int bi = 0; bi < nBands; ++bi)
        {
            auto* b = bands[bi];
            auto& info = bandInfos[(size_t) bi];

            // Frequency edges from center freq and analysis Q
            float bw = b->currentFreq / juce::jmax (0.5f, b->analysisQ);
            info.centerFreq = b->currentFreq;
            info.lowFreq  = juce::jmax (20.0f, b->currentFreq - bw * 0.5f);
            info.highFreq = juce::jmin ((float) (currentSampleRate * 0.45), b->currentFreq + bw * 0.5f);

            // Combined per-band gain (spectral tilt, merge norm, band gain)
            float bandPos = (nBands > 1) ? (float) bi / (float) (nBands - 1) : 0.5f;
            float spectralMult = calculateSpectralGain (bandPos, spectralTilt, spectralIntensity);
            float mergeNorm = additiveMode ? 1.0f : (1.0f / (float) juce::jmax (1, b->mergeGroupSize));

            info.gain       = bandGains[(size_t) bi] * spectralMult * mergeNorm;
            info.analysisQ  = b->analysisQ;
            info.chordGate  = b->chordGate;
            info.isExcluded = noteExclusions[(size_t) b->cachedPitchClass];
            info.isSecondary = !additiveMode && b->isMergedSecondary;
        }

        stftVocoder.updateBands (bandInfos.data(), nBands);
        stftVocoder.setEnvelopeParams (attackCoeff, releaseCoeff);
    }

    // ===== Process through STFT vocoder =====
    // wet buffer was already cleared above
    stftVocoder.process (monoMod.data(), carrierL, carrierR,
                         wet.getWritePointer (0), wet.getWritePointer (1),
                         numSamples, depth,
                         (gateAmount > 1.0f) ? gateThresholdLinear : 0.0f,
                         gateGainLinear,
                         gateAttackCoeff, gateReleaseCoeff);

    // Update GUI band envelopes from STFT
    for (int bi = 0; bi < nBands; ++bi)
        bands[bi]->env = stftVocoder.getBandEnvelope (bi);

    // ===== Spectral centroid from STFT band envelopes =====
    if (lowFreqBandCount > 0 && highFreqBandCount > 0)
    {
        float lowEnergy = 0.0f, highEnergy = 0.0f;
        for (int bi = 0; bi < lowFreqBandEnd; ++bi)
            lowEnergy += stftVocoder.getBandEnvelope (bi);
        for (int bi = highFreqBandStart; bi < nBands; ++bi)
            highEnergy += stftVocoder.getBandEnvelope (bi);

        lowEnergy  /= (float) lowFreqBandCount;
        highEnergy /= (float) highFreqBandCount;

        float totalEnergy = lowEnergy + highEnergy + 1e-10f;
        float centroid = highEnergy / totalEnergy;

        for (int n = 0; n < numSamples; ++n)
        {
            float hpLpScore = unvoicedScoreBuf[(size_t) n];
            unvoicedScoreBuf[(size_t) n] = 0.6f * hpLpScore + 0.4f * centroid;
            highBandEnvBuf[(size_t) n] = highEnergy;
        }
    }

    // ===== CROSSFADE: Skip old bands (STFT windowing provides smooth transitions) =====
    if (isCrossfading)
    {
        crossfadeSamplesRemaining -= numSamples;
        if (crossfadeSamplesRemaining <= 0)
        {
            isCrossfading = false;
            bandsOld.clear (true);
        }
    }

    // Depth-based loudness compensation DISABLED for STFT vocoder.
    // FFT preserves energy naturally; IIR needed +6 dB to compensate filter losses.

    // Width-based loudness compensation DISABLED for STFT vocoder.
    // FFT uses contiguous bin assignment — no energy loss from narrow BW.
    // The IIR version needed +23 dB at narrow BW because narrow filters lose energy.

    // ===== COMBINED NOISE SYSTEM (Carrier-Mode-Aware) =====
    // Smart noise system that adapts to use case:
    // - Vocal mode: High reactivity (35% base + 65% reactive) - S, T, CH, F intelligibility
    // - Synth mode: Constant grit (70% base + 30% reactive) - colour bass texture
    // - Auto mode: Carrier-aware (External=Vocal, Saw/Square/Noise=Synth)
    //
    // This beats Vocodex (3 separate controls) with a single smart knob.
    if (unvoicedLevel > 0.0f)
    {
        // Determine base/reactive ratio based on noiseMode and carrierMode
        float baseRatio, reactiveRatio;

        if (noiseModeInt == 1)  // Vocal mode (forced)
        {
            baseRatio = 0.35f;
            reactiveRatio = 0.65f;
        }
        else if (noiseModeInt == 2)  // Synth mode (forced)
        {
            baseRatio = 0.70f;
            reactiveRatio = 0.30f;
        }
        else  // Auto mode - carrier-aware
        {
            // Carrier modes: 0=Self, 1=External, 2=Noise, 3=Saw, 4=Square, 5=PitchTrack
            switch (carrierModeInt)
            {
                case 1:  // External - assume vocals, high reactivity
                    baseRatio = 0.35f;
                    reactiveRatio = 0.65f;
                    break;
                case 2:  // Noise carrier - already noisy, mostly constant
                    baseRatio = 0.80f;
                    reactiveRatio = 0.20f;
                    break;
                case 3:  // Saw - synth mode, constant grit
                case 4:  // Square - synth mode, constant grit
                    baseRatio = 0.65f;
                    reactiveRatio = 0.35f;
                    break;
                case 5:  // Pitch Track - balanced
                    baseRatio = 0.50f;
                    reactiveRatio = 0.50f;
                    break;
                case 0:  // Self - could be vocals or synths, balanced
                default:
                    baseRatio = 0.50f;
                    reactiveRatio = 0.50f;
                    break;
            }
        }

        const float baseNoise = unvoicedLevel * baseRatio;
        const float consonantBoost = unvoicedLevel * reactiveRatio;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = wet.getWritePointer (ch);
            const float* noise = noiseBuf.getReadPointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                // ===== ADVANCED UNVOICED DETECTION (Ableton-style, independent of Gate) =====
                // Uses HP/LP ratio + spectral centroid for robust sibilant detection
                // NO dependency on band gate threshold - noise has its own detection

                // High-band envelope for timing (ensures noise matches band timing)
                float highBandEnv = highBandEnvBuf[(size_t) n];

                // Smooth the envelope to prevent crackle
                if (highBandEnv > noiseEnvSmoothed)
                    noiseEnvSmoothed = attackCoeff * noiseEnvSmoothed + (1.0f - attackCoeff) * highBandEnv;
                else
                    noiseEnvSmoothed = noiseReleaseCoeff * noiseEnvSmoothed + (1.0f - noiseReleaseCoeff) * highBandEnv;

                // Unvoiced score from HP/LP ratio + spectral centroid (0-1)
                // High score = sibilant/consonant detected = more noise
                float unvoicedScore = unvoicedScoreBuf[(size_t) n];

                // Gate based on high-band energy (ensures silence when no signal)
                float energyGate = fastTanh (noiseEnvSmoothed * 4.0f);

                // Sibilant envelope (HP at 3kHz) for additional character shaping
                float sibilantEnv = noiseEnvBuf[(size_t) n];

                // ===== COMBINED NOISE FORMULA =====
                // Base noise: constant hiss (for synth/texture modes)
                // Reactive noise: follows unvoiced detection (for vocal clarity)
                // Energy gate: ensures noise only plays when there's signal
                float reactiveNoise = unvoicedScore * sibilantEnv * consonantBoost;
                float totalNoiseGain = (baseNoise + reactiveNoise) * energyGate;

                w[n] += noise[n] * totalNoiseGain;
            }
        }
    }

    // ===== HP BLEND: High-passed modulator passthrough for sibilant clarity =====
    // This is the "clarity trick" from Vocodex - passing through high-frequency
    // content (S, F, T, CH sounds) directly to improve intelligibility
    // Slope: 0=12dB (1 stage), 1=24dB (2 stages), 2=48dB (4 stages)
    if (hpBlend > 0.0f)
    {
        int numStages = (hpBlendSlope == 0) ? 1 : (hpBlendSlope == 1) ? 2 : 4;
        bool hpGateActive = (gateAmount > 0.0f);

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

            wet.getWritePointer (0)[n] += hpL * hpBlend * hpGateGain;

            // Process right channel if stereo
            if (numCh > 1)
            {
                float hpR = dry.getSample (1, n);
                for (int s = 0; s < numStages; ++s)
                    hpR = hpBlendFiltersR[s].processSample (0, hpR);

                wet.getWritePointer (1)[n] += hpR * hpBlend * hpGateGain;
            }
        }
    }

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
        // Only process when complement mode is ON — otherwise bellComplement has stale data
        // and the filter accumulates corrupted state that causes noise on next toggle
        if (complementMode && lpActive && bellComplement.getNumSamples() > 0)
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
