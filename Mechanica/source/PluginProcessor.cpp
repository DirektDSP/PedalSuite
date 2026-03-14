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

static DSP::Core::MechanicaParams loadParams (juce::AudioProcessorValueTreeState& apvts)
{
    DSP::Core::MechanicaParams p;
    p.inputGainDb   = apvts.getRawParameterValue ("INPUT_GAIN")->load();
    p.outputGainDb  = apvts.getRawParameterValue ("OUTPUT_GAIN")->load();
    p.mixPercent    = apvts.getRawParameterValue ("MIX")->load();
    p.drive         = apvts.getRawParameterValue ("DRIVE")->load();
    p.waveshaper    = static_cast<int> (apvts.getRawParameterValue ("WAVESHAPER")->load());
    p.bias          = apvts.getRawParameterValue ("BIAS")->load();
    p.asymmetry     = apvts.getRawParameterValue ("ASYMMETRY")->load();
    p.preLow        = apvts.getRawParameterValue ("PRE_LOW")->load();
    p.preMid        = apvts.getRawParameterValue ("PRE_MID")->load();
    p.preMidFreq    = apvts.getRawParameterValue ("PRE_MID_FREQ")->load();
    p.preMidQ       = apvts.getRawParameterValue ("PRE_MID_Q")->load();
    p.preHigh       = apvts.getRawParameterValue ("PRE_HIGH")->load();
    p.postLow       = apvts.getRawParameterValue ("POST_LOW")->load();
    p.postMid       = apvts.getRawParameterValue ("POST_MID")->load();
    p.postMidFreq   = apvts.getRawParameterValue ("POST_MID_FREQ")->load();
    p.postMidQ      = apvts.getRawParameterValue ("POST_MID_Q")->load();
    p.postHigh      = apvts.getRawParameterValue ("POST_HIGH")->load();
    p.oversampling  = static_cast<int> (apvts.getRawParameterValue ("OVERSAMPLING")->load());
    p.feedback      = apvts.getRawParameterValue ("FEEDBACK")->load();
    p.feedbackTone  = apvts.getRawParameterValue ("FEEDBACK_TONE")->load();
    p.gateThreshold = apvts.getRawParameterValue ("GATE_THRESHOLD")->load();
    p.gateRelease   = apvts.getRawParameterValue ("GATE_RELEASE")->load();
    p.stages        = static_cast<int> (apvts.getRawParameterValue ("STAGES")->load());
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

    // Apply full params after prepare
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
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    dspProcessor.updateParameters (loadParams (apvts));
    dspProcessor.processBlock(buffer);

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
