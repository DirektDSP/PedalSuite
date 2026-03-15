#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.state.setProperty(Service::PresetManager::presetNameProperty, "", nullptr);
    presetManager = std::make_unique<Service::PresetManager>(apvts);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================

static DSP::Core::ElectricaParams loadParams (juce::AudioProcessorValueTreeState& apvts)
{
    DSP::Core::ElectricaParams p;
    p.inputGainDb     = apvts.getRawParameterValue ("INPUT_GAIN")->load();
    p.outputGainDb    = apvts.getRawParameterValue ("OUTPUT_GAIN")->load();
    p.mixPercent      = apvts.getRawParameterValue ("MIX")->load();
    p.oscWave         = static_cast<int> (apvts.getRawParameterValue ("OSC_WAVE")->load());
    p.oscOctave       = static_cast<int> (apvts.getRawParameterValue ("OSC_OCTAVE")->load());
    p.oscDetune       = apvts.getRawParameterValue ("OSC_DETUNE")->load();
    p.oscLevelDb      = apvts.getRawParameterValue ("OSC_LEVEL")->load();
    p.envAttack       = apvts.getRawParameterValue ("ENV_ATTACK")->load();
    p.envRelease      = apvts.getRawParameterValue ("ENV_RELEASE")->load();
    p.envSensitivity  = apvts.getRawParameterValue ("ENV_SENSITIVITY")->load();
    p.filterType      = static_cast<int> (apvts.getRawParameterValue ("FILTER_TYPE")->load());
    p.filterFreq      = apvts.getRawParameterValue ("FILTER_FREQ")->load();
    p.filterReso      = apvts.getRawParameterValue ("FILTER_RESO")->load();
    p.distDrive       = apvts.getRawParameterValue ("DIST_DRIVE")->load();
    p.distType        = static_cast<int> (apvts.getRawParameterValue ("DIST_TYPE")->load());
    p.compAmount      = apvts.getRawParameterValue ("COMP_AMOUNT")->load();
    p.compSpeed       = static_cast<int> (apvts.getRawParameterValue ("COMP_SPEED")->load());
    p.inputMode       = static_cast<int> (apvts.getRawParameterValue ("INPUT_MODE")->load());
    p.glide           = apvts.getRawParameterValue ("GLIDE")->load();
    p.tracking        = static_cast<int> (apvts.getRawParameterValue ("TRACKING")->load());
    p.pitchAlgorithm  = static_cast<int> (apvts.getRawParameterValue ("PITCH_ALGO")->load());
    p.snapToNote      = apvts.getRawParameterValue ("SNAP_TO_NOTE")->load() >= 0.5f;
    p.yinWindowMs     = apvts.getRawParameterValue ("YIN_WINDOW")->load();
    p.yinThreshold    = apvts.getRawParameterValue ("YIN_THRESHOLD")->load();
    p.confidenceGate  = apvts.getRawParameterValue ("CONFIDENCE_GATE")->load();
    // MIDI Output
    p.midiOutput          = apvts.getRawParameterValue ("MIDI_OUT")->load() >= 0.5f;
    p.midiChannel         = static_cast<int> (apvts.getRawParameterValue ("MIDI_CHANNEL")->load());
    p.midiPolySpread      = apvts.getRawParameterValue ("MIDI_POLY_SPREAD")->load() >= 0.5f;
    p.midiVelOverride     = apvts.getRawParameterValue ("MIDI_VEL_OVERRIDE")->load() >= 0.5f;
    p.midiVelValue        = static_cast<int> (apvts.getRawParameterValue ("MIDI_VEL_VALUE")->load());
    p.midiVelCurve        = static_cast<int> (apvts.getRawParameterValue ("MIDI_VEL_CURVE")->load());
    p.midiScaleLock       = apvts.getRawParameterValue ("MIDI_SCALE_LOCK")->load() >= 0.5f;
    p.midiScaleRoot       = static_cast<int> (apvts.getRawParameterValue ("MIDI_SCALE_ROOT")->load());
    p.midiScaleType       = static_cast<int> (apvts.getRawParameterValue ("MIDI_SCALE_TYPE")->load());
    p.midiGateDb          = apvts.getRawParameterValue ("MIDI_GATE")->load();
    p.midiNoteOffDelayMs  = apvts.getRawParameterValue ("MIDI_NOTEOFF_DELAY")->load();
    p.midiTranspose       = static_cast<int> (apvts.getRawParameterValue ("MIDI_TRANSPOSE")->load());
    p.midiNoteMin         = static_cast<int> (apvts.getRawParameterValue ("MIDI_NOTE_MIN")->load());
    p.midiNoteMax         = static_cast<int> (apvts.getRawParameterValue ("MIDI_NOTE_MAX")->load());
    p.midiRetrigger       = static_cast<int> (apvts.getRawParameterValue ("MIDI_RETRIGGER")->load());
    p.midiNoteHold        = apvts.getRawParameterValue ("MIDI_NOTE_HOLD")->load() >= 0.5f;
    p.midiTransientSens   = apvts.getRawParameterValue ("MIDI_TRANSIENT_SENS")->load();
    p.midiTransientHoldMs = apvts.getRawParameterValue ("MIDI_TRANSIENT_HOLD")->load();
    p.midiPitchBend       = apvts.getRawParameterValue ("MIDI_PITCHBEND")->load() >= 0.5f;
    {
        int pbIdx = static_cast<int> (apvts.getRawParameterValue ("MIDI_PITCHBEND_RANGE")->load());
        constexpr int pbRanges[] = { 2, 12, 24 };
        p.midiPitchBendRange = pbRanges[std::clamp (pbIdx, 0, 2)];
    }
    p.midiCCEnable        = apvts.getRawParameterValue ("MIDI_CC_ENABLE")->load() >= 0.5f;
    p.midiCCNumber        = static_cast<int> (apvts.getRawParameterValue ("MIDI_CC_NUMBER")->load());
    p.midiCCSmoothMs      = apvts.getRawParameterValue ("MIDI_CC_SMOOTH")->load();
    return p;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
    spec.numChannels = static_cast<uint32>(getTotalNumOutputChannels());

    auto inputGainDb = apvts.getRawParameterValue("INPUT_GAIN")->load();
    auto outputGainDb = apvts.getRawParameterValue("OUTPUT_GAIN")->load();
    auto mixPercent = apvts.getRawParameterValue("MIX")->load();

    dspProcessor.prepare(spec, inputGainDb, outputGainDb, mixPercent);
    dspProcessor.updateParameters (loadParams (apvts));

    MOONBASE_PREPARE_TO_PLAY (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    auto inputGainDb = apvts.getRawParameterValue("INPUT_GAIN")->load();
    auto outputGainDb = apvts.getRawParameterValue("OUTPUT_GAIN")->load();
    auto mixPercent = apvts.getRawParameterValue("MIX")->load();

    dspProcessor.reset(inputGainDb, outputGainDb, mixPercent);
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
   #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    dspProcessor.updateParameters (loadParams (apvts));
    dspProcessor.processBlock(buffer);

    // Convert DSP MIDI events to JUCE MidiBuffer
    const auto& events = dspProcessor.getMidiEvents();
    if (! events.empty())
    {
        for (const auto& e : events)
        {
            switch (e.type)
            {
                case DSP::Core::MidiEvent::NoteOn:
                {
                    float vel = std::max (e.velocity, 1.0f / 127.0f);
                    midiMessages.addEvent (
                        juce::MidiMessage::noteOn (e.channel, e.noteNumber, vel), e.samplePosition);
                    break;
                }
                case DSP::Core::MidiEvent::NoteOff:
                    midiMessages.addEvent (
                        juce::MidiMessage::noteOff (e.channel, e.noteNumber, 0.0f), e.samplePosition);
                    break;

                case DSP::Core::MidiEvent::ControlChange:
                    midiMessages.addEvent (
                        juce::MidiMessage::controllerEvent (e.channel, e.ccNumber, e.ccValue), e.samplePosition);
                    break;

                case DSP::Core::MidiEvent::PitchBendMsg:
                    midiMessages.addEvent (
                        juce::MidiMessage::pitchWheel (e.channel, e.pitchBendValue), e.samplePosition);
                    break;
            }
        }
        dspProcessor.clearMidiEvents();
    }

    MOONBASE_PROCESS (buffer);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
