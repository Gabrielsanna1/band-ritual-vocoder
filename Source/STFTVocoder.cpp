#include "STFTVocoder.h"
#include <cmath>
#include <algorithm>

STFTVocoder::STFTVocoder()
    : fft_ (kFFTOrder)
{
}

void STFTVocoder::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    // Build sqrt-Hann window
    for (int i = 0; i < kFFTSize; ++i)
    {
        float hann = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                                * (float) i / (float) kFFTSize));
        window_[(size_t) i] = std::sqrt (hann);
    }

    // COLA normalization: sqrt-Hann × sqrt-Hann = Hann, 4 overlapping Hanns sum to 1.5
    colaNorm_ = 1.0f / 1.5f;

    // FFT amplitude normalization
    fftNorm_ = 1.0f / std::sqrt ((float) kFFTSize * 0.25f);  // 1/sqrt(N/4)

    reset();
}

void STFTVocoder::reset()
{
    modRing_.fill (0.0f);
    carRingL_.fill (0.0f);
    carRingR_.fill (0.0f);
    inputWritePos_ = 0;

    olaL_.fill (0.0f);
    olaR_.fill (0.0f);
    olaReadPos_ = 0;

    hopCounter_ = kFFTSize;

    for (auto& b : bands_)
    {
        b.envelope = 0.0f;
        b.noisinessSmoothed = 1.0f;
        b.gateGainSmoothed = 1.0f;
    }
}

void STFTVocoder::updateBands (const BandInfo* bands, int numBands)
{
    numBands_ = numBands;
    bands_.resize ((size_t) numBands);

    float binWidth = (float) sampleRate_ / (float) kFFTSize;

    // ===== CONTIGUOUS BIN ASSIGNMENT =====
    // Each bin is assigned to the nearest band (by log-space midpoints).
    // No gaps, no overlaps. Smooth carrier output.

    std::vector<float> logCenters ((size_t) numBands);
    for (int i = 0; i < numBands; ++i)
        logCenters[(size_t) i] = std::log2 (juce::jmax (20.0f, bands[i].centerFreq));

    for (int i = 0; i < numBands; ++i)
    {
        auto& ib = bands_[(size_t) i];

        float lowHz;
        if (i == 0)
            lowHz = juce::jmax (20.0f, bands[i].centerFreq * 0.7f);
        else
        {
            float logMid = 0.5f * (logCenters[(size_t) (i - 1)] + logCenters[(size_t) i]);
            lowHz = std::pow (2.0f, logMid);
        }

        float highHz;
        if (i == numBands - 1)
            highHz = juce::jmin ((float) (sampleRate_ * 0.45), bands[i].centerFreq * 1.4f);
        else
        {
            float logMid = 0.5f * (logCenters[(size_t) i] + logCenters[(size_t) (i + 1)]);
            highHz = std::pow (2.0f, logMid);
        }

        ib.binLo = juce::jmax (1, (int) std::round (lowHz / binWidth));
        ib.binHi = juce::jmin (kNumBins - 1, (int) std::round (highHz / binWidth));

        if (ib.binHi < ib.binLo)
            ib.binHi = ib.binLo;

        ib.centerFreq  = bands[i].centerFreq;
        ib.gain        = bands[i].gain;
        ib.chordGate   = bands[i].chordGate;
        ib.isExcluded  = bands[i].isExcluded;
        ib.isSecondary = bands[i].isSecondary;

        // Pre-compute the noise-optimal tilt correction for this band.
        // sqrt(f/ref) gives ~3 dB/octave — corrects the linear-vs-log frequency mismatch
        // that occurs when using contiguous FFT bins with per-bin RMS envelope.
        // For tonal (sine) signals, this correction should NOT be applied.
        // The blend is controlled by global spectral flatness in processFrame.
        constexpr float kTiltRefFreq = 4000.0f;
        ib.noiseTilt = std::sqrt (bands[i].centerFreq / kTiltRefFreq);

        // Note: bandwidth correction (contBW/iirBW) was attempted but reverted.
        // The Q value depends on mode/params, making correction fragile.
        // LOWBW +2-3 dB excess is accepted as structural (contiguous bins > IIR passband).
    }

    // Resolve overlaps: lower band wins
    for (int i = 1; i < numBands; ++i)
    {
        auto& prev = bands_[(size_t) (i - 1)];
        auto& curr = bands_[(size_t) i];
        if (curr.binLo <= prev.binHi)
            curr.binLo = prev.binHi + 1;
        if (curr.binLo > curr.binHi)
            curr.binHi = curr.binLo;
    }

    // Compute average noiseTilt for normalization.
    // When tilt is applied (noise), the average correction should be ~1.0 so that
    // the overall level matches the no-tilt case (sine).
    float tiltSum = 0.0f;
    int activeBands = 0;
    for (int i = 0; i < numBands; ++i)
    {
        if (!bands_[(size_t) i].isExcluded && !bands_[(size_t) i].isSecondary)
        {
            tiltSum += bands_[(size_t) i].noiseTilt;
            activeBands++;
        }
    }
    avgNoiseTilt_ = (activeBands > 0) ? (tiltSum / (float) activeBands) : 1.0f;
}

