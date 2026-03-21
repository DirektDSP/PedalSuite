#pragma once

#include <juce_dsp/juce_dsp.h>
#include "GeneratedDictionaries.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>

namespace DSP {
namespace Core {

// Polyphonic pitch detector using sparse dictionary decomposition.
// Inspired by github.com/jaylmiller/polyphonic_track: decomposes the
// incoming FFT magnitude spectrum as a sparse linear combination of
// pre-computed note templates using Matching Pursuit (greedy OMP).
//
// Dictionary data comes from GeneratedDictionaries.h, built from real
// instrument recordings (NSynth guitar/keyboard, Iowa MIS piano,
// VocalSet voice).  Falls back to synthetic harmonic templates when
// no generated dictionary is available.
//
// 4096-point FFT, 1024-sample hop, first 512 bins analysed (~5.5 kHz)
template <typename SampleType>
class SparsePitchDetector
{
public:
    static constexpr int maxPeaks = 6;

    struct PeakResult
    {
        SampleType frequency = SampleType (0);
        SampleType amplitude = SampleType (0);
        bool active = false;
    };

    // Minimum coefficient from matching pursuit to accept a note
    static constexpr float minCoefficient = 0.08f;

    // Temporal hysteresis in cents — stable notes resist small drifts
    static constexpr float stableHysteresisCents = 80.0f;

    SparsePitchDetector() = default;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;

        fftOrder = 12; // 2^12 = 4096
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftSize = fft->getSize();
        numBins = fftSize / 2 + 1;

        // Only analyse bins up to ~5.5 kHz (covers fundamentals + harmonics)
        cropBins = std::min (numBins, static_cast<int> (5500.0 / (sampleRate / fftSize)) + 1);

        fftData.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        magnitude.resize (static_cast<size_t> (numBins), 0.0f);
        ringBuffer.resize (static_cast<size_t> (fftSize), 0.0f);

        // Hann window
        hannWindow.resize (static_cast<size_t> (fftSize));
        for (int i = 0; i < fftSize; ++i)
            hannWindow[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (
                2.0f * juce::MathConstants<float>::pi
                * static_cast<float> (i) / static_cast<float> (fftSize - 1)));

        binResolution = static_cast<float> (sampleRate) / static_cast<float> (fftSize);

        loadDictionary (currentInstrument);
        reset();
    }

    void updateParameters (float gateDb, int instrumentType = -1)
    {
        peakGateLinear = std::pow (10.0f, gateDb / 20.0f);

        // Switch instrument dictionary if changed
        if (instrumentType >= 0 && instrumentType != currentInstrument)
        {
            currentInstrument = instrumentType;
            loadDictionary (currentInstrument);
        }
    }

    void pushSample (SampleType monoSample)
    {
        ringBuffer[static_cast<size_t> (writePos)] = static_cast<float> (monoSample);
        writePos = (writePos + 1) % fftSize;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            analyseFrame();
        }
    }

    const std::array<PeakResult, maxPeaks>& getResults() const
    {
        return cachedResults;
    }

    void reset()
    {
        std::fill (ringBuffer.begin(), ringBuffer.end(), 0.0f);
        std::fill (magnitude.begin(), magnitude.end(), 0.0f);
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        writePos = 0;
        hopCounter = 0;

        for (size_t i = 0; i < maxPeaks; ++i)
        {
            cachedResults[i] = {};
            stablePeaks[i] = {};
        }
    }

private:
    // Dictionary dimensions — must match GeneratedDictionaries.h
    static constexpr int dictMinNote = DictData::dictMinNote;
    static constexpr int dictMaxNote = DictData::dictMaxNote;
    static constexpr int dictSize = DictData::dictSize;
    static constexpr int dictCropBins = DictData::cropBins;

    // Maximum harmonics for synthetic fallback
    static constexpr int maxHarmonics = 12;
    static constexpr float harmonicDecay = 1.2f;

