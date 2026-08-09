#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <set>
#include <map>
#include "LicenseManager.h"
#include "CarrierGenerator.h"
#include "STFTVocoder.h"

//==============================================================================
// InputFilter - Variable-slope high-pass and low-pass filters for input
//==============================================================================
class InputFilter
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    void setHPFrequency (float freq);
    void setHPSlope (int dbPerOct);  // 6, 12, 24, 48
    void setHPResonance (float boostDb, float qWidth);  // Boost in dB + Q width separately
    void setLPFrequency (float freq);
    void setLPSlope (int dbPerOct);
    void setLPResonance (float boostDb, float qWidth);  // Boost in dB + Q width separately

private:
    double sampleRate = 44100.0;

    // Up to 8 stages for 96dB/oct (each StateVariableTPT = 12dB/oct)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 8> hpFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 8> lpFilters;

    // For 6dB/oct we use simple 1-pole filters
    float hp1PoleCoeff = 0.0f;
    float hp1PoleStateL = 0.0f;
    float hp1PoleStateR = 0.0f;
    float lp1PoleCoeff = 1.0f;
    float lp1PoleStateL = 0.0f;
    float lp1PoleStateR = 0.0f;

    int hpStages = 1;   // 0 = use 1-pole, 1-8 = use that many StateVariableTPT
    int lpStages = 1;
    bool use1PoleHP = false;
    bool use1PoleLp = false;

    float hpFreq = 20.0f;
    float lpFreq = 20000.0f;

    void update1PoleCoeffs();
};

//==============================================================================
class ColorVocoderAudioProcessor : public juce::AudioProcessor
{
public:
    using BusesLayout = juce::AudioProcessor::BusesLayout;

    ColorVocoderAudioProcessor();
    ~ColorVocoderAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Band Ritual Final 1"; }

    bool acceptsMidi()  const override { return true; }   // Enable MIDI for MIDI Follow mode
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // For GUI band visual
    void getBandSnapshot (std::vector<float>& freqs,
                          std::vector<float>& envs,
                          float& lowHz,
                          float& highHz,
                          std::vector<int>* outPrimaryIndices = nullptr);

    // Input/Output RMS levels for GUI meters
    float getInputRmsLevel() const { return inputRmsLevel.load(); }
    float getOutputRmsLevel() const { return outputRmsLevel.load(); }

    // Get count of unique bands (after merge-deduplication)
    int getUniqueBandCount() const;

    // Calculate maximum useful bands with current scale/chord/freq settings
    // Simulates 100 bands and counts how many would be unique after merging
    int calculateMaxUsefulBands() const;

    // True once bandCount has been initialised for THIS plugin instance. Set when the editor
    // first opens on a brand-new instance (auto-max once) OR when a saved state is restored.
    // Survives editor close/reopen (lives on the processor), so reopening the GUI no longer
    // clobbers the user's chosen band count with max-useful. See editor ctor + setStateInformation.
    bool bandCountInitialized = false;

    // ===== PER-BAND GAIN CONTROL =====
    static constexpr int maxBands = 100;

    // Get/set individual band gain (0.0 to 1.0, where 1.0 = 100%)
    float getBandGain (int bandIndex) const;
    void setBandGain (int bandIndex, float gain);

    // Get all band gains for display
    const std::array<float, maxBands>& getAllBandGains() const { return bandGains; }

    // Reset all bands to 100%
    void resetAllBandGains();

    // ===== PITCH CLASS NOTE EXCLUSION =====
    // User can exclude specific pitch classes (C, C#, D, etc.) from vocoder bands
    // When a pitch class is excluded, ALL bands at that note (across octaves) are muted
    void setNoteExclusion (int pitchClass, bool excluded);
    bool getNoteExclusion (int pitchClass) const;
    void toggleNoteExclusion (int pitchClass);
    void resetAllNoteExclusions();

    // ===== MIDI BAND CONTROL =====
    // Check if MIDI Band Control mode is currently active
    bool isMidiBandControlActive() const;

    // Get which notes are in the current scale (for keyboard visualization)
    // Returns array[12] where true = note is in current scale
    std::array<bool, 12> getScaleNoteFlags() const;

    // Get which notes are chord tones (for keyboard highlighting)
    std::array<bool, 12> getChordNoteFlags() const;

    // Convert frequency to pitch class (0-11, where 0=C, 1=C#, etc.)
    int frequencyToPitchClass (float freq) const;