void STFTVocoder::setEnvelopeParams (float attackCoeff, float releaseCoeff)
{
    attackCoeffFrame_  = std::pow (attackCoeff, (float) kHopSize);
    releaseCoeffFrame_ = std::pow (releaseCoeff, (float) kHopSize);
}

void STFTVocoder::process (const float* modulator,
                            const float* carrierL, const float* carrierR,
                            float* outL, float* outR,
                            int numSamples,
                            float depth,
                            float gateThresholdLinear, float gateGainLinear,
                            float gateAttackCoeff, float gateReleaseCoeff,
                            float globalNoisiness)
{
    frameDepth_            = depth;
    frameGateThreshold_    = gateThresholdLinear;
    frameGateGain_         = gateGainLinear;
    frameGateAttackCoeff_  = std::pow (gateAttackCoeff, (float) kHopSize);
    frameGateReleaseCoeff_ = std::pow (gateReleaseCoeff, (float) kHopSize);
    frameGlobalNoisiness_  = globalNoisiness;

    for (int i = 0; i < numSamples; ++i)
    {
        modRing_[(size_t) inputWritePos_]  = modulator[i];
        carRingL_[(size_t) inputWritePos_] = carrierL[i];
        carRingR_[(size_t) inputWritePos_] = carrierR[i];
        inputWritePos_ = (inputWritePos_ + 1) % kFFTSize;

        outL[i] += olaL_[(size_t) olaReadPos_];
        outR[i] += olaR_[(size_t) olaReadPos_];

        olaL_[(size_t) olaReadPos_] = 0.0f;
        olaR_[(size_t) olaReadPos_] = 0.0f;
        olaReadPos_ = (olaReadPos_ + 1) % kOLASize;

        hopCounter_--;
        if (hopCounter_ == 0)
        {
            processFrame();
            hopCounter_ = kHopSize;
        }
    }
}

