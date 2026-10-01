#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Silverline/AmpModels/Silverline68Amp.h"
#include <cmath>

SolarisSilverlineAudioProcessor::SolarisSilverlineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    ampRegistry.registerModelType<solaris::Silverline68Amp>();
    ampRegistry.select("silverline68");

    inputGainParameter = parameters.getRawParameterValue(solaris::ParameterIDs::inputGain);
    outputGainParameter = parameters.getRawParameterValue(solaris::ParameterIDs::outputGain);
    ampEnabledParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampEnabled);
    ampChannelParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampChannel);
    ampVolumeParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampVolume);
    ampBassParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampBass);
    ampTrebleParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampTreble);
    ampReverbParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampReverb);
    ampTremoloSpeedParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampTremoloSpeed);
    ampTremoloIntensityParameter = parameters.getRawParameterValue(solaris::ParameterIDs::ampTremoloIntensity);

    jassert(inputGainParameter != nullptr && outputGainParameter != nullptr);
    jassert(ampEnabledParameter != nullptr && ampChannelParameter != nullptr);
    jassert(ampVolumeParameter != nullptr && ampBassParameter != nullptr && ampTrebleParameter != nullptr);
    jassert(ampReverbParameter != nullptr && ampTremoloSpeedParameter != nullptr
            && ampTremoloIntensityParameter != nullptr);
}

void SolarisSilverlineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec processSpec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        numChannels
    };

    inputGainStage.prepare(processSpec);
    inputGainStage.setRampDurationSeconds(0.020);
    inputGainStage.setGainDecibels(inputGainParameter->load(std::memory_order_relaxed));

    preFxStage.prepare(processSpec);

    solaris::AmpPrepareSpec ampSpec;
    ampSpec.sampleRate = sampleRate;
    ampSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    ampSpec.numChannels = numChannels;
    ampRegistry.prepare(ampSpec);
    ampRegistry.setParameters(readAmpParameters());

    cabStage.prepare(processSpec);
    postFxStage.prepare(processSpec);
    eqStage.prepare(processSpec);

    outputGainStage.prepare(processSpec);
    outputGainStage.setRampDurationSeconds(0.020);
    outputGainStage.setGainDecibels(outputGainParameter->load(std::memory_order_relaxed));
}

void SolarisSilverlineAudioProcessor::releaseResources()
{
    inputGainStage.reset();
    preFxStage.reset();
    ampRegistry.reset();
    cabStage.reset();
    postFxStage.reset();
    eqStage.reset();
    outputGainStage.reset();
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

solaris::AmpParameters SolarisSilverlineAudioProcessor::readAmpParameters() const noexcept
{
    solaris::AmpParameters result;
    result.enabled = ampEnabledParameter->load(std::memory_order_relaxed) >= 0.5f;
    result.channel = ampChannelParameter->load(std::memory_order_relaxed) >= 0.5f
        ? solaris::AmpChannel::vintage
        : solaris::AmpChannel::custom;
    result.volume = ampVolumeParameter->load(std::memory_order_relaxed) * 0.1f;
    result.bass = ampBassParameter->load(std::memory_order_relaxed) * 0.1f;
    result.treble = ampTrebleParameter->load(std::memory_order_relaxed) * 0.1f;
    result.reverb = ampReverbParameter->load(std::memory_order_relaxed) * 0.1f;
    result.tremoloSpeedHz = ampTremoloSpeedParameter->load(std::memory_order_relaxed);
    result.tremoloIntensity = ampTremoloIntensityParameter->load(std::memory_order_relaxed) * 0.1f;
    result.clampToValidRange();
    return result;
}

void SolarisSilverlineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    inputGainStage.setGainDecibels(inputGainParameter->load(std::memory_order_relaxed));
    outputGainStage.setGainDecibels(outputGainParameter->load(std::memory_order_relaxed));
    ampRegistry.setParameters(readAmpParameters());

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Fixed shell order. Placeholder stages are intentionally transparent for V1.
    inputGainStage.process(context);
    preFxStage.process(buffer);
    ampRegistry.process(buffer);
    cabStage.process(buffer);
    postFxStage.process(buffer);
    eqStage.process(buffer);
    outputGainStage.process(context);
}

juce::AudioProcessorEditor* SolarisSilverlineAudioProcessor::createEditor()
{
    return new SolarisSilverlineAudioProcessorEditor(*this);
}

double SolarisSilverlineAudioProcessor::getTailLengthSeconds() const
{
    return ampRegistry.tailLengthSeconds();
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
        juce::ParameterID{solaris::ParameterIDs::inputGain, 1},
        "Input Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f},
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::outputGain, 1},
        "Output Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f},
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{solaris::ParameterIDs::ampEnabled, 1},
        "Amp Enabled",
        true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{solaris::ParameterIDs::ampChannel, 1},
        "Channel",
        juce::StringArray{"Custom", "Vintage"},
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampVolume, 1},
        "Volume",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f},
        4.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampBass, 1},
        "Bass",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f},
        5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTreble, 1},
        "Treble",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f},
        5.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampReverb, 1},
        "Reverb",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f},
        2.0f));

    auto tremoloSpeedRange = juce::NormalisableRange<float>{0.5f, 12.0f, 0.01f};
    tremoloSpeedRange.setSkewForCentre(4.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTremoloSpeed, 1},
        "Tremolo Speed",
        tremoloSpeedRange,
        4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTremoloIntensity, 1},
        "Tremolo Intensity",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f},
        0.0f));

    return {params.begin(), params.end()};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolarisSilverlineAudioProcessor();
}
