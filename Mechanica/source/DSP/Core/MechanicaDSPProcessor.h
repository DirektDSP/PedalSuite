#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Utils/DSPUtils.h"
#include "../Utils/ParameterSmoother.h"
#include "WaveshaperBank.h"
#include "ThreeBandEQ.h"
#include "NoiseGate.h"
#include "FeedbackProcessor.h"

namespace DSP
{
    namespace Core
    {

        struct MechanicaParams
        {
            float inputGainDb = 0.0f;
            float outputGainDb = 0.0f;
            float mixPercent = 50.0f;
            float drive = 12.0f;
            int waveshaper = 0;
            float bias = 0.0f;
            float asymmetry = 0.0f;
            float preLow = 0.0f;
            float preMid = 0.0f;
            float preMidFreq = 800.0f;
            float preMidQ = 1.0f;
            float preHigh = 0.0f;
            float postLow = 0.0f;
            float postMid = 0.0f;
            float postMidFreq = 800.0f;
            float postMidQ = 1.0f;
            float postHigh = 0.0f;
            int oversampling = 1;
            float feedback = 0.0f;
            float feedbackTone = 2000.0f;
            float gateThreshold = -80.0f;
            float gateRelease = 50.0f;
            int stages = 1;
        };

        template <typename SampleType>
        class MechanicaDSPProcessor
        {
        public:
            MechanicaDSPProcessor() = default;

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

                // Prepare EQ
                preEQ.prepare (spec);
                postEQ.prepare (spec);

                // Prepare noise gate (per channel)
                for (auto& g : gate)
                    g.prepare (sampleRate);

                // Prepare feedback
                feedbackProc.prepare (spec);

                // Prepare DC blockers
                auto dcCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighPass (
                    sampleRate, static_cast<SampleType> (20.0));
                for (int stage = 0; stage < 3; ++stage)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        dcBlocker[stage][ch].coefficients = dcCoeffs;
                        dcBlocker[stage][ch].reset();
                    }

                // Prepare oversampling (2x, 4x, 8x)
                for (int i = 0; i < 3; ++i)
                {
                    oversampler[i] = std::make_unique<juce::dsp::Oversampling<SampleType>> (
                        static_cast<size_t> (numChannels),
                        static_cast<size_t> (i + 1), // 1=2x, 2=4x, 3=8x
                        juce::dsp::Oversampling<SampleType>::filterHalfBandPolyphaseIIR,
                        true);
                    oversampler[i]->initProcessing (static_cast<size_t> (samplesPerBlock));
                }

                reset(inputGainDb, outputGainDb, mixPercent);
            }

