#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : DirektBaseEditor (p, p.apvts, p.getPresetManager(),
                        "Pneumatica", DirektDSP::Colours::accentCyan,
                        16.0f / 10.0f, 700, createDescriptors())
{
    debugBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF8B0000));
    debugBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    debugBtn.setBounds (0, 0, 50, 22);
    debugBtn.onClick = [this, &p] {
        if (debugOverlay == nullptr)
            debugOverlay = std::make_unique<DebugOverlay> (p.getDSPProcessor().debugData);
        showPopup ("Debug Overlay", debugOverlay.get(), 400, 300);
    };
    addAndMakeVisible (debugBtn);

    setSize (700, 438);
}

void PluginEditor::layoutCustomSections (juce::Rectangle<int> mainArea)
{
    auto area = mainArea.reduced (8);

    auto* topSection = findSection ("Crossover / Width / Shimmer");
    auto* crunchSection = findSection ("Crunch");
    auto* noiseSection = findSection ("Noise");
    auto* airSection = findSection ("Air EQ");
    auto* commonSection = findSection ("Common");

    if (topSection == nullptr || crunchSection == nullptr ||
        noiseSection == nullptr || airSection == nullptr || commonSection == nullptr)
    {
        DirektBaseEditor::layoutCustomSections (mainArea);
        return;
    }

    // Top row ~30%
    auto topArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.30f));
    topSection->setBounds (topArea.reduced (0, 2));

    // Middle row ~35% — Crunch left, Noise right
    auto midArea = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.50f));
    auto midLeft = midArea.removeFromLeft (midArea.getWidth() / 2);
    crunchSection->setBounds (midLeft.reduced (2));
    noiseSection->setBounds (midArea.reduced (2));

    // Bottom row — Air EQ left, Common right, debug button far right
    auto btnW = 60;
    auto bottomRow = area;
    auto bottomRight = bottomRow.removeFromRight (btnW);
    auto bottomLeft = bottomRow.removeFromLeft (bottomRow.getWidth() / 2);
    airSection->setBounds (bottomLeft.reduced (2));
    commonSection->setBounds (bottomRow.reduced (2));
    debugBtn.setBounds (bottomRight.reduced (2));
}

std::vector<DirektDSP::SectionDescriptor> PluginEditor::createDescriptors()
{
    using CT = DirektDSP::ControlType;
    return {
        { "Crossover / Width / Shimmer", {
            { "CROSSOVER", "Crossover", CT::Knob, "Crossover frequency — processing applies above this point" },
            { "WIDTH",     "Width",     CT::Knob, "Stereo width of the high-frequency band" },
            { "SHIMMER",   "Shimmer",   CT::Knob, "Shimmer amount — adds pitched octave reflections to highs" },
        }, 3 },

        { "Crunch", {
            { "CRUNCH",      "Amount",     CT::Knob,     "High-frequency saturation amount — adds crunch and edge" },
            { "CRUNCH_TYPE", "Type",       CT::ComboBox, "Saturation type applied to the high band" },
        }, 2 },

        { "Noise", {
            { "NOISE_LEVEL",  "Level",  CT::Knob,     "Noise generator level — adds analog-style hiss and texture" },
            { "NOISE_TYPE",   "Type",   CT::ComboBox, "Noise color (White, Pink, etc.)" },
            { "NOISE_FILTER", "Filter", CT::Knob,     "Low-pass filter on the noise — shapes the noise character" },
        }, 3 },

        { "Air EQ", {
            { "AIR_FREQ", "Freq", CT::Knob, "Air band center frequency — high shelf for presence and sparkle" },
            { "AIR_GAIN", "Gain", CT::Knob, "Air band gain — boost or cut the top-end sheen" },
        }, 2 },

        { "Common", {
            { "INPUT_GAIN",  "Input",  CT::Knob, "Input gain before processing" },
            { "OUTPUT_GAIN", "Output", CT::Knob, "Output gain after processing" },
            { "MIX",         "Mix",    CT::Knob, "Dry/wet mix — 0% = clean, 100% = fully processed" },
        }, 3 },
    };
}
