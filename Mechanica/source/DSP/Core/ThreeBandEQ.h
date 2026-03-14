#pragma once

#include <juce_dsp/juce_dsp.h>

namespace DSP {
namespace Core {

template <typename SampleType>
class ThreeBandEQ
{
public:
    ThreeBandEQ() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto& filter : lowShelf)
            filter.reset();
        for (auto& filter : midPeak)
            filter.reset();
        for (auto& filter : highShelf)
            filter.reset();

        maxChannels = static_cast<int> (spec.numChannels);
    }

    void updateCoefficients (float lowGainDb, float midGainDb, float midFreq,
                             float midQ, float highGainDb)
    {
        auto lowCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowShelf (
            sampleRate, static_cast<SampleType> (200.0), static_cast<SampleType> (0.707),
            juce::Decibels::decibelsToGain (static_cast<SampleType> (lowGainDb)));

        auto midCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makePeakFilter (
            sampleRate, static_cast<SampleType> (midFreq), static_cast<SampleType> (midQ),
            juce::Decibels::decibelsToGain (static_cast<SampleType> (midGainDb)));

        auto highCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighShelf (
            sampleRate, static_cast<SampleType> (3000.0), static_cast<SampleType> (0.707),
            juce::Decibels::decibelsToGain (static_cast<SampleType> (highGainDb)));

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            *lowShelf[ch].coefficients = *lowCoeffs;
            *midPeak[ch].coefficients = *midCoeffs;
            *highShelf[ch].coefficients = *highCoeffs;
        }
    }

    SampleType processSample (int channel, SampleType sample)
    {
        sample = lowShelf[channel].processSample (sample);
        sample = midPeak[channel].processSample (sample);
        sample = highShelf[channel].processSample (sample);
        return sample;
    }

    void reset()
    {
        for (auto& f : lowShelf) f.reset();
        for (auto& f : midPeak) f.reset();
        for (auto& f : highShelf) f.reset();
    }

private:
    juce::dsp::IIR::Filter<SampleType> lowShelf[2];
    juce::dsp::IIR::Filter<SampleType> midPeak[2];
    juce::dsp::IIR::Filter<SampleType> highShelf[2];

    double sampleRate = 44100.0;
    int maxChannels = 2;
};

} // namespace Core
} // namespace DSP
