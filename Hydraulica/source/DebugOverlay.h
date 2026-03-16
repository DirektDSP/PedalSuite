#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Core/HydraulicaDSPProcessor.h"

class DebugOverlay : public juce::Component, private juce::Timer
{
public:
    using DebugData = DSP::Core::HydraulicaDSPProcessor<float>::DebugData;

    explicit DebugOverlay (DebugData& data) : debug (data) { startTimerHz (30); }
    ~DebugOverlay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xEE1a1a1a));
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        auto area = getLocalBounds().reduced (10);
        int lineH = 18;
        auto textArea = area.removeFromTop (area.getHeight() / 2);

        float grL    = debug.gainReductionDbL.load (std::memory_order_relaxed);
        float grR    = debug.gainReductionDbR.load (std::memory_order_relaxed);
        float envL   = debug.envelopeLevelL.load (std::memory_order_relaxed);
        float envR   = debug.envelopeLevelR.load (std::memory_order_relaxed);
        float doom   = debug.doomMix.load (std::memory_order_relaxed);
        bool  limOn  = debug.limiterActive.load (std::memory_order_relaxed);

        auto drawLine = [&] (const juce::String& text, juce::Colour col = juce::Colours::lightgrey) {
            g.setColour (col);
            g.drawText (text, textArea.removeFromTop (lineH), juce::Justification::left);
        };

        drawLine ("GR L: " + juce::String (grL, 1) + " dB   GR R: " + juce::String (grR, 1) + " dB",
                  grL < -1.0f ? juce::Colours::orange : juce::Colours::limegreen);
        drawLine ("Env L: " + juce::String (envL, 4) + "   Env R: " + juce::String (envR, 4), juce::Colours::cyan);
        drawLine ("DOOM Mix: " + juce::String (static_cast<int> (doom * 100.0f)) + "%",
                  doom > 0.5f ? juce::Colours::red : juce::Colours::lightgrey);
        drawLine ("Limiter: " + juce::String (limOn ? "ON" : "OFF"),
                  limOn ? juce::Colours::yellow : juce::Colours::grey);

        // GR history graph
        auto graphArea = area.reduced (0, 4);
        g.setColour (juce::Colour (0xFF222222));
        g.fillRoundedRectangle (graphArea.toFloat(), 3.0f);

        // Y axis: 0 to -30 dB
        g.setColour (juce::Colour (0xFF333333));
        for (int db = -5; db >= -25; db -= 5)
        {
            float y = graphArea.getY() + static_cast<float> (-db) / 30.0f * static_cast<float> (graphArea.getHeight());
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (graphArea.getX()), static_cast<float> (graphArea.getRight()));
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.setColour (juce::Colours::grey.withAlpha (0.5f));
            g.drawText (juce::String (db) + "dB", graphArea.getX() + 2, static_cast<int> (y) - 8, 40, 16, juce::Justification::left);
            g.setColour (juce::Colour (0xFF333333));
        }

        int wp = debug.grHistoryWritePos.load (std::memory_order_relaxed);
        juce::Path grPath;
        bool started = false;

        for (int i = 0; i < DebugData::historySize; ++i)
        {
            int idx = (wp + i) % DebugData::historySize;
            float gr = debug.grHistory[idx].load (std::memory_order_relaxed);
            float x = graphArea.getX() + static_cast<float> (i) / static_cast<float> (DebugData::historySize - 1) * static_cast<float> (graphArea.getWidth());
            float y = graphArea.getY() + std::clamp (-gr, 0.0f, 30.0f) / 30.0f * static_cast<float> (graphArea.getHeight());

            if (! started) { grPath.startNewSubPath (x, y); started = true; }
            else grPath.lineTo (x, y);
        }

        g.setColour (juce::Colours::red);
        g.strokePath (grPath, juce::PathStrokeType (1.5f));
    }

    void timerCallback() override { repaint(); }

private:
    DebugData& debug;
};
