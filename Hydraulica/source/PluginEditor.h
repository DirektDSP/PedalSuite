#pragma once

#include "PluginProcessor.h"
#include <DirektBaseEditor.h>

class PluginEditor : public DirektDSP::DirektBaseEditor
{
public:
    explicit PluginEditor (PluginProcessor&);

protected:
    void layoutCustomSections (juce::Rectangle<int> mainArea) override;

private:
    static std::vector<DirektDSP::SectionDescriptor> createDescriptors();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
