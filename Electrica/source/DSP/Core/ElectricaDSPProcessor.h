#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Utils/DSPUtils.h"
#include "../Utils/ParameterSmoother.h"
#include "MPMPitchDetector.h"
#include "CycfiQPitchDetector.h"
#include "OnsetDetector.h"
#include "SpectralFluxDetector.h"
#include "NoteTracker.h"
#include "PolyphonicTracker.h"
#include "FFTPeakDetector.h"
#include "SynthVoice.h"
#include <vector>
#include <atomic>

namespace DSP
{
    namespace Core
    {

        struct ElectricaParams
        {
            // ===== COMMON =====
            float inputGainDb = 0.0f;
            float outputGainDb = 0.0f;
            float mixPercent = 50.0f;

            // ===== OSCILLATOR =====
            int oscWave = 0;
            int oscOctave = 0;
            float oscDetune = 0.0f;
            float oscLevelDb = 0.0f;

            // ===== ENVELOPE =====
            float envAttack = 10.0f;
            float envRelease = 200.0f;
            float envSensitivity = -40.0f;

            // ===== FILTER =====
            int filterType = 0;
            float filterFreq = 2000.0f;
            float filterReso = 0.707f;

            // ===== DISTORTION =====
            float distDrive = 0.0f;
            int distType = 0;

            // ===== COMPRESSOR =====
            float compAmount = 0.0f;
            int compSpeed = 1;

            // ===== TRACKING =====
            int inputMode = 0;         // 0 = Guitar, 1 = Vocal
            float glide = 20.0f;
            int tracking = 0;          // 0 = mono, 1 = poly
            float polyPeakGateDb = -40.0f;  // -80 to -10 dB: amplitude gate for FFT peak detection
            int pitchAlgorithm = 0;    // 0 = MPM, 1 = Cycfi Q
            bool snapToNote = false;
            float yinWindowMs = 20.0f;
            float yinThreshold = 0.20f;
            float confidenceGate = 0.30f;

            // ===== MIDI OUTPUT =====
            bool midiOutput = false;

            // Velocity
            bool midiVelOverride = false;
            int midiVelValue = 100;       // 1-127
            int midiVelCurve = 0;         // 0=Linear, 1=Soft, 2=Hard, 3=S-Curve

            // Scale lock
            bool midiScaleLock = false;
            int midiScaleRoot = 0;        // 0=C .. 11=B
            int midiScaleType = 0;        // 0=Chromatic, 1=Major, ...

            // Gate & timing
            float midiGateDb = -60.0f;
            float midiNoteOffDelayMs = 0.0f;  // 0-200ms

            // Channel & routing
            int midiChannel = 1;          // 1-16
            bool midiPolySpread = false;  // poly bands on separate channels

            // Transpose & range
            int midiTranspose = 0;        // -24 to +24 semitones
            int midiNoteMin = 0;          // 0-127
            int midiNoteMax = 127;        // 0-127

            // Articulation
            int midiRetrigger = 0;        // 0=Retrigger, 1=Legato
            bool midiNoteHold = false;    // latch last note

            // Transient-gated retrigger
            float midiTransientSens = 50.0f;   // 0-100%: higher = needs bigger transient
            float midiTransientHoldMs = 50.0f;  // 20-500ms: min time between retriggers

            // Pitch bend
            bool midiPitchBend = false;
            int midiPitchBendRange = 2;   // semitones: 2, 12, or 24

            // CC from envelope
            bool midiCCEnable = false;
            int midiCCNumber = 1;         // 0-127 (default: mod wheel)
            float midiCCSmoothMs = 10.0f; // 1-100ms
        };

        struct MidiEvent
        {
            enum Type { NoteOn, NoteOff, ControlChange, PitchBendMsg };

            Type type = NoteOn;
            int samplePosition = 0;
            int channel = 1;           // 1-16
            int noteNumber = 0;        // note on/off
            float velocity = 0.0f;     // note on (0.0-1.0)
            int ccNumber = 0;          // CC
            int ccValue = 0;           // CC (0-127)
            int pitchBendValue = 8192; // pitch bend (0-16383, center=8192)
        };

        template <typename SampleType>
        class ElectricaDSPProcessor
        {
        public:
            static constexpr int maxVoices = 6;

            ElectricaDSPProcessor() = default;

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
                oscLevelSmoother.setTargetValue(SampleType (1));
                oscLevelSmoother.snapToTargetValue();

                wetBuffer.setSize (numChannels, samplesPerBlock);
                dryBuffer.setSize (numChannels, samplesPerBlock);

