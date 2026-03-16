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

static DSP::Core::PneumaticaParams loadParams (juce::AudioProcessorValueTreeState& apvts)
{
    DSP::Core::PneumaticaParams p;
    p.inputGainDb   = apvts.getRawParameterValue ("INPUT_GAIN")->load();
    p.outputGainDb  = apvts.getRawParameterValue ("OUTPUT_GAIN")->load();
    p.mixPercent    = apvts.getRawParameterValue ("MIX")->load();
    p.crossover     = apvts.getRawParameterValue ("CROSSOVER")->load();
    p.width         = apvts.getRawParameterValue ("WIDTH")->load();
    p.crunch        = apvts.getRawParameterValue ("CRUNCH")->load();
    p.crunchType    = static_cast<int> (apvts.getRawParameterValue ("CRUNCH_TYPE")->load());
    p.noiseLevel    = apvts.getRawParameterValue ("NOISE_LEVEL")->load();
    p.noiseType     = static_cast<int> (apvts.getRawParameterValue ("NOISE_TYPE")->load());
    p.noiseFilter   = apvts.getRawParameterValue ("NOISE_FILTER")->load();
    p.airFreq       = apvts.getRawParameterValue ("AIR_FREQ")->load();
    p.airGain       = apvts.getRawParameterValue ("AIR_GAIN")->load();
    p.shimmer       = apvts.getRawParameterValue ("SHIMMER")->load();
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
    return new PluginEditor (*this);
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
