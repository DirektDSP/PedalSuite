#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Utils/DSPUtils.h"

namespace DSP {
namespace Core {

template <typename SampleType>
class NoiseGate
{
public:
    NoiseGate() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = static_cast<SampleType> (0.0);
        gateGain = static_cast<SampleType> (0.0);
    }

    void updateParameters (float thresholdDb, float releaseMs)
    {
        threshold = Utils::DSPUtils::dbToGain (thresholdDb);
        releaseCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (releaseMs * 0.001 * sampleRate)));
        // Fast fixed attack for gate
        attackCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (0.1 * 0.001 * sampleRate)));
    }

    SampleType processSample (SampleType input)
    {
        // Envelope follower on absolute value
        auto absInput = std::abs (input);
        if (absInput > envelope)
            envelope = attackCoeff * envelope + (static_cast<SampleType> (1.0) - attackCoeff) * absInput;
        else
            envelope = releaseCoeff * envelope + (static_cast<SampleType> (1.0) - releaseCoeff) * absInput;

        // Gate open/close
        auto target = (envelope >= threshold)
                          ? static_cast<SampleType> (1.0)
                          : static_cast<SampleType> (0.0);

        // Smooth the gate gain to avoid clicks
        if (target > gateGain)
            gateGain = gateGain + (static_cast<SampleType> (1.0) - attackCoeff) * (target - gateGain);
        else
            gateGain = gateGain + (static_cast<SampleType> (1.0) - releaseCoeff) * (target - gateGain);

        return input * gateGain;
    }

    void reset()
    {
        envelope = static_cast<SampleType> (0.0);
        gateGain = static_cast<SampleType> (0.0);
    }

private:
    double sampleRate = 44100.0;
    SampleType threshold = static_cast<SampleType> (0.0);
    SampleType attackCoeff = static_cast<SampleType> (0.0);
    SampleType releaseCoeff = static_cast<SampleType> (0.0);
    SampleType envelope = static_cast<SampleType> (0.0);
    SampleType gateGain = static_cast<SampleType> (0.0);
};

} // namespace Core
} // namespace DSP
