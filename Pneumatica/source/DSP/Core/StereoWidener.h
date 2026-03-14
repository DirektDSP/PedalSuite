#pragma once

namespace DSP {
namespace Core {

// Mid/Side stereo width processor
template <typename SampleType>
class StereoWidener
{
public:
    StereoWidener() = default;

    void updateWidth (float widthPercent)
    {
        // 100% = no change, 200% = max widening, 0% = mono
        widthFactor = static_cast<SampleType> (widthPercent * 0.01);
    }

    void processStereo (SampleType& left, SampleType& right)
    {
        auto mid  = (left + right) * static_cast<SampleType> (0.5);
        auto side = (left - right) * static_cast<SampleType> (0.5);

        // Scale side signal by width factor
        side *= widthFactor;

        left  = mid + side;
        right = mid - side;
    }

private:
    SampleType widthFactor = static_cast<SampleType> (1.0);
};

} // namespace Core
} // namespace DSP
