#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace DSP {
namespace Core {

enum class WaveshaperType
{
    Tanh = 0,
    Atan,
    Foldback,
    HardClip,
    Tube,
    Rectify
};

template <typename SampleType>
class WaveshaperBank
{
public:
    WaveshaperBank() = default;

    static SampleType process (SampleType input, SampleType drive, WaveshaperType type,
                               SampleType bias, SampleType asymmetry)
    {
        auto x = input + bias;
        x *= drive;

        SampleType shaped {};

        switch (type)
        {
            case WaveshaperType::Tanh:
                shaped = std::tanh (x);
                break;

            case WaveshaperType::Atan:
                shaped = std::atan (x) * static_cast<SampleType> (2.0 / juce::MathConstants<double>::pi);
                break;

            case WaveshaperType::Foldback:
                shaped = std::sin (x);
                break;

            case WaveshaperType::HardClip:
                shaped = juce::jlimit (static_cast<SampleType> (-1.0),
                                       static_cast<SampleType> (1.0), x);
                break;

            case WaveshaperType::Tube:
            {
                if (x >= static_cast<SampleType> (0.0))
                    shaped = static_cast<SampleType> (1.0) - std::exp (-x);
                else
                    shaped = static_cast<SampleType> (-1.0 / 3.0)
                             * (static_cast<SampleType> (1.0)
                                - std::exp (static_cast<SampleType> (3.0) * x));
                break;
            }

            case WaveshaperType::Rectify:
                shaped = std::abs (x);
                break;
        }

        // Asymmetry: blend between full shaped and positive-only
        if (asymmetry > static_cast<SampleType> (0.0))
        {
            auto positiveOnly = std::max (shaped, static_cast<SampleType> (0.0));
            shaped = shaped + asymmetry * (positiveOnly - shaped);
        }

        return shaped;
    }
};

} // namespace Core
} // namespace DSP
