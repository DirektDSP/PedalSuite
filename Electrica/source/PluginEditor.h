#pragma once

#include "PluginProcessor.h"
#include "DebugOverlay.h"
#include <DirektDSP.h>

// Simple resizable container that lays out DirektSections in a grid
class PopupContentPanel : public juce::Component
{
public:
    void addSection (DirektDSP::DirektSection* s) { sections.push_back (s); addAndMakeVisible (s); }
    void setColumns (int c) { cols = c; }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);
        int rows = (static_cast<int>(sections.size()) + cols - 1) / cols;
        if (rows == 0) return;
        int cellW = bounds.getWidth() / cols;
        int cellH = bounds.getHeight() / rows;
        for (int i = 0; i < static_cast<int>(sections.size()); ++i)
        {
            int c_ = i % cols;
            int r_ = i / cols;
            sections[static_cast<size_t>(i)]->setBounds (
                juce::Rectangle<int> (bounds.getX() + c_ * cellW,
                                       bounds.getY() + r_ * cellH,
                                       cellW, cellH).reduced (2));
        }
    }

private:
    std::vector<DirektDSP::DirektSection*> sections;
    int cols = 2;
};

class PluginEditor : public DirektDSP::DirektBaseEditor
{
public:
    explicit PluginEditor (PluginProcessor&);

protected:
    void layoutCustomSections (juce::Rectangle<int> mainArea) override;

private:
    static std::vector<DirektDSP::SectionDescriptor> createDescriptors();

    void buildMidiPopup();
    void buildAdvancedPopup();

    juce::TextButton midiSettingsBtn  { "MIDI Settings" };
    juce::TextButton advancedBtn      { "Advanced" };
    juce::TextButton debugBtn         { "Debug" };

    std::unique_ptr<DebugOverlay> debugOverlay;

    std::unique_ptr<PopupContentPanel> midiPopupContent;
    std::unique_ptr<PopupContentPanel> advancedPopupContent;

    std::vector<DirektDSP::BuiltSection> midiSections;
    std::vector<DirektDSP::BuiltSection> advancedSections;

    juce::AudioProcessorValueTreeState& apvtsRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
