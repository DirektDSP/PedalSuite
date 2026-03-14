#pragma once

#include <juce_dsp/juce_dsp.h>
#include "MPMPitchDetector.h"
#include "NoteTracker.h"
#include <array>

namespace DSP {
namespace Core {

// Polyphonic pitch tracker using bandpass-per-string approach.
// Splits the input into 6 frequency bands (one per guitar string in standard tuning),
// runs an independent MPM detector on each band, and resolves harmonic conflicts
// between adjacent bands. Returns up to 6 active pitch detections.
template <typename SampleType>
class PolyphonicTracker
{
public:
    static constexpr int numBands = 6;

    struct BandResult
    {
        SampleType frequency = SampleType (0);
        SampleType confidence = SampleType (0);
        bool active = false;
    };

    PolyphonicTracker() = default;

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;

        // 4x decimation: pitch detection runs at ~11kHz (plenty for guitar fundamentals up to ~1.4kHz)
        decimationFactor = 4;
        decimatedRate = sampleRate / decimationFactor;
        decimationCounter = 0;

        // Band configurations for standard guitar tuning
        // Wide Q values to preserve harmonics for autocorrelation-based detection
        struct BandConfig
        {
            float centerHz;
            float qFactor;
            float windowMs;
        };
        const BandConfig configs[numBands] = {
            { 100.0f, 0.5f, 40.0f },  // E2 band: ~55-180Hz
            { 155.0f, 0.5f, 25.0f },  // A2 band: ~85-280Hz
            { 220.0f, 0.5f, 20.0f },  // D3 band: ~120-400Hz
            { 310.0f, 0.5f, 15.0f },  // G3 band: ~170-560Hz
            { 440.0f, 0.5f, 12.0f },  // B3 band: ~240-800Hz
            { 700.0f, 0.4f, 8.0f },   // E4+ band: ~350-1400Hz
        };

        for (int b = 0; b < numBands; ++b)
        {
            auto freq = static_cast<SampleType> (configs[b].centerHz);
            auto q = static_cast<SampleType> (configs[b].qFactor);

            // Single-stage bandpass filter — wider passband preserves harmonics
            // needed for good autocorrelation in MPM
            auto coeffs = juce::dsp::IIR::Coefficients<SampleType>::makeBandPass (
                sampleRate, static_cast<double> (freq), static_cast<double> (q));
            bandFilter1[b].coefficients = coeffs;
            bandFilter1[b].reset();

            // MPM detectors run at decimated rate
            detector[b].prepare (decimatedRate, maxBlockSize / decimationFactor + 1);
            detector[b].updateParameters (configs[b].windowMs, 0.80f);

            // 50% overlap at the decimated rate
            int windowSamples = 1;
            int desiredWindow = std::max (64, static_cast<int> (decimatedRate * configs[b].windowMs * 0.001));
            while (windowSamples < desiredWindow)
                windowSamples <<= 1;
            detector[b].setHopSize (windowSamples / 2);

            tracker[b].prepare (decimatedRate);
            tracker[b].updateParameters (0.5f, 30.0f, 40.0f, false);
        }

        // Stagger analysis timing so at most 1 detector fires per decimated sample
        {
            int totalOffset = 0;
            for (int b = 0; b < numBands; ++b)
            {
                detector[b].setHopOffset (totalOffset);
                totalOffset += 19 + b * 11; // smaller offsets since decimated rate is lower
            }
        }

        // Initialize cached results
        for (auto& r : cachedResults)
        {
            r.frequency = SampleType (0);
            r.confidence = SampleType (0);
            r.active = false;
        }

        // Expected frequency ranges per string (open tuning through ~fret 24)
        bandMinFreq[0] = SampleType (55);
        bandMaxFreq[0] = SampleType (200);
        bandMinFreq[1] = SampleType (85);
        bandMaxFreq[1] = SampleType (300);
        bandMinFreq[2] = SampleType (120);
        bandMaxFreq[2] = SampleType (420);
        bandMinFreq[3] = SampleType (165);
        bandMaxFreq[3] = SampleType (600);
        bandMinFreq[4] = SampleType (230);
        bandMaxFreq[4] = SampleType (850);
        bandMinFreq[5] = SampleType (320);
        bandMaxFreq[5] = SampleType (1500);
    }

