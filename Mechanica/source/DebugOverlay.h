#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Core/MechanicaDSPProcessor.h"

class DebugOverlay : public juce::Component, private juce::Timer
{
public:
    using DebugData = DSP::Core::MechanicaDSPProcessor<float>::DebugData;

    explicit DebugOverlay (DebugData& data) : debug (data) { startTimerHz (30); }
    ~DebugOverlay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xEE1a1a1a));
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        auto area = getLocalBounds().reduced (10);
        int lineH = 20;

        int ws       = debug.waveshaperType.load (std::memory_order_relaxed);
        int stages   = debug.activeStages.load (std::memory_order_relaxed);
        int os       = debug.oversamplingRate.load (std::memory_order_relaxed);
        float drive  = debug.driveLevel.load (std::memory_order_relaxed);
        float outLvl = debug.outputLevelL.load (std::memory_order_relaxed);
        bool gateL   = debug.gateOpenL.load (std::memory_order_relaxed);
        bool gateR   = debug.gateOpenR.load (std::memory_order_relaxed);

        static const char* wsNames[] = { "Tanh", "Atan", "Foldback", "HardClip", "Tube", "Rectify" };
        static const char* osNames[] = { "Off", "2x", "4x", "8x" };

        auto wsName = (ws >= 0 && ws <= 5) ? wsNames[ws] : "?";
        auto osName = (os >= 0 && os <= 3) ? osNames[os] : "?";

        auto drawLine = [&] (const juce::String& text, juce::Colour col = juce::Colours::lightgrey) {
            g.setColour (col);
            g.drawText (text, area.removeFromTop (lineH), juce::Justification::left);
        };

        drawLine ("Waveshaper: " + juce::String (wsName), juce::Colours::orange);
        drawLine ("Stages: " + juce::String (stages) + "   Oversampling: " + juce::String (osName), juce::Colours::cyan);
        drawLine ("Drive (linear): " + juce::String (drive, 2), juce::Colours::yellow);
        drawLine ("Output Level: " + juce::String (20.0f * std::log10 (std::max (outLvl, 1e-10f)), 1) + " dB",
                  juce::Colours::limegreen);
        drawLine ("Gate: L=" + juce::String (gateL ? "OPEN" : "CLOSED") + "  R=" + juce::String (gateR ? "OPEN" : "CLOSED"),
                  (gateL && gateR) ? juce::Colours::limegreen : juce::Colours::red);
    }

    void timerCallback() override { repaint(); }

private:
    DebugData& debug;
};
