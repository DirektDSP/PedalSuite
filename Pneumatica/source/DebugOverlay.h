#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/Core/PneumaticaDSPProcessor.h"

class DebugOverlay : public juce::Component, private juce::Timer
{
public:
    using DebugData = DSP::Core::PneumaticaDSPProcessor<float>::DebugData;

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

        float lowL  = debug.lowBandLevelL.load (std::memory_order_relaxed);
        float hiL   = debug.highBandLevelL.load (std::memory_order_relaxed);
        float lowR  = debug.lowBandLevelR.load (std::memory_order_relaxed);
        float hiR   = debug.highBandLevelR.load (std::memory_order_relaxed);
        float noise = debug.noiseLevel.load (std::memory_order_relaxed);

        auto toDb = [] (float lin) { return 20.0f * std::log10 (std::max (lin, 1e-10f)); };

        auto drawLine = [&] (const juce::String& text, juce::Colour col = juce::Colours::lightgrey) {
            g.setColour (col);
            g.drawText (text, area.removeFromTop (lineH), juce::Justification::left);
        };

        drawLine ("Low Band  L: " + juce::String (toDb (lowL), 1) + " dB   R: " + juce::String (toDb (lowR), 1) + " dB",
                  juce::Colours::cyan);
        drawLine ("High Band L: " + juce::String (toDb (hiL), 1) + " dB   R: " + juce::String (toDb (hiR), 1) + " dB",
                  juce::Colours::orange);
        drawLine ("Noise Floor: " + juce::String (toDb (noise), 1) + " dB",
                  noise > 0.001f ? juce::Colours::yellow : juce::Colours::grey);

        // Band level bars
        auto barArea = area.reduced (0, 8);
        int barH = 16;
        auto drawBar = [&] (const juce::String& label, float level, juce::Colour col) {
            auto row = barArea.removeFromTop (barH + 4);
            g.setColour (juce::Colours::grey);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (label, row.removeFromLeft (60), juce::Justification::left);
            auto barBg = row.reduced (0, 2);
            g.setColour (juce::Colour (0xFF333333));
            g.fillRect (barBg);
            float norm = std::clamp ((toDb (level) + 60.0f) / 60.0f, 0.0f, 1.0f);
            g.setColour (col);
            g.fillRect (barBg.removeFromLeft (static_cast<int> (norm * static_cast<float> (barBg.getWidth()))));
        };

        drawBar ("Low L", lowL, juce::Colours::cyan);
        drawBar ("High L", hiL, juce::Colours::orange);
        drawBar ("Low R", lowR, juce::Colours::cyan.darker());
        drawBar ("High R", hiR, juce::Colours::orange.darker());
        drawBar ("Noise", noise, juce::Colours::yellow);
    }

    void timerCallback() override { repaint(); }

private:
    DebugData& debug;
};