    void updateParameters (float confidenceGate, bool snapToNote)
    {
        for (int b = 0; b < numBands; ++b)
            tracker[b].updateParameters (confidenceGate, 30.0f, 40.0f, snapToNote);
    }

    void pushSample (SampleType monoSample)
    {
        // Bandpass filter at full rate (also anti-aliases before decimation)
        for (int b = 0; b < numBands; ++b)
        {
            filteredAccum[b] += bandFilter1[b].processSample (monoSample);
        }

        // Only feed detectors every Nth sample (4x decimation)
        if (++decimationCounter >= decimationFactor)
        {
            decimationCounter = 0;

            for (int b = 0; b < numBands; ++b)
            {
                // Average the accumulated samples for this decimation period
                detector[b].pushSample (filteredAccum[b] / static_cast<SampleType> (decimationFactor));
                filteredAccum[b] = SampleType (0);
            }

            // Update cached results only when new data was fed to detectors
            updateCachedResults();
        }
    }

    const std::array<BandResult, numBands>& getResults() const
    {
        return cachedResults;
    }

    void reset()
    {
        for (int b = 0; b < numBands; ++b)
        {
            bandFilter1[b].reset();
            filteredAccum[b] = SampleType (0);
            detector[b].reset();
            tracker[b].reset();
        }
        decimationCounter = 0;

        for (auto& r : cachedResults)
        {
            r.frequency = SampleType (0);
            r.confidence = SampleType (0);
            r.active = false;
        }
    }

private:
    void updateCachedResults()
    {
        for (int b = 0; b < numBands; ++b)
        {
            auto freq = detector[b].getFrequency();
            auto conf = detector[b].getConfidence();

            if (freq < bandMinFreq[b] || freq > bandMaxFreq[b])
                conf = SampleType (0);

            auto tracked = tracker[b].process (freq, conf, false);

            cachedResults[static_cast<size_t> (b)].frequency = tracked.frequency;
            cachedResults[static_cast<size_t> (b)].confidence = tracked.confidence;
            cachedResults[static_cast<size_t> (b)].active = tracked.noteActive;
        }

        // Conflict resolution between adjacent bands
        for (int b = 0; b < numBands - 1; ++b)
        {
            if (! cachedResults[static_cast<size_t> (b)].active
                || ! cachedResults[static_cast<size_t> (b + 1)].active)
                continue;

            auto f1 = cachedResults[static_cast<size_t> (b)].frequency;
            auto f2 = cachedResults[static_cast<size_t> (b + 1)].frequency;

            if (f1 <= SampleType (0) || f2 <= SampleType (0))
                continue;

            auto ratio = f1 > f2 ? f1 / f2 : f2 / f1;
            if (ratio < SampleType (1.06))
            {
                if (cachedResults[static_cast<size_t> (b)].confidence
                    >= cachedResults[static_cast<size_t> (b + 1)].confidence)
                {
                    cachedResults[static_cast<size_t> (b + 1)].active = false;
                    cachedResults[static_cast<size_t> (b + 1)].frequency = SampleType (0);
                }
                else
                {
                    cachedResults[static_cast<size_t> (b)].active = false;
                    cachedResults[static_cast<size_t> (b)].frequency = SampleType (0);
                }
            }

            auto octaveRatio = f2 / f1;
            if (octaveRatio > SampleType (1.9) && octaveRatio < SampleType (2.1))
            {
                cachedResults[static_cast<size_t> (b + 1)].active = false;
                cachedResults[static_cast<size_t> (b + 1)].frequency = SampleType (0);
            }
        }
    }

    double sampleRate = 44100.0;
    double decimatedRate = 11025.0;
    int decimationFactor = 4;
    int decimationCounter = 0;

    juce::dsp::IIR::Filter<SampleType> bandFilter1[numBands];
    SampleType filteredAccum[numBands] = {};
    MPMPitchDetector<SampleType> detector[numBands];
    NoteTracker<SampleType> tracker[numBands];

    SampleType bandMinFreq[numBands] = {};
    SampleType bandMaxFreq[numBands] = {};

    std::array<BandResult, numBands> cachedResults;
};

} // namespace Core
} // namespace DSP
