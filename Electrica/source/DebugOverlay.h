#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Core/ElectricaDSPProcessor.h"

class DebugOverlay : public juce::Component, private juce::Timer
{
public:
    using DebugData = DSP::Core::ElectricaDSPProcessor<float>::DebugData;

    DebugOverlay (DebugData& data,
                  std::atomic<int>& midiNote,
                  std::atomic<int>& midiChan,
                  std::atomic<float>& midiVel)
        : debug (data), actNote (midiNote), actChan (midiChan), actVel (midiVel)
    {
        startTimerHz (30);
    }

    ~DebugOverlay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xEE1a1a1a));
        g.fillRoundedRectangle (bounds, 4.0f);

        auto font = juce::Font (juce::FontOptions (13.0f));
        g.setFont (font);

        auto area = getLocalBounds().reduced (10);
        int lineH = 18;

        // ---- Text readouts ----
        auto textArea = area.removeFromTop (area.getHeight() / 2);

        float rawFreq   = debug.rawDetectedFreq.load (std::memory_order_relaxed);
        float conf      = debug.pitchConfidence.load (std::memory_order_relaxed);
        float locked    = debug.lockedFreq.load (std::memory_order_relaxed);
        int   state     = debug.noteTrackerState.load (std::memory_order_relaxed);
        int   onsets    = debug.onsetCounter.load (std::memory_order_relaxed);
        int   noteIn    = debug.inputNotePreQuantize.load (std::memory_order_relaxed);
        int   noteOut   = debug.outputNotePostQuantize.load (std::memory_order_relaxed);
        int   mNote     = actNote.load (std::memory_order_relaxed);
        int   mChan     = actChan.load (std::memory_order_relaxed);
        float mVel      = actVel.load (std::memory_order_relaxed);

        static const char* stateNames[] = { "SILENCE", "ONSET", "SUSTAIN", "RELEASE" };
        auto stateName = (state >= 0 && state <= 3) ? stateNames[state] : "?";

        // Onset flash
        int currentOnsets = onsets;
        if (currentOnsets != lastOnsetCount)
        {
            lastOnsetCount = currentOnsets;
            onsetFlashFrames = 8;
        }
        if (onsetFlashFrames > 0)
            --onsetFlashFrames;

        auto drawLine = [&] (const juce::String& text, juce::Colour col = juce::Colours::lightgrey) {
            g.setColour (col);
            g.drawText (text, textArea.removeFromTop (lineH), juce::Justification::left);
        };

        drawLine ("Raw Freq: " + juce::String (rawFreq, 1) + " Hz   Conf: " + juce::String (conf, 2),
                  juce::Colours::cyan);
        drawLine ("Locked: " + juce::String (locked, 1) + " Hz (" + midiNoteStr (locked) + ")   State: " + stateName,
                  state == 2 ? juce::Colours::limegreen : juce::Colours::orange);
        drawLine ("Onset: " + juce::String (onsets) + (onsetFlashFrames > 0 ? "  ***" : ""),
                  onsetFlashFrames > 0 ? juce::Colours::red : juce::Colours::grey);

        if (mNote >= 0)
            drawLine ("MIDI: " + noteNameFromNumber (mNote) + " (" + juce::String (mNote) + ") Ch=" + juce::String (mChan) + " Vel=" + juce::String (static_cast<int> (mVel * 127.0f)),
                      juce::Colours::yellow);
        else
            drawLine ("MIDI: --", juce::Colours::grey);

        if (noteIn >= 0)
            drawLine ("Scale Lock: " + noteNameFromNumber (noteIn) + " -> " + noteNameFromNumber (noteOut)
                      + (noteIn != noteOut ? "  [quantized]" : ""),
                      noteIn != noteOut ? juce::Colours::magenta : juce::Colours::lightgrey);

        // Poly peaks
        juce::String polyStr = "Peaks: ";
        for (int b = 0; b < 6; ++b)
        {
            bool on = debug.polyPeakActive[b].load (std::memory_order_relaxed);
            float f = debug.polyPeakFreq[b].load (std::memory_order_relaxed);
            if (on && f > 0.0f)
                polyStr += juce::String (f, 0) + " ";
            else
                polyStr += "-- ";
        }
        drawLine (polyStr, juce::Colours::lightblue);

        // ---- Pitch history graph ----
        auto graphArea = area.reduced (0, 4);
        g.setColour (juce::Colour (0xFF222222));
        g.fillRoundedRectangle (graphArea.toFloat(), 3.0f);

        // Y axis: MIDI note 30-90
        constexpr float midiLo = 30.0f;
        constexpr float midiHi = 90.0f;

        // Grid lines at C notes
        g.setColour (juce::Colour (0xFF333333));
        for (int note = 36; note <= 84; note += 12)
        {
            float y = graphArea.getBottom() - (static_cast<float> (note) - midiLo) / (midiHi - midiLo) * static_cast<float> (graphArea.getHeight());
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (graphArea.getX()), static_cast<float> (graphArea.getRight()));
            g.setColour (juce::Colours::grey.withAlpha (0.5f));
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawText (noteNameFromNumber (note), static_cast<int> (graphArea.getX() + 2), static_cast<int> (y) - 8, 30, 16, juce::Justification::left);
            g.setColour (juce::Colour (0xFF333333));
        }

        // Read pitch history
        int wp = debug.pitchHistoryWritePos.load (std::memory_order_relaxed);
        juce::Path pitchPath;
        bool started = false;

        for (int i = 0; i < DebugData::historySize; ++i)
        {
            int idx = (wp + i) % DebugData::historySize;
            float freq = debug.pitchHistory[idx].load (std::memory_order_relaxed);
            if (freq < 20.0f)
            {
                started = false;
                continue;
            }

            float midi = 69.0f + 12.0f * std::log2 (freq / 440.0f);
            float x = graphArea.getX() + static_cast<float> (i) / static_cast<float> (DebugData::historySize - 1) * static_cast<float> (graphArea.getWidth());
            float y = graphArea.getBottom() - (midi - midiLo) / (midiHi - midiLo) * static_cast<float> (graphArea.getHeight());
            y = std::clamp (y, static_cast<float> (graphArea.getY()), static_cast<float> (graphArea.getBottom()));

            if (! started)
            {
                pitchPath.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                pitchPath.lineTo (x, y);
            }
        }

        g.setColour (juce::Colours::cyan);
        g.strokePath (pitchPath, juce::PathStrokeType (1.5f));
    }

    void timerCallback() override { repaint(); }

private:
    DebugData& debug;
    std::atomic<int>& actNote;
    std::atomic<int>& actChan;
    std::atomic<float>& actVel;

    mutable int lastOnsetCount = 0;
    mutable int onsetFlashFrames = 0;

    static juce::String noteNameFromNumber (int note)
    {
        if (note < 0 || note > 127) return "--";
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (note / 12) - 1;
        return juce::String (names[note % 12]) + juce::String (octave);
    }

    static juce::String midiNoteStr (float freqHz)
    {
        if (freqHz < 20.0f) return "--";
        int note = static_cast<int> (std::round (69.0f + 12.0f * std::log2 (freqHz / 440.0f)));
        return noteNameFromNumber (std::clamp (note, 0, 127));
    }
};
