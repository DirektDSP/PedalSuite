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
            { "DOOM", "DOOM", CT::Knob, "The DOOM knob — extreme normalizer that brings everything to the same level" },
        }, 1 },

        { "Compressor", {
            { "THRESHOLD", "Threshold", CT::Knob, "Compressor threshold — signal above this gets compressed" },
            { "RATIO",     "Ratio",     CT::Knob, "Compression ratio — higher = more aggressive compression" },
            { "ATTACK",    "Attack",    CT::Knob, "Compressor attack time — how fast it reacts to loud transients" },
            { "RELEASE",   "Release",   CT::Knob, "Compressor release time — how fast gain recovers after compression" },
            { "KNEE",      "Knee",      CT::Knob, "Compressor knee — soft knee eases into compression, hard knee is abrupt" },
            { "MAKEUP",    "Makeup",    CT::Knob, "Makeup gain to compensate for compression-induced volume loss" },
        }, 6 },

        { "Controls", {
            { "SC_HPF",          "SC HPF",   CT::Knob,   "Sidechain high-pass filter — prevents low frequencies from triggering compression" },
            { "LIMITER_ON",      "Limiter",  CT::Toggle,  "Enable output brickwall limiter" },
            { "LIMITER_CEILING", "Ceiling",  CT::Knob,    "Limiter ceiling — maximum output level" },
            { "INPUT_GAIN",      "Input",    CT::Knob,    "Input gain before processing" },
            { "OUTPUT_GAIN",     "Output",   CT::Knob,    "Output gain after processing" },
            { "MIX",             "Mix",      CT::Knob,    "Dry/wet mix — blend compressed and clean signals (parallel compression)" },
        }, 6 },
    };
}
