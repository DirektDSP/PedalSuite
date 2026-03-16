#pragma once

#include <cmath>
#include <algorithm>

namespace DSP {
namespace Core {

// Note locking state machine with adaptive hysteresis for stable pitch tracking.
// Uses a 4-state model: Silence → Onset → Sustain → Release.
// Hysteresis adapts to detected vibrato width — wider vibrato (vocals) gets
// wider deadzone to prevent note flickering. Guitar mode: 50-cent base,
// Vocal mode: 100-cent base + adaptive widening from pitch variance.
template <typename SampleType>
class NoteTracker
{
public:
    struct Result
    {
        SampleType frequency = SampleType (0);
        SampleType confidence = SampleType (0);
        bool noteActive = false;
        bool isOnset = false;
    };

    NoteTracker() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void updateParameters (float confGate, float holdMs, float minDurationMs, bool snap, int mode = 0)
    {
        confidenceGate = static_cast<SampleType> (confGate);
        holdSamples = static_cast<int> (sampleRate * holdMs * 0.001);
        minNoteSamples = static_cast<int> (sampleRate * minDurationMs * 0.001);
        snapToNote = snap;
        inputMode = mode;
        baseHysteresisCents = (mode == 1) ? SampleType (100) : SampleType (50);
    }

    Result process (SampleType detectedFreq, SampleType confidence, bool onsetDetected)
    {
        Result result;
        result.isOnset = false;

        bool validPitch = confidence > confidenceGate && detectedFreq > SampleType (30);

        // Track pitch variance for adaptive hysteresis (vocal vibrato detection)
        if (validPitch)
        {
            auto midi = freqToMidi (detectedFreq);
            auto delta = midi - smoothedMidi;
            smoothedMidi += SampleType (0.05) * delta;
            // Exponential moving variance of pitch in MIDI note units
            pitchVariance = SampleType (0.97) * pitchVariance + SampleType (0.03) * delta * delta;
        }

        // Effective hysteresis: base + adaptive component from pitch variance
        // sqrt(variance) gives standard deviation in semitones, scale to cents
        // Cap at 300 cents (3 semitones) to prevent runaway
        auto adaptiveComponent = std::min (SampleType (300),
                                           std::sqrt (pitchVariance) * SampleType (150));
        auto effectiveHysteresis = baseHysteresisCents + adaptiveComponent;

        switch (state)
        {
            case State::Silence:
                if (onsetDetected || validPitch)
                {
                    state = State::Onset;
                    result.isOnset = true;
                    onsetWaitCounter = 0;
                    if (validPitch)
                    {
                        lockNote (detectedFreq);
                        state = State::Sustain;
                        noteCounter = 0;
                        // Initialize smoothed pitch at locked note
                        smoothedMidi = lockedMidiNote;
                        pitchVariance = SampleType (0);
                    }
                }
                break;

            case State::Onset:
                if (validPitch)
                {
                    lockNote (detectedFreq);
                    state = State::Sustain;
                    noteCounter = 0;
                    smoothedMidi = lockedMidiNote;
                    pitchVariance = SampleType (0);
                }
                else
                {
                    ++onsetWaitCounter;
                    if (onsetWaitCounter > static_cast<int> (sampleRate * 0.050))
                    {
                        state = State::Silence;
                        onsetWaitCounter = 0;
                    }
                }
                break;

            case State::Sustain:
                ++noteCounter;
                if (validPitch)
                {
                    auto newMidi = freqToMidi (detectedFreq);
                    auto diff = std::abs (newMidi - lockedMidiNote);

                    // Two thresholds: onset-assisted relock uses normal
                    // hysteresis; non-onset relock requires 2x the threshold
                    // to prevent noisy pitch detection from causing jumps.
                    auto onsetThreshold = effectiveHysteresis / SampleType (100);
                    auto driftThreshold = effectiveHysteresis / SampleType (50);

                    if (noteCounter >= minNoteSamples)
                    {
                        if (onsetDetected && diff > onsetThreshold)
                        {
                            result.isOnset = true;
                            lockNote (detectedFreq);
                            noteCounter = 0;
                        }
                        else if (! onsetDetected && diff > driftThreshold)
                        {
                            lockNote (detectedFreq);
                            noteCounter = 0;
                        }
                    }
                    holdCounter = holdSamples;
                }
                else
                {
                    if (--holdCounter <= 0)
                    {
                        state = State::Release;
                        releaseCounter = static_cast<int> (sampleRate * 0.030);
                    }
                }
                break;

            case State::Release:
                if (validPitch)
                {
                    lockNote (detectedFreq);
                    state = State::Sustain;
                    noteCounter = 0;
                    result.isOnset = true;
                }
                else if (--releaseCounter <= 0)
                {
                    state = State::Silence;
                    lockedFrequency = SampleType (0);
                }
                break;
        }

        result.frequency = lockedFrequency;
        result.confidence = (state == State::Sustain || state == State::Release) ? SampleType (1) : SampleType (0);
        result.noteActive = (state != State::Silence);

        return result;
    }

    void reset()
    {
        state = State::Silence;
        lockedMidiNote = SampleType (0);
        lockedFrequency = SampleType (0);
        holdCounter = 0;
        noteCounter = 0;
        releaseCounter = 0;
        onsetWaitCounter = 0;
        smoothedMidi = SampleType (60);
        pitchVariance = SampleType (0);
    }

private:
    enum class State
    {
        Silence,
        Onset,
        Sustain,
        Release
    };

    void lockNote (SampleType freq)
    {
        lockedMidiNote = freqToMidi (freq);
        if (snapToNote)
            lockedFrequency = midiToFreq (std::round (lockedMidiNote));
        else
            lockedFrequency = freq;
        holdCounter = holdSamples;
    }

    static SampleType freqToMidi (SampleType freq)
    {
        return SampleType (69) + SampleType (12) * std::log2 (freq / SampleType (440));
    }

    static SampleType midiToFreq (SampleType midi)
    {
        return SampleType (440) * std::pow (SampleType (2), (midi - SampleType (69)) / SampleType (12));
    }

    double sampleRate = 44100.0;
    State state = State::Silence;

    SampleType lockedMidiNote = SampleType (0);
    SampleType lockedFrequency = SampleType (0);
    SampleType confidenceGate = SampleType (0.3);

    int holdSamples = 1323;
    int holdCounter = 0;
    int minNoteSamples = 1764;
    int noteCounter = 0;
    int releaseCounter = 0;
    int onsetWaitCounter = 0;
    bool snapToNote = false;

    // Adaptive hysteresis
    int inputMode = 0; // 0=Guitar, 1=Vocal
    SampleType baseHysteresisCents = SampleType (50);
    SampleType smoothedMidi = SampleType (60);
    SampleType pitchVariance = SampleType (0);
};

} // namespace Core
} // namespace DSP
