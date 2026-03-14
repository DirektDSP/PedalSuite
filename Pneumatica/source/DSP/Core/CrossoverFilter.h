#pragma once

#include <juce_dsp/juce_dsp.h>

namespace DSP {
namespace Core {

// Linkwitz-Riley 4th-order crossover filter for splitting into low and high bands.
// Implemented as two cascaded 2nd-order Butterworth filters per band.
// LR4 sums to unity: lowOut + highOut = input (flat magnitude, allpass phase).
template <typename SampleType>
class CrossoverFilter
{
public:
    CrossoverFilter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (int ch = 0; ch < 2; ++ch)
        {
            lpf1[ch].reset();
            lpf2[ch].reset();
            hpf1[ch].reset();
            hpf2[ch].reset();
        }
    }

    void updateCrossoverFrequency (float freq)
    {
        auto lpCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowPass (
            sampleRate, static_cast<SampleType> (freq), static_cast<SampleType> (0.707));
        auto hpCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighPass (
            sampleRate, static_cast<SampleType> (freq), static_cast<SampleType> (0.707));

        for (int ch = 0; ch < 2; ++ch)
        {
            *lpf1[ch].coefficients = *lpCoeffs;
            *lpf2[ch].coefficients = *lpCoeffs;
            *hpf1[ch].coefficients = *hpCoeffs;
            *hpf2[ch].coefficients = *hpCoeffs;
        }
    }

    void processSample (int channel, SampleType input, SampleType& lowOut, SampleType& highOut)
    {
        // Two cascaded Butterworth stages = Linkwitz-Riley 4th order
        lowOut  = lpf2[channel].processSample (lpf1[channel].processSample (input));
        highOut = hpf2[channel].processSample (hpf1[channel].processSample (input));
    }

    void reset()
    {
        for (auto& f : lpf1) f.reset();
        for (auto& f : lpf2) f.reset();
        for (auto& f : hpf1) f.reset();
        for (auto& f : hpf2) f.reset();
    }

private:
    // Two cascaded stages per band, per channel
    juce::dsp::IIR::Filter<SampleType> lpf1[2], lpf2[2];
    juce::dsp::IIR::Filter<SampleType> hpf1[2], hpf2[2];
    double sampleRate = 44100.0;
};

} // namespace Core
} // namespace DSP