    // ===== BAND DISTRIBUTION CURVE =====
    // Custom frequency distribution (Vocodex-style)
    // Curve maps band index (0-1) to frequency position (0-1 in log space)
    void setBandDistribution (const std::vector<float>& curve);
    const std::vector<float>& getBandDistribution() const { return bandDistributionCurve; }
    void resetBandDistribution();  // Reset to linear

    // ===== BAND OFFSET CURVE =====
    // Maps modulator band to carrier band (for band routing mismatch)
    // Curve[i] = 0-1 value indicating which carrier band modulator band i controls
    void setBandOffsetCurve (const std::vector<float>& curve);
    const std::vector<float>& getBandOffsetCurve() const { return bandOffsetCurve; }
    void resetBandOffsetCurve();  // Reset to linear 1:1 mapping

    float getGlobalBandOffset() const { return globalBandOffset; }
    void setGlobalBandOffset (float offset);  // -12 to +12 bands

    // ===== BANK GLIDE CURVE =====
    // Easing curve for keytrack/position glide: maps normalized time (0-1) to normalized
    // pitch progress (0-1). Endpoints locked at (0,0)/(1,1) in the editor; middle points free.
    void setBankGlideCurve (const std::vector<float>& curve);
    const std::vector<float>& getBankGlideCurve() const { return bankGlideCurve; }
    void resetBankGlideCurve();  // Reset to linear diagonal (= plain linear glide)

    // TEMP keytrack diagnostics (remove after verification): live gate + factor + shift.
    std::atomic<bool>  dbgBankFreeMode { false };
    std::atomic<float> dbgBankFactor { 1.0f };
    float getDbgBankShift() const { return currentBankShift.load(); }

    // ===== PER-BAND Q CONTROL =====
    // Per-band Q uses multipliers: effectiveQ = globalBandQ × multiplier
    float getPerBandQ (int bandIndex) const;
    float getPerBandQMultiplier (int bandIndex) const;
    void setPerBandQ (int bandIndex, float q);
    bool isPerBandQLocked (int bandIndex) const;
    void setPerBandQLocked (int bandIndex, bool locked);
    void setGlobalBandQ (float q);  // Sets Q for all unlocked bands
    float getGlobalBandQ() const { return globalBandQ; }
    void resetAllPerBandQLocks();  // Clear all locks and reset to global Q

    // ===== BAND MERGE GROUP SIZES (for carousel stacking display) =====
    void getBandMergeGroupSizes (std::vector<int>& sizes);

    // Access to LicenseManager for the editor
    LicenseManager& getLicenseManager() noexcept       { return licenseManager; }
    const LicenseManager& getLicenseManager() const noexcept { return licenseManager; }

    // Spectrum analyzer - get FFT data for visualization
    static constexpr int fftOrder = 11;  // 2048 points
    static constexpr int fftSize = 1 << fftOrder;
    void getSpectrumData (std::array<float, fftSize / 2>& spectrum) const;

    // Dual-color spectrum data (wet vs dry) for complement mode visualization
    void getDualSpectrumData (std::array<float, fftSize / 2>& wetSpectrum,
                              std::array<float, fftSize / 2>& drySpectrum) const;

    // Get current sample rate for GUI
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

private:
    struct Band
    {
        juce::IIRFilter analysisFilter;
        juce::IIRFilter analysisFilter2;
        juce::IIRFilter analysisFilter3;  // V3: 3-stage analysis for PVR match at Q=37

        // Carrier filters – IIR (legacy, used by Ritual/Split modes)
        juce::IIRFilter carrierFilterL;
        juce::IIRFilter carrierFilterR;
        juce::IIRFilter carrierFilterL2;
        juce::IIRFilter carrierFilterR2;
        juce::IIRFilter carrierFilterL3;
        juce::IIRFilter carrierFilterR3;
        juce::IIRFilter carrierFilterL4;
        juce::IIRFilter carrierFilterR4;
        juce::IIRFilter carrierFilterL5;
        juce::IIRFilter carrierFilterR5;