                // Mono pitch detection chain
                mpmDetector.prepare (sampleRate, samplesPerBlock);
                cycfiDetector.prepare (sampleRate, samplesPerBlock);
                onsetDetector.prepare (sampleRate);
                midiOnsetDetector.prepare (sampleRate);
                spectralFluxDetector.prepare (sampleRate);
                noteTracker.prepare (sampleRate);

                // Phonation gate release coefficient (~100ms release)
                phonationReleaseCoeff = static_cast<SampleType> (
                    std::exp (-1.0 / (0.1 * sampleRate)));

                // Polyphonic trackers
                polyTracker.prepare (sampleRate, samplesPerBlock);
                fftPeakDetector.prepare (sampleRate, samplesPerBlock);

                // All voices
                for (auto& v : voices)
                    v.prepare (sampleRate);

                reset(inputGainDb, outputGainDb, mixPercent);
            }

            void updateParameters (const ElectricaParams& params)
            {
                inputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.inputGainDb)));
                outputGainSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.outputGainDb)));
                mixSmoother.setTargetValue (Utils::DSPUtils::percentageToNormalized (static_cast<SampleType> (params.mixPercent)));
                oscLevelSmoother.setTargetValue (Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.oscLevelDb)));

                // MIDI params
                midiOutputEnabled     = params.midiOutput;
                midiVelOverrideOn     = params.midiVelOverride;
                midiVelFixed          = std::clamp (params.midiVelValue, 1, 127);
                midiVelCurveType      = params.midiVelCurve;
                midiScaleLockOn       = params.midiScaleLock;
                midiScaleRootNote     = params.midiScaleRoot;
                midiScaleIdx          = params.midiScaleType;
                midiGateLinear        = Utils::DSPUtils::dbToGain (static_cast<SampleType> (params.midiGateDb));
                midiNoteOffDelaySamps = static_cast<int> (sampleRate * params.midiNoteOffDelayMs * 0.001);
                midiChan              = params.midiChannel;
                midiPolySpreadOn      = params.midiPolySpread;
                midiTransposeSemis    = params.midiTranspose;
                midiNoteMinVal        = params.midiNoteMin;
                midiNoteMaxVal        = params.midiNoteMax;
                midiRetriggerMode     = params.midiRetrigger;
                midiNoteHoldOn        = params.midiNoteHold;
                midiPitchBendOn       = params.midiPitchBend;
                midiPitchBendSemis    = params.midiPitchBendRange;
                midiCCOn              = params.midiCCEnable;
                midiCCNum             = params.midiCCNumber;

                // CC envelope smoother coefficient
                if (params.midiCCSmoothMs > 0.0f)
                {
                    auto tau = static_cast<SampleType> (params.midiCCSmoothMs * 0.001);
                    midiCCSmoothCoeff = SampleType (1) - std::exp (SampleType (-1) / (tau * static_cast<SampleType> (sampleRate)));
                }
                else
                    midiCCSmoothCoeff = SampleType (1);

                inputMode = params.inputMode;
                trackingMode = params.tracking;
                pitchAlgorithm = params.pitchAlgorithm;
                snapToNote = params.snapToNote;
                confidenceGate = static_cast<SampleType> (params.confidenceGate);

                float mpmThreshold = 0.8f + params.yinThreshold * 0.35f;
                mpmDetector.updateParameters (params.yinWindowMs, mpmThreshold);
                cycfiDetector.updateParameters (params.yinWindowMs, mpmThreshold);

                // Onset detectors: HFC always configured, spectral flux for vocal mode
                onsetDetector.updateParameters (3.0f + params.confidenceGate * 5.0f);
                float midiOnsetThresh = 2.0f + params.midiTransientSens * 0.1f;
                midiOnsetDetector.updateParameters (midiOnsetThresh, params.midiTransientHoldMs);
                // Spectral flux uses same sensitivity mapping for vocal mode
                float sfThresh = 1.5f + params.confidenceGate * 3.0f;
                spectralFluxDetector.updateParameters (sfThresh,
                    (inputMode == 1) ? 80.0f : 50.0f);  // longer hold for vocals
                float sfMidiThresh = 1.5f + params.midiTransientSens * 0.05f;
                spectralFluxMidiThreshold = sfMidiThresh;
                spectralFluxMidiHoldSamples = std::max (1,
                    static_cast<int> (sampleRate * params.midiTransientHoldMs * 0.001));

                noteTracker.updateParameters (params.confidenceGate, 30.0f, 40.0f,
                                              params.snapToNote, inputMode);
                polyTracker.updateParameters (params.confidenceGate, params.snapToNote);
                fftPeakDetector.updateParameters (params.polyPeakGateDb);

                for (auto& v : voices)
                    v.updateParameters (params.oscWave, params.oscOctave, params.oscDetune,
                                        params.envAttack, params.envRelease, params.envSensitivity,
                                        params.filterType, params.filterFreq, params.filterReso,
                                        params.distDrive, params.distType,
                                        params.compAmount, params.compSpeed,
                                        params.glide);
            }

            void processBlock (juce::AudioBuffer<SampleType>& buffer)
            {
                jassert (buffer.getNumChannels() >= 1);

                const int numSamples = buffer.getNumSamples();
                const int numCh = buffer.getNumChannels();

                midiEvents.clear();

                if (wetBuffer.getNumSamples() != numSamples)
                {
                    wetBuffer.setSize (numChannels, numSamples, false, false, true);
                    dryBuffer.setSize (numChannels, numSamples, false, false, true);
                }

                dryBuffer.makeCopyOf (buffer);

                if (trackingMode == 0)
                    processMonoPath (buffer, numSamples, numCh);
                else
                    processPolyPath (buffer, numSamples, numCh);

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

            const std::vector<MidiEvent>& getMidiEvents() const { return midiEvents; }
            void clearMidiEvents() { midiEvents.clear(); }

            // Readable from GUI thread for MIDI activity display
            std::atomic<int>   midiActivityNote    { -1 };
            std::atomic<int>   midiActivityChannel { 0 };
            std::atomic<float> midiActivityVelocity{ 0.0f };

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
                oscLevelSmoother.reset(SampleType (1));
                oscLevelSmoother.setTargetValue(SampleType (1));
                oscLevelSmoother.snapToTargetValue();

                mpmDetector.reset();
                cycfiDetector.reset();
                onsetDetector.reset();
                midiOnsetDetector.reset();
                spectralFluxDetector.reset();
                noteTracker.reset();
                polyTracker.reset();
                fftPeakDetector.reset();
                smoothedPeriodicity = SampleType (0);
                spectralFluxMidiMean = 0.0f;
                spectralFluxMidiHoldCounter = 0;

                for (auto& v : voices)
                    v.reset();

                midiEvents.clear();
                for (auto& n : currentMidiNote) n = 0;
                for (auto& p : pendingNoteOffNote) p = 0;
                for (auto& p : pendingNoteOffChan) p = 1;
                for (auto& p : pendingNoteOffCount) p = 0;
                for (auto& w : prevPolyActive) w = false;
                smoothedCCLevel = SampleType (0);
                lastCCValue = -1;
                midiActivityNote.store (-1, std::memory_order_relaxed);
                midiActivityChannel.store (0, std::memory_order_relaxed);
                midiActivityVelocity.store (0.0f, std::memory_order_relaxed);
            }

        private:
            // ======================== MONO PATH ========================
            void processMonoPath (juce::AudioBuffer<SampleType>& buffer,
                                  int numSamples, int numCh)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto inputGain = inputGainSmoother.getNextValue();
                    const auto oscLevel = oscLevelSmoother.getNextValue();

                    SampleType monoIn = SampleType (0);
                    for (int ch = 0; ch < numCh; ++ch)
                        monoIn += buffer.getSample (ch, i) * inputGain;
                    monoIn /= static_cast<SampleType> (numCh);

                    // Onset detection: HFC for guitar, spectral flux for vocal
                    bool onset;
                    if (inputMode == 1)
                    {
                        onset = spectralFluxDetector.processSample (monoIn);
                        lastSpectralFluxOnset = onset;  // cache for MIDI onset path
                    }
                    else
                    {
                        onset = onsetDetector.processSample (monoIn);
                        lastSpectralFluxOnset = false;
                    }

                    SampleType detectedFreq, pitchConfidence;
                    if (pitchAlgorithm == 1)
                    {
                        cycfiDetector.pushSample (monoIn);
                        detectedFreq = cycfiDetector.getFrequency();
                        pitchConfidence = cycfiDetector.getConfidence();
                    }
                    else
                    {
                        mpmDetector.pushSample (monoIn);
                        detectedFreq = mpmDetector.getFrequency();
                        pitchConfidence = mpmDetector.getConfidence();
                    }

                    auto tracked = noteTracker.process (detectedFreq, pitchConfidence, onset);

                    if (tracked.noteActive && tracked.frequency > SampleType (0))
                    {
                        voices[0].setTargetFrequency (tracked.frequency);
                        voices[1].setTargetFrequency (tracked.frequency);
                    }

                    if (midiOutputEnabled)
                    {
                        auto inputLevel = std::abs (monoIn);

                        // MIDI onset: mode-dependent
                        bool midiOnset;
                        if (inputMode == 1)
                            midiOnset = computeSpectralFluxMidiOnset (monoIn);
                        else
                            midiOnset = midiOnsetDetector.processSample (monoIn);

                        // Phonation gate (vocal mode): use periodicity to filter noise/breath
                        bool phonationActive = true;
                        if (inputMode == 1)
                        {
                            auto periodicity = (pitchAlgorithm == 1)
                                ? cycfiDetector.getConfidence()
                                : mpmDetector.getConfidence();
                            // Instant attack, slow release (~100ms)
                            if (periodicity > smoothedPeriodicity)
                                smoothedPeriodicity = periodicity;
                            else
                                smoothedPeriodicity *= phonationReleaseCoeff;
                            phonationActive = smoothedPeriodicity > confidenceGate;
                        }

                        bool silenceToSound = tracked.noteActive && phonationActive
                                              && currentMidiNote[0] == 0;
                        tickPendingNoteOffs (i);
                        emitMidiFromTrackedNote (0, tracked.noteActive && phonationActive,
                                                 tracked.frequency,
                                                 inputLevel, i, midiOnset || silenceToSound);
                        emitMidiCC (inputLevel, i);
                    }

                    if (! midiOutputEnabled)
                    {
                        auto inputLevel = std::abs (monoIn);
                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            auto guitar = buffer.getSample (ch, i) * inputGain;
                            auto synthSample = voices[ch < 2 ? ch : 0].processSample (inputLevel) * oscLevel;
                            wetBuffer.setSample (ch, i, guitar + synthSample);
                        }
                    }
                    else
                    {
                        for (int ch = 0; ch < numCh; ++ch)
                            wetBuffer.setSample (ch, i, buffer.getSample (ch, i) * inputGain);
                    }
                }
            }

            // ======================== POLY PATH ========================
            void processPolyPath (juce::AudioBuffer<SampleType>& buffer,
                                  int numSamples, int numCh)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto inputGain = inputGainSmoother.getNextValue();
                    const auto oscLevel = oscLevelSmoother.getNextValue();

                    SampleType monoIn = SampleType (0);
                    for (int ch = 0; ch < numCh; ++ch)
                        monoIn += buffer.getSample (ch, i) * inputGain;
                    monoIn /= static_cast<SampleType> (numCh);

                    // FFT peak detector — updates internally at hop boundaries
                    fftPeakDetector.pushSample (monoIn);
                    const auto& peakResults = fftPeakDetector.getResults();

                    for (int b = 0; b < FFTPeakDetector<SampleType>::maxPeaks; ++b)
                    {
                        if (peakResults[static_cast<size_t> (b)].active
                            && peakResults[static_cast<size_t> (b)].frequency > SampleType (0))
                        {
                            voices[b].setTargetFrequency (peakResults[static_cast<size_t> (b)].frequency);
                        }
                    }

                    auto inputLevel = std::abs (monoIn);

                    if (midiOutputEnabled)
                    {
                        bool midiOnset;
                        if (inputMode == 1)
                            midiOnset = computeSpectralFluxMidiOnset (monoIn);
                        else
                            midiOnset = midiOnsetDetector.processSample (monoIn);

                        tickPendingNoteOffs (i);

                        for (int b = 0; b < FFTPeakDetector<SampleType>::maxPeaks; ++b)
                        {
                            bool bandActive = peakResults[static_cast<size_t> (b)].active;
                            bool bandSilenceToSound = bandActive && ! prevPolyActive[b];
                            prevPolyActive[b] = bandActive;

                            emitMidiFromTrackedNote (b, bandActive,
                                peakResults[static_cast<size_t> (b)].frequency,
                                inputLevel, i, midiOnset || bandSilenceToSound);
                        }

                        emitMidiCC (inputLevel, i);
                    }

                    if (! midiOutputEnabled)
                    {
                        SampleType synthSum = SampleType (0);

                        for (int b = 0; b < FFTPeakDetector<SampleType>::maxPeaks; ++b)
                            synthSum += voices[b].processSample (inputLevel);

                        int activeCount = 0;
                        for (int b = 0; b < FFTPeakDetector<SampleType>::maxPeaks; ++b)
                        {
                            if (peakResults[static_cast<size_t> (b)].active)
                                ++activeCount;
                        }
                        if (activeCount > 1)
                            synthSum /= std::sqrt (static_cast<SampleType> (activeCount));

                        synthSum *= oscLevel;

                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            auto guitar = buffer.getSample (ch, i) * inputGain;
                            wetBuffer.setSample (ch, i, guitar + synthSum);
                        }
                    }
                    else
                    {
                        for (int ch = 0; ch < numCh; ++ch)
                            wetBuffer.setSample (ch, i, buffer.getSample (ch, i) * inputGain);
                    }
                }
            }

            // ======================== SCALE LOCK ========================

            static constexpr bool scales[][12] = {
                { 1,1,1,1,1,1,1,1,1,1,1,1 }, // 0: Chromatic
                { 1,0,1,0,1,1,0,1,0,1,0,1 }, // 1: Major (Ionian)
                { 1,0,1,1,0,1,0,1,1,0,1,0 }, // 2: Natural Minor (Aeolian)
                { 1,0,1,1,0,1,0,1,1,0,0,1 }, // 3: Harmonic Minor
                { 1,0,1,1,0,1,0,1,0,1,0,1 }, // 4: Dorian
                { 1,1,0,1,0,1,0,1,1,0,1,0 }, // 5: Phrygian
                { 1,0,1,0,1,0,1,1,0,1,0,1 }, // 6: Lydian
                { 1,0,1,0,1,1,0,1,0,1,1,0 }, // 7: Mixolydian
                { 1,0,1,0,1,0,1,0,1,0,1,0 }, // 8: Whole Tone
                { 1,0,0,1,0,1,1,1,0,0,1,0 }, // 9: Blues
                { 1,0,1,0,0,1,0,1,0,0,1,0 }, // 10: Pentatonic Major
                { 1,0,0,1,0,1,0,1,0,0,1,0 }, // 11: Pentatonic Minor
            };

            int quantizeToScale (int midiNote) const
            {
                if (! midiScaleLockOn || midiScaleIdx < 0 || midiScaleIdx > 11)
                    return midiNote;

                int pc = ((midiNote % 12) - midiScaleRootNote + 12) % 12;
                if (scales[midiScaleIdx][pc])
                    return midiNote;

                for (int offset = 1; offset <= 6; ++offset)
                {
                    int below = ((pc - offset) + 12) % 12;
                    int above = (pc + offset) % 12;
                    if (scales[midiScaleIdx][below])
                        return std::clamp (midiNote - offset, 0, 127);
                    if (scales[midiScaleIdx][above])
                        return std::clamp (midiNote + offset, 0, 127);
                }
                return midiNote;
            }

            // ======================== VELOCITY CURVE ========================

            float applyVelocityCurve (float linear) const
            {
                float v = std::clamp (linear, 0.0f, 1.0f);
                switch (midiVelCurveType)
                {
                    case 1: // Soft — more sensitive at low levels
                        v = std::sqrt (v);
                        break;
                    case 2: // Hard — less sensitive at low levels
                        v = v * v;
                        break;
                    case 3: // S-Curve — gentle at extremes
                        v = v * v * (3.0f - 2.0f * v);
                        break;
                    default: // 0: Linear
                        break;
                }
                return std::clamp (v, 1.0f / 127.0f, 1.0f);
            }

            // ======================== MIDI CHANNEL ========================

            int channelForVoice (int voiceIndex) const
            {
                if (midiPolySpreadOn && trackingMode == 1)
                    return std::clamp (voiceIndex + 1, 1, 16);
                return std::clamp (midiChan, 1, 16);
            }

            // ======================== PITCH BEND ========================

            void emitPitchBend (int channel, SampleType exactMidiNote, int samplePos)
            {
                if (! midiPitchBendOn || midiPitchBendSemis <= 0)
                    return;

                float rounded = std::round (static_cast<float> (exactMidiNote));
                float fractional = static_cast<float> (exactMidiNote) - rounded;

                int bendValue = 8192 + static_cast<int> ((fractional / static_cast<float> (midiPitchBendSemis)) * 8192.0f);
                bendValue = std::clamp (bendValue, 0, 16383);

                MidiEvent e;
                e.type = MidiEvent::PitchBendMsg;
                e.samplePosition = samplePos;
                e.channel = channel;
                e.pitchBendValue = bendValue;
                midiEvents.push_back (e);
            }

            // ======================== CC FROM ENVELOPE ========================

            void emitMidiCC (SampleType inputLevel, int samplePos)
            {
                if (! midiCCOn)
                    return;

                // Smooth the input level
                smoothedCCLevel += midiCCSmoothCoeff * (inputLevel - smoothedCCLevel);

                int ccVal = std::clamp (static_cast<int> (static_cast<float> (smoothedCCLevel) * 127.0f * 4.0f), 0, 127);

                if (ccVal != lastCCValue)
                {
                    MidiEvent e;
                    e.type = MidiEvent::ControlChange;
                    e.samplePosition = samplePos;
                    e.channel = std::clamp (midiChan, 1, 16);
                    e.ccNumber = std::clamp (midiCCNum, 0, 127);
                    e.ccValue = ccVal;
                    midiEvents.push_back (e);
                    lastCCValue = ccVal;
                }
            }

            // ======================== NOTE-OFF DELAY ========================

            void tickPendingNoteOffs (int samplePos)
            {
                for (int v = 0; v < maxVoices; ++v)
                {
                    if (pendingNoteOffCount[v] > 0)
                    {
                        --pendingNoteOffCount[v];
                        if (pendingNoteOffCount[v] <= 0)
                        {
                            MidiEvent e;
                            e.type = MidiEvent::NoteOff;
                            e.samplePosition = samplePos;
                            e.channel = pendingNoteOffChan[v];
                            e.noteNumber = pendingNoteOffNote[v];
                            midiEvents.push_back (e);
                            pendingNoteOffNote[v] = 0;
                        }
                    }
                }
            }

            void scheduleNoteOff (int voiceIndex, int note, int channel, int samplePos)
            {
                if (midiNoteOffDelaySamps > 0)
                {
                    pendingNoteOffNote[voiceIndex] = note;
                    pendingNoteOffChan[voiceIndex] = channel;
                    pendingNoteOffCount[voiceIndex] = midiNoteOffDelaySamps;
                }
                else
                {
                    MidiEvent e;
                    e.type = MidiEvent::NoteOff;
                    e.samplePosition = samplePos;
                    e.channel = channel;
                    e.noteNumber = note;
                    midiEvents.push_back (e);
                }
            }

            void cancelPendingNoteOff (int voiceIndex)
            {
                pendingNoteOffCount[voiceIndex] = 0;
                pendingNoteOffNote[voiceIndex] = 0;
            }

            // ======================== PITCH BEND (relative to held note) ========================

            void emitPitchBendRelative (int channel, SampleType exactMidiNote, int heldMidiNote, int samplePos)
            {
                if (! midiPitchBendOn || midiPitchBendSemis <= 0)
                    return;

                float diff = static_cast<float> (exactMidiNote) - static_cast<float> (heldMidiNote);
                float normalizedBend = diff / static_cast<float> (midiPitchBendSemis);
                normalizedBend = std::clamp (normalizedBend, -1.0f, 1.0f);
                int bendValue = 8192 + static_cast<int> (normalizedBend * 8191.0f);
                bendValue = std::clamp (bendValue, 0, 16383);

                MidiEvent e;
                e.type = MidiEvent::PitchBendMsg;
                e.samplePosition = samplePos;
                e.channel = channel;
                e.pitchBendValue = bendValue;
                midiEvents.push_back (e);
            }

            // ======================== CORE MIDI EMISSION ========================

            void emitMidiFromTrackedNote (int voiceIndex, bool active, SampleType freq,
                                          SampleType inputLevel, int samplePos, bool isOnset)
            {
                // Input gate
                if (inputLevel < midiGateLinear)
                    active = false;

                int channel = channelForVoice (voiceIndex);
                float exactMidi = 0.0f;
                int newNote = 0;

                if (active && freq > SampleType (30))
                {
                    exactMidi = static_cast<float> (SampleType (69) + SampleType (12) * std::log2 (freq / SampleType (440)));
                    exactMidi += static_cast<float> (midiTransposeSemis);
                    newNote = std::clamp (static_cast<int> (std::round (exactMidi)), 0, 127);
                    newNote = quantizeToScale (newNote);
                    if (newNote < midiNoteMinVal || newNote > midiNoteMaxVal)
                        newNote = 0;
                }

                // Note hold: keep last note active even when input goes silent
                if (midiNoteHoldOn && newNote == 0 && currentMidiNote[voiceIndex] > 0 && ! active)
                    return;

                int oldNote = currentMidiNote[voiceIndex];

                // Determine action: only retrigger on transient or silence→sound
                bool doNoteOff = false;
                bool doNoteOn = false;

                if (newNote == 0 && oldNote > 0)
                {
                    // Going silent
                    doNoteOff = true;
                }
                else if (newNote > 0 && oldNote == 0)
                {
                    // Silence → sound
                    doNoteOn = true;
                }
                else if (newNote > 0 && isOnset)
                {
                    // Transient detected — retrigger
                    // (In legato mode, same-note onset doesn't retrigger)
                    if (midiRetriggerMode == 1 && newNote == oldNote)
                    {
                        // Legato: no retrigger on same note, just update pitch bend
                        if (midiPitchBendOn)
                            emitPitchBendRelative (channel, static_cast<SampleType> (exactMidi), oldNote, samplePos);
                        return;
                    }
                    doNoteOff = (oldNote > 0);
                    doNoteOn = true;
                }
                else if (newNote > 0 && newNote != oldNote && oldNote > 0)
                {
                    // Pitch changed without transient — use pitch bend if enabled, don't retrigger
                    if (midiPitchBendOn)
                        emitPitchBendRelative (channel, static_cast<SampleType> (exactMidi), oldNote, samplePos);
                    return;
                }
                else if (newNote > 0 && newNote == oldNote)
                {
                    // Same note, no onset — update micro-pitch via pitch bend
                    if (midiPitchBendOn)
                        emitPitchBendRelative (channel, static_cast<SampleType> (exactMidi), oldNote, samplePos);
                    return;
                }
                else
                {
                    return;
                }

                // Execute note-off
                if (doNoteOff)
                {
                    // Cancel any pending delayed note-off for this voice
                    if (pendingNoteOffCount[voiceIndex] > 0)
                    {
                        MidiEvent offEvt;
                        offEvt.type = MidiEvent::NoteOff;
                        offEvt.samplePosition = samplePos;
                        offEvt.channel = pendingNoteOffChan[voiceIndex];
                        offEvt.noteNumber = pendingNoteOffNote[voiceIndex];
                        midiEvents.push_back (offEvt);
                        cancelPendingNoteOff (voiceIndex);
                    }

                    if (oldNote > 0)
                        scheduleNoteOff (voiceIndex, oldNote, channel, samplePos);
                }

                // Execute note-on
                if (doNoteOn)
                {
                    // For same-note retrigger, flush any pending note-off immediately
                    if (doNoteOff && newNote == oldNote && midiNoteOffDelaySamps > 0)
                    {
                        if (pendingNoteOffCount[voiceIndex] > 0)
                        {
                            MidiEvent offEvt;
                            offEvt.type = MidiEvent::NoteOff;
                            offEvt.samplePosition = samplePos;
                            offEvt.channel = pendingNoteOffChan[voiceIndex];
                            offEvt.noteNumber = pendingNoteOffNote[voiceIndex];
                            midiEvents.push_back (offEvt);
                            cancelPendingNoteOff (voiceIndex);
                        }
                    }

                    float vel;
                    if (midiVelOverrideOn)
                        vel = static_cast<float> (midiVelFixed) / 127.0f;
                    else
                        vel = applyVelocityCurve (static_cast<float> (inputLevel) * 4.0f);

                    MidiEvent onEvt;
                    onEvt.type = MidiEvent::NoteOn;
                    onEvt.samplePosition = samplePos;
                    onEvt.channel = channel;
                    onEvt.noteNumber = newNote;
                    onEvt.velocity = vel;
                    midiEvents.push_back (onEvt);

                    // Pitch bend for micro-tonal accuracy (relative to new note)
                    emitPitchBend (channel, static_cast<SampleType> (exactMidi), samplePos);

                    midiActivityNote.store (newNote, std::memory_order_relaxed);
                    midiActivityChannel.store (channel, std::memory_order_relaxed);
                    midiActivityVelocity.store (vel, std::memory_order_relaxed);

                    currentMidiNote[voiceIndex] = newNote;
                }
                else if (doNoteOff)
                {
                    midiActivityNote.store (-1, std::memory_order_relaxed);
                    currentMidiNote[voiceIndex] = 0;
                }
            }

            // ======================== SPECTRAL FLUX MIDI ONSET ========================
            // Shares the spectral flux detector with the NoteTracker onset path,
            // but applies independent threshold/hold for MIDI retrigger decisions.
            bool computeSpectralFluxMidiOnset (SampleType /*monoIn*/)
            {
                // The spectral flux detector was already called this sample in the
                // NoteTracker onset path. We piggyback on its internal flux value
                // via a separate threshold/hold tracker to avoid running a second FFT.
                // Since we can't easily extract the raw flux from the detector,
                // we run the same sample through the detector's processSample —
                // but it was already called. So instead we use a lightweight
                // energy-rise detector tuned for vocal mode.

                // Actually, we use the spectralFluxDetector as a shared computation.
                // The onset was already computed for NoteTracker. For MIDI, we use
                // the midiOnsetDetector (HFC) even in vocal mode as a complementary
                // detector — it catches consonant plosives that spectral flux might
                // batch into a hop boundary. The spectral flux result from the
                // NoteTracker path is also available via the onset flag passed to
                // noteTracker.process().

                // For the MIDI path in vocal mode, we combine:
                // 1. The spectral flux onset (already computed, stored in lastSpectralFluxOnset)
                // 2. Independent hold timing for MIDI

                if (spectralFluxMidiHoldCounter > 0)
                {
                    --spectralFluxMidiHoldCounter;
                    return false;
                }

                if (lastSpectralFluxOnset)
                {
                    spectralFluxMidiHoldCounter = spectralFluxMidiHoldSamples;
                    return true;
                }

                return false;
            }

            // ======================== SMOOTHERS ========================

            void prepareParameterSmoothers()
            {
                inputGainSmoother.prepare (sampleRate, 1.0);
                outputGainSmoother.prepare (sampleRate, 1.0);
                mixSmoother.prepare (sampleRate, 5.0);
                oscLevelSmoother.prepare (sampleRate, 1.0);
            }

            // ======================== MEMBERS ========================

            // Parameter smoothers
            Utils::ParameterSmoother<SampleType> inputGainSmoother;
            Utils::ParameterSmoother<SampleType> outputGainSmoother;
            Utils::ParameterSmoother<SampleType> mixSmoother;
            Utils::ParameterSmoother<SampleType> oscLevelSmoother;

            // Pitch detection
            MPMPitchDetector<SampleType> mpmDetector;
            CycfiQPitchDetector<SampleType> cycfiDetector;
            OnsetDetector<SampleType> onsetDetector;
            OnsetDetector<SampleType> midiOnsetDetector;  // independent onset for MIDI retrigger
            SpectralFluxDetector<SampleType> spectralFluxDetector;  // vocal mode onset
            NoteTracker<SampleType> noteTracker;
            PolyphonicTracker<SampleType> polyTracker;
            FFTPeakDetector<SampleType> fftPeakDetector;

            // Synth voices
            SynthVoice<SampleType> voices[maxVoices];

            int inputMode = 0;       // 0=Guitar, 1=Vocal
            int trackingMode = 0;
            int pitchAlgorithm = 0;
            bool snapToNote = false;
            SampleType confidenceGate = SampleType (0.3);

            // Spectral flux MIDI onset state (vocal mode)
            bool lastSpectralFluxOnset = false;
            float spectralFluxMidiThreshold = 3.0f;
            float spectralFluxMidiMean = 0.0f;
            int spectralFluxMidiHoldSamples = 4410;
            int spectralFluxMidiHoldCounter = 0;

            // Phonation gate (vocal mode)
            SampleType smoothedPeriodicity = SampleType (0);
            SampleType phonationReleaseCoeff = SampleType (0.9999);

            // MIDI output configuration
            bool midiOutputEnabled = false;
            bool midiVelOverrideOn = false;
            int  midiVelFixed = 100;
            int  midiVelCurveType = 0;
            bool midiScaleLockOn = false;
            int  midiScaleRootNote = 0;
            int  midiScaleIdx = 0;
            SampleType midiGateLinear = SampleType (0.001);
            int  midiNoteOffDelaySamps = 0;
            int  midiChan = 1;
            bool midiPolySpreadOn = false;
            int  midiTransposeSemis = 0;
            int  midiNoteMinVal = 0;
            int  midiNoteMaxVal = 127;
            int  midiRetriggerMode = 0;
            bool midiNoteHoldOn = false;
            bool midiPitchBendOn = false;
            int  midiPitchBendSemis = 2;
            bool midiCCOn = false;
            int  midiCCNum = 1;
            SampleType midiCCSmoothCoeff = SampleType (0.1);

            // MIDI runtime state
            std::vector<MidiEvent> midiEvents;
            int   currentMidiNote[maxVoices] = {};
            int   pendingNoteOffNote[maxVoices] = {};
            int   pendingNoteOffChan[maxVoices] = {};
            int   pendingNoteOffCount[maxVoices] = {};
            bool  prevPolyActive[maxVoices] = {};
            SampleType smoothedCCLevel = SampleType (0);
            int   lastCCValue = -1;

            // Buffers
            juce::AudioBuffer<SampleType> wetBuffer;
            juce::AudioBuffer<SampleType> dryBuffer;

            double sampleRate = 44100.0;
            int samplesPerBlock = 512;
            int numChannels = 2;
        };

    } // namespace Core
} // namespace DSP
