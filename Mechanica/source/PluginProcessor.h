#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if PEDALSUITE_NO_MOONBASE
  #include "MoonbaseStubs.h"
#else
  #include "moonbase_JUCEClient/moonbase_JUCEClient.h"
#endif
#include "Service/PresetManager.h"
#include "DSP/MechanicaDSP.h"

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    MOONBASE_DECLARE_LICENSING("DirektDSP", "mechanica", VERSION)

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

        // --- Drive & Waveshaping ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"DRIVE", 1}, "Drive",
            juce::NormalisableRange<float>(0.0f, 60.0f, 0.1f), 12.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"WAVESHAPER", 1}, "Waveshaper",
            juce::StringArray{"Tanh", "Atan", "Foldback", "Hard Clip", "Tube", "Rectify"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"BIAS", 1}, "Bias",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"ASYMMETRY", 1}, "Asymmetry",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"STAGES", 1}, "Stages", 1, 3, 1));

        // --- Pre-EQ ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"PRE_LOW", 1}, "Pre Low",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"PRE_MID", 1}, "Pre Mid",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"PRE_MID_FREQ", 1}, "Pre Mid Freq",
            juce::NormalisableRange<float>(200.0f, 5000.0f, 1.0f, 0.3f), 800.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"PRE_MID_Q", 1}, "Pre Mid Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"PRE_HIGH", 1}, "Pre High",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        // --- Post-EQ ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POST_LOW", 1}, "Post Low",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POST_MID", 1}, "Post Mid",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POST_MID_FREQ", 1}, "Post Mid Freq",
            juce::NormalisableRange<float>(200.0f, 5000.0f, 1.0f, 0.3f), 800.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POST_MID_Q", 1}, "Post Mid Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POST_HIGH", 1}, "Post High",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        // --- Oversampling ---
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"OVERSAMPLING", 1}, "Oversampling",
            juce::StringArray{"Off", "2x", "4x", "8x"}, 1));

        // --- Feedback ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"FEEDBACK", 1}, "Feedback",
            juce::NormalisableRange<float>(0.0f, 80.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"FEEDBACK_TONE", 1}, "Feedback Tone",
            juce::NormalisableRange<float>(200.0f, 8000.0f, 1.0f, 0.3f), 2000.0f));

        // --- Noise Gate ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"GATE_THRESHOLD", 1}, "Gate",
            juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -80.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"GATE_RELEASE", 1}, "Gate Release",
            juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 50.0f));

        return { params.begin(), params.end() };
    }

private:

    std::unique_ptr<Service::PresetManager> presetManager;

    DSP::FloatProcessor dspProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
