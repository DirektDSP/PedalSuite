#pragma once

#include <cmath>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

namespace DSP {
namespace Core {

// Simple pitch-shift +12 semitones using dual overlapping delay lines with Hann windows.
// Designed to work on an already-isolated high band, so artifacts are less critical.
template <typename SampleType>
class ShimmerProcessor
{
public:
    ShimmerProcessor() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        // Buffer for ~50ms at max sample rate
        bufferSize = static_cast<int> (sampleRate * 0.05) + 1;
        delayBuffer.resize (static_cast<size_t> (bufferSize), static_cast<SampleType> (0.0));
        writePos = 0;

        // Grain size: ~20ms
        grainSize = static_cast<SampleType> (sampleRate * 0.02);

        // Pitch shift ratio for +12 semitones (octave up)
        pitchRatio = static_cast<SampleType> (2.0);

        // Initialize two grains, offset by half
        grainPhase[0] = static_cast<SampleType> (0.0);
        grainPhase[1] = static_cast<SampleType> (0.5);
    }

    void updateParameters (float shimmerPercent)
    {
        shimmerAmount = static_cast<SampleType> (shimmerPercent * 0.01);
    }

    SampleType processSample (SampleType input)
    {
        if (shimmerAmount <= static_cast<SampleType> (0.001))
            return input;

        // Write to delay buffer
        delayBuffer[static_cast<size_t> (writePos)] = input;

        // Read from two grains
        SampleType output = static_cast<SampleType> (0.0);

        for (int g = 0; g < 2; ++g)
        {
            // Advance grain phase
            auto phaseInc = (pitchRatio - static_cast<SampleType> (1.0)) / grainSize;
            grainPhase[g] += phaseInc;

            if (grainPhase[g] >= static_cast<SampleType> (1.0))
                grainPhase[g] -= static_cast<SampleType> (1.0);

            // Read position
            auto delay = grainPhase[g] * grainSize;
            auto readPos = static_cast<SampleType> (writePos) - delay;
            if (readPos < static_cast<SampleType> (0.0))
                readPos += static_cast<SampleType> (bufferSize);

            // Linear interpolation
            auto readPosInt = static_cast<int> (readPos);
            auto frac = readPos - static_cast<SampleType> (readPosInt);
            auto idx0 = readPosInt % bufferSize;
            auto idx1 = (readPosInt + 1) % bufferSize;

            auto sample = delayBuffer[static_cast<size_t> (idx0)] * (static_cast<SampleType> (1.0) - frac)
                          + delayBuffer[static_cast<size_t> (idx1)] * frac;

            // Hann window based on grain phase
            auto window = static_cast<SampleType> (0.5)
                          * (static_cast<SampleType> (1.0)
                             - std::cos (static_cast<SampleType> (2.0 * juce::MathConstants<double>::pi)
                                         * grainPhase[g]));

            output += sample * window;
        }

        writePos = (writePos + 1) % bufferSize;

        // Blend shimmer with original
        return input + output * shimmerAmount;
    }

    void reset()
    {
        std::fill (delayBuffer.begin(), delayBuffer.end(), static_cast<SampleType> (0.0));
        writePos = 0;
        grainPhase[0] = static_cast<SampleType> (0.0);
        grainPhase[1] = static_cast<SampleType> (0.5);
    }

private:
    double sampleRate = 44100.0;
    std::vector<SampleType> delayBuffer;
    int bufferSize = 2048;
    int writePos = 0;

    SampleType grainSize = static_cast<SampleType> (882.0); // ~20ms at 44.1k
    SampleType pitchRatio = static_cast<SampleType> (2.0);
    SampleType grainPhase[2] = {};
    SampleType shimmerAmount = static_cast<SampleType> (0.0);
};

} // namespace Core
} // namespace DSP
