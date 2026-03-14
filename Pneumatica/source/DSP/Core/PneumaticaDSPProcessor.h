#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Utils/DSPUtils.h"
#include "../Utils/ParameterSmoother.h"
#include "CrossoverFilter.h"
#include "StereoWidener.h"
#include "Saturator.h"
#include "NoiseGenerator.h"
#include "ShimmerProcessor.h"

namespace DSP
{
    namespace Core
    {

        struct PneumaticaParams
        {
            float inputGainDb = 0.0f;
            float outputGainDb = 0.0f;
            float mixPercent = 50.0f;
            float crossover = 3000.0f;
            float width = 100.0f;
            float crunch = 0.0f;
            int crunchType = 0;
            float noiseLevel = -80.0f;
            int noiseType = 0;
            float noiseFilter = 8000.0f;
            float airFreq = 12000.0f;
            float airGain = 0.0f;
            float shimmer = 0.0f;
        };

        template <typename SampleType>
        class PneumaticaDSPProcessor
        {
        public:
            PneumaticaDSPProcessor() = default;

            void prepare (const juce::dsp::ProcessSpec& spec,
                          SampleType inputGainDb = SampleType{0.0},
                          SampleType outputGainDb = SampleType{0.0},
                          SampleType mixPercent = SampleType{50.0})
            {
                sampleRate = spec.sampleRate;
                samplesPerBlock = static_cast<int> (spec.maximumBlockSize);
                numChannels = static_cast<int> (spec.numChannels);

                prepareParameterSmoothers();

                inputGainSmoother.setTargetValue(Utils::DSPUtils::dbToGain(inputGainDb));
                inputGainSmoother.snapToTargetValue();
                outputGainSmoother.setTargetValue(Utils::DSPUtils::dbToGain(outputGainDb));
                outputGainSmoother.snapToTargetValue();
                mixSmoother.setTargetValue(Utils::DSPUtils::percentageToNormalized(mixPercent));
                mixSmoother.snapToTargetValue();

                wetBuffer.setSize (numChannels, samplesPerBlock);
                dryBuffer.setSize (numChannels, samplesPerBlock);

                // Prepare crossover
                crossover.prepare (spec);

                // Prepare noise generator
                noiseGen.prepare (sampleRate);

                // Prepare shimmer (per channel)
                for (auto& s : shimmerProc)
                    s.prepare (sampleRate);

                // Prepare air EQ (per channel)
                auto airCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighShelf (
                    sampleRate, static_cast<SampleType> (12000.0),
                    static_cast<SampleType> (0.707), static_cast<SampleType> (1.0));
                for (int ch = 0; ch < 2; ++ch)
                {
                    airShelf[ch].coefficients = airCoeffs;
                    airShelf[ch].reset();
                }

                reset(inputGainDb, outputGainDb, mixPercent);
            }

