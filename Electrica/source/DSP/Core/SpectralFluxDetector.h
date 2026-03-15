#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <memory>

namespace DSP {
namespace Core {

// Spectral flux onset detector for signals with gradual onsets (vocals, bowed strings).
// Computes half-wave rectified spectral flux from consecutive FFT frames.
// Better than HFC for detecting vocal entries and soft note transitions.
// Window: 1024 samples (~23ms at 44.1kHz), Hop: 256 samples (~5.8ms).
template <typename SampleType>
class SpectralFluxDetector
{
public:
    SpectralFluxDetector() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        fftOrder = 10; // 2^10 = 1024
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftSize = fft->getSize(); // 1024
        numBins = fftSize / 2 + 1;

        fftData.resize (static_cast<size_t> (fftSize * 2), 0.0f);
        prevMagnitude.resize (static_cast<size_t> (numBins), 0.0f);
        ringBuffer.resize (static_cast<size_t> (fftSize), 0.0f);

        // Pre-compute Hann window
        hannWindow.resize (static_cast<size_t> (fftSize));
        for (int i = 0; i < fftSize; ++i)
            hannWindow[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                                                             * static_cast<float> (i)
                                                                             / static_cast<float> (fftSize - 1)));

        holdSamples = static_cast<int> (sampleRate * 0.050);
        reset();
    }

    void updateParameters (float thresholdMultiplier, float holdTimeMs = -1.0f)
    {
        onsetThreshold = thresholdMultiplier;
        if (holdTimeMs >= 0.0f)
            holdSamples = std::max (1, static_cast<int> (sampleRate * holdTimeMs * 0.001));
    }

    // Push one sample. Returns true on the sample where an onset is detected.
    bool processSample (SampleType inputSample)
    {
        ringBuffer[static_cast<size_t> (writePos)] = static_cast<float> (inputSample);
        writePos = (writePos + 1) % fftSize;
        ++hopCounter;

        if (holdCounter > 0)
        {
            --holdCounter;
            if (hopCounter >= hopSize)
                hopCounter = 0;
            return false;
        }

        if (hopCounter < hopSize)
            return false;

        hopCounter = 0;

        // Fill FFT input from ring buffer with Hann window
        for (int i = 0; i < fftSize; ++i)
        {
            int idx = (writePos + i) % fftSize;
            fftData[static_cast<size_t> (i)] = ringBuffer[static_cast<size_t> (idx)]
                                                * hannWindow[static_cast<size_t> (i)];
        }
        // Zero the second half (workspace for FFT)
        for (int i = fftSize; i < fftSize * 2; ++i)
            fftData[static_cast<size_t> (i)] = 0.0f;

        fft->performRealOnlyForwardTransform (fftData.data(), true);

        // Compute magnitude spectrum and half-wave rectified spectral flux
        float flux = 0.0f;
        for (int k = 0; k < numBins; ++k)
        {
            float re = fftData[static_cast<size_t> (k * 2)];
            float im = fftData[static_cast<size_t> (k * 2 + 1)];
            float mag = std::sqrt (re * re + im * im);

            // Half-wave rectification: only count increases (onset = new energy appearing)
            float diff = mag - prevMagnitude[static_cast<size_t> (k)];
            if (diff > 0.0f)
                flux += diff;

            prevMagnitude[static_cast<size_t> (k)] = mag;
        }

        // Adaptive threshold: running mean of flux
        meanFlux = 0.95f * meanFlux + 0.05f * flux;

        // Onset: flux significantly exceeds running mean
        if (meanFlux > 1e-10f && flux > onsetThreshold * meanFlux)
        {
            holdCounter = holdSamples;
            return true;
        }

        return false;
    }

    void reset()
    {
        std::fill (ringBuffer.begin(), ringBuffer.end(), 0.0f);
        std::fill (prevMagnitude.begin(), prevMagnitude.end(), 0.0f);
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        writePos = 0;
        hopCounter = 0;
        holdCounter = 0;
        meanFlux = 0.0f;
    }

private:
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> prevMagnitude;
    std::vector<float> hannWindow;
    std::vector<float> ringBuffer;

    double sampleRate = 44100.0;
    int fftOrder = 10;
    int fftSize = 1024;
    int numBins = 513;
    static constexpr int hopSize = 256;

    int writePos = 0;
    int hopCounter = 0;

    float onsetThreshold = 3.0f;
    float meanFlux = 0.0f;

    int holdSamples = 2205;
    int holdCounter = 0;
};

} // namespace Core
} // namespace DSP
