#pragma once

#include <cmath>

namespace DSP {
namespace Core {

// Envelope follower driven by the input signal level.
// Outputs an amplitude value 0..1 that shapes the synth oscillator.
template <typename SampleType>
class SynthEnvelope
{
public:
    SynthEnvelope() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = SampleType (0);
        gateOpen = false;
    }

    void updateParameters (float attackMs, float releaseMs, float sensitivityDb)
    {
        attackCoeff = std::exp (SampleType (-1) / (static_cast<SampleType> (attackMs * 0.001) * static_cast<SampleType> (sampleRate)));
        releaseCoeff = std::exp (SampleType (-1) / (static_cast<SampleType> (releaseMs * 0.001) * static_cast<SampleType> (sampleRate)));
        threshold = static_cast<SampleType> (std::pow (10.0, sensitivityDb * 0.05));
    }

    // Feed the absolute value of the input signal
    SampleType processSample (SampleType inputLevel)
    {
        // Gate with hysteresis: open when input exceeds threshold,
        // close when envelope falls well below
        if (inputLevel > threshold)
            gateOpen = true;
        else if (envelope < threshold * SampleType (0.5))
            gateOpen = false;

        // Follow the actual input amplitude (not binary on/off)
        SampleType target = gateOpen ? inputLevel : SampleType (0);

        // Smooth with attack/release ballistics
        if (target > envelope)
            envelope = attackCoeff * envelope + (SampleType (1) - attackCoeff) * target;
        else
            envelope = releaseCoeff * envelope + (SampleType (1) - releaseCoeff) * target;

        return envelope;
    }

    void reset()
    {
        envelope = SampleType (0);
        gateOpen = false;
    }

private:
    double sampleRate = 44100.0;
    SampleType envelope = SampleType (0);
    SampleType attackCoeff = SampleType (0.999);
    SampleType releaseCoeff = SampleType (0.9999);
    SampleType threshold = SampleType (0.01);
    bool gateOpen = false;
};

} // namespace Core
} // namespace DSP
