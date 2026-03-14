#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace DSP {
namespace Core {

template <typename SampleType>
class FeedbackProcessor
{
public:
    FeedbackProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxChannels = static_cast<int> (spec.numChannels);

        // One block of delay
        feedbackBuffer.resize (maxChannels);
        bufferSize = static_cast<int> (spec.maximumBlockSize);

        for (auto& ch : feedbackBuffer)
        {
            ch.resize (static_cast<size_t> (bufferSize), static_cast<SampleType> (0.0));
        }

        writePos = 0;

        for (auto& f : toneFilter)
            f.reset();
    }

    void updateParameters (float feedbackAmount, float toneFreq)
    {
        feedback = static_cast<SampleType> (feedbackAmount * 0.01); // 0-80% -> 0-0.8

        auto coeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowPass (
            sampleRate, static_cast<SampleType> (toneFreq));

        for (int ch = 0; ch < maxChannels; ++ch)
            *toneFilter[ch].coefficients = *coeffs;
    }

    SampleType getFeedbackSample (int channel)
    {
        if (feedback <= static_cast<SampleType> (0.0001))
            return static_cast<SampleType> (0.0);

        auto sample = feedbackBuffer[channel][static_cast<size_t> (writePos)];
        sample = toneFilter[channel].processSample (sample);
        return sample * feedback;
    }

    void writeSample (int channel, SampleType sample)
    {
        feedbackBuffer[channel][static_cast<size_t> (writePos)] = sample;
    }

    void advanceWritePosition()
    {
        writePos = (writePos + 1) % bufferSize;
    }

    void reset()
    {
        for (auto& ch : feedbackBuffer)
            std::fill (ch.begin(), ch.end(), static_cast<SampleType> (0.0));

        for (auto& f : toneFilter)
            f.reset();

        writePos = 0;
    }

private:
    double sampleRate = 44100.0;
    int maxChannels = 2;
    int bufferSize = 512;
    int writePos = 0;

    SampleType feedback = static_cast<SampleType> (0.0);
    std::vector<std::vector<SampleType>> feedbackBuffer;
    juce::dsp::IIR::Filter<SampleType> toneFilter[2];
};

} // namespace Core
} // namespace DSP
