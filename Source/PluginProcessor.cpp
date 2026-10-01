#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Silverline/AmpModels/Silverline68Amp.h"

SolarisSilverlineAudioProcessor::SolarisSilverlineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Silverline 68 is the first amp, but the registry is intentionally generic.
    ampRegistry.registerModel(std::make_unique<solaris::Silverline68Amp>());
    ampRegistry.select("silverline68");
}

void SolarisSilverlineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    solaris::AmpPrepareSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    ampRegistry.prepare(spec);
}

void SolarisSilverlineAudioProcessor::releaseResources()
{
    ampRegistry.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SolarisSilverlineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void SolarisSilverlineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    // V0.1 intentionally behaves as transparent passthrough.
    // The amp engine is already modular and ready for real DSP in the next milestone.
    ampRegistry.process(buffer);
}

void SolarisSilverlineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SolarisSilverlineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
SolarisSilverlineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputGain", 1},
        "Input Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f},
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1},
        "Output Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f},
        0.0f));

    return {params.begin(), params.end()};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolarisSilverlineAudioProcessor();
}