            void updateParameters (const PneumaticaParams& params)
            {
                inputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.inputGainDb)));
                outputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.outputGainDb)));
                mixSmoother.setTargetValue (Utils::DSPUtils::percentageToNormalized (static_cast<SampleType> (params.mixPercent)));

                crossover.updateCrossoverFrequency (params.crossover);
                widener.updateWidth (params.width);
                saturator.updateParameters (params.crunch, params.crunchType);
                noiseGen.updateParameters (params.noiseLevel, params.noiseType, params.noiseFilter);

                for (auto& s : shimmerProc)
                    s.updateParameters (params.shimmer);

                // Update air shelf EQ
                auto gain = juce::Decibels::decibelsToGain (static_cast<SampleType> (params.airGain));
                auto airCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighShelf (
                    sampleRate, static_cast<SampleType> (params.airFreq),
                    static_cast<SampleType> (0.707), gain);
                for (int ch = 0; ch < 2; ++ch)
                    *airShelf[ch].coefficients = *airCoeffs;
            }

            void processBlock (juce::AudioBuffer<SampleType>& buffer)
            {
                jassert (buffer.getNumChannels() >= 1);

                const int numSamples = buffer.getNumSamples();
                const int numCh = buffer.getNumChannels();
                const bool isStereo = numCh >= 2;

                if (wetBuffer.getNumSamples() != numSamples)
                {
                    wetBuffer.setSize (numChannels, numSamples, false, false, true);
                    dryBuffer.setSize (numChannels, numSamples, false, false, true);
                }

                dryBuffer.makeCopyOf (buffer);

                // Apply input gain to wet buffer
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto inputGain = inputGainSmoother.getNextValue();
                    for (int ch = 0; ch < numCh; ++ch)
                        wetBuffer.setSample (ch, i, buffer.getSample (ch, i) * inputGain);
                }

                // Process: crossover split → process highs → recombine
                for (int i = 0; i < numSamples; ++i)
                {
                    SampleType lowL {}, highL {}, lowR {}, highR {};

                    // Split into bands
                    crossover.processSample (0, wetBuffer.getSample (0, i), lowL, highL);
                    if (isStereo)
                        crossover.processSample (1, wetBuffer.getSample (1, i), lowR, highR);
                    else
                        { lowR = lowL; highR = highL; }

                    // --- Process HIGH band ---

                    // Stereo width (only meaningful in stereo)
                    if (isStereo)
                        widener.processStereo (highL, highR);

                    // Saturation
                    highL = saturator.processSample (highL);
                    highR = saturator.processSample (highR);

                    // Shimmer
                    highL = shimmerProc[0].processSample (highL);
                    if (isStereo)
                        highR = shimmerProc[1].processSample (highR);
                    else
                        highR = highL;

                    // Air EQ
                    highL = airShelf[0].processSample (highL);
                    highR = airShelf[1].processSample (highR);

                    // Add noise to high band
                    auto noise = noiseGen.generateSample();
                    highL += noise;
                    highR += noise;

                    // Recombine bands
                    wetBuffer.setSample (0, i, lowL + highL);
                    if (isStereo)
                        wetBuffer.setSample (1, i, lowR + highR);
                }

                // Mix dry/wet and apply output gain
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto mix = mixSmoother.getNextValue();
                    const auto outputGain = outputGainSmoother.getNextValue();

                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto* channelData = buffer.getWritePointer (ch);
                        const auto drySample = dryBuffer.getSample (ch, i);
                        const auto wetSample = wetBuffer.getSample (ch, i);
                        channelData[i] = (drySample * (static_cast<SampleType> (1.0) - mix) + wetSample * mix) * outputGain;
                    }
                }
            }

            void reset (SampleType inputGainDb = SampleType{0.0},
                        SampleType outputGainDb = SampleType{0.0},
                        SampleType mixPercent = SampleType{50.0})
            {
                inputGainSmoother.reset(Utils::DSPUtils::dbToGain(inputGainDb));
                inputGainSmoother.setTargetValue(Utils::DSPUtils::dbToGain(inputGainDb));
                inputGainSmoother.snapToTargetValue();
                outputGainSmoother.reset(Utils::DSPUtils::dbToGain(outputGainDb));
                outputGainSmoother.setTargetValue(Utils::DSPUtils::dbToGain(outputGainDb));
                outputGainSmoother.snapToTargetValue();
                mixSmoother.reset(Utils::DSPUtils::percentageToNormalized(mixPercent));
                mixSmoother.setTargetValue(Utils::DSPUtils::percentageToNormalized(mixPercent));
                mixSmoother.snapToTargetValue();

                crossover.reset();
                noiseGen.reset();
                for (auto& s : shimmerProc) s.reset();
                for (auto& f : airShelf) f.reset();
            }

        private:
            void prepareParameterSmoothers()
            {
                inputGainSmoother.prepare (sampleRate, 1.0);
                outputGainSmoother.prepare (sampleRate, 1.0);
                mixSmoother.prepare (sampleRate, 5.0);
            }

            // Parameter smoothers
            Utils::ParameterSmoother<SampleType> inputGainSmoother;
            Utils::ParameterSmoother<SampleType> outputGainSmoother;
            Utils::ParameterSmoother<SampleType> mixSmoother;

            // DSP components
            CrossoverFilter<SampleType> crossover;
            StereoWidener<SampleType> widener;
            Saturator<SampleType> saturator;
            NoiseGenerator<SampleType> noiseGen;
            ShimmerProcessor<SampleType> shimmerProc[2]; // per channel
            juce::dsp::IIR::Filter<SampleType> airShelf[2];

            // Buffers
            juce::AudioBuffer<SampleType> wetBuffer;
            juce::AudioBuffer<SampleType> dryBuffer;

            double sampleRate = 44100.0;
            int samplesPerBlock = 512;
            int numChannels = 2;
        };

    } // namespace Core
} // namespace DSP
