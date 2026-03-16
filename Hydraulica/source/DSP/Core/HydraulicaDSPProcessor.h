#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "../Utils/DSPUtils.h"
#include "../Utils/ParameterSmoother.h"
#include "GainComputer.h"
#include "EnvelopeFollower.h"
#include "DoomNormalizer.h"

namespace DSP
{
    namespace Core
    {

        struct HydraulicaParams
        {
            float inputGainDb = 0.0f;
            float outputGainDb = 0.0f;
            float mixPercent = 50.0f;
            float doom = 0.0f;
            float threshold = -20.0f;
            float ratio = 4.0f;
            float attack = 5.0f;
            float release = 100.0f;
            float knee = 6.0f;
            float makeup = 0.0f;
            float scHpf = 20.0f;
            bool limiterOn = true;
            float limiterCeiling = -0.3f;
        };

        template <typename SampleType>
        class HydraulicaDSPProcessor
        {
        public:
            HydraulicaDSPProcessor() = default;

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
                scBuffer.setSize (numChannels, samplesPerBlock);

                // Prepare compressor envelope followers (per channel)
                for (auto& env : compEnvelope)
                    env.prepare (sampleRate);

                // Prepare doom normalizers (per channel)
                for (auto& doom : doomNorm)
                    doom.prepare (sampleRate);

                // Prepare sidechain HPF (per channel)
                auto hpfCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighPass (
                    sampleRate, static_cast<SampleType> (20.0));
                for (int ch = 0; ch < 2; ++ch)
                {
                    scHpfFilter[ch].coefficients = hpfCoeffs;
                    scHpfFilter[ch].reset();
                }

                // Prepare limiter
                limiter.prepare (spec);
                limiter.setThreshold (-0.3f);
                limiter.setRelease (50.0f);

                reset(inputGainDb, outputGainDb, mixPercent);
            }

