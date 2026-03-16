#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>

namespace DSP {
namespace Core {

// FFT-based polyphonic peak detector.
// Finds spectral peaks, suppresses harmonics, and maps to note frequencies.
// Works well for instruments where the fundamental is the loudest partial
// (guitar, voice, piano). Not true polyphonic pitch detection, but catches
// F0 peaks reliably with low CPU cost.
//
// 4096-point FFT (~93ms window at 44.1kHz, ~11Hz bin resolution)
// 1024-sample hop (~23ms update rate)
// Parabolic interpolation for sub-bin frequency accuracy
// Harmonic suppression: loudest peaks accepted first, their harmonics rejected
template <typename SampleType>
class FFTPeakDetector
{
public:
    static constexpr int maxPeaks = 6;

    struct PeakResult
    {
        SampleType frequency = SampleType (0);
        SampleType amplitude = SampleType (0);
        bool active = false;
    };

    // Threshold in cents: stable peaks only update frequency when the
    // raw peak drifts further than this from the locked value.
    static constexpr float stableHysteresisCents = 80.0f;

    FFTPeakDetector() = default;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;

        fftOrder = 12; // 2^12 = 4096
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftSize = fft->getSize();
        numBins = fftSize / 2 + 1;

        fftData.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        magnitude.resize (static_cast<size_t> (numBins), 0.0f);
        ringBuffer.resize (static_cast<size_t> (fftSize), 0.0f);

        // Pre-compute Hann window
        hannWindow.resize (static_cast<size_t> (fftSize));
        for (int i = 0; i < fftSize; ++i)
            hannWindow[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (
                2.0f * juce::MathConstants<float>::pi
                * static_cast<float> (i) / static_cast<float> (fftSize - 1)));

        // Frequency range limits (bins)
        binResolution = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
        minBin = std::max (1, static_cast<int> (minFreqHz / binResolution));
        maxBin = std::min (numBins - 2, static_cast<int> (maxFreqHz / binResolution));

        reset();
    }