void STFTVocoder::processFrame()
{
    // ===== 1. Window and extract from ring buffer =====
    for (int j = 0; j < kFFTSize; ++j)
    {
        int idx = (inputWritePos_ + j) % kFFTSize;
        float w = window_[(size_t) j];
        modFFT_[(size_t) j]  = modRing_[(size_t) idx] * w;
        carFFTL_[(size_t) j] = carRingL_[(size_t) idx] * w;
        carFFTR_[(size_t) j] = carRingR_[(size_t) idx] * w;
    }

    std::fill (modFFT_.begin() + kFFTSize, modFFT_.end(), 0.0f);
    std::fill (carFFTL_.begin() + kFFTSize, carFFTL_.end(), 0.0f);
    std::fill (carFFTR_.begin() + kFFTSize, carFFTR_.end(), 0.0f);

    // ===== 2. Forward FFT =====
    fft_.performRealOnlyForwardTransform (modFFT_.data());
    fft_.performRealOnlyForwardTransform (carFFTL_.data());
    fft_.performRealOnlyForwardTransform (carFFTR_.data());

    // ===== 3. Vocoder processing =====
    if (numBands_ > 0)
    {
        outFFTL_.fill (0.0f);
        outFFTR_.fill (0.0f);

        // RECALIBRATION FOR BW=10%: Start with neutral constants, measure raw shape.
        // Original HIGHBW values: kNoiseEnvComp=1.18, kTonalEnvComp=0.042
        constexpr float kNoiseEnvComp = 1.0f;   // neutral
        constexpr float kTonalEnvComp = 1.0f;   // neutral (was 0.042 for HIGHBW)

        for (int bi = 0; bi < numBands_; ++bi)
        {
            auto& band = bands_[(size_t) bi];

            if (band.isExcluded || band.isSecondary)
            {
                band.envelope = 0.0f;
                continue;
            }

            // --- Band envelope: flat per-bin RMS across contiguous bins ---
            float bandEnergy = 0.0f;
            float sumMag4 = 0.0f;
            int binCount = band.binHi - band.binLo + 1;

            for (int bin = band.binLo; bin <= band.binHi; ++bin)
            {
                float re = modFFT_[(size_t) (2 * bin)];
                float im = modFFT_[(size_t) (2 * bin + 1)];
                float mag2 = re * re + im * im;
                bandEnergy += mag2;
                sumMag4 += mag2 * mag2;
            }

            // --- Per-band participation ratio → noisiness ---
            // PR = (sum|X|²)² / (N × sum|X|⁴)
            // For complex Gaussian noise: E[PR] = (N+1)/(2N) ≈ 0.5
            // (|X_k|² is exponential → E[|X|⁴] = 2×E[|X|²]²)
            // Pure sine in 1 bin: PR = 1/N
            // Normalize between [1/N, (N+1)/(2N)] → [0, 1]
            // Per-band participation ratio for tonal/noise detection
            // Only reliable for bands with enough bins (>= 8)
            // Default: use global noisiness for small bands (< 8 bins can't detect locally)
            float noisiness = frameGlobalNoisiness_;
            if (binCount >= 8 && sumMag4 > 0.0f)
            {
                float N = (float) binCount;
                float pr = (bandEnergy * bandEnergy) / (N * sumMag4);
                float prSine  = 1.0f / N;
                float prNoise = (N + 1.0f) / (2.0f * N);
                noisiness = (pr - prSine) / (prNoise - prSine);
                noisiness = juce::jlimit (0.0f, 1.0f, noisiness);
            }

            // --- Dual RMS: geometric blend for N-independent tonal response ---
            // Linear blend of perBinRMS and totalRMS has N-dependent excess at
            // moderate noisiness (~0.18): excess ∝ 1/sqrt(N), causing +1-2 dB
            // at mid bands. Geometric blend reduces this dependency dramatically.
            // For noise (η=1): perBinRMS × kNoiseEnvComp (same as linear)
            // For tonal (η=0): totalRMS × kTonalEnvComp (same as linear)
            // For moderate η: pow(noiseRMS,η) × pow(tonalRMS,1-η)
            float perBinRMS = std::sqrt (bandEnergy / juce::jmax (1, binCount)) * fftNorm_;
            float totalRMS  = std::sqrt (bandEnergy) * fftNorm_;

            float noiseRMS = perBinRMS * kNoiseEnvComp;
            float tonalRMS = totalRMS * kTonalEnvComp;

            float bandRMS;
            if (noiseRMS > 1e-20f && tonalRMS > 1e-20f)
            {
                bandRMS = std::exp (noisiness * std::log (noiseRMS)
                                  + (1.0f - noisiness) * std::log (tonalRMS));
                // Guard against NaN/Inf from floating-point edge cases
                if (std::isnan (bandRMS) || std::isinf (bandRMS))
                    bandRMS = noisiness * noiseRMS + (1.0f - noisiness) * tonalRMS;
            }
            else
                bandRMS = noisiness * noiseRMS + (1.0f - noisiness) * tonalRMS;

            // --- Envelope follower (frame-rate) ---
            float env = band.envelope;
            if (std::isnan (bandRMS) || std::isinf (bandRMS))
                bandRMS = 0.0f;
            if (bandRMS > env)
                env = attackCoeffFrame_ * env + (1.0f - attackCoeffFrame_) * bandRMS;
            else
                env = releaseCoeffFrame_ * env + (1.0f - releaseCoeffFrame_) * bandRMS;
            if (std::isnan (env) || std::isinf (env))
                env = 0.0f;
            band.envelope = env;

            // --- Gate ---
            float gatedEnv = env;
            if (frameGateThreshold_ > 0.0f)
            {
                float targetGateGain = (env < frameGateThreshold_) ? frameGateGain_ : 1.0f;

                if (targetGateGain > band.gateGainSmoothed)
                    band.gateGainSmoothed = frameGateAttackCoeff_ * band.gateGainSmoothed
                                          + (1.0f - frameGateAttackCoeff_) * targetGateGain;
                else
                    band.gateGainSmoothed = frameGateReleaseCoeff_ * band.gateGainSmoothed
                                          + (1.0f - frameGateReleaseCoeff_) * targetGateGain;

                gatedEnv *= band.gateGainSmoothed;
            }
            else
            {
                band.gateGainSmoothed = 1.0f;
            }

            gatedEnv *= band.chordGate;

            // --- Depth + soft limiter ---
            float envGain = 1.0f - frameDepth_ + frameDepth_ * gatedEnv;
            if (envGain > 2.0f)
                envGain = 2.0f + fastTanh (envGain - 2.0f);

            float totalGain = envGain * band.gain;

            // --- Per-band adaptive tilt correction ---
            // Noise: sqrt(f/ref) brightens HF (corrects contiguous-bin energy distribution).
            // Tonal: pow(f/ref, -0.08) darkens HF (BR sine consistently brighter than Ableton).
            constexpr float kTonalTiltExp = -0.15f;
            constexpr float kTonalTiltRef = 4000.0f;
            float tonalTilt = std::pow (band.centerFreq / kTonalTiltRef, kTonalTiltExp);
            // RECALIBRATION: bin-count normalization for carrier output.
            // Output energy per band ∝ sqrt(binCount) (envGain normalizes by 1/sqrt(N)
            // but carrier sums N bins). Divide by sqrt(binCount) to flatten.
            float binNorm = 1.0f / std::sqrt ((float) juce::jmax (1, binCount));
            float finalGain = totalGain * binNorm;
            if (std::isnan (finalGain) || std::isinf (finalGain))
                finalGain = 0.0f;

            // --- Apply to carrier bins (flat, no Gaussian) ---
            for (int bin = band.binLo; bin <= band.binHi; ++bin)
            {
                float g = finalGain;
                int ri = 2 * bin;
                int ii = 2 * bin + 1;
                outFFTL_[(size_t) ri] += carFFTL_[(size_t) ri] * g;
                outFFTL_[(size_t) ii] += carFFTL_[(size_t) ii] * g;
                outFFTR_[(size_t) ri] += carFFTR_[(size_t) ri] * g;
                outFFTR_[(size_t) ii] += carFFTR_[(size_t) ii] * g;
            }
        }

        std::copy (outFFTL_.begin(), outFFTL_.end(), carFFTL_.begin());
        std::copy (outFFTR_.begin(), outFFTR_.end(), carFFTR_.begin());
    }

    // ===== 4. Inverse FFT =====
    fft_.performRealOnlyInverseTransform (carFFTL_.data());
    fft_.performRealOnlyInverseTransform (carFFTR_.data());

    // ===== 5. Synthesis window + overlap-add (with NaN guard) =====
    for (int j = 0; j < kFFTSize; ++j)
    {
        // Guard: replace NaN/Inf from IFFT with 0
        if (std::isnan (carFFTL_[(size_t) j]) || std::isinf (carFFTL_[(size_t) j]))
            carFFTL_[(size_t) j] = 0.0f;
        if (std::isnan (carFFTR_[(size_t) j]) || std::isinf (carFFTR_[(size_t) j]))
            carFFTR_[(size_t) j] = 0.0f;
        float w = window_[(size_t) j] * colaNorm_;
        int pos = (olaReadPos_ + j) % kOLASize;
        olaL_[(size_t) pos] += carFFTL_[(size_t) j] * w;
        olaR_[(size_t) pos] += carFFTR_[(size_t) j] * w;
    }
}

