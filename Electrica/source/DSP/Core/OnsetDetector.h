#pragma once

#include <cmath>

namespace DSP {
namespace Core {

// Onset detector using high-frequency content (HFC) method.
// Detects pick attacks within 1-5ms by tracking energy spikes in
// high-passed signal. Outputs onset trigger flag per sample.
template <typename SampleType>
class OnsetDetector
{
public:
    OnsetDetector() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        // Energy smoothing coefficients
        // Fast attack (~1ms) for catching transients
        energyAttack = std::exp (SampleType (-1) / (SampleType (0.001) * static_cast<SampleType> (sampleRate)));
        // Slower release (~10ms)
        energyRelease = std::exp (SampleType (-1) / (SampleType (0.01) * static_cast<SampleType> (sampleRate)));

        // Long-term mean energy (~100ms window)
        meanCoeff = std::exp (SampleType (-1) / (SampleType (0.1) * static_cast<SampleType> (sampleRate)));

        holdSamples = static_cast<int> (sampleRate * 0.030); // 30ms hold between onsets

        reset();
    }

    void updateParameters (float thresholdMultiplier)
    {
        onsetThreshold = static_cast<SampleType> (thresholdMultiplier);
    }

    // Returns true on the sample where an onset is detected
    bool processSample (SampleType inputSample)
    {
        // 1st-order high-pass filter to isolate transient content
        auto hp = inputSample - prevSample;
        prevSample = inputSample;

        // Short-term energy tracking with fast attack / slower release
        auto instantEnergy = hp * hp;
        if (instantEnergy > shortEnergy)
            shortEnergy = energyAttack * shortEnergy + (SampleType (1) - energyAttack) * instantEnergy;
        else
            shortEnergy = energyRelease * shortEnergy + (SampleType (1) - energyRelease) * instantEnergy;

        // Long-term running mean energy
        meanEnergy = meanCoeff * meanEnergy + (SampleType (1) - meanCoeff) * shortEnergy;

        // Hold timer prevents rapid re-triggering
        if (holdCounter > 0)
        {
            --holdCounter;
            return false;
        }

        // Onset: short-term energy significantly exceeds long-term mean
        if (meanEnergy > SampleType (1e-10) && shortEnergy > onsetThreshold * meanEnergy)
        {
            holdCounter = holdSamples;
            return true;
        }

        return false;
    }

    void reset()
    {
        prevSample = SampleType (0);
        shortEnergy = SampleType (0);
        meanEnergy = SampleType (0);
        holdCounter = 0;
    }

private:
    double sampleRate = 44100.0;

    SampleType energyAttack = SampleType (0.99);
    SampleType energyRelease = SampleType (0.999);
    SampleType meanCoeff = SampleType (0.9999);
    SampleType onsetThreshold = SampleType (5);

    SampleType prevSample = SampleType (0);
    SampleType shortEnergy = SampleType (0);
    SampleType meanEnergy = SampleType (0);

    int holdSamples = 1323;
    int holdCounter = 0;
};

} // namespace Core
} // namespace DSP
