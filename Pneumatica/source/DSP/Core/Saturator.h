#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace DSP {
namespace Core {

enum class SaturationType
{
    Soft = 0,   // tanh
    Warm,       // atan
    Crispy      // foldback
};

template <typename SampleType>
class Saturator
{
public:
    Saturator() = default;

    void updateParameters (float crunchPercent, int type)
    {
        // Map 0-100% to drive 1.0 - 10.0
        drive = static_cast<SampleType> (1.0 + crunchPercent * 0.01 * 9.0);
        satType = static_cast<SaturationType> (type);
        enabled = crunchPercent > 0.1f;
    }

    SampleType processSample (SampleType input)
    {
        if (!enabled)
            return input;

        auto x = input * drive;

        switch (satType)
        {
            case SaturationType::Soft:
                return std::tanh (x);

            case SaturationType::Warm:
                return std::atan (x) * static_cast<SampleType> (2.0 / juce::MathConstants<double>::pi);

            case SaturationType::Crispy:
                return std::sin (x);
        }

        return input;
    }

private:
    SampleType drive = static_cast<SampleType> (1.0);
    SaturationType satType = SaturationType::Soft;
    bool enabled = false;
};

} // namespace Core
} // namespace DSP