float STFTVocoder::getBandEnvelope (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands_)
        return bands_[(size_t) bandIndex].envelope;
    return 0.0f;
}

float STFTVocoder::getBandNoisiness (int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands_)
        return bands_[(size_t) bandIndex].noisinessSmoothed;
    return 1.0f;
}

void STFTVocoder::processModulatorOnly (const float* modulator, int numSamples,
                                         float depth, float globalNoisiness)
{
    frameDepth_           = depth;
    frameGlobalNoisiness_ = globalNoisiness;

    for (int i = 0; i < numSamples; ++i)
    {
        modRing_[(size_t) inputWritePos_] = modulator[i];
        inputWritePos_ = (inputWritePos_ + 1) % kFFTSize;

        hopCounter_--;
        if (hopCounter_ == 0)
        {
            processModulatorFrame();
            hopCounter_ = kHopSize;
        }
    }
}

void STFTVocoder::processModulatorFrame()
{
    // ===== 1. Window and extract modulator from ring buffer =====
    for (int j = 0; j < kFFTSize; ++j)
    {
        int idx = (inputWritePos_ + j) % kFFTSize;
        modFFT_[(size_t) j] = modRing_[(size_t) idx] * window_[(size_t) j];
    }
    std::fill (modFFT_.begin() + kFFTSize, modFFT_.end(), 0.0f);

    // ===== 2. Forward FFT (modulator only) =====
    fft_.performRealOnlyForwardTransform (modFFT_.data());

    // ===== 3. Per-band envelope detection =====
    if (numBands_ <= 0) return;

    constexpr float kNoiseEnvComp = 1.18f;
    constexpr float kTonalEnvComp = 0.042f;

    for (int bi = 0; bi < numBands_; ++bi)
    {
        auto& band = bands_[(size_t) bi];
        if (band.isExcluded || band.isSecondary)
        {
            band.envelope = 0.0f;
            continue;
        }

        float bandEnergy = 0.0f;
        float sumMag4 = 0.0f;
        int binCount = band.binHi - band.binLo + 1;

        for (int bin = band.binLo; bin <= band.binHi; ++bin)
        {
            float re = modFFT_[(size_t) (2 * bin)];
            float im = modFFT_[(size_t) (2 * bin + 1)];
            float mag2 = re * re + im * im;
            bandEnergy += mag2;
            sumMag4 += mag2 * mag2;
        }

        // Noisiness detection (PR-based, global fallback for small bands)
        float noisiness = frameGlobalNoisiness_;
        if (binCount >= 8 && sumMag4 > 0.0f)
        {
            float N = (float) binCount;
            float pr = (bandEnergy * bandEnergy) / (N * sumMag4);
            float prSine  = 1.0f / N;
            float prNoise = (N + 1.0f) / (2.0f * N);
            noisiness = (pr - prSine) / (prNoise - prSine);
            noisiness = juce::jlimit (0.0f, 1.0f, noisiness);
        }

        // Smooth noisiness for IIR compensation chain
        band.noisinessSmoothed = 0.8f * band.noisinessSmoothed + 0.2f * noisiness;

        // Geometric dual RMS
        float perBinRMS = std::sqrt (bandEnergy / juce::jmax (1, binCount)) * fftNorm_;
        float totalRMS  = std::sqrt (bandEnergy) * fftNorm_;
        float noiseRMS = perBinRMS * kNoiseEnvComp;
        float tonalRMS = totalRMS * kTonalEnvComp;

        float bandRMS;
        if (noiseRMS > 1e-20f && tonalRMS > 1e-20f)
        {
            bandRMS = std::exp (noisiness * std::log (noiseRMS)
                              + (1.0f - noisiness) * std::log (tonalRMS));
            if (std::isnan (bandRMS) || std::isinf (bandRMS))
                bandRMS = noisiness * noiseRMS + (1.0f - noisiness) * tonalRMS;
        }
        else
            bandRMS = noisiness * noiseRMS + (1.0f - noisiness) * tonalRMS;

        if (std::isnan (bandRMS) || std::isinf (bandRMS))
            bandRMS = 0.0f;

        // Envelope follower
        float env = band.envelope;
        if (bandRMS > env)
            env = attackCoeffFrame_ * env + (1.0f - attackCoeffFrame_) * bandRMS;
        else
            env = releaseCoeffFrame_ * env + (1.0f - releaseCoeffFrame_) * bandRMS;
        if (std::isnan (env) || std::isinf (env))
            env = 0.0f;
        band.envelope = env;
    }
}