            void updateParameters (const MechanicaParams& params)
            {
                inputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.inputGainDb)));
                outputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.outputGainDb)));
                mixSmoother.setTargetValue (Utils::DSPUtils::percentageToNormalized (static_cast<SampleType> (params.mixPercent)));

                driveSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.drive)));
                biasSmoother.setTargetValue (static_cast<SampleType> (params.bias));
                asymmetrySmoother.setTargetValue (Utils::DSPUtils::percentageToNormalized (static_cast<SampleType> (params.asymmetry)));

                currentWaveshaper = static_cast<WaveshaperType> (params.waveshaper);
                currentStages = juce::jlimit (1, 3, params.stages);
                currentOversamplingIndex = juce::jlimit (0, 3, params.oversampling);

                preEQ.updateCoefficients (params.preLow, params.preMid, params.preMidFreq,
                                          params.preMidQ, params.preHigh);
                postEQ.updateCoefficients (params.postLow, params.postMid, params.postMidFreq,
                                           params.postMidQ, params.postHigh);

                for (auto& g : gate)
                    g.updateParameters (params.gateThreshold, params.gateRelease);

                feedbackProc.updateParameters (params.feedback, params.feedbackTone);
            }

            void processBlock (juce::AudioBuffer<SampleType>& buffer)
            {
                jassert (buffer.getNumChannels() >= 1);

                const int numSamples = buffer.getNumSamples();

                if (wetBuffer.getNumSamples() != numSamples)
                {
                    wetBuffer.setSize (numChannels, numSamples, false, false, true);
                    dryBuffer.setSize (numChannels, numSamples, false, false, true);
                }

                dryBuffer.makeCopyOf (buffer);

                // Apply input gain
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto inputGain = inputGainSmoother.getNextValue();
                    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    {
                        auto sample = buffer.getSample(channel, i) * inputGain;
                        wetBuffer.setSample(channel, i, sample);
                    }
                }

                // Noise gate (pre-distortion)
                if (gate[0].processSample (static_cast<SampleType> (0.0)) >= static_cast<SampleType> (0.0)) // just to init
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
                        {
                            auto sample = wetBuffer.getSample (ch, i);
                            sample = gate[ch].processSample (sample);
                            wetBuffer.setSample (ch, i, sample);
                        }
                    }
                }

                // Add feedback
                for (int i = 0; i < numSamples; ++i)
                {
                    for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
                    {
                        auto sample = wetBuffer.getSample (ch, i);
                        sample += feedbackProc.getFeedbackSample (ch);
                        wetBuffer.setSample (ch, i, sample);
                    }
                    feedbackProc.advanceWritePosition();
                }

                // Oversampling + distortion core
                if (currentOversamplingIndex > 0 && currentOversamplingIndex <= 3)
                {
                    auto& os = *oversampler[currentOversamplingIndex - 1];
                    juce::dsp::AudioBlock<SampleType> block (wetBuffer);
                    auto oversampledBlock = os.processSamplesUp (block);
                    processDistortionCore (oversampledBlock);
                    os.processSamplesDown (block);
                }
                else
                {
                    juce::dsp::AudioBlock<SampleType> block (wetBuffer);
                    processDistortionCore (block);
                }

                // Write feedback tap
                for (int i = 0; i < numSamples; ++i)
                {
                    for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
                        feedbackProc.writeSample (ch, wetBuffer.getSample (ch, i));
                }

                // Mix dry/wet and apply output gain
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto mix = mixSmoother.getNextValue();
                    const auto outputGain = outputGainSmoother.getNextValue();

                    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    {
                        auto* channelData = buffer.getWritePointer (channel);
                        const auto drySample = dryBuffer.getSample (channel, i);
                        const auto wetSample = wetBuffer.getSample (channel, i);
                        channelData[i] = (drySample * (SampleType { 1.0 } - mix) + wetSample * mix) * outputGain;
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
                driveSmoother.reset (Utils::DSPUtils::dbToGain (static_cast<SampleType> (12.0)));
                biasSmoother.reset (static_cast<SampleType> (0.0));
                asymmetrySmoother.reset (static_cast<SampleType> (0.0));

                preEQ.reset();
                postEQ.reset();

                for (auto& g : gate)
                    g.reset();

                feedbackProc.reset();

                for (int stage = 0; stage < 3; ++stage)
                    for (int ch = 0; ch < 2; ++ch)
                        dcBlocker[stage][ch].reset();

                for (auto& os : oversampler)
                    if (os)
                        os->reset();
            }

        private:
            void processDistortionCore (juce::dsp::AudioBlock<SampleType>& block)
            {
                const auto numSamples = static_cast<int> (block.getNumSamples());
                const auto numCh = static_cast<int> (block.getNumChannels());

                // Pre-EQ (update already done in updateParameters, coefficients set per-block)
                for (int i = 0; i < numSamples; ++i)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto sample = block.getSample (ch, i);
                        sample = preEQ.processSample (ch, sample);
                        block.setSample (ch, i, sample);
                    }
                }

                // Distortion stages
                for (int stage = 0; stage < currentStages; ++stage)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto drive = driveSmoother.getNextValue();
                        const auto bias = biasSmoother.getNextValue();
                        const auto asym = asymmetrySmoother.getNextValue();

                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            auto sample = block.getSample (ch, i);
                            sample = WaveshaperBank<SampleType>::process (sample, drive, currentWaveshaper, bias, asym);
                            sample = dcBlocker[stage][ch].processSample (sample);
                            block.setSample (ch, i, sample);
                        }
                    }
                }

                // Post-EQ
                for (int i = 0; i < numSamples; ++i)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto sample = block.getSample (ch, i);
                        sample = postEQ.processSample (ch, sample);
                        block.setSample (ch, i, sample);
                    }
                }
            }

            void prepareParameterSmoothers()
            {
                inputGainSmoother.prepare (sampleRate, 1.0);
                outputGainSmoother.prepare (sampleRate, 1.0);
                mixSmoother.prepare (sampleRate, 5.0);
                driveSmoother.prepare (sampleRate, 5.0);
                biasSmoother.prepare (sampleRate, 5.0);
                asymmetrySmoother.prepare (sampleRate, 5.0);
            }

            // Parameter smoothers
            Utils::ParameterSmoother<SampleType> inputGainSmoother;
            Utils::ParameterSmoother<SampleType> outputGainSmoother;
            Utils::ParameterSmoother<SampleType> mixSmoother;
            Utils::ParameterSmoother<SampleType> driveSmoother;
            Utils::ParameterSmoother<SampleType> biasSmoother;
            Utils::ParameterSmoother<SampleType> asymmetrySmoother;

            // DSP components
            ThreeBandEQ<SampleType> preEQ;
            ThreeBandEQ<SampleType> postEQ;
            NoiseGate<SampleType> gate[2];
            FeedbackProcessor<SampleType> feedbackProc;
            juce::dsp::IIR::Filter<SampleType> dcBlocker[3][2]; // [stage][channel]
            std::unique_ptr<juce::dsp::Oversampling<SampleType>> oversampler[3]; // 2x, 4x, 8x

            // Current state
            WaveshaperType currentWaveshaper = WaveshaperType::Tanh;
            int currentStages = 1;
            int currentOversamplingIndex = 1;

            // Buffers
            juce::AudioBuffer<SampleType> wetBuffer;
            juce::AudioBuffer<SampleType> dryBuffer;

            double sampleRate = 44100.0;
            int samplesPerBlock = 512;
            int numChannels = 2;
        };

    } // namespace Core
} // namespace DSP
