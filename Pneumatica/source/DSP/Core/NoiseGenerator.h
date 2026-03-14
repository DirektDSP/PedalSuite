#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace DSP {
namespace Core {

enum class NoiseType
{
    AnalogHiss = 0,
    TapeHiss,
    DigitalCrackle
};

template <typename SampleType>
class NoiseGenerator
{
public:
    NoiseGenerator() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        noiseFilter.reset();
    }

    void updateParameters (float levelDb, int type, float filterFreq)
    {
        noiseLevel = static_cast<SampleType> (std::pow (10.0, levelDb * 0.05));
        noiseType = static_cast<NoiseType> (type);
        enabled = levelDb > -79.0f;

        auto coeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowPass (
            sampleRate, static_cast<SampleType> (filterFreq));
        *noiseFilter.coefficients = *coeffs;
    }

    SampleType generateSample()
    {
        if (!enabled)
            return static_cast<SampleType> (0.0);

        SampleType noise {};

        switch (noiseType)
        {
            case NoiseType::AnalogHiss:
            {
                // White noise filtered
                noise = getWhiteNoise();
                noise = noiseFilter.processSample (noise);
                break;
            }

            case NoiseType::TapeHiss:
            {
                // Slightly colored noise — pink-ish via simple filtering
                noise = getWhiteNoise();
                // Simple one-pole lowpass for pink-ish character
                tapeState = tapeState * static_cast<SampleType> (0.95)
                            + noise * static_cast<SampleType> (0.05);
                noise = (noise + tapeState) * static_cast<SampleType> (0.5);
                noise = noiseFilter.processSample (noise);
                break;
            }

            case NoiseType::DigitalCrackle:
            {
                // Sparse random impulses
                crackleCounter++;
                if (crackleCounter >= crackleThreshold)
                {
                    noise = getWhiteNoise() * static_cast<SampleType> (2.0);
                    crackleCounter = 0;
                    crackleThreshold = 100 + (random.nextInt (500));
                }
                break;
            }
        }

        return noise * noiseLevel;
    }

    void reset()
    {
        noiseFilter.reset();
        tapeState = static_cast<SampleType> (0.0);
        crackleCounter = 0;
    }

private:
    SampleType getWhiteNoise()
    {
        return static_cast<SampleType> (random.nextFloat() * 2.0f - 1.0f);
    }

    double sampleRate = 44100.0;
    SampleType noiseLevel = static_cast<SampleType> (0.0);
    NoiseType noiseType = NoiseType::AnalogHiss;
    bool enabled = false;

    juce::Random random;
    juce::dsp::IIR::Filter<SampleType> noiseFilter;

    // Tape hiss state
    SampleType tapeState = static_cast<SampleType> (0.0);

    // Crackle state
    int crackleCounter = 0;
    int crackleThreshold = 200;
};

} // namespace Core
} // namespace DSP
