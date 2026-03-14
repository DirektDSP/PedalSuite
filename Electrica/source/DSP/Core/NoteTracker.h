#pragma once

#include <cmath>

namespace DSP {
namespace Core {

// Note locking state machine with hysteresis for stable pitch tracking.
// Uses a 4-state model: Silence → Onset → Sustain → Release.
// 50-cent hysteresis prevents vibrato from causing note flickering.
// Minimum note duration and hold time prevent spurious note changes.
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

    void updateParameters (float confGate, float holdMs, float minDurationMs, bool snap)
    {
        confidenceGate = static_cast<SampleType> (confGate);
        holdSamples = static_cast<int> (sampleRate * holdMs * 0.001);
        minNoteSamples = static_cast<int> (sampleRate * minDurationMs * 0.001);
        snapToNote = snap;
    }

    Result process (SampleType detectedFreq, SampleType confidence, bool onsetDetected)
    {
        Result result;
        result.isOnset = false;

        bool validPitch = confidence > confidenceGate && detectedFreq > SampleType (30);

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
                    }
                }
                break;

            case State::Onset:
                if (validPitch)
                {
                    lockNote (detectedFreq);
                    state = State::Sustain;
                    noteCounter = 0;
                }
                else
                {
                    ++onsetWaitCounter;
                    // Timeout: if no pitch detected within 50ms of onset, return to silence
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
                    // Check if pitch changed beyond hysteresis threshold
                    auto newMidi = freqToMidi (detectedFreq);
                    auto diff = std::abs (newMidi - lockedMidiNote);

                    if (diff > hysteresisCents / SampleType (100))
                    {
                        // Only allow note change after minimum duration
                        if (noteCounter >= minNoteSamples)
                        {
                            if (onsetDetected)
                                result.isOnset = true;
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

    static constexpr SampleType hysteresisCents = SampleType (50);
};

} // namespace Core
} // namespace DSP
