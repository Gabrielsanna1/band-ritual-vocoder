#pragma once

#include <JuceHeader.h>
#include <vector>

/**
 * CarrierGenerator - Internal carrier signal generator for Band Ritual vocoder
 *
 * Supports multiple carrier modes:
 * - Self: Use modulator as carrier (original Band Ritual behavior)
 * - External: Use sidechain input as carrier
 * - Noise: White noise generator
 * - Saw: Sawtooth oscillator (POLYPHONIC - follows MIDI)
 * - Square: Square wave oscillator (POLYPHONIC - follows MIDI)
 * - Sine: Sine wave oscillator
 * - PitchTrack: Sawtooth that follows detected input pitch
 */
class CarrierGenerator
{
public:
    enum class Type
    {
        Self = 0,       // Use modulator as carrier (original behavior)
        External,       // Use sidechain input
        Noise,          // White noise
        Saw,            // Sawtooth wave (polyphonic MIDI)
        Square,         // Square wave (polyphonic MIDI)
        Sine,           // Sine wave
        PitchTrack      // Oscillator following input pitch
    };

    static constexpr int maxVoices = 16;  // Max polyphony

    CarrierGenerator();

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    void setType (Type newType);
    void setFrequency (float freqHz);  // For monophonic/PitchTrack mode
    void setPitchFromInput (float detectedPitch);

    // Polyphonic MIDI support for Saw/Square modes
    void setMidiFrequencies (const std::vector<float>& frequencies);
    void clearMidiFrequencies();

    /**
     * Generate carrier signal into the output buffer.
     */
    void generateCarrier (juce::AudioBuffer<float>& outputBuffer,
                          const juce::AudioBuffer<float>* externalCarrier,
                          const juce::AudioBuffer<float>* modulatorForSelf);

    Type getCurrentType() const noexcept { return currentType; }

private:
    Type currentType = Type::Self;
    double sampleRate = 44100.0;

    // Monophonic oscillator state (for PitchTrack/Sine)
    float phase = 0.0f;
    float frequency = 440.0f;
    float phaseIncrement = 0.0f;
    float smoothedFrequency = 440.0f;
    float frequencySmoothing = 0.99f;

    // Polyphonic oscillator voices (for Saw/Square with MIDI)
    struct Voice {
        float phase = 0.0f;
        float frequency = 440.0f;
        float smoothedFrequency = 440.0f;
        bool active = false;
    };
    std::array<Voice, maxVoices> voices;
    int activeVoiceCount = 0;

    // Noise generator
    juce::Random noiseRandom;

    void updatePhaseIncrement();

    // Oscillator sample generators
    float generateSawSample();
    float generateSquareSample();
    float generateSineSample();
    float generateNoiseSample();

    // Polyphonic sample generators
    float generatePolySawSample();
    float generatePolySquareSample();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarrierGenerator)
};
