#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <memory>

namespace DSP {
namespace Core {

// FFT-accelerated McLeod Pitch Method (MPM) for guitar pitch detection.
// Uses NSDF (Normalized Square Difference Function) with McLeod's key
// peak-picking strategy: select the first peak above k * max_peak.
// O(N log N) via FFT autocorrelation, replacing the O(N²) YIN approach.
template <typename SampleType>
class MPMPitchDetector
{
public:
    MPMPitchDetector() = default;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        rebuildBuffers();
    }

    void updateParameters (float windowMs, float clarityThresh)
    {
        threshold = clarityThresh;

        int desiredWindow = std::max (64, static_cast<int> (sampleRate * windowMs * 0.001));
        // Round up to power of 2 for FFT efficiency
        int newWindow = 1;
        while (newWindow < desiredWindow)
            newWindow <<= 1;

        if (newWindow != windowSize)
        {
            windowSize = newWindow;
            rebuildBuffers();
        }
    }

    void pushSample (SampleType monoSample)
    {
        inputRing[static_cast<size_t> (writePos)] = static_cast<float> (monoSample);
        writePos = (writePos + 1) % ringSize;

        if (--samplesUntilNextHop <= 0)
        {
            samplesUntilNextHop = hopSize;
            analyse();
        }
    }

    SampleType getFrequency() const { return static_cast<SampleType> (detectedFrequency); }
    SampleType getConfidence() const { return static_cast<SampleType> (clarity); }

    // Set hop size directly (call after updateParameters which resets it)
    void setHopSize (int newHop) { hopSize = std::max (1, newHop); }

    // Set initial delay before first analysis (for staggering multiple detectors)
    void setHopOffset (int offset) { samplesUntilNextHop = std::max (0, offset); }

    void reset()
    {
        std::fill (inputRing.begin(), inputRing.end(), 0.0f);
        writePos = 0;
        samplesUntilNextHop = 0;
        detectedFrequency = 0.0f;
        clarity = 0.0f;
    }

private:
    void rebuildBuffers()
    {
        ringSize = windowSize;
        inputRing.assign (static_cast<size_t> (ringSize), 0.0f);

        // FFT size must be >= 2*windowSize for zero-padded autocorrelation
        fftOrder = 0;
        fftSize = 1;
        while (fftSize < windowSize * 2)
        {
            fftSize <<= 1;
            ++fftOrder;
        }

        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftInput.assign (static_cast<size_t> (fftSize), std::complex<float> (0.0f));
        fftOutput.assign (static_cast<size_t> (fftSize), std::complex<float> (0.0f));
        nsdf.assign (static_cast<size_t> (windowSize), 0.0f);

        hopSize = std::max (1, windowSize / 4);
        writePos = 0;
        samplesUntilNextHop = 0;
        detectedFrequency = 0.0f;
        clarity = 0.0f;
    }