        // DOUBLE-PRECISION carrier bandpass (TDF-II) — used by the active 4-stage carrier path.
        // float32 biquads lose precision at low freq + high Q (poles hug the unit circle): measured
        // ~-18 dB error at 40 Hz Q=37 -> grit/detune in low resonances. Double precision fixes it.
        // Uses JUCE's EXACT bandpass formula (same shape -> volume/Ableton match unchanged).
        struct DBP
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, s1 = 0.0, s2 = 0.0;
            inline double process (double x) noexcept { double y = b0*x + s1; s1 = b1*x - a1*y + s2; s2 = b2*x - a2*y; return y; }
            void reset() noexcept { s1 = s2 = 0.0; }
            void setBandPass (double sr, double f, double Q) noexcept
            {
                double n = 1.0 / std::tan (juce::MathConstants<double>::pi * f / sr);
                double n2 = n*n, c1 = 1.0 / (1.0 + n/Q + n2);
                b0 = c1*n/Q; b1 = 0.0; b2 = -c1*n/Q; a1 = c1*2.0*(1.0 - n2); a2 = c1*(1.0 - n/Q + n2);
            }
        };
        DBP dCarL[4], dCarR[4];
        DBP dAna[3];   // double-precision 3-stage analysis bandpass (mono) — clean low-band envelope

        // SVF carrier filters — used in Linked mode for crackle-free formant shifting.
        // StateVariableTPT handles frequency changes smoothly (unlike biquads).
        juce::dsp::StateVariableTPTFilter<float> svfCarrierL1, svfCarrierR1;
        juce::dsp::StateVariableTPTFilter<float> svfCarrierL2, svfCarrierR2;
        juce::dsp::StateVariableTPTFilter<float> svfCarrierL3, svfCarrierR3;

        // ITER 19 (2026-04-10): True Butterworth bandpass via 4th-order LP × 4th-order HP.
        // Replaces cascaded RBJ BPF2 (which had narrow peak + steep off-center rejection).
        // Butterworth has flat passband + smooth transition → matches Ableton's "wide
        // shoulder" response measured today (slow sweep showed BR -42 dB at 500 Hz between
        // bands while ABL is only -22 dB).
        // 4th-order LP = 2 biquads, 4th-order HP = 2 biquads, total 4 biquads = 8th-order BP.
        juce::IIRFilter butterLP_L[2], butterLP_R[2];  // 4th-order LP at upper band edge
        juce::IIRFilter butterHP_L[2], butterHP_R[2];  // 4th-order HP at lower band edge

        float env             = 0.0f;   // Stage 1 envelope (peak detector / cascade stage 1)
        float env2            = 0.0f;   // Stage 2 envelope (cascade stage 2)
        float env3            = 0.0f;   // Stage 3 envelope (Linked-mode cascaded LP)
        float env4            = 0.0f;   // Stage 4 envelope (Linked-mode cascaded LP)
        float env5            = 0.0f;   // Stage 5 (iter 21)
        float env6            = 0.0f;   // Stage 6 (iter 21)

        // ITER 21 PHASE 4: Ring-buffer moving average envelope (boxcar filter).
        // Simulation proved: boxcar at 200ms → cosine 0.9153 (vs cascade LP → 0.6797).
        static constexpr int kEnvRingMaxSize = 8820;  // 200ms at 44100 Hz
        float envRingBuf[kEnvRingMaxSize] = {};
        int envRingHead = 0;
        float envRingSum = 0.0f;
        int envRingSize = 8820;  // actual window size (set from sample rate)
        float centerFreqBase  = 100.0f;
        float centerFreqScale = 100.0f;
        float currentFreq     = 100.0f;  // Analysis frequency (envelope detection)
        float carrierFreq     = 100.0f;  // Carrier frequency (can be pitch-shifted)
        float bandQ           = 2.0f;    // Carrier Q
        float analysisQ       = 2.0f;    // Modulator/analysis Q (for envelope compensation)
        float chordGate       = 1.0f;  // HYBRID chord mode: 1.0 = full, 0.0 = muted (far from chord)
        float gateGainSmoothed = 1.0f; // Smoothed gate gain to prevent crackle
        float dynGainSmoothed  = 1.0f; // Smoothed per-band dynamic threshold gain (Pro-Q 3-style expander)
        bool  formantMute     = false; // V3: mute band when formant shift pushes it out of range
        float envDisplay      = 0.0f;  // Gated envelope for VISUALIZATION only (== env when no
                                       // muting is active). Never fed back into the follower.

