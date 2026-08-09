#include "CarrierGenerator.h"

CarrierGenerator::CarrierGenerator()
{
    for (auto& v : voices)
    {
        v.phase = 0.0f;
        v.frequency = 440.0f;
        v.smoothedFrequency = 440.0f;
        v.active = false;
    }
}

void CarrierGenerator::prepare (double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    smoothedFrequency = frequency;
    updatePhaseIncrement();

    // Frequency smoothing coefficient: ~5ms smoothing time
    frequencySmoothing = std::exp (-1.0f / (float) (sampleRate * 0.005));

    // Reset all voices
    for (auto& v : voices)
    {
        v.smoothedFrequency = v.frequency;
    }
}

void CarrierGenerator::reset()
{
    phase = 0.0f;
    smoothedFrequency = frequency;

    for (auto& v : voices)
    {
        v.phase = 0.0f;
        v.smoothedFrequency = v.frequency;
    }
}

void CarrierGenerator::setType (Type newType)
{
    currentType = newType;
}

void CarrierGenerator::setFrequency (float freqHz)
{
    frequency = juce::jlimit (20.0f, 20000.0f, freqHz);
}

void CarrierGenerator::setPitchFromInput (float detectedPitch)
{
    // For pitch tracking mode - set frequency from detected pitch
    if (currentType == Type::PitchTrack && detectedPitch > 20.0f)
        frequency = juce::jlimit (20.0f, 5000.0f, detectedPitch);
}

void CarrierGenerator::setMidiFrequencies (const std::vector<float>& frequencies)
{
    // Set polyphonic voices from MIDI note frequencies
    activeVoiceCount = juce::jmin ((int) frequencies.size(), maxVoices);

    for (int i = 0; i < maxVoices; ++i)
    {
        if (i < activeVoiceCount)
        {
            voices[i].frequency = frequencies[i];
            voices[i].active = true;
        }
        else
        {
            voices[i].active = false;
        }
    }
}

void CarrierGenerator::clearMidiFrequencies()
{
    activeVoiceCount = 0;
    for (auto& v : voices)
        v.active = false;
}

void CarrierGenerator::updatePhaseIncrement()
{
    phaseIncrement = smoothedFrequency / (float) sampleRate;
}

float CarrierGenerator::generateSawSample()
{
    // Sawtooth: ramp from -1 to 1
    float sample = 2.0f * phase - 1.0f;
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;
    return sample;
}

float CarrierGenerator::generateSquareSample()
{
    // Square wave: -1 or 1
    float sample = (phase < 0.5f) ? 1.0f : -1.0f;
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;
    return sample;
}

float CarrierGenerator::generateSineSample()
{
    // Sine wave
    float sample = std::sin (2.0f * juce::MathConstants<float>::pi * phase);
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;
    return sample;
}

float CarrierGenerator::generateNoiseSample()
{
    // White noise: random values between -1 and 1
    return noiseRandom.nextFloat() * 2.0f - 1.0f;
}

float CarrierGenerator::generatePolySawSample()
{
    if (activeVoiceCount == 0)
        return 0.0f;

    float mixedSample = 0.0f;

    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[i];
        if (!v.active) continue;

        // Smooth frequency to prevent clicks
        v.smoothedFrequency = frequencySmoothing * v.smoothedFrequency
                            + (1.0f - frequencySmoothing) * v.frequency;

        float phaseInc = v.smoothedFrequency / (float) sampleRate;

        // Generate saw sample for this voice
        float sample = 2.0f * v.phase - 1.0f;
        mixedSample += sample;

        // Advance phase
        v.phase += phaseInc;
        if (v.phase >= 1.0f)
            v.phase -= 1.0f;
    }

    // Normalize by number of voices (prevent clipping)
    return mixedSample / (float) activeVoiceCount;
}