    struct StablePeak
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
        int holdFrames = 0;
        bool active = false;
    };

    static constexpr int peakHoldFrames = 4;

    static float freqToCents (float f1, float f2)
    {
        if (f1 <= 0.0f || f2 <= 0.0f) return 99999.0f;
        return std::abs (1200.0f * std::log2 (f1 / f2));
    }

    static float midiToFreq (int note)
    {
        return 440.0f * std::pow (2.0f, (static_cast<float> (note) - 69.0f) / 12.0f);
    }

    // Load a dictionary from GeneratedDictionaries.h, or build synthetic fallback
    void loadDictionary (int instrumentType)
    {
        // Map instrument index to DictData::Instrument enum:
        //   0 = Guitar, 1 = Keyboard, 2 = Voice, 3+ = Synthetic
        DictData::Instrument inst;
        switch (instrumentType)
        {
            case 0: inst = DictData::Instrument::Guitar;   break;
            case 1: inst = DictData::Instrument::Keyboard; break;
            case 2: inst = DictData::Instrument::Voice;    break;
            default: inst = DictData::Instrument::Synthetic; break;
        }

        auto dictPtr = DictData::getDictionary (inst);

        // The generated dictionaries use dictCropBins (512).
        // Our runtime cropBins may differ if sampleRate != 44100.
        // Use whichever is smaller.
        int usableBins = std::min (cropBins, dictCropBins);

        dictionary.resize (static_cast<size_t> (dictSize));
        dictFreqs.resize (static_cast<size_t> (dictSize));

        for (int n = 0; n < dictSize; ++n)
        {
            int midiNote = dictMinNote + n;
            dictFreqs[static_cast<size_t> (n)] = midiToFreq (midiNote);

            dictionary[static_cast<size_t> (n)].resize (static_cast<size_t> (cropBins), 0.0f);

            if (dictPtr != nullptr)
            {
                // Copy from generated dictionary, zero-pad if cropBins > dictCropBins
                for (int k = 0; k < usableBins; ++k)
                    dictionary[static_cast<size_t> (n)][static_cast<size_t> (k)]
                        = dictPtr[n][k];
                for (int k = usableBins; k < cropBins; ++k)
                    dictionary[static_cast<size_t> (n)][static_cast<size_t> (k)] = 0.0f;
            }
            else
            {
                // Synthetic fallback
                buildSyntheticAtom (midiNote, dictionary[static_cast<size_t> (n)]);
            }
        }
    }

    // Build a single synthetic harmonic template (fallback when no generated data)
    void buildSyntheticAtom (int midiNote, std::vector<float>& spectrum)
    {
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        float f0 = midiToFreq (midiNote);
        float sigma = 1.5f;

        for (int h = 1; h <= maxHarmonics; ++h)
        {
            float hFreq = f0 * static_cast<float> (h);
            if (hFreq >= static_cast<float> (cropBins) * binResolution)
                break;

            float binF = hFreq / binResolution;
            float amp = 1.0f / std::pow (static_cast<float> (h), harmonicDecay);

            int binStart = std::max (0, static_cast<int> (binF - 4.0f * sigma));
            int binEnd = std::min (cropBins - 1, static_cast<int> (binF + 4.0f * sigma));

            for (int k = binStart; k <= binEnd; ++k)
            {
                float d = static_cast<float> (k) - binF;
                spectrum[static_cast<size_t> (k)] += amp * std::exp (-0.5f * (d * d) / (sigma * sigma));
            }
        }

        // L2-normalise
        float norm = 0.0f;
        for (int k = 0; k < cropBins; ++k)
            norm += spectrum[static_cast<size_t> (k)] * spectrum[static_cast<size_t> (k)];
        norm = std::sqrt (norm);
        if (norm > 1e-10f)
        {
            float invNorm = 1.0f / norm;
            for (int k = 0; k < cropBins; ++k)
                spectrum[static_cast<size_t> (k)] *= invNorm;
        }
    }

    // Matching Pursuit: greedily decompose the spectrum using dictionary atoms
    void matchingPursuit (const std::vector<float>& signal,
                          std::array<int, maxPeaks>& outIndices,
                          std::array<float, maxPeaks>& outCoeffs,
                          int& outCount)
    {
        outCount = 0;

        // Work on a copy as residual
        residual.assign (signal.begin(), signal.end());

        std::array<bool, dictSize> used {};

        for (int iter = 0; iter < maxPeaks; ++iter)
        {
            // Find dictionary atom with highest correlation to residual
            float bestCorr = -1.0f;
            int bestIdx = -1;

            for (int d = 0; d < dictSize; ++d)
            {
                if (used[static_cast<size_t> (d)])
                    continue;

                float corr = 0.0f;
                const auto& atom = dictionary[static_cast<size_t> (d)];
                for (int k = 0; k < cropBins; ++k)
                    corr += atom[static_cast<size_t> (k)] * residual[static_cast<size_t> (k)];

                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestIdx = d;
                }
            }

            if (bestIdx < 0 || bestCorr < minCoefficient)
                break;

            // Also suppress notes within 1 semitone of this one
            // (prevents near-duplicate detections)
            used[static_cast<size_t> (bestIdx)] = true;
            if (bestIdx > 0)
                used[static_cast<size_t> (bestIdx - 1)] = true;
            if (bestIdx < dictSize - 1)
                used[static_cast<size_t> (bestIdx + 1)] = true;

            // Subtract this atom's contribution from residual
            const auto& bestAtom = dictionary[static_cast<size_t> (bestIdx)];
            for (int k = 0; k < cropBins; ++k)
                residual[static_cast<size_t> (k)] -= bestCorr * bestAtom[static_cast<size_t> (k)];

            outIndices[static_cast<size_t> (iter)] = bestIdx;
            outCoeffs[static_cast<size_t> (iter)] = bestCorr;
            ++outCount;
        }
    }

    void analyseFrame()
    {
        // Copy ring buffer into FFT input with Hann window
        for (int i = 0; i < fftSize; ++i)
        {
            int idx = (writePos + i) % fftSize;
            fftData[static_cast<size_t> (i)] = ringBuffer[static_cast<size_t> (idx)]
                                                * hannWindow[static_cast<size_t> (i)];
        }
        for (int i = fftSize; i < fftSize * 2; ++i)
            fftData[static_cast<size_t> (i)] = 0.0f;

        fft->performRealOnlyForwardTransform (fftData.data(), true);

        // Compute magnitude spectrum (cropped)
        float maxMag = 0.0f;
        for (int k = 0; k < cropBins; ++k)
        {
            float re = fftData[static_cast<size_t> (k * 2)];
            float im = fftData[static_cast<size_t> (k * 2 + 1)];
            magnitude[static_cast<size_t> (k)] = std::sqrt (re * re + im * im);
            if (magnitude[static_cast<size_t> (k)] > maxMag)
                maxMag = magnitude[static_cast<size_t> (k)];
        }

        if (maxMag < 1e-10f)
        {
            // Silence — decay stable peaks
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (stablePeaks[si].active && --stablePeaks[si].holdFrames <= 0)
                    stablePeaks[si].active = false;
            }
            updateCachedFromStable();
            return;
        }

        // L2-normalise the cropped magnitude spectrum
        float l2norm = 0.0f;
        for (int k = 0; k < cropBins; ++k)
            l2norm += magnitude[static_cast<size_t> (k)] * magnitude[static_cast<size_t> (k)];
        l2norm = std::sqrt (l2norm);

        if (l2norm < 1e-10f)
        {
            updateCachedFromStable();
            return;
        }

        // Normalised signal for matching pursuit
        normSignal.resize (static_cast<size_t> (cropBins));
        float invL2 = 1.0f / l2norm;
        for (int k = 0; k < cropBins; ++k)
            normSignal[static_cast<size_t> (k)] = magnitude[static_cast<size_t> (k)] * invL2;

        // Run matching pursuit
        std::array<int, maxPeaks> mpIndices {};
        std::array<float, maxPeaks> mpCoeffs {};
        int mpCount = 0;
        matchingPursuit (normSignal, mpIndices, mpCoeffs, mpCount);

        // Filter by coefficient threshold and convert to frequency/amplitude
        struct RawDetection
        {
            float frequency;
            float coefficient;
        };
        std::vector<RawDetection> detections;

        for (int i = 0; i < mpCount; ++i)
        {
            if (mpCoeffs[static_cast<size_t> (i)] < minCoefficient)
                continue;

            float freq = dictFreqs[static_cast<size_t> (mpIndices[static_cast<size_t> (i)])];

            // Gate: check if the actual magnitude around the fundamental is above threshold
            int fundBin = std::min (cropBins - 1, static_cast<int> (freq / binResolution));
            float maxNear = 0.0f;
            for (int k = std::max (0, fundBin - 2); k <= std::min (cropBins - 1, fundBin + 2); ++k)
                maxNear = std::max (maxNear, magnitude[static_cast<size_t> (k)] / maxMag);

            if (maxNear < peakGateLinear)
                continue;

            detections.push_back ({ freq, mpCoeffs[static_cast<size_t> (i)] });
        }

        // Sort by frequency for stable voice assignment
        std::sort (detections.begin(), detections.end(),
                   [] (const RawDetection& a, const RawDetection& b) {
                       return a.frequency < b.frequency;
                   });

        // ---- Temporal stabilization (same logic as FFTPeakDetector) ----
        std::array<bool, maxPeaks> stableMatched {};
        std::vector<bool> rawMatched (detections.size(), false);

        for (size_t ri = 0; ri < detections.size(); ++ri)
        {
            float bestDist = 99999.0f;
            int bestSlot = -1;
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (! stablePeaks[si].active || stableMatched[si])
                    continue;
                float cents = freqToCents (detections[ri].frequency, stablePeaks[si].frequency);
                if (cents < bestDist)
                {
                    bestDist = cents;
                    bestSlot = static_cast<int> (si);
                }
            }
            if (bestSlot >= 0 && bestDist < 600.0f)
            {
                auto bs = static_cast<size_t> (bestSlot);
                stableMatched[bs] = true;
                rawMatched[ri] = true;
                stablePeaks[bs].amplitude = detections[ri].coefficient;
                stablePeaks[bs].holdFrames = peakHoldFrames;
                if (bestDist > stableHysteresisCents)
                    stablePeaks[bs].frequency = detections[ri].frequency;
            }
        }

        for (size_t si = 0; si < maxPeaks; ++si)
        {
            if (stablePeaks[si].active && ! stableMatched[si])
            {
                if (--stablePeaks[si].holdFrames <= 0)
                    stablePeaks[si].active = false;
            }
        }

        for (size_t ri = 0; ri < detections.size(); ++ri)
        {
            if (rawMatched[ri])
                continue;
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (! stablePeaks[si].active)
                {
                    stablePeaks[si].frequency = detections[ri].frequency;
                    stablePeaks[si].amplitude = detections[ri].coefficient;
                    stablePeaks[si].holdFrames = peakHoldFrames;
                    stablePeaks[si].active = true;
                    break;
                }
            }
        }

        updateCachedFromStable();
    }

    void updateCachedFromStable()
    {
        std::array<StablePeak, maxPeaks> sorted = stablePeaks;
        std::sort (sorted.begin(), sorted.end(),
                   [] (const StablePeak& a, const StablePeak& b) {
                       if (a.active != b.active) return a.active > b.active;
                       return a.frequency < b.frequency;
                   });

        for (size_t i = 0; i < maxPeaks; ++i)
        {
            cachedResults[i].frequency = static_cast<SampleType> (sorted[i].frequency);
            cachedResults[i].amplitude = static_cast<SampleType> (sorted[i].amplitude);
            cachedResults[i].active = sorted[i].active;
        }
    }

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> magnitude;
    std::vector<float> hannWindow;
    std::vector<float> ringBuffer;

    // Dictionary (loaded from GeneratedDictionaries.h or built synthetically)
    std::vector<std::vector<float>> dictionary;
    std::vector<float> dictFreqs;
    int currentInstrument = 0; // 0=Guitar, 1=Keyboard, 2=Piano, 3=Voice, 4=Synthetic

    // Working buffers (reused across frames)
    std::vector<float> normSignal;
    std::vector<float> residual;

    // Output
    std::array<PeakResult, maxPeaks> cachedResults;
    std::array<StablePeak, maxPeaks> stablePeaks;

    double sampleRate = 44100.0;
    int fftOrder = 12;
    int fftSize = 4096;
    int numBins = 2049;
    int cropBins = 512;
    static constexpr int hopSize = 1024;

    float binResolution = 10.77f;
    int writePos = 0;
    int hopCounter = 0;

    float peakGateLinear = 0.01f;
};

} // namespace Core
} // namespace DSP
