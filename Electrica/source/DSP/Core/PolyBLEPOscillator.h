#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace DSP {
namespace Core {

enum class OscWaveform
{
    Saw = 0,
    Square,
    Sine,
    Triangle
};

template <typename SampleType>
class PolyBLEPOscillator
{
public:
    PolyBLEPOscillator() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        phase = SampleType (0);
        phaseInc = SampleType (0);
    }

    void setFrequency (SampleType freqHz)
    {
        if (freqHz > SampleType (0) && freqHz < static_cast<SampleType> (sampleRate * 0.5))
            phaseInc = freqHz / static_cast<SampleType> (sampleRate);
        else
            phaseInc = SampleType (0);
    }

    void setWaveform (int type)
    {
        waveform = static_cast<OscWaveform> (type);
    }

    SampleType processSample()
    {
        if (phaseInc <= SampleType (0))
            return SampleType (0);

        SampleType output = SampleType (0);

        switch (waveform)
        {
            case OscWaveform::Saw:
            {
                // Naive saw: phase goes 0..1, output -1..1
                output = SampleType (2) * phase - SampleType (1);
                output -= polyBLEP (phase, phaseInc);
                break;
            }

            case OscWaveform::Square:
            {
                output = phase < SampleType (0.5) ? SampleType (1) : SampleType (-1);
                output += polyBLEP (phase, phaseInc);
                output -= polyBLEP (std::fmod (phase + SampleType (0.5), SampleType (1)), phaseInc);
                break;
            }

            case OscWaveform::Sine:
            {
                output = std::sin (SampleType (2) * static_cast<SampleType> (juce::MathConstants<double>::pi) * phase);
                break;
            }

            case OscWaveform::Triangle:
            {
                // Integrate the square wave for a triangle
                output = phase < SampleType (0.5) ? SampleType (1) : SampleType (-1);
                output += polyBLEP (phase, phaseInc);
                output -= polyBLEP (std::fmod (phase + SampleType (0.5), SampleType (1)), phaseInc);
                // Leaky integrator to form triangle from square
                triState = SampleType (0.999) * triState + phaseInc * SampleType (4) * output;
                output = triState;
                break;
            }
        }

        // Advance phase
        phase += phaseInc;
        if (phase >= SampleType (1))
            phase -= SampleType (1);

        return output;
    }

    void reset()
    {
        phase = SampleType (0);
        triState = SampleType (0);
    }

private:
    static SampleType polyBLEP (SampleType t, SampleType dt)
    {
        if (dt <= SampleType (0))
            return SampleType (0);

        // Rising edge
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - SampleType (1);
        }
        // Falling edge
        else if (t > SampleType (1) - dt)
        {
            t = (t - SampleType (1)) / dt;
            return t * t + t + t + SampleType (1);
        }
        return SampleType (0);
    }

    double sampleRate = 44100.0;
    SampleType phase = SampleType (0);
    SampleType phaseInc = SampleType (0);
    SampleType triState = SampleType (0);
    OscWaveform waveform = OscWaveform::Saw;
};

} // namespace Core
} // namespace DSP