            void updateParameters (const HydraulicaParams& params)
            {
                inputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.inputGainDb)));
                outputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.outputGainDb)));
                mixSmoother.setTargetValue (Utils::DSPUtils::percentageToNormalized (static_cast<SampleType> (params.mixPercent)));

                makeupSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.makeup)));

                gainComputer.updateParameters (params.threshold, params.ratio, params.knee);

                for (auto& env : compEnvelope)
                    env.updateParameters (params.attack, params.release);

                for (auto& doom : doomNorm)
                    doom.updateParameters (params.doom);

                doomAmount = static_cast<SampleType> (params.doom * 0.01f);

                // Update sidechain HPF
                auto hpfCoeffs = juce::dsp::IIR::Coefficients<SampleType>::makeHighPass (
                    sampleRate, static_cast<SampleType> (params.scHpf));
                for (int ch = 0; ch < 2; ++ch)
                    *scHpfFilter[ch].coefficients = *hpfCoeffs;

                limiterEnabled = params.limiterOn;
                limiter.setThreshold (params.limiterCeiling);
            }

            // ===== Debug overlay data (audio thread writes, GUI timer reads) =====
            struct DebugData
            {
                std::atomic<float> gainReductionDbL { 0.0f };
                std::atomic<float> gainReductionDbR { 0.0f };
                std::atomic<float> envelopeLevelL   { 0.0f };
                std::atomic<float> envelopeLevelR   { 0.0f };
                std::atomic<float> doomMix          { 0.0f };
                std::atomic<float> inputLevelL      { 0.0f };
                std::atomic<float> inputLevelR      { 0.0f };
                std::atomic<float> outputLevelL     { 0.0f };
                std::atomic<float> outputLevelR     { 0.0f };
                std::atomic<bool>  limiterActive     { false };

                static constexpr int historySize = 256;
                std::atomic<float> grHistory[historySize] {};
                std::atomic<int>   grHistoryWritePos { 0 };
            };

            DebugData debugData;

            void processBlock (juce::AudioBuffer<SampleType>& buffer)
            {
                jassert (buffer.getNumChannels() >= 1);

                const int numSamples = buffer.getNumSamples();
                const int numCh = buffer.getNumChannels();

                if (wetBuffer.getNumSamples() != numSamples)
                {
                    wetBuffer.setSize (numChannels, numSamples, false, false, true);
                    dryBuffer.setSize (numChannels, numSamples, false, false, true);
                    scBuffer.setSize (numChannels, numSamples, false, false, true);
                }

                dryBuffer.makeCopyOf (buffer);

                // Apply input gain to wet buffer
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto inputGain = inputGainSmoother.getNextValue();
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto sample = buffer.getSample (ch, i) * inputGain;
                        wetBuffer.setSample (ch, i, sample);
                        scBuffer.setSample (ch, i, sample); // copy for sidechain
                    }
                }

                // Sidechain HPF
                for (int i = 0; i < numSamples; ++i)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto scSample = scBuffer.getSample (ch, i);
                        scSample = scHpfFilter[ch].processSample (scSample);
                        scBuffer.setSample (ch, i, scSample);
                    }
                }

                // Compression + DOOM processing (per sample, per channel)
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto makeup = makeupSmoother.getNextValue();

                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        auto wetSample = wetBuffer.getSample (ch, i);
                        auto scSample = scBuffer.getSample (ch, i);

                        // --- Standard compressor path ---
                        // Envelope follower on sidechain
                        auto scAbs = std::abs (scSample);
                        auto envLevel = compEnvelope[ch].process (scAbs);

                        // Convert envelope to dB
                        auto envDb = static_cast<SampleType> (
                            20.0 * std::log10 (std::max (static_cast<double> (envLevel), 1e-10)));

                        // Gain computer: how much gain reduction
                        auto gainReductionDb = gainComputer.computeGainReduction (envDb);

                        // Apply gain reduction
                        auto gainReduction = static_cast<SampleType> (
                            std::pow (10.0, static_cast<double> (gainReductionDb) * 0.05));
                        auto compressed = wetSample * gainReduction;

                        // --- DOOM path ---
                        auto doomSignal = doomNorm[ch].process (wetSample);

                        // Crossfade between compressed and DOOM-normalized
                        auto result = compressed * (static_cast<SampleType> (1.0) - doomAmount)
                                      + doomSignal * doomAmount;

                        // Apply makeup gain
                        result *= makeup;

                        wetBuffer.setSample (ch, i, result);

                        // Debug data (write at end of last sample in block for ch 0/1)
                        if (i == numSamples - 1)
                        {
                            if (ch == 0)
                            {
                                debugData.gainReductionDbL.store (static_cast<float> (gainReductionDb), std::memory_order_relaxed);
                                debugData.envelopeLevelL.store (static_cast<float> (envLevel), std::memory_order_relaxed);
                                debugData.inputLevelL.store (static_cast<float> (std::abs (wetBuffer.getSample (0, 0))), std::memory_order_relaxed);
                            }
                            else
                            {
                                debugData.gainReductionDbR.store (static_cast<float> (gainReductionDb), std::memory_order_relaxed);
                                debugData.envelopeLevelR.store (static_cast<float> (envLevel), std::memory_order_relaxed);
                            }
                            debugData.doomMix.store (static_cast<float> (doomAmount), std::memory_order_relaxed);
                        }
                    }
                }

                // GR history for scrolling graph
                {
                    float grAvg = debugData.gainReductionDbL.load (std::memory_order_relaxed);
                    int wp = debugData.grHistoryWritePos.load (std::memory_order_relaxed);
                    debugData.grHistory[wp].store (grAvg, std::memory_order_relaxed);
                    debugData.grHistoryWritePos.store ((wp + 1) % DebugData::historySize, std::memory_order_relaxed);
                }

                debugData.limiterActive.store (limiterEnabled, std::memory_order_relaxed);

                // Brick-wall limiter
                if (limiterEnabled)
                {
                    juce::dsp::AudioBlock<SampleType> wetBlock (wetBuffer);
                    juce::dsp::ProcessContextReplacing<SampleType> context (wetBlock);
                    limiter.process (context);
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
                makeupSmoother.reset (static_cast<SampleType> (1.0));

                for (auto& env : compEnvelope)
                    env.reset();
                for (auto& doom : doomNorm)
                    doom.reset();
                for (auto& f : scHpfFilter)
                    f.reset();

                limiter.reset();
            }

        private:
            void prepareParameterSmoothers()
            {
                inputGainSmoother.prepare (sampleRate, 1.0);
                outputGainSmoother.prepare (sampleRate, 1.0);
                mixSmoother.prepare (sampleRate, 5.0);
                makeupSmoother.prepare (sampleRate, 5.0);
            }

            // Parameter smoothers
            Utils::ParameterSmoother<SampleType> inputGainSmoother;
            Utils::ParameterSmoother<SampleType> outputGainSmoother;
            Utils::ParameterSmoother<SampleType> mixSmoother;
            Utils::ParameterSmoother<SampleType> makeupSmoother;

            // DSP components
            GainComputer<SampleType> gainComputer;
            EnvelopeFollower<SampleType> compEnvelope[2];
            DoomNormalizer<SampleType> doomNorm[2];
            juce::dsp::IIR::Filter<SampleType> scHpfFilter[2];
            juce::dsp::Limiter<SampleType> limiter;

            // State
            SampleType doomAmount = static_cast<SampleType> (0.0);
            bool limiterEnabled = true;

            // Buffers
            juce::AudioBuffer<SampleType> wetBuffer;
            juce::AudioBuffer<SampleType> dryBuffer;
            juce::AudioBuffer<SampleType> scBuffer;

            double sampleRate = 44100.0;
            int samplesPerBlock = 512;
            int numChannels = 2;
        };

    } // namespace Core
} // namespace DSP