        float overlapComp     = 1.0f;  // Combined noise shape + BW compensation
        float bwComp          = 1.0f;  // Bilinear transform BW compression correction
        float bandNormGain    = 1.0f;  // V3: energy normalization gain
        float tiltCorr        = 1.0f;  // V3: spectral tilt correction (gain-neutral)
        int cachedPitchClass  = 0;     // CPU OPT: cached from currentFreq in updateBandFilters

        // Merge tracking - prevents volume stacking when bands converge to same frequency
        int mergeGroupId      = -1;    // Which group this band belongs to (-1 = none)
        int mergeGroupSize    = 1;     // How many bands in this group (primary only)
        bool isMergedSecondary = false; // True = skip filter processing, contribute envelope only
    };

    juce::OwnedArray<Band> bands;
    juce::SpinLock bandsLock;  // Protects bands array during rebuild vs UI read

    // ===== PER-BAND GAIN (user-adjustable, 0.0-1.0) =====
    std::array<float, maxBands> bandGains;  // Each band's gain multiplier

    // ===== PITCH CLASS NOTE EXCLUSION =====
    // Per-pitch-class exclusion (user mute via piano keyboard)
    // Index 0-11 = C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    std::array<bool, 12> noteExclusions = { false, false, false, false, false, false,
                                             false, false, false, false, false, false };

    // ===== BAND DISTRIBUTION CURVE =====
    // 101 points (0-100), each value is 0-1 representing position in log frequency range
    // Default is linear: curve[i] = i/100
    std::vector<float> bandDistributionCurve;

    // ===== BAND OFFSET CURVE =====
    // 101 points for smooth mapping between modulator and carrier bands
    // Default is linear: curve[i] = i/100 (1:1 mapping)
    std::vector<float> bandOffsetCurve;
    float globalBandOffset = 0.0f;  // -12 to +12 bands global shift

    // ===== BANK KEYTRACK / GLIDE =====
    // Editable easing curve (101 pts, linear diagonal default) shaping the bank-transpose glide.
    std::vector<float> bankGlideCurve;
    // Last-note recency for keytrack (heldMidiNotes is a pitch-ordered set with no recency).
    std::vector<int> keytrackHeldStack;    // ordered by note-on recency; .back() = active note
    int   keytrackNote = 60;               // most-recent MIDI note; RETAINED after release (hold)
    // Curve-shaped glide state (audio-thread owned; advanced once per block).
    float glideStartShift  = 0.0f;         // shift (semitones) at the moment the target changed
    float glideTargetShift = 0.0f;         // destination shift (semitones)
    float glideProgress    = 1.0f;         // 0..1 linear time progress through the glide
    std::atomic<float> currentBankShift { 0.0f };  // live shift read by updateBandFilters()
    // Sample-and-eval helper for the glide curve (linear interp of bankGlideCurve at t in [0,1]).
    float evalGlideCurve (float t) const;

    // ===== PER-BAND Q CONTROL (multiplier-based) =====
    std::array<float, maxBands> perBandQMultiplier;  // Multiplier per band (default 1.0)
    std::array<bool, maxBands> perBandQLocked;       // Lock status per band
    float globalBandQ = 8.0f;                        // Default Q for unlocked bands

    // ===== SMOOTH BAND TRANSITIONS =====
    // Dual-bank crossfade to prevent clicks when band count changes
    juce::OwnedArray<Band> bandsOld;       // Previous band config during crossfade
    bool isCrossfading = false;            // Whether we're currently crossfading
    int crossfadeSamplesTotal = 0;         // Total samples for crossfade (~50ms)
    int crossfadeSamplesRemaining = 0;     // Samples left in crossfade

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    // Thread-safe deferred band rebuild flag
    // Set by GUI thread, consumed by audio thread
    std::atomic<bool> bandDistributionDirty { false };

    // cache (to know when to rebuild bands)
    int   lastBandCount   = 0;
    int   lastKey         = 0;
    int   lastScaleType   = 0;
    float lastHarmShift   = 0.0f;
    float lastScaleAmt    = -1.0f;
    float lastLowFreq     = 0.0f;
    float lastHighFreq    = 0.0f;
    float lastBandWidth   = 0.0f;

    // envelope coefficients (shared by bands and unvoiced detection)
    float attackCoeff        = 0.0f;
    float releaseCoeff       = 0.0f;
    float cascadeEnvCoef     = 0.0f;   // Linked mode: voclib-style 4-stage cascaded LP coef
    float lastReleaseSec     = 0.05f;  // ITER 15: cached release in seconds for per-band cascade tau
    float lastEnvAttackMs    = -1.0f;  // CPU OPT: cache for updateEnvelopeCoeffs
    float lastEnvReleaseMs   = -1.0f;

    // Gate smoothing coefficients (sample-rate dependent, NOT arbitrary)
    // Gate attack: 2ms (fast open, lets transients through)
    // Gate release: 30ms (slow close, prevents crackle)
    float gateAttackCoeff    = 0.0f;
    float gateReleaseCoeff   = 0.0f;

    // HP blend gate (follows same gate as vocoder bands)
    float hpBlendEnvelope       = 0.0f;
    float hpBlendGateSmoothed   = 1.0f;

    // Noise envelope smoother (prevents crackle at fast release settings)
    // Uses asymmetric smoothing: fast attack, minimum 30ms release
    float noiseEnvSmoothed   = 0.0f;
    float noiseReleaseCoeff  = 0.0f;  // Computed with minimum 30ms release

    // scale note frequencies cache
    std::vector<float> scaleNoteFreqs;

    // ===== CHORD CONTROL =====
    std::vector<float> chordNoteFreqs;  // Frequencies of current chord tones
    int lastChordDegree = -1;
    int lastChordVariation = -1;

    // ===== CUSTOM CHORD STORAGE =====
    std::map<int, std::vector<int>> customChords;  // keyChar → MIDI notes
    juce::SpinLock customChordLock;

    // ===== MIDI FOLLOW MODE =====
    // When MIDI Follow is enabled, bands snap to held MIDI notes instead of chord degrees
    std::set<int> heldMidiNotes;           // Currently held MIDI note numbers (0-127)
    std::vector<float> midiNoteFreqs;      // Frequencies of held MIDI notes

    // ===== MIDI BAND CONTROL =====
    // When enabled, MIDI notes D3-C#4 (50-61) control which pitch classes are active.
    // Note ON = un-exclude (bands play), Note OFF = exclude (bands mute).
    // All pitch classes start excluded when mode activates.
    static constexpr int midiBandControlBaseNote = 50;  // D3 in standard MIDI (C4=60)
    std::array<bool, 12> midiBandControlHeld = {};      // Which pitch classes are held via MIDI
    bool lastMidiBandControlState = false;               // Detect mode activation/deactivation
    std::array<bool, 12> savedNoteExclusions = {};       // User's manual exclusions before mode activated

