#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "PolyBLEPOscillator.h"
#include "SynthEnvelope.h"

namespace DSP {
namespace Core {

enum class SynthFilterType { LPF = 0, HPF, BPF };
enum class SynthDistType { Soft = 0, Hard, Fold };
enum class SynthCompSpeed { Fast = 0, Medium, Slow };

// Combines oscillator + envelope + filter + distortion + compressor
// into a single synth voice processing chain.
template <typename SampleType>
class SynthVoice
{
public:
    SynthVoice() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        osc.prepare (sampleRate);
        env.prepare (sampleRate);

        auto coeffs = juce::dsp::IIR::Coefficients<SampleType>::makeLowPass (
            sampleRate, SampleType (2000), SampleType (0.707));
        filter.coefficients = coeffs;
        filter.reset();

        compEnvelope = SampleType (0);
        currentFreq = SampleType (0);
        targetFreq = SampleType (0);
        glideCoeff = SampleType (0.999);
    }

    void updateParameters (int oscWave, int oscOctave, float oscDetuneCents,
                           float envAttackMs, float envReleaseMs, float envSensitivityDb,
                           int filterType, float filterFreqHz, float filterReso,
                           float distDriveDb, int distType,
                           float compAmount, int compSpeed,
                           float glideMs)
    {
        osc.setWaveform (oscWave);
        octaveShift = oscOctave;
        detuneCents = static_cast<SampleType> (oscDetuneCents);

        env.updateParameters (envAttackMs, envReleaseMs, envSensitivityDb);

        // Filter
        synthFilterType = static_cast<SynthFilterType> (filterType);
        auto freq = static_cast<SampleType> (std::max (50.0f, std::min (filterFreqHz, static_cast<float> (sampleRate * 0.45))));
        auto q = static_cast<SampleType> (filterReso);

        switch (synthFilterType)
        {
            case SynthFilterType::LPF:
                *filter.coefficients = *juce::dsp::IIR::Coefficients<SampleType>::makeLowPass (sampleRate, freq, q);
                break;
            case SynthFilterType::HPF:
                *filter.coefficients = *juce::dsp::IIR::Coefficients<SampleType>::makeHighPass (sampleRate, freq, q);
                break;
            case SynthFilterType::BPF:
                *filter.coefficients = *juce::dsp::IIR::Coefficients<SampleType>::makeBandPass (sampleRate, freq, q);
                break;
        }

        // Distortion
        synthDistType = static_cast<SynthDistType> (distType);
        distDrive = static_cast<SampleType> (std::pow (10.0, distDriveDb * 0.05));
        distEnabled = distDriveDb > 0.1f;

        // Compressor
        compAmountNorm = static_cast<SampleType> (compAmount * 0.01);
        switch (static_cast<SynthCompSpeed> (compSpeed))
        {
            case SynthCompSpeed::Fast:
                compAttack = std::exp (SampleType (-1) / (SampleType (0.001) * static_cast<SampleType> (sampleRate)));
                compRelease = std::exp (SampleType (-1) / (SampleType (0.01) * static_cast<SampleType> (sampleRate)));
                break;
            case SynthCompSpeed::Medium:
                compAttack = std::exp (SampleType (-1) / (SampleType (0.005) * static_cast<SampleType> (sampleRate)));
                compRelease = std::exp (SampleType (-1) / (SampleType (0.05) * static_cast<SampleType> (sampleRate)));
                break;
            case SynthCompSpeed::Slow:
                compAttack = std::exp (SampleType (-1) / (SampleType (0.02) * static_cast<SampleType> (sampleRate)));
                compRelease = std::exp (SampleType (-1) / (SampleType (0.2) * static_cast<SampleType> (sampleRate)));
                break;
        }

        // Glide
        if (glideMs > 0.1f)
            glideCoeff = std::exp (SampleType (-1) / (static_cast<SampleType> (glideMs * 0.001) * static_cast<SampleType> (sampleRate)));
        else
            glideCoeff = SampleType (0); // instant
    }

    // Set the target pitch from pitch detector
    void setTargetFrequency (SampleType freqHz)
    {
        if (freqHz > SampleType (20) && freqHz < SampleType (5000))
            targetFreq = freqHz;
    }

    // Process one sample. inputLevel = abs(monoInput) for envelope.
    SampleType processSample (SampleType inputLevel)
    {
        // Glide toward target frequency
        if (glideCoeff > SampleType (0) && currentFreq > SampleType (0))
            currentFreq = glideCoeff * currentFreq + (SampleType (1) - glideCoeff) * targetFreq;
        else
            currentFreq = targetFreq;

        // Apply octave shift and detune
        auto freq = currentFreq * static_cast<SampleType> (std::pow (2.0, octaveShift))
                    * static_cast<SampleType> (std::pow (2.0, static_cast<double> (detuneCents) / 1200.0));

        osc.setFrequency (freq);

        // Generate oscillator
        auto oscOut = osc.processSample();

        // Apply envelope from input level
        auto envVal = env.processSample (inputLevel);
        oscOut *= envVal;

        // Filter
        oscOut = filter.processSample (oscOut);

        // Distortion
        if (distEnabled)
        {
            auto x = oscOut * distDrive;
            switch (synthDistType)
            {
                case SynthDistType::Soft:
                    oscOut = std::tanh (x);
                    break;
                case SynthDistType::Hard:
                    oscOut = std::max (SampleType (-1), std::min (x, SampleType (1)));
                    break;
                case SynthDistType::Fold:
                    oscOut = std::sin (x);
                    break;
            }
        }

        // Simple compressor (envelope-based gain reduction)
        if (compAmountNorm > SampleType (0.001))
        {
            auto absVal = std::abs (oscOut);
            if (absVal > compEnvelope)
                compEnvelope = compAttack * compEnvelope + (SampleType (1) - compAttack) * absVal;
            else
                compEnvelope = compRelease * compEnvelope + (SampleType (1) - compRelease) * absVal;

            if (compEnvelope > SampleType (0.001))
            {
                auto gain = SampleType (1) / (SampleType (1) + compEnvelope * compAmountNorm * SampleType (4));
                oscOut *= gain;
            }
        }

        return oscOut;
    }

    void reset()
    {
        osc.reset();
        env.reset();
        filter.reset();
        compEnvelope = SampleType (0);
        currentFreq = SampleType (0);
        targetFreq = SampleType (0);
    }

private:
    double sampleRate = 44100.0;

    PolyBLEPOscillator<SampleType> osc;
    SynthEnvelope<SampleType> env;
    juce::dsp::IIR::Filter<SampleType> filter;

    int octaveShift = 0;
    SampleType detuneCents = SampleType (0);

    SynthFilterType synthFilterType = SynthFilterType::LPF;
    SynthDistType synthDistType = SynthDistType::Soft;

    SampleType distDrive = SampleType (1);
    bool distEnabled = false;

    SampleType compAmountNorm = SampleType (0);
    SampleType compAttack = SampleType (0.999);
    SampleType compRelease = SampleType (0.9999);
    SampleType compEnvelope = SampleType (0);

    SampleType glideCoeff = SampleType (0.999);
    SampleType currentFreq = SampleType (0);
    SampleType targetFreq = SampleType (0);
};

} // namespace Core
} // namespace DSP
