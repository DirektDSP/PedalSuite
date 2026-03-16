#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : DirektBaseEditor (p, p.apvts, p.getPresetManager(),
                        "Hydraulica", DirektDSP::Colours::accentRed,
                        16.0f / 10.0f, 700, createDescriptors())
{
    debugBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF8B0000));
    debugBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    debugBtn.onClick = [this, &p] {
        if (debugOverlay == nullptr)
            debugOverlay = std::make_unique<DebugOverlay> (p.getDSPProcessor().debugData);
        showPopup ("Debug Overlay", debugOverlay.get(), 450, 350);
    };
    addAndMakeVisible (debugBtn);

    setSize (700, 438);
}

void PluginEditor::layoutCustomSections (juce::Rectangle<int> mainArea)
{
    auto area = mainArea.reduced (8);

    auto* doomSection = findSection ("Doom");
    auto* compSection = findSection ("Compressor");
    auto* bottomSection = findSection ("Controls");

    if (doomSection == nullptr || compSection == nullptr || bottomSection == nullptr)
    {
        DirektBaseEditor::layoutCustomSections (mainArea);
        return;
    }

    // Doom hero knob gets top ~38%
    auto doomArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.38f));
    doomSection->setBounds (doomArea.reduced (0, 2));

    // Compressor row gets ~35%
    auto compArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.55f));
    compSection->setBounds (compArea.reduced (0, 2));

    // Bottom controls get the rest — leave space for debug button
    auto btnW = 60;
    auto bottomRow = area;
    bottomSection->setBounds (bottomRow.withTrimmedRight (btnW + 4).reduced (0, 2));
    debugBtn.setBounds (bottomRow.removeFromRight (btnW).reduced (2));
}

std::vector<DirektDSP::SectionDescriptor> PluginEditor::createDescriptors()
{
    using CT = DirektDSP::ControlType;
    return {
        { "Doom", {
            { "DOOM", "DOOM", CT::Knob },
        }, 1 },

        { "Compressor", {
            { "THRESHOLD", "Threshold", CT::Knob },
            { "RATIO",     "Ratio",     CT::Knob },
            { "ATTACK",    "Attack",    CT::Knob },
            { "RELEASE",   "Release",   CT::Knob },
            { "KNEE",      "Knee",      CT::Knob },
            { "MAKEUP",    "Makeup",    CT::Knob },
        }, 6 },

        { "Controls", {
            { "SC_HPF",          "SC HPF",   CT::Knob },
            { "LIMITER_ON",      "Limiter",  CT::Toggle },
            { "LIMITER_CEILING", "Ceiling",  CT::Knob },
            { "INPUT_GAIN",      "Input",    CT::Knob },
            { "OUTPUT_GAIN",     "Output",   CT::Knob },
            { "MIX",             "Mix",      CT::Knob },
        }, 6 },
    };
}
