#pragma once

#include <cmath>
#include <algorithm>

namespace DSP {
namespace Core {

// The DOOM normalizer: a fast AGC that normalizes signal to constant amplitude.
// As the doom amount increases, the envelope follower gets faster and the output
// approaches constant amplitude — every sample at the same level.
template <typename SampleType>
class DoomNormalizer
{
public:
    DoomNormalizer() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = static_cast<SampleType> (0.0001);
    }

    void updateParameters (float doomPercent)
    {
        // Map doom 0-100% to envelope speed
        // At 0%: slow envelope (effectively off)
        // At 100%: near-instant envelope (total destruction)
        auto doomNorm = static_cast<double> (doomPercent) * 0.01;
        doomAmount = static_cast<SampleType> (doomNorm);

        // Attack: from 50ms (doom=0) to 0.01ms (doom=100)
        auto attackMs = 50.0 * std::pow (0.0002, doomNorm); // exponential sweep
        attackCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (std::max (0.001, attackMs) * 0.001 * sampleRate)));

        // Release: from 200ms (doom=0) to 0.1ms (doom=100)
        auto releaseMs = 200.0 * std::pow (0.0005, doomNorm);
        releaseCoeff = static_cast<SampleType> (
            std::exp (-1.0 / (std::max (0.01, releaseMs) * 0.001 * sampleRate)));
    }

    // Process a sample: returns the doom-normalized version
    SampleType process (SampleType input)
    {
        auto absInput = std::abs (input);

        // Track the envelope
        if (absInput > envelope)
            envelope = attackCoeff * envelope + (static_cast<SampleType> (1.0) - attackCoeff) * absInput;
        else
            envelope = releaseCoeff * envelope + (static_cast<SampleType> (1.0) - releaseCoeff) * absInput;

        // Prevent division by zero
        auto safeEnvelope = std::max (envelope, static_cast<SampleType> (0.0001));

        // Normalize: divide signal by envelope to get constant amplitude
        return input / safeEnvelope;
    }

    SampleType getDoomAmount() const { return doomAmount; }

    void reset()
    {
        envelope = static_cast<SampleType> (0.0001);
    }

private:
    double sampleRate = 44100.0;
    SampleType doomAmount = static_cast<SampleType> (0.0);
    SampleType attackCoeff = static_cast<SampleType> (0.0);
    SampleType releaseCoeff = static_cast<SampleType> (0.0);
    SampleType envelope = static_cast<SampleType> (0.0001);
};

} // namespace Core
} // namespace DSP
