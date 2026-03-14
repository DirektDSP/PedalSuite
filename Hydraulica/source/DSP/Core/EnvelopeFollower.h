#pragma once

#include <cmath>
#include <algorithm>

namespace DSP {
namespace Core {

template <typename SampleType>
class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = static_cast<SampleType> (0.0);
    }

    void updateParameters (float attackMs, float releaseMs)
    {
        attackCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (std::max (0.01, static_cast<double> (attackMs)) * 0.001 * sampleRate)));
        releaseCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (std::max (1.0, static_cast<double> (releaseMs)) * 0.001 * sampleRate)));
    }

    // Feed absolute sample value, returns smoothed envelope
    SampleType process (SampleType inputAbs)
    {
        if (inputAbs > envelope)
            envelope = attackCoeff * envelope + (static_cast<SampleType> (1.0) - attackCoeff) * inputAbs;
        else
            envelope = releaseCoeff * envelope + (static_cast<SampleType> (1.0) - releaseCoeff) * inputAbs;

        return envelope;
    }

    SampleType getEnvelope() const { return envelope; }

    void reset()
    {
        envelope = static_cast<SampleType> (0.0);
    }

private:
    double sampleRate = 44100.0;
    SampleType attackCoeff = static_cast<SampleType> (0.0);
    SampleType releaseCoeff = static_cast<SampleType> (0.0);
    SampleType envelope = static_cast<SampleType> (0.0);
};

} // namespace Core
} // namespace DSP
