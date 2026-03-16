#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if PEDALSUITE_NO_MOONBASE
  #include "MoonbaseStubs.h"
#else
  #include "moonbase_JUCEClient/moonbase_JUCEClient.h"
#endif
#include "Service/PresetManager.h"
#include "DSP/PneumaticaDSP.h"

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    MOONBASE_DECLARE_LICENSING("DirektDSP", "pneumatica", VERSION)

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
    DSP::FloatProcessor& getDSPProcessor() { return dspProcessor; }

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

        // --- Crossover ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"CROSSOVER", 1}, "Crossover",
            juce::NormalisableRange<float>(500.0f, 16000.0f, 1.0f, 0.3f), 3000.0f));

        // --- Width ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"WIDTH", 1}, "Width",
            juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));

        // --- Crunch ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"CRUNCH", 1}, "Crunch",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"CRUNCH_TYPE", 1}, "Crunch Type",
            juce::StringArray{"Soft", "Warm", "Crispy"}, 0));

        // --- Noise ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"NOISE_LEVEL", 1}, "Noise Level",
            juce::NormalisableRange<float>(-80.0f, -20.0f, 0.1f), -80.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"NOISE_TYPE", 1}, "Noise Type",
            juce::StringArray{"Analog Hiss", "Tape Hiss", "Digital Crackle"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"NOISE_FILTER", 1}, "Noise Filter",
            juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 8000.0f));

        // --- Air EQ ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"AIR_FREQ", 1}, "Air Freq",
            juce::NormalisableRange<float>(4000.0f, 20000.0f, 1.0f, 0.3f), 12000.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"AIR_GAIN", 1}, "Air Gain",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        // --- Shimmer ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"SHIMMER", 1}, "Shimmer",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        return { params.begin(), params.end() };
    }

private:

    std::unique_ptr<Service::PresetManager> presetManager;

    DSP::FloatProcessor dspProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
