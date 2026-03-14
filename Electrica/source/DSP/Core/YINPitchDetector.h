#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace DSP {
namespace Core {

// YIN autocorrelation pitch detector with median filtering for stability.
// Operates on a mono sum of the input. Outputs detected frequency in Hz.
template <typename SampleType>
class YINPitchDetector
{
public:
    YINPitchDetector() = default;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        rebuildBuffers();
    }

    // Update runtime parameters (safe to call per-block)
    void updateParameters (float windowMs, float yinThreshold)
    {
        threshold = static_cast<SampleType> (yinThreshold);

        int newWindowSize = std::max (64, static_cast<int> (sampleRate * windowMs * 0.001));
        if (newWindowSize != windowSize)
        {
            windowSize = newWindowSize;
            rebuildBuffers();
        }
    }

    void pushSample (SampleType monoSample)
    {
        inputBuffer[static_cast<size_t> (writePos)] = monoSample;
        writePos = (writePos + 1) % windowSize;

        if (--samplesUntilNextHop <= 0)
        {
            samplesUntilNextHop = hopSize;
            analyse();
        }
    }

    SampleType getFrequency() const { return smoothedFrequency; }
    SampleType getConfidence() const { return confidence; }

    void reset()
    {
        std::fill (inputBuffer.begin(), inputBuffer.end(), SampleType (0));
        writePos = 0;
        samplesUntilNextHop = 0;
        detectedFrequency = SampleType (0);
        smoothedFrequency = SampleType (0);
        confidence = SampleType (0);
        for (auto& v : medianBuf)
            v = SampleType (0);
        medianIdx = 0;
    }

private:
    void rebuildBuffers()
    {
        halfWindow = windowSize / 2;
        hopSize = std::max (1, static_cast<int> (sampleRate * 0.0025));

        inputBuffer.resize (static_cast<size_t> (windowSize), SampleType (0));
        yinBuffer.resize (static_cast<size_t> (halfWindow), SampleType (0));

        // Reset state when buffers change size
        writePos = 0;
        samplesUntilNextHop = 0;
        detectedFrequency = SampleType (0);
        smoothedFrequency = SampleType (0);
        confidence = SampleType (0);
        for (auto& v : medianBuf)
            v = SampleType (0);
        medianIdx = 0;
    }

    void analyse()
    {
        // Step 1 & 2: Difference function
        for (int tau = 0; tau < halfWindow; ++tau)
        {
            SampleType sum = SampleType (0);
            for (int j = 0; j < halfWindow; ++j)
            {
                auto idx1 = (writePos - windowSize + j + windowSize) % windowSize;
                auto idx2 = (writePos - windowSize + j + tau + windowSize) % windowSize;
                auto diff = inputBuffer[static_cast<size_t> (idx1)]
                            - inputBuffer[static_cast<size_t> (idx2)];
                sum += diff * diff;
            }
            yinBuffer[static_cast<size_t> (tau)] = sum;
        }

        // Step 3: Cumulative mean normalized difference
        yinBuffer[0] = SampleType (1);
        SampleType runningSum = SampleType (0);
        for (int tau = 1; tau < halfWindow; ++tau)
        {
            runningSum += yinBuffer[static_cast<size_t> (tau)];
            if (runningSum > SampleType (0))
                yinBuffer[static_cast<size_t> (tau)] *= static_cast<SampleType> (tau) / runningSum;
            else
                yinBuffer[static_cast<size_t> (tau)] = SampleType (1);
        }

        // Step 4: Absolute threshold
        int tauEstimate = -1;
        int minTau = std::max (2, static_cast<int> (sampleRate / 5000.0));
        for (int tau = minTau; tau < halfWindow; ++tau)
        {
            if (yinBuffer[static_cast<size_t> (tau)] < threshold)
            {
                while (tau + 1 < halfWindow
                       && yinBuffer[static_cast<size_t> (tau + 1)]
                              < yinBuffer[static_cast<size_t> (tau)])
                    ++tau;

                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate < 0)
        {
            confidence = SampleType (0);
            return;
        }

        // Step 5: Parabolic interpolation
        SampleType betterTau = static_cast<SampleType> (tauEstimate);
        if (tauEstimate > 0 && tauEstimate < halfWindow - 1)
        {
            auto s0 = yinBuffer[static_cast<size_t> (tauEstimate - 1)];
            auto s1 = yinBuffer[static_cast<size_t> (tauEstimate)];
            auto s2 = yinBuffer[static_cast<size_t> (tauEstimate + 1)];
            auto denom = s0 - SampleType (2) * s1 + s2;
            if (std::abs (denom) > SampleType (1e-12))
                betterTau -= (s0 - s2) / (SampleType (2) * denom);
        }

        confidence = SampleType (1) - yinBuffer[static_cast<size_t> (tauEstimate)];

        if (betterTau > SampleType (0))
        {
            detectedFrequency = static_cast<SampleType> (sampleRate) / betterTau;

            // Median filter (5-point) to reject outlier jumps
            medianBuf[medianIdx] = detectedFrequency;
            medianIdx = (medianIdx + 1) % medianSize;

            SampleType sorted[medianSize];
            for (int i = 0; i < medianSize; ++i)
                sorted[i] = medianBuf[i];
            std::sort (sorted, sorted + medianSize);
            smoothedFrequency = sorted[medianSize / 2];
        }
    }

    double sampleRate = 44100.0;
    int windowSize = 882;
    int halfWindow = 441;
    int hopSize = 110;
    int writePos = 0;
    int samplesUntilNextHop = 0;

    SampleType threshold = SampleType (0.2);

    std::vector<SampleType> inputBuffer;
    std::vector<SampleType> yinBuffer;

    SampleType detectedFrequency = SampleType (0);
    SampleType smoothedFrequency = SampleType (0);
    SampleType confidence = SampleType (0);

    // Median filter for stability
    static constexpr int medianSize = 5;
    SampleType medianBuf[medianSize] = {};
    int medianIdx = 0;
};

} // namespace Core
} // namespace DSP