float CarrierGenerator::generatePolySquareSample()
{
    if (activeVoiceCount == 0)
        return 0.0f;

    float mixedSample = 0.0f;

    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[i];
        if (!v.active) continue;

        // Smooth frequency to prevent clicks
        v.smoothedFrequency = frequencySmoothing * v.smoothedFrequency
                            + (1.0f - frequencySmoothing) * v.frequency;

        float phaseInc = v.smoothedFrequency / (float) sampleRate;

        // Generate square sample for this voice
        float sample = (v.phase < 0.5f) ? 1.0f : -1.0f;
        mixedSample += sample;

        // Advance phase
        v.phase += phaseInc;
        if (v.phase >= 1.0f)
            v.phase -= 1.0f;
    }

    // Normalize by number of voices (prevent clipping)
    return mixedSample / (float) activeVoiceCount;
}

void CarrierGenerator::generateCarrier (juce::AudioBuffer<float>& outputBuffer,
                                         const juce::AudioBuffer<float>* externalCarrier,
                                         const juce::AudioBuffer<float>* modulatorForSelf)
{
    const int numSamples = outputBuffer.getNumSamples();
    const int numChannels = outputBuffer.getNumChannels();

    // Handle External mode
    if (currentType == Type::External)
    {
        if (externalCarrier != nullptr && externalCarrier->getNumChannels() > 0)
        {
            // Copy external carrier to output
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int srcCh = juce::jmin (ch, externalCarrier->getNumChannels() - 1);
                outputBuffer.copyFrom (ch, 0, *externalCarrier, srcCh, 0, numSamples);
            }
            return;
        }
        else
        {
            // No external carrier available - output SILENCE (standard vocoder behavior)
            // User needs to connect a sidechain input for External mode to work
            outputBuffer.clear();
            return;
        }
    }

    // Handle Self mode (original Band Ritual behavior)
    if (currentType == Type::Self)
    {
        if (modulatorForSelf != nullptr && modulatorForSelf->getNumChannels() > 0)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int srcCh = juce::jmin (ch, modulatorForSelf->getNumChannels() - 1);
                outputBuffer.copyFrom (ch, 0, *modulatorForSelf, srcCh, 0, numSamples);
            }
            return;
        }
    }

    // Generate internal carrier
    for (int n = 0; n < numSamples; ++n)
    {
        float sample = 0.0f;

        switch (currentType)
        {
            case Type::Noise:
                sample = generateNoiseSample();
                break;

            case Type::Saw:
                // Use polyphonic if MIDI notes are held, otherwise monophonic
                if (activeVoiceCount > 0)
                    sample = generatePolySawSample();
                else
                {
                    // Fallback: monophonic at default frequency
                    smoothedFrequency = frequencySmoothing * smoothedFrequency
                                      + (1.0f - frequencySmoothing) * frequency;
                    updatePhaseIncrement();
                    sample = generateSawSample();
                }
                break;

            case Type::Square:
                // Use polyphonic if MIDI notes are held, otherwise monophonic
                if (activeVoiceCount > 0)
                    sample = generatePolySquareSample();
                else
                {
                    // Fallback: monophonic at default frequency
                    smoothedFrequency = frequencySmoothing * smoothedFrequency
                                      + (1.0f - frequencySmoothing) * frequency;
                    updatePhaseIncrement();
                    sample = generateSquareSample();
                }
                break;

            case Type::PitchTrack:
                // Monophonic, follows detected pitch
                smoothedFrequency = frequencySmoothing * smoothedFrequency
                                  + (1.0f - frequencySmoothing) * frequency;
                updatePhaseIncrement();
                sample = generateSawSample();
                break;

            case Type::Sine:
                smoothedFrequency = frequencySmoothing * smoothedFrequency
                                  + (1.0f - frequencySmoothing) * frequency;
                updatePhaseIncrement();
                sample = generateSineSample();
                break;

            default:
                sample = generateNoiseSample();
                break;
        }

        // Write to all channels
        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.setSample (ch, n, sample);
    }
}
