#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class STFTVocoder
{
public:
    static constexpr int kFFTOrder = 11;
    static constexpr int kFFTSize  = 1 << kFFTOrder;   // 2048
    static constexpr int kHopSize  = kFFTSize / 4;     // 512 (75% overlap)
    static constexpr int kNumBins  = kFFTSize / 2 + 1; // 1025

    STFTVocoder();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Band configuration (set by PluginProcessor when bands change)
    struct BandInfo
    {
        float centerFreq  = 100.0f;   // Hz
        float lowFreq     = 80.0f;    // Hz — lower edge of band
        float highFreq    = 120.0f;   // Hz — upper edge of band
        float gain        = 1.0f;     // Combined per-band gain multiplier
        float analysisQ   = 2.0f;    // Analysis Q (for bandwidth correction)
        float chordGate   = 1.0f;    // Chord gate attenuation
        bool  isExcluded  = false;   // Note exclusion
        bool  isSecondary = false;   // Merged secondary band (skip processing)
    };

    void updateBands (const BandInfo* bands, int numBands);

    // Set envelope parameters (sample-rate coefficients; converted to frame-rate internally)
    void setEnvelopeParams (float attackCoeff, float releaseCoeff);

    // Process one block: modulator(mono) + carrier(stereo) → output(stereo)
    // Output is ADDED to outL/outR (not replaced), matching IIR accumulation pattern
    void process (const float* modulator,
                  const float* carrierL, const float* carrierR,
                  float* outL, float* outR,
                  int numSamples,
                  float depth,
                  float gateThresholdLinear, float gateGainLinear,
                  float gateAttackCoeff, float gateReleaseCoeff,
                  float globalNoisiness = 1.0f);

    // Analyze modulator only — compute per-band envelopes without processing carrier.
    // Used in hybrid mode: STFT envelope + IIR carrier.
    void processModulatorOnly (const float* modulator, int numSamples,
                               float depth, float globalNoisiness = 1.0f);

    // Get band envelope for GUI visualization or hybrid IIR carrier drive
    float getBandEnvelope (int bandIndex) const;

    // Get band noisiness for hybrid mode (drives IIR compensation chain)
    float getBandNoisiness (int bandIndex) const;

    int getLatencySamples() const { return kFFTSize; }

private:
    double sampleRate_ = 44100.0;

    // JUCE FFT processor
    juce::dsp::FFT fft_;

    // sqrt-Hann window (used for both analysis and synthesis)
    std::array<float, kFFTSize> window_;

    // COLA normalization factor (1 / sum_of_overlapping_windows)
    float colaNorm_ = 1.0f;

    // FFT magnitude normalization (2.0 / N)
    float fftNorm_ = 1.0f;

    // Circular input ring buffers (exactly fftSize long)
    std::array<float, kFFTSize> modRing_;
    std::array<float, kFFTSize> carRingL_;
    std::array<float, kFFTSize> carRingR_;
    int inputWritePos_ = 0;

    // Overlap-add output buffers (2x fftSize for safe circular overlap)
    static constexpr int kOLASize = kFFTSize * 2;
    std::array<float, kOLASize> olaL_;
    std::array<float, kOLASize> olaR_;
    int olaReadPos_ = 0;

    // Hop counter (counts down to next frame processing)
    int hopCounter_ = 0;

    // FFT scratch buffers (JUCE FFT needs 2*fftSize floats)
    std::array<float, kFFTSize * 2> modFFT_;
    std::array<float, kFFTSize * 2> carFFTL_;
    std::array<float, kFFTSize * 2> carFFTR_;

    // Output FFT buffers (pre-allocated to avoid stack allocation in processFrame)
    std::array<float, kFFTSize * 2> outFFTL_;
    std::array<float, kFFTSize * 2> outFFTR_;

    // Internal band data
    struct InternalBand
    {
        int   binLo            = 0;
        int   binHi            = 0;
        float centerFreq       = 100.0f;
        float noiseTilt        = 1.0f;  // sqrt(centerFreq / refFreq) — max tilt for noise
        float gain             = 1.0f;
        float chordGate        = 1.0f;
        float envelope         = 0.0f;
        float noisinessSmoothed = 1.0f;
        float gateGainSmoothed = 1.0f;
        bool  isExcluded       = false;
        bool  isSecondary      = false;
    };

    std::vector<InternalBand> bands_;
    int numBands_ = 0;
    float avgNoiseTilt_ = 1.0f;  // Mean of noiseTilt across bands (for normalization)

    // Frame-rate envelope coefficients (converted from sample-rate in setEnvelopeParams)
    float attackCoeffFrame_  = 0.0f;
    float releaseCoeffFrame_ = 0.0f;

    // Current frame parameters (set before processFrame, used inside it)
    float frameDepth_              = 1.0f;
    float frameGateThreshold_      = 0.0f;
    float frameGateGain_           = 0.0f;
    float frameGateAttackCoeff_    = 0.0f;
    float frameGateReleaseCoeff_   = 0.0f;
    float frameGlobalNoisiness_    = 1.0f;

    // Process one FFT frame (called every hopSize samples)
    void processFrame();

    // Process modulator-only FFT frame (envelope detection, no carrier)
    void processModulatorFrame();

    // Fast tanh approximation
    static inline float fastTanh (float x)
    {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (STFTVocoder)
};