    void updateParameters (float gateDb)
    {
        peakGateLinear = std::pow (10.0f, gateDb / 20.0f);
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
            cachedResults[i].frequency = SampleType (0);
            cachedResults[i].amplitude = SampleType (0);
            cachedResults[i].active = false;
            stablePeaks[i].frequency = 0.0f;
            stablePeaks[i].amplitude = 0.0f;
            stablePeaks[i].holdFrames = 0;
            stablePeaks[i].active = false;
        }
    }

private:
    struct RawPeak
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
    };

    struct StablePeak
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
        int holdFrames = 0;
        bool active = false;
    };

    // How many FFT frames a peak persists after disappearing
    static constexpr int peakHoldFrames = 4;

    static float freqToCents (float f1, float f2)
    {
        if (f1 <= 0.0f || f2 <= 0.0f) return 99999.0f;
        return std::abs (1200.0f * std::log2 (f1 / f2));
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

        // Compute magnitude spectrum
        float maxMag = 0.0f;
        for (int k = 0; k < numBins; ++k)
        {
            float re = fftData[static_cast<size_t> (k * 2)];
            float im = fftData[static_cast<size_t> (k * 2 + 1)];
            magnitude[static_cast<size_t> (k)] = std::sqrt (re * re + im * im);
            if (magnitude[static_cast<size_t> (k)] > maxMag)
                maxMag = magnitude[static_cast<size_t> (k)];
        }

        if (maxMag < 1e-10f)
        {
            // Silence: decay all stable peaks
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (stablePeaks[si].active && --stablePeaks[si].holdFrames <= 0)
                    stablePeaks[si].active = false;
            }
            for (size_t i = 0; i < maxPeaks; ++i)
            {
                cachedResults[i].frequency = static_cast<SampleType> (stablePeaks[i].frequency);
                cachedResults[i].amplitude = SampleType (0);
                cachedResults[i].active = stablePeaks[i].active;
            }
            return;
        }

        // Normalize magnitude
        float invMax = 1.0f / maxMag;
        for (int k = 0; k < numBins; ++k)
            magnitude[static_cast<size_t> (k)] *= invMax;

        // Find local maxima above gate
        rawPeaks.clear();
        for (int k = minBin; k <= maxBin; ++k)
        {
            float m = magnitude[static_cast<size_t> (k)];
            if (m < peakGateLinear)
                continue;

            float mPrev = magnitude[static_cast<size_t> (k - 1)];
            float mNext = magnitude[static_cast<size_t> (k + 1)];

            if (m > mPrev && m > mNext)
            {
                // Parabolic interpolation for sub-bin accuracy
                float denom = mPrev - 2.0f * m + mNext;
                float delta = 0.0f;
                float interpMag = m;

                if (std::abs (denom) > 1e-10f)
                {
                    delta = 0.5f * (mPrev - mNext) / denom;
                    interpMag = m - 0.25f * (mPrev - mNext) * delta;
                }

                float freq = (static_cast<float> (k) + delta) * binResolution;
                rawPeaks.push_back ({ freq, interpMag });
            }
        }

        // Sort by amplitude (loudest first) for harmonic suppression
        std::sort (rawPeaks.begin(), rawPeaks.end(),
                   [] (const RawPeak& a, const RawPeak& b) { return a.amplitude > b.amplitude; });

        // Accept peaks, suppressing harmonics of already-accepted fundamentals
        acceptedPeaks.clear();
        for (const auto& peak : rawPeaks)
        {
            if (acceptedPeaks.size() >= maxPeaks)
                break;

            bool isHarmonic = false;
            for (const auto& accepted : acceptedPeaks)
            {
                // Check if this peak is a harmonic of an accepted peak
                // (ratio close to 2, 3, 4, 5, or 6)
                float lower = std::min (peak.frequency, accepted.frequency);
                float upper = std::max (peak.frequency, accepted.frequency);

                if (lower < 1.0f)
                    continue;

                float ratio = upper / lower;
                for (int h = 2; h <= 8; ++h)
                {
                    float hf = static_cast<float> (h);
                    if (ratio > hf * 0.96f && ratio < hf * 1.04f)
                    {
                        isHarmonic = true;
                        break;
                    }
                }
                if (isHarmonic)
                    break;
            }

            if (! isHarmonic)
                acceptedPeaks.push_back (peak);
        }

        // Sort accepted peaks by frequency for stable voice assignment
        std::sort (acceptedPeaks.begin(), acceptedPeaks.end(),
                   [] (const RawPeak& a, const RawPeak& b) { return a.frequency < b.frequency; });

        // ---- Temporal stabilization: match new peaks to stable peaks ----
        std::array<bool, maxPeaks> stableMatched {};
        std::vector<bool> rawMatched (acceptedPeaks.size(), false);

        // Match each new peak to the nearest existing stable peak within hysteresis
        for (size_t ri = 0; ri < acceptedPeaks.size(); ++ri)
        {
            float bestDist = 99999.0f;
            int bestSlot = -1;
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (! stablePeaks[si].active || stableMatched[si])
                    continue;
                float cents = freqToCents (acceptedPeaks[ri].frequency, stablePeaks[si].frequency);
                if (cents < bestDist)
                {
                    bestDist = cents;
                    bestSlot = static_cast<int> (si);
                }
            }
            if (bestSlot >= 0 && bestDist < 600.0f) // within 6 semitones = same peak
            {
                auto bs = static_cast<size_t> (bestSlot);
                stableMatched[bs] = true;
                rawMatched[ri] = true;
                stablePeaks[bs].amplitude = acceptedPeaks[ri].amplitude;
                stablePeaks[bs].holdFrames = peakHoldFrames;
                // Only update frequency if drift exceeds hysteresis
                if (bestDist > stableHysteresisCents)
                    stablePeaks[bs].frequency = acceptedPeaks[ri].frequency;
            }
        }

        // Unmatched stable peaks: decrement hold, deactivate when expired
        for (size_t si = 0; si < maxPeaks; ++si)
        {
            if (stablePeaks[si].active && ! stableMatched[si])
            {
                if (--stablePeaks[si].holdFrames <= 0)
                    stablePeaks[si].active = false;
            }
        }

        // Unmatched raw peaks: assign to empty stable slots
        for (size_t ri = 0; ri < acceptedPeaks.size(); ++ri)
        {
            if (rawMatched[ri])
                continue;
            for (size_t si = 0; si < maxPeaks; ++si)
            {
                if (! stablePeaks[si].active)
                {
                    stablePeaks[si].frequency = acceptedPeaks[ri].frequency;
                    stablePeaks[si].amplitude = acceptedPeaks[ri].amplitude;
                    stablePeaks[si].holdFrames = peakHoldFrames;
                    stablePeaks[si].active = true;
                    break;
                }
            }
        }

        // Sort stable peaks by frequency and output to cached results
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

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> magnitude;
    std::vector<float> hannWindow;
    std::vector<float> ringBuffer;

    // Working buffers for peak detection (reused across frames)
    std::vector<RawPeak> rawPeaks;
    std::vector<RawPeak> acceptedPeaks;

    std::array<PeakResult, maxPeaks> cachedResults;
    std::array<StablePeak, maxPeaks> stablePeaks;

    double sampleRate = 44100.0;
    int fftOrder = 12;
    int fftSize = 4096;
    int numBins = 2049;
    static constexpr int hopSize = 1024;

    float binResolution = 10.77f;
    int minBin = 5;
    int maxBin = 465;
    static constexpr float minFreqHz = 50.0f;
    static constexpr float maxFreqHz = 5000.0f;

    int writePos = 0;
    int hopCounter = 0;

    float peakGateLinear = 0.01f; // -40dB default
};

} // namespace Core
} // namespace DSP
