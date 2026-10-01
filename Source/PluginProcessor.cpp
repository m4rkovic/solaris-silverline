#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Silverline/AmpModels/Silverline68Amp.h"
#include "../SolarisCore/Presets/PresetState.h"
#include <array>

namespace
{
    float rawParameter(const juce::AudioProcessorValueTreeState& state, const juce::String& id, float fallback=0.0f) noexcept
    {
        if(const auto* value=state.getRawParameterValue(id)) return value->load();
        return fallback;
    }

    bool rawBool(const juce::AudioProcessorValueTreeState& state, const juce::String& id, bool fallback=false) noexcept
    {
        return rawParameter(state,id,fallback?1.0f:0.0f)>=0.5f;
    }
}

SolarisSilverlineAudioProcessor::SolarisSilverlineAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"PARAMETERS",createParameterLayout())
{
    ampRegistry.registerModel(std::make_unique<solaris::Silverline68Amp>());
    ampRegistry.select("silverline68");
    cabinetEngine.setCabinetModelId("silverline-default");
}

void SolarisSilverlineAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock)
{
    solaris::AmpPrepareSpec ampSpec; ampSpec.sampleRate=sampleRate; ampSpec.maximumBlockSize=static_cast<juce::uint32>(samplesPerBlock); ampSpec.numChannels=static_cast<juce::uint32>(getTotalNumOutputChannels()); ampRegistry.prepare(ampSpec);
    juce::dsp::ProcessSpec dspSpec{sampleRate,static_cast<juce::uint32>(samplesPerBlock),static_cast<juce::uint32>(getTotalNumOutputChannels())};
    cabinetEngine.prepare(dspSpec); postEq.prepare(sampleRate); tunerEngine.prepare(sampleRate); syncPostEqParameters(); setLatencySamples(cabinetEngine.getLatencySamples());
}

void SolarisSilverlineAudioProcessor::releaseResources(){ tunerEngine.stop(); cabinetEngine.reset(); postEq.reset(); ampRegistry.reset(); }

#ifndef JucePlugin_PreferredChannelConfigurations
bool SolarisSilverlineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output=layouts.getMainOutputChannelSet();
    if(output!=juce::AudioChannelSet::mono()&&output!=juce::AudioChannelSet::stereo())return false;
    return output==layouts.getMainInputChannelSet();
}
#endif

void SolarisSilverlineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for(auto ch=getTotalNumInputChannels();ch<getTotalNumOutputChannels();++ch)buffer.clear(ch,0,buffer.getNumSamples());
    tunerEngine.pushSamples(buffer);
    buffer.applyGain(juce::Decibels::decibelsToGain(rawParameter(parameters,"inputGain")));
    ampRegistry.process(buffer);
    cabinetEngine.process(buffer);
    syncPostEqParameters();
    postEq.process(buffer);
    buffer.applyGain(juce::Decibels::decibelsToGain(rawParameter(parameters,"outputGain")));
}

void SolarisSilverlineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    syncPostEqParameters();
    const auto* selected=ampRegistry.current();
    const auto ampId=selected!=nullptr?juce::String(selected->metadata().id):juce::String("silverline68");
    const auto state=solaris::PresetState::create(parameters,ampId,cabinetEngine.createState(),postEq.createState());
    if(auto xml=state.createXml())copyXmlToBinary(*xml,destData);
}

void SolarisSilverlineAudioProcessor::setStateInformation(const void* data,int sizeInBytes)
{
    if(auto xml=getXmlFromBinary(data,sizeInBytes))
    {
        const auto state=solaris::PresetState::normalise(juce::ValueTree::fromXml(*xml),parameters.state.getType());
        if(!state.isValid())return;
        const auto parameterState=solaris::PresetState::parameterState(state,parameters.state.getType());
        if(parameterState.isValid())parameters.replaceState(parameterState.createCopy());
        ampRegistry.select(solaris::PresetState::ampModelId(state).toStdString());
        cabinetEngine.restoreState(solaris::PresetState::cabinetState(state));
        syncPostEqParameters();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SolarisSilverlineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGain",1},"Input Gain",juce::NormalisableRange<float>{-24.0f,24.0f,0.1f},0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"outputGain",1},"Output Gain",juce::NormalisableRange<float>{-24.0f,24.0f,0.1f},0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"eqHpfBypass",1},"EQ HPF Bypass",true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"eqHpfFrequency",1},"EQ HPF Frequency",juce::NormalisableRange<float>{20.0f,1000.0f,1.0f,0.35f},70.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"eqLpfBypass",1},"EQ LPF Bypass",true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"eqLpfFrequency",1},"EQ LPF Frequency",juce::NormalisableRange<float>{1000.0f,22000.0f,1.0f,0.35f},18000.0f));
    const std::array<float,4> defaults{100.0f,400.0f,1600.0f,6400.0f};
    for(int i=0;i<4;++i)
    {
        const auto number=juce::String(i+1); const auto prefix=juce::String("eqBand")+number;
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix+"Bypass",1},"EQ Band "+number+" Bypass",true));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Frequency",1},"EQ Band "+number+" Frequency",juce::NormalisableRange<float>{20.0f,22000.0f,1.0f,0.3f},defaults[static_cast<std::size_t>(i)]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Gain",1},"EQ Band "+number+" Gain",juce::NormalisableRange<float>{-18.0f,18.0f,0.1f},0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Q",1},"EQ Band "+number+" Q",juce::NormalisableRange<float>{0.1f,18.0f,0.01f,0.4f},0.707f));
    }
    return {params.begin(),params.end()};
}

void SolarisSilverlineAudioProcessor::syncPostEqParameters() noexcept
{
    postEq.setHighPass(rawParameter(parameters,"eqHpfFrequency",70.0f),rawBool(parameters,"eqHpfBypass",true));
    postEq.setLowPass(rawParameter(parameters,"eqLpfFrequency",18000.0f),rawBool(parameters,"eqLpfBypass",true));
    for(int i=0;i<4;++i)
    {
        const auto prefix=juce::String("eqBand")+juce::String(i+1);
        postEq.setBand(i,rawParameter(parameters,prefix+"Frequency",1000.0f),rawParameter(parameters,prefix+"Gain",0.0f),rawParameter(parameters,prefix+"Q",0.707f),rawBool(parameters,prefix+"Bypass",true));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new SolarisSilverlineAudioProcessor(); }
