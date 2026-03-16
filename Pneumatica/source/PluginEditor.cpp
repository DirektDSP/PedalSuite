#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : DirektBaseEditor (p, p.apvts, p.getPresetManager(),
                        "Pneumatica", DirektDSP::Colours::accentCyan,
                        16.0f / 10.0f, 700, createDescriptors())
{
}

std::vector<DirektDSP::SectionDescriptor> PluginEditor::createDescriptors()
{
    using CT = DirektDSP::ControlType;
    return {
        { "Crossover / Width / Shimmer", {
            { "CROSSOVER", "Crossover", CT::Knob },
            { "WIDTH",     "Width",     CT::Knob },
            { "SHIMMER",   "Shimmer",   CT::Knob },
        }, 3 },

        { "Crunch", {
            { "CRUNCH",      "Amount",     CT::Knob },
            { "CRUNCH_TYPE", "Type",       CT::ComboBox },
        }, 2 },

        { "Noise", {
            { "NOISE_LEVEL",  "Level",  CT::Knob },
            { "NOISE_TYPE",   "Type",   CT::ComboBox },
            { "NOISE_FILTER", "Filter", CT::Knob },
        }, 3 },

        { "Air EQ", {
            { "AIR_FREQ", "Freq", CT::Knob },
            { "AIR_GAIN", "Gain", CT::Knob },
        }, 2 },

        { "Common", {
            { "INPUT_GAIN",  "Input",  CT::Knob },
            { "OUTPUT_GAIN", "Output", CT::Knob },
            { "MIX",         "Mix",    CT::Knob },
        }, 3 },
    };
}
