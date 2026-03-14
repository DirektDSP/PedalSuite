#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if PEDALSUITE_NO_MOONBASE
  #include "MoonbaseStubs.h"
#else
  #include "moonbase_JUCEClient/moonbase_JUCEClient.h"
#endif
#include "Service/PresetManager.h"
#include "DSP/HydraulicaDSP.h"

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    MOONBASE_DECLARE_LICENSING("DirektDSP", "hydraulica", VERSION)

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AudioProcessorValueTreeState& getApvts() { return apvts; }

    Service::PresetManager& getPresetManager() { return *presetManager; }

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // --- Common ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"INPUT_GAIN", 1}, "Input",
            juce::NormalisableRange<float>(-48.0f, 24.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"OUTPUT_GAIN", 1}, "Output",
            juce::NormalisableRange<float>(-48.0f, 24.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIX", 1}, "Mix",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));

        // --- DOOM ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"DOOM", 1}, "Doom",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        // --- Compressor ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"THRESHOLD", 1}, "Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -20.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"RATIO", 1}, "Ratio",
            juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 0.3f), 4.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"ATTACK", 1}, "Attack",
            juce::NormalisableRange<float>(0.01f, 100.0f, 0.01f, 0.3f), 5.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"RELEASE", 1}, "Release",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 100.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"KNEE", 1}, "Knee",
            juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 6.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MAKEUP", 1}, "Makeup",
            juce::NormalisableRange<float>(0.0f, 60.0f, 0.1f), 0.0f));

        // --- Sidechain ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"SC_HPF", 1}, "SC HPF",
            juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.3f), 20.0f));

        // --- Limiter ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"LIMITER_ON", 1}, "Limiter", true));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"LIMITER_CEILING", 1}, "Ceiling",
            juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f));

        return { params.begin(), params.end() };
    }

private:

    std::unique_ptr<Service::PresetManager> presetManager;

    DSP::FloatProcessor dspProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