public:
    // Debug: track last MIDI note received + channel (for diagnosing routing issues)
    std::atomic<int> dbgLastMidiNote { -1 };    // -1 = no MIDI received
    std::atomic<int> dbgLastMidiChannel { -1 };
    std::atomic<int> dbgMidiNoteCount { 0 };    // Total note-on count since mode activated

    // ===== MIDI CHORD TRIGGER =====
    // Remembers last variation per chord degree (indices into chordVariation choices)
    std::array<int, 7> lastVariationPerDegree = { 0, 0, 0, 0, 0, 0, 0 };
    bool midiChordFirstPress = true;  // Auto-set chordAmount to 100% on first key press

    // ===== CUSTOM CHORD EDITOR =====
    // Per-key custom chords: key = uppercase letter char ('Q','W',...'M'), value = MIDI note numbers
    void setCustomChord (int keyChar, const std::vector<int>& midiNotes);
    std::vector<int> getCustomChord (int keyChar) const;
    bool hasCustomChord (int keyChar) const;
    void clearCustomChord (int keyChar);
    void clearAllCustomChords();

    // Edit mode state (GUI toggle)
    std::atomic<bool> editModeActive { false };

    // MIDI Learn: when active, next MIDI chord input is captured for the target key
    std::atomic<bool> midiLearnActive { false };
    std::atomic<int> midiLearnTargetKey { 0 };  // Which letter key receives the learned chord

    // Track which letter key was last triggered (for custom chord lookup)
    std::atomic<int> lastTriggeredLetterKey { 'A' };

