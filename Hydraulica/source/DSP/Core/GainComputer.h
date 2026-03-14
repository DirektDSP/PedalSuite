#pragma once

#include <cmath>
#include <algorithm>

namespace DSP {
namespace Core {

template <typename SampleType>
class GainComputer
{
public:
    GainComputer() = default;

    void updateParameters (float thresholdDb, float ratio, float kneeDb)
    {
        threshold = static_cast<SampleType> (thresholdDb);
        slope = static_cast<SampleType> (1.0 - 1.0 / ratio);
        halfKnee = static_cast<SampleType> (kneeDb * 0.5);
    }

    // Returns gain reduction in dB (negative value) for a given input level in dB
    SampleType computeGainReduction (SampleType inputDb) const
    {
        auto overshoot = inputDb - threshold;

        SampleType gainReduction {};

        if (halfKnee > static_cast<SampleType> (0.001))
        {
            // Soft knee
            if (overshoot <= -halfKnee)
            {
                gainReduction = static_cast<SampleType> (0.0);
            }
            else if (overshoot >= halfKnee)
            {
                gainReduction = -slope * overshoot;
            }
            else
            {
                // Quadratic interpolation in the knee region
                auto x = overshoot + halfKnee;
                gainReduction = -slope * x * x / (static_cast<SampleType> (4.0) * halfKnee);
            }
        }
        else
        {
            // Hard knee
            if (overshoot > static_cast<SampleType> (0.0))
                gainReduction = -slope * overshoot;
            else
                gainReduction = static_cast<SampleType> (0.0);
        }

        return gainReduction;
    }

private:
    SampleType threshold = static_cast<SampleType> (-20.0);
    SampleType slope = static_cast<SampleType> (0.75);  // 1 - 1/ratio
    SampleType halfKnee = static_cast<SampleType> (3.0);
};

} // namespace Core
} // namespace DSP
