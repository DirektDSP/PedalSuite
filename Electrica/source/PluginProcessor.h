#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#if PEDALSUITE_NO_MOONBASE
  #include "MoonbaseStubs.h"
#else
  #include "moonbase_JUCEClient/moonbase_JUCEClient.h"
#endif
#include "Service/PresetManager.h"
#include "DSP/ElectricaDSP.h"

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    MOONBASE_DECLARE_LICENSING("DirektDSP", "electrica", VERSION)

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

        // --- Oscillator ---
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"OSC_WAVE", 1}, "Oscillator",
            juce::StringArray{"Saw", "Square", "Sine", "Triangle"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"OSC_OCTAVE", 1}, "Octave", -2, 2, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"OSC_DETUNE", 1}, "Detune",
            juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"OSC_LEVEL", 1}, "Osc Level",
            juce::NormalisableRange<float>(-48.0f, 12.0f, 0.1f), 0.0f));

        // --- Envelope ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"ENV_ATTACK", 1}, "Env Attack",
            juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.3f), 10.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"ENV_RELEASE", 1}, "Env Release",
            juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.3f), 200.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"ENV_SENSITIVITY", 1}, "Sensitivity",
            juce::NormalisableRange<float>(-60.0f, -10.0f, 0.1f), -40.0f));

        // --- Filter ---
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"FILTER_TYPE", 1}, "Filter Type",
            juce::StringArray{"LPF", "HPF", "BPF"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"FILTER_FREQ", 1}, "Filter Freq",
            juce::NormalisableRange<float>(50.0f, 16000.0f, 1.0f, 0.3f), 2000.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"FILTER_RESO", 1}, "Resonance",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.3f), 0.707f));

        // --- Distortion ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"DIST_DRIVE", 1}, "Synth Drive",
            juce::NormalisableRange<float>(0.0f, 40.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"DIST_TYPE", 1}, "Dist Type",
            juce::StringArray{"Soft", "Hard", "Fold"}, 0));

        // --- Compressor ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"COMP_AMOUNT", 1}, "Synth Comp",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"COMP_SPEED", 1}, "Comp Speed",
            juce::StringArray{"Fast", "Medium", "Slow"}, 1));

        // --- Input Mode ---
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"INPUT_MODE", 1}, "Input Mode",
            juce::StringArray{"Guitar", "Vocal"}, 0));

        // --- Tracking ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"GLIDE", 1}, "Glide",
            juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f, 0.3f), 20.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"TRACKING", 1}, "Tracking",
            juce::StringArray{"Mono", "Poly"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"POLY_PEAK_GATE", 1}, "Poly Peak Gate",
            juce::NormalisableRange<float>(-80.0f, -10.0f, 0.1f), -40.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"PITCH_ALGO", 1}, "Pitch Algorithm",
            juce::StringArray{"MPM", "Cycfi Q"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"SNAP_TO_NOTE", 1}, "Snap to Note", false));

        // --- Advanced: Pitch Detection ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"YIN_WINDOW", 1}, "Detection Window",
            juce::NormalisableRange<float>(5.0f, 50.0f, 0.5f), 20.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"YIN_THRESHOLD", 1}, "Detection Tolerance",
            juce::NormalisableRange<float>(0.05f, 0.50f, 0.01f), 0.20f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"CONFIDENCE_GATE", 1}, "Confidence Gate",
            juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.30f));

        // --- MIDI Output: Enable ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_OUT", 1}, "MIDI Output", false));

        // --- MIDI Output: Channel & Routing ---
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_CHANNEL", 1}, "MIDI Channel", 1, 16, 1));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_POLY_SPREAD", 1}, "Poly Channel Spread", false));

        // --- MIDI Output: Velocity ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_VEL_OVERRIDE", 1}, "Velocity Override", false));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_VEL_VALUE", 1}, "Velocity Value", 1, 127, 100));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"MIDI_VEL_CURVE", 1}, "Velocity Curve",
            juce::StringArray{"Linear", "Soft", "Hard", "S-Curve"}, 0));

        // --- MIDI Output: Scale Lock ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_SCALE_LOCK", 1}, "Scale Lock", false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"MIDI_SCALE_ROOT", 1}, "Scale Root",
            juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"MIDI_SCALE_TYPE", 1}, "Scale Type",
            juce::StringArray{"Chromatic", "Major", "Minor", "Harmonic Min",
                              "Dorian", "Phrygian", "Lydian", "Mixolydian",
                              "Whole Tone", "Blues", "Penta Maj", "Penta Min"}, 1));

        // --- MIDI Output: Gate & Timing ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_GATE", 1}, "MIDI Gate",
            juce::NormalisableRange<float>(-80.0f, -10.0f, 0.1f), -60.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_NOTEOFF_DELAY", 1}, "Note-Off Delay",
            juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f, 0.3f), 0.0f));

        // --- MIDI Output: Transpose & Range ---
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_TRANSPOSE", 1}, "MIDI Transpose", -24, 24, 0));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_NOTE_MIN", 1}, "MIDI Note Min", 0, 127, 0));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_NOTE_MAX", 1}, "MIDI Note Max", 0, 127, 127));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_OCTAVE_LOCK", 1}, "Octave Lock", false));

        // --- MIDI Output: Articulation ---
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"MIDI_RETRIGGER", 1}, "Retrigger Mode",
            juce::StringArray{"Retrigger", "Legato"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_NOTE_HOLD", 1}, "Note Hold", false));

        // --- MIDI Output: Transient-gated Retrigger ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_TRANSIENT_SENS", 1}, "Transient Sensitivity",
            juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_TRANSIENT_HOLD", 1}, "Transient Hold",
            juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.3f), 50.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_MIN_NOTE_DUR", 1}, "Min Note Duration",
            juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.3f), 50.0f));

        // --- MIDI Output: Pitch Bend ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_PITCHBEND", 1}, "Pitch Bend Output", false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{"MIDI_PITCHBEND_RANGE", 1}, "Pitch Bend Range",
            juce::StringArray{"2 semi", "12 semi", "24 semi"}, 0));

        // --- MIDI Output: CC from Envelope ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"MIDI_CC_ENABLE", 1}, "CC Output", false));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"MIDI_CC_NUMBER", 1}, "CC Number", 0, 127, 1));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"MIDI_CC_SMOOTH", 1}, "CC Smoothing",
            juce::NormalisableRange<float>(1.0f, 100.0f, 0.5f, 0.3f), 10.0f));

        return { params.begin(), params.end() };
    }

private:

    std::unique_ptr<Service::PresetManager> presetManager;

    DSP::FloatProcessor dspProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