private:

    // noise / unvoiced (legacy)
    juce::IIRFilter noiseFilterL, noiseFilterR;
    juce::Random    noiseRandom;

    // ===== NEW PHASE 1 MEMBERS =====

    // Carrier generator for internal/external carrier modes
    CarrierGenerator carrierGenerator;

    // STFT vocoder for Linked mode (alternative to IIR carrier path)
    STFTVocoder stftVocoder;
    bool useSTFTLinked = false;  // STFT wired in but needs normalization fix

    // Unvoiced detection filters (LP vs HP energy comparison)
    juce::IIRFilter unvoicedLPF, unvoicedHPF;
    float unvoicedEnvLP = 0.0f;
    float unvoicedEnvHP = 0.0f;
    float lastSibFreq = 3000.0f;   // tracks "sibFreq" param to retune unvoicedHPF on change
    juce::IIRFilter noiseHpL, noiseHpR;   // optional high-pass on the injected noise (hiss-only)
    float lastNoiseHp = 20.0f;            // tracks "noiseHp" param
    // Ableton tonal-match: 3 cascaded low-shelves on the wet bus (Self mode only) to flatten BR's
    // structural +6dB-sub->0 tilt so the tonal balance matches Ableton's vocoder exactly.
    juce::IIRFilter ablTiltL[3], ablTiltR[3];
    juce::IIRFilter ablHpL[2], ablHpR[2];   // steep HP to match Ableton's deep-sub rolloff (Self mode)
    juce::dsp::DelayLine<float> airDelayL { 16384 }, airDelayR { 16384 };  // AIR (hp-blend) delay

    // Carrier buffer for generated carrier signal
    juce::AudioBuffer<float> carrierBuffer;

    // ===== INPUT/OUTPUT FILTER =====
    InputFilter inputFilter;           // Applied to wet signal
    InputFilter dryFilter;             // Applied to dry signal (same settings as inputFilter, separate state)
    InputFilter complementFilter;      // Applied to complement buffer (inverse settings)

    // ===== HP BLEND (sibilant passthrough) =====
    // Up to 4 stages for 48dB/oct (each StateVariableTPT = 12dB/oct)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 4> hpBlendFiltersL;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 4> hpBlendFiltersR;

    // ===== PINK NOISE FILTER (for Noise Color) =====
    // Simple 1-pole lowpass for pink noise approximation
    float pinkFilterStateL = 0.0f;
    float pinkFilterStateR = 0.0f;
    float pinkFilterCoeff = 0.0f;

    // License
    LicenseManager licenseManager;

    // ===== TRIAL NAG SYSTEM =====
    // Every 7 seconds: 0.7 seconds of silence (mute output)
    static constexpr float NAG_INTERVAL_SECONDS = 7.0f;
    static constexpr float NAG_DURATION_SECONDS = 0.7f;

    int nagIntervalSamples = 0;      // Samples between nags (computed in prepareToPlay)
    int nagDurationSamples = 0;      // Duration of nag in samples
    int nagSampleCounter = 0;        // Counts up to nagIntervalSamples
    int nagActiveSamples = 0;        // Counts down during active nag (0 = not nagging)

    // ===== CPU OPTIMIZATION: Pre-allocated buffers =====
    std::vector<float> bandEnvBuffer;  // Reusable envelope buffer (avoids heap alloc per band)
    std::vector<std::vector<float>> allBandEnvs;  // [band][sample] for formant remapping

    // Global modulator noisiness (0=tonal, 1=noise) — smoothed across blocks
    float smoothedGlobalNoisiness_ = 0.5f;

    // Pre-allocated audio buffers — eliminates ~110 heap allocs per processBlock
    juce::AudioBuffer<float> dryBuf_, wetBuf_, wetOldBuf_, modBuf_;
    juce::AudioBuffer<float> noiseBuf_, bandCarBuf_;
    juce::AudioBuffer<float> complementBuf_, bellComplementBuf_;
    juce::AudioBuffer<float> wetEqBuf_, dryEqBuf_;
    std::vector<float> noiseEnvBuf_, unvoicedScoreBuf_;
    std::vector<float> lowBandEnvBuf_, highBandEnvBuf_;
    std::vector<float> monoModBuf_;  // CPU OPT: pre-computed mono modulator
    juce::AudioBuffer<float> sidechainBuf_;  // CPU OPT: pre-allocated sidechain buffer

    // Fast tanh approximation - accurate to ~0.1%, avoids expensive std::tanh
    static inline float fastTanh(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // ===== SPECTRUM ANALYZER =====
    // FFT created as unique_ptr for safer initialization/destruction during plugin scan
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::array<float, fftSize * 2> fftBuffer {};  // FFT needs 2*fftSize!
    std::array<float, fftSize * 2> fftWetBuffer {};  // Pre-allocated wet FFT buffer
    std::atomic<bool> fftEnabled { true };  // Can disable FFT when spectrum not visible

    // ===== INPUT / OUTPUT RMS LEVELS (for GUI meters) =====
    std::atomic<float> inputRmsLevel { 0.0f };
    std::atomic<float> outputRmsLevel { 0.0f };

    // Double-buffered spectrum data for thread-safe GUI access
    std::array<float, fftSize / 2> spectrumWriteBuffer {};  // Written by audio thread
    std::array<float, fftSize / 2> spectrumReadBuffer {};   // Read by GUI thread
    std::atomic<bool> spectrumDataReady { false };          // Flag for new data available

    int fftWriteIndex = 0;
    bool fftInitialized = false;

    // ===== BELL EQ FILTERS =====
    // Two parametric bell/peak EQ bands that can be enabled independently
    juce::dsp::IIR::Filter<float> bell1FilterL, bell1FilterR;
    juce::dsp::IIR::Filter<float> bell2FilterL, bell2FilterR;
    juce::dsp::IIR::Coefficients<float>::Ptr bell1Coeffs, bell2Coeffs;

    // Complement bell filters (inverted gain for complement buffer)
    juce::dsp::IIR::Filter<float> bell1CompL, bell1CompR;
    juce::dsp::IIR::Filter<float> bell2CompL, bell2CompR;
    juce::dsp::IIR::Coefficients<float>::Ptr bell1CompCoeffs, bell2CompCoeffs;

    // Dry signal bell filters (normal gain - master EQ behavior)
    juce::dsp::IIR::Filter<float> bell1DryL, bell1DryR;
    juce::dsp::IIR::Filter<float> bell2DryL, bell2DryR;

    // ===== COMPLEMENT EQ LAYER =====
    // Separate bell filters for complement mode (controls wet/dry ratio per frequency)
    // These define WHERE the wet/dry ratio changes, not gain
    // IMPORTANT: Wet and dry need SEPARATE filter instances to avoid state corruption
    juce::dsp::IIR::Filter<float> compBell1WetL, compBell1WetR;  // For wet signal
    juce::dsp::IIR::Filter<float> compBell1DryL, compBell1DryR;  // For dry signal
    juce::dsp::IIR::Filter<float> compBell2WetL, compBell2WetR;  // For wet signal
    juce::dsp::IIR::Filter<float> compBell2DryL, compBell2DryR;  // For dry signal
    juce::dsp::IIR::Coefficients<float>::Ptr compBell1WetCoeffs, compBell1DryCoeffs;
    juce::dsp::IIR::Coefficients<float>::Ptr compBell2WetCoeffs, compBell2DryCoeffs;

    // Sub complement filter (LP at HP frequency - isolates clean sub)
    InputFilter subComplementFilter;

    // High complement filter (HP at LP frequency - isolates clean highs)
    InputFilter highComplementFilter;

    // ===== DUAL-COLOR SPECTRUM (wet vs dry visualization) =====
    std::array<float, fftSize / 2> spectrumWetBuffer {};   // Wet signal spectrum
    std::array<float, fftSize / 2> spectrumDryBuffer {};   // Dry signal spectrum
    std::array<float, fftSize * 2> fftBufferDry {};         // FFT needs 2*fftSize!
    int fftWriteIndexDry = 0;

    // ===== MASTER GATE (Smooth, musical gating) =====
    // RMS-based envelope for smoother detection
    float masterGateEnv = 0.0f;
    float masterGateRMS = 0.0f;           // RMS accumulator
    int masterGateHoldCounter = 0;         // Hold time counter (samples)
    float masterGateGainSmoothed = 1.0f;   // Smoothed gain to prevent clicks

    // ===== PARAMETER SMOOTHING =====
    // Prevents crackling/zipper noise when automating parameters
    // Uses ~10ms smoothing time (sounds natural, like analog slew)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSpectralTilt { 0.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSpectralIntensity { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFormant { 0.0f };

    // ===== COMPLEMENT MODE EQ MEMORY =====
    // Stores separate EQ positions for Normal and Complement modes
    bool lastComplementModeState = false;

    // HP Filter memory
    float savedHPFreqNormal = 20.0f;           // Default: flat (no filtering)
    float savedHPFreqComplement = 180.0f;      // Default: 180Hz for bass separation

    // LP Filter memory
    float savedLPFreqNormal = 20000.0f;        // Default: flat (no filtering)
    float savedLPFreqComplement = 20000.0f;    // Default: flat

    // Bell 1 memory
    float savedBell1FreqNormal = 500.0f;
    float savedBell1GainNormal = 0.0f;
    float savedBell1FreqComplement = 500.0f;
    float savedBell1GainComplement = 0.0f;

    // Bell 2 memory
    float savedBell2FreqNormal = 2000.0f;
    float savedBell2GainNormal = 0.0f;
    float savedBell2FreqComplement = 2000.0f;
    float savedBell2GainComplement = 0.0f;

    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateEnvelopeCoeffs();
    void updateBandsIfNeeded();
    void rebuildBands();
    void updateBandFilters();
    void updateNoiseFilters();

    void buildScaleNoteFrequencies (std::vector<float>& outNotesHz,
                                    int key, int scaleType,
                                    float lowHz, float highHz) const;

    float snapToScale (float freqHz) const;
    int   findClosestScaleNoteIndex (float freqHz) const;

    // ===== CHORD FUNCTIONS =====
    void buildChordFrequencies (std::vector<float>& outChordHz,
                                int key, int scaleType, int chordDegree, int chordVariation,
                                float lowHz, float highHz) const;
    std::vector<int> getChordIntervals (int degree, int variation, int scaleType) const;
    float snapToChord (float freqHz) const;
    float snapToMidi (float freqHz) const;  // Snap to held MIDI notes (MIDI Follow mode)

    // ===== MICROTUNING =====
    float getMicrotunedFrequency (int midiNote) const;  // Get frequency with microtuning applied
    void loadScalaFile (const juce::File& scalaFile);   // Load .scl tuning file
    void resetMicrotuning();                             // Reset to equal temperament

    // Microtuning state
    float referencePitch = 440.0f;                      // A4 reference frequency
    std::array<float, 12> centOffsets = {};             // Per-note cent offsets (C, C#, D, ... B)
    bool scalaFileLoaded = false;                       // Whether a Scala file is active
    juce::String currentScalaName;                      // Name of loaded Scala file

    // ===== SPECTRAL X/Y PAD (Producer-approved version) =====
    // Calculate spectral gain multiplier for a band based on X/Y pad position
    // pos = band position 0-1 (low freq to high freq)
    // tiltX = 0-1 (0=boost lows/cut highs, 0.5=flat, 1=boost highs/cut lows)
    // intensityY = 0-1 (0=flat/no effect, 1=max effect ~16dB range)
    static float calculateSpectralGain (float pos, float tiltX, float intensityY)
    {
        // No effect when intensity is zero
        if (intensityY < 0.001f)
            return 1.0f;

        // Convert tiltX from 0-1 to -1 to +1
        float tilt = (tiltX - 0.5f) * 2.0f;

        // Steepness increases with intensity (1.0 to 3.0)
        float steepness = 1.0f + intensityY * 2.0f;

        // Calculate boost and cut curves
        // boostCurve: 1.0 to 3.0 (up to +9.5dB boost)
        // cutCurve: 0.15 to 1.0 (up to -16dB cut)
        float boostAmount = 1.0f + 2.0f * intensityY;  // 1.0 to 3.0
        float cutAmount = 0.15f + 0.85f * (1.0f - intensityY);  // 0.15 to 1.0

        float gain;
        if (tilt >= 0)
        {
            // Positive tilt: boost highs, cut lows
            float highBoost = 1.0f + (boostAmount - 1.0f) * std::pow (pos, steepness);
            float lowCut = cutAmount + (1.0f - cutAmount) * std::pow (pos, steepness);
            gain = highBoost * lowCut * std::abs (tilt) + 1.0f * (1.0f - std::abs (tilt));
        }
        else
        {
            // Negative tilt: boost lows, cut highs
            float lowBoost = 1.0f + (boostAmount - 1.0f) * std::pow (1.0f - pos, steepness);
            float highCut = cutAmount + (1.0f - cutAmount) * std::pow (1.0f - pos, steepness);
            gain = lowBoost * highCut * std::abs (tilt) + 1.0f * (1.0f - std::abs (tilt));
        }

        return gain;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorVocoderAudioProcessor)
};