    void analyse()
    {
        const int W = windowSize;

        // Copy ring buffer samples into FFT input (zero-padded complex)
        std::fill (fftInput.begin(), fftInput.end(), std::complex<float> (0.0f));
        for (int i = 0; i < W; ++i)
        {
            int idx = (writePos - W + i + ringSize) % ringSize;
            fftInput[static_cast<size_t> (i)] = std::complex<float> (inputRing[static_cast<size_t> (idx)], 0.0f);
        }

        // Forward FFT
        fft->perform (fftInput.data(), fftOutput.data(), false);

        // Power spectrum (|X[k]|²) → store back in fftInput
        for (int k = 0; k < fftSize; ++k)
        {
            float mag2 = std::norm (fftOutput[static_cast<size_t> (k)]);
            fftInput[static_cast<size_t> (k)] = std::complex<float> (mag2, 0.0f);
        }

        // Inverse FFT → autocorrelation in fftOutput
        fft->perform (fftInput.data(), fftOutput.data(), true);

        // r[τ] is the real part of fftOutput[τ], scaled by fftSize (unnormalized IFFT)
        // Compute NSDF: n(τ) = 2*r(τ) / m(τ)
        // where m(0) = 2*Σx[j]², m(τ+1) = m(τ) - x[W-1-τ]² - x[τ]²
        // Energy must be computed from raw input (not FFT) to match the incremental m update
        float energy = 0.0f;
        for (int i = 0; i < W; ++i)
        {
            int idx = (writePos - W + i + ringSize) % ringSize;
            energy += inputRing[static_cast<size_t> (idx)] * inputRing[static_cast<size_t> (idx)];
        }
        float m = 2.0f * energy;

        if (m < 1e-10f)
        {
            clarity = 0.0f;
            return;
        }

        nsdf[0] = 1.0f;

        for (int tau = 1; tau < W; ++tau)
        {
            // Incremental update of m(τ)
            int idx1 = (writePos - tau + ringSize) % ringSize;
            int idx2 = (writePos - W + tau - 1 + ringSize) % ringSize;
            m -= inputRing[static_cast<size_t> (idx1)] * inputRing[static_cast<size_t> (idx1)]
                 + inputRing[static_cast<size_t> (idx2)] * inputRing[static_cast<size_t> (idx2)];

            // JUCE's inverse FFT already normalizes by 1/N, so no extra scaling needed
            if (m > 1e-10f)
                nsdf[static_cast<size_t> (tau)] = 2.0f * fftOutput[static_cast<size_t> (tau)].real() / m;
            else
                nsdf[static_cast<size_t> (tau)] = 0.0f;
        }

        // McLeod peak picking: find peaks in positive NSDF regions
        int minTau = std::max (2, static_cast<int> (sampleRate / 5000.0));
        int maxTau = std::min (W - 1, static_cast<int> (sampleRate / 30.0));

        struct Peak
        {
            int tau;
            float value;
        };
        static constexpr int maxPeaks = 64;
        Peak peaks[maxPeaks];
        int numPeaks = 0;
        float maxPeakVal = 0.0f;

        bool inPositive = false;
        int regionStart = minTau;

        for (int tau = minTau; tau <= maxTau; ++tau)
        {
            if (! inPositive && nsdf[static_cast<size_t> (tau)] > 0.0f)
            {
                inPositive = true;
                regionStart = tau;
            }
            else if (inPositive && (nsdf[static_cast<size_t> (tau)] <= 0.0f || tau == maxTau))
            {
                // Find peak within this positive region
                float bestVal = -1.0f;
                int bestTau = regionStart;
                int end = (nsdf[static_cast<size_t> (tau)] <= 0.0f) ? tau : tau + 1;
                for (int j = regionStart; j < end; ++j)
                {
                    if (nsdf[static_cast<size_t> (j)] > bestVal)
                    {
                        bestVal = nsdf[static_cast<size_t> (j)];
                        bestTau = j;
                    }
                }
                if (numPeaks < maxPeaks && bestVal > 0.0f)
                {
                    peaks[numPeaks++] = { bestTau, bestVal };
                    if (bestVal > maxPeakVal)
                        maxPeakVal = bestVal;
                }
                inPositive = false;
            }
        }

        if (numPeaks == 0 || maxPeakVal < 0.1f)
        {
            clarity = 0.0f;
            return;
        }

        // Select first peak above threshold * maxPeakVal (McLeod's key insight)
        float cutoff = threshold * maxPeakVal;
        int bestTau = -1;
        float bestClarity = 0.0f;

        for (int i = 0; i < numPeaks; ++i)
        {
            if (peaks[i].value >= cutoff)
            {
                bestTau = peaks[i].tau;
                bestClarity = peaks[i].value;
                break;
            }
        }

        if (bestTau < 1)
        {
            clarity = 0.0f;
            return;
        }

        // Parabolic interpolation for sub-sample accuracy
        float betterTau = static_cast<float> (bestTau);
        if (bestTau > 0 && bestTau < W - 1)
        {
            float a = nsdf[static_cast<size_t> (bestTau - 1)];
            float b = nsdf[static_cast<size_t> (bestTau)];
            float c = nsdf[static_cast<size_t> (bestTau + 1)];
            float denom = a - 2.0f * b + c;
            if (std::abs (denom) > 1e-12f)
                betterTau -= (a - c) / (2.0f * denom);
        }

        clarity = bestClarity;
        if (betterTau > 0.0f)
            detectedFrequency = static_cast<float> (sampleRate) / betterTau;
    }

    double sampleRate = 44100.0;
    int windowSize = 1024;
    int ringSize = 1024;
    int fftSize = 2048;
    int fftOrder = 11;
    int hopSize = 256;
    int writePos = 0;
    int samplesUntilNextHop = 0;

    float threshold = 0.9f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> inputRing;
    std::vector<std::complex<float>> fftInput;
    std::vector<std::complex<float>> fftOutput;
    std::vector<float> nsdf;

    float detectedFrequency = 0.0f;
    float clarity = 0.0f;
};

} // namespace Core
} // namespace DSP
