#pragma once

#include <q/pitch/pitch_detector.hpp>

namespace DSP {
namespace Core {

// Wrapper around Cycfi Q's pitch detector for use in ElectricaDSPProcessor.
// Cycfi Q uses bitstream autocorrelation (BACF) with multi-detector fusion,
// achieving single-cycle detection (~12ms at E2) with near-zero CPU cost.
// API matches MPMPitchDetector for drop-in switching.
template <typename SampleType>
class CycfiQPitchDetector
{
public:
    CycfiQPitchDetector() = default;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        rebuildDetector();
    }

    void updateParameters (float /*windowMs*/, float /*clarityThresh*/)
    {
        // Cycfi Q manages its own windowing and thresholds internally.
        // The hysteresis parameter is set at construction time.
        // windowMs and clarityThresh are ignored — kept for API compatibility.
    }

    void pushSample (SampleType monoSample)
    {
        if (! detector)
            return;

        bool ready = (*detector) (static_cast<float> (monoSample));
        if (ready)
        {
            float freq = detector->get_frequency();
            if (freq > 0.0f)
            {
                detectedFrequency = freq;
                clarity = detector->periodicity();
            }
            else
            {
                clarity = 0.0f;
            }
        }
    }

    SampleType getFrequency() const { return static_cast<SampleType> (detectedFrequency); }
    SampleType getConfidence() const { return static_cast<SampleType> (clarity); }

    void reset()
    {
        detectedFrequency = 0.0f;
        clarity = 0.0f;
        rebuildDetector();
    }

private:
    void rebuildDetector()
    {
        using namespace cycfi::q::literals;
        // Guitar range: ~30Hz (below drop tunings) to ~5kHz (harmonics)
        detector = std::make_unique<cycfi::q::pitch_detector> (
            30_Hz, 5000_Hz,
            static_cast<float> (sampleRate),
            -45_dB // hysteresis: -45dB works well for guitar pickups
        );
    }

    double sampleRate = 44100.0;

    std::unique_ptr<cycfi::q::pitch_detector> detector;

    float detectedFrequency = 0.0f;
    float clarity = 0.0f;
};

} // namespace Core
} // namespace DSP
