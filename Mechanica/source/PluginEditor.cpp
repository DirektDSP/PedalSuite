#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : DirektBaseEditor (p, p.apvts, p.getPresetManager(),
                        "Mechanica", DirektDSP::Colours::accentOrange,
                        16.0f / 10.0f, 800, createDescriptors())
{
    debugBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF8B0000));
    debugBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    debugBtn.onClick = [this, &p] {
        if (debugOverlay == nullptr)
            debugOverlay = std::make_unique<DebugOverlay> (p.getDSPProcessor().debugData);
        showPopup ("Debug Overlay", debugOverlay.get(), 400, 250);
    };
    addAndMakeVisible (debugBtn);

    setSize (800, 500);
}

void PluginEditor::layoutCustomSections (juce::Rectangle<int> mainArea)
{
    auto area = mainArea.reduced (8);

    auto* driveSection = findSection ("Drive");
    auto* preEqSection = findSection ("Pre-EQ");
    auto* postEqSection = findSection ("Post-EQ");
    auto* bottomSection = findSection ("Controls");

    if (driveSection == nullptr || preEqSection == nullptr ||
        postEqSection == nullptr || bottomSection == nullptr)
    {
        DirektBaseEditor::layoutCustomSections (mainArea);
        return;
    }

    // Drive hero row gets top ~30%
    auto driveArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.30f));
    driveSection->setBounds (driveArea.reduced (0, 2));

    // Pre-EQ and Post-EQ split the next ~40%
    auto eqArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.57f));
    auto eqLeft = eqArea.removeFromLeft (eqArea.getWidth() / 2);
    preEqSection->setBounds (eqLeft.reduced (2));
    postEqSection->setBounds (eqArea.reduced (2));

    // Bottom controls — leave space for debug button
    auto btnW = 60;
    auto bottomRow = area;
    bottomSection->setBounds (bottomRow.withTrimmedRight (btnW + 4).reduced (0, 2));
    debugBtn.setBounds (bottomRow.removeFromRight (btnW).reduced (2));
}

std::vector<DirektDSP::SectionDescriptor> PluginEditor::createDescriptors()
{
    using CT = DirektDSP::ControlType;
    return {
        { "Drive", {
            { "DRIVE",        "Drive",        CT::Knob },
            { "WAVESHAPER",   "Waveshaper",   CT::ComboBox },
            { "BIAS",         "Bias",         CT::Knob },
            { "ASYMMETRY",    "Asymmetry",    CT::Knob },
            { "STAGES",       "Stages",       CT::Knob },
            { "OVERSAMPLING", "Oversampling", CT::ComboBox },
        }, 6 },

        { "Pre-EQ", {
            { "PRE_LOW",      "Low",  CT::Knob },
            { "PRE_MID",      "Mid",  CT::Knob },
            { "PRE_MID_FREQ", "Freq", CT::Knob },
            { "PRE_MID_Q",    "Q",    CT::Knob },
            { "PRE_HIGH",     "High", CT::Knob },
        }, 5 },

        { "Post-EQ", {
            { "POST_LOW",      "Low",  CT::Knob },
            { "POST_MID",      "Mid",  CT::Knob },
            { "POST_MID_FREQ", "Freq", CT::Knob },
            { "POST_MID_Q",    "Q",    CT::Knob },
            { "POST_HIGH",     "High", CT::Knob },
        }, 5 },

        { "Controls", {
            { "FEEDBACK",       "Feedback",  CT::Knob },
            { "FEEDBACK_TONE",  "Tone",      CT::Knob },
            { "GATE_THRESHOLD", "Gate",      CT::Knob },
            { "GATE_RELEASE",   "Gate Rel",  CT::Knob },
            { "INPUT_GAIN",     "Input",     CT::Knob },
            { "OUTPUT_GAIN",    "Output",    CT::Knob },
            { "MIX",            "Mix",       CT::Knob },
        }, 7 },
    };
}
