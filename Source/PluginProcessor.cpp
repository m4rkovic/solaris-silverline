#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Silverline/AmpModels/Silverline68Amp.h"
#include "../SolarisCore/Amp/NeuralAmpModel.h"
#include "../SolarisCore/Presets/PresetState.h"
#include <cmath>

namespace
{
    float loadParameter(const std::atomic<float>* value, float fallback = 0.0f) noexcept
    {
        return value != nullptr ? value->load(std::memory_order_relaxed) : fallback;
    }

    bool loadBool(const std::atomic<float>* value, bool fallback = false) noexcept
    {
        return loadParameter(value, fallback ? 1.0f : 0.0f) >= 0.5f;
    }
}

SolarisSilverlineAudioProcessor::SolarisSilverlineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    ampRegistry.registerModelType<solaris::Silverline68Amp>();
    ampRegistry.registerModelType<solaris::NeuralAmpModel>();
    ampRegistry.select("silverline68");
    cabinetEngine.setCabinetModelId("silverline-2x10");
    cacheParameterPointers();
}

SolarisSilverlineAudioProcessor::~SolarisSilverlineAudioProcessor()
{
    neuralModelLoader.shutdown();
}

void SolarisSilverlineAudioProcessor::cacheParameterPointers()
{
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
    cabWetParameter = parameters.getRawParameterValue("cabWet");
    cabMicBlendParameter = parameters.getRawParameterValue("cabMicBlend");
    cabPhaseBParameter = parameters.getRawParameterValue("cabPhaseB");
    eqHpfBypassParameter = parameters.getRawParameterValue("eqHpfBypass");
    eqHpfFrequencyParameter = parameters.getRawParameterValue("eqHpfFrequency");
    eqLpfBypassParameter = parameters.getRawParameterValue("eqLpfBypass");
    eqLpfFrequencyParameter = parameters.getRawParameterValue("eqLpfFrequency");

    for (int i = 0; i < solaris::ParametricEQ::numBands; ++i)
    {
        const auto prefix = juce::String("eqBand") + juce::String(i + 1);
        auto& band = eqBandParameters[static_cast<std::size_t>(i)];
        band.bypass = parameters.getRawParameterValue(prefix + "Bypass");
        band.frequency = parameters.getRawParameterValue(prefix + "Frequency");
        band.gain = parameters.getRawParameterValue(prefix + "Gain");
        band.q = parameters.getRawParameterValue(prefix + "Q");
    }
}

void SolarisSilverlineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec dspSpec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), channels };

    inputGainStage.prepare(dspSpec);
    inputGainStage.setRampDurationSeconds(0.020);
    inputGainStage.setGainDecibels(loadParameter(inputGainParameter));
    preFxStage.prepare(dspSpec);

    lastAmpSpec.sampleRate = sampleRate;
    lastAmpSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    lastAmpSpec.numChannels = channels;
    ampRegistry.prepare(lastAmpSpec);
    ampRegistry.setParameters(readAmpParameters());

    cabinetEngine.prepare(dspSpec);
    syncCabParameters();
    postFxStage.prepare(dspSpec);
    postEq.prepare(sampleRate);
    syncPostEqParameters();
    tunerEngine.prepare(sampleRate);

    outputGainStage.prepare(dspSpec);
    outputGainStage.setRampDurationSeconds(0.020);
    outputGainStage.setGainDecibels(loadParameter(outputGainParameter));

    setLatencySamples(cabinetEngine.getLatencySamples());
    prepared = true;
    scheduleDesiredNeuralModel();
}

void SolarisSilverlineAudioProcessor::releaseResources()
{
    prepared = false;
    neuralModelLoader.cancelPending();
    tunerEngine.stop();
    inputGainStage.reset();
    preFxStage.reset();
    ampRegistry.reset();
    cabinetEngine.reset();
    postFxStage.reset();
    postEq.reset();
    outputGainStage.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SolarisSilverlineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;
    return output == layouts.getMainInputChannelSet();
}
#endif

solaris::AmpParameters SolarisSilverlineAudioProcessor::readAmpParameters() const noexcept
{
    solaris::AmpParameters result;
    result.enabled = loadBool(ampEnabledParameter, true);
    result.channel = loadParameter(ampChannelParameter) >= 0.5f ? solaris::AmpChannel::vintage : solaris::AmpChannel::custom;
    result.volume = loadParameter(ampVolumeParameter, 4.5f) * 0.1f;
    result.bass = loadParameter(ampBassParameter, 5.0f) * 0.1f;
    result.treble = loadParameter(ampTrebleParameter, 5.5f) * 0.1f;
    result.reverb = loadParameter(ampReverbParameter, 2.0f) * 0.1f;
    result.tremoloSpeedHz = loadParameter(ampTremoloSpeedParameter, 4.0f);
    result.tremoloIntensity = loadParameter(ampTremoloIntensityParameter) * 0.1f;
    result.clampToValidRange();
    return result;
}

void SolarisSilverlineAudioProcessor::syncCabParameters() noexcept
{
    cabinetEngine.setWetMix(loadParameter(cabWetParameter, 100.0f) * 0.01f);
    cabinetEngine.setMicBlend(loadParameter(cabMicBlendParameter, 50.0f) * 0.01f);
    cabinetEngine.setPhaseInverted(solaris::CabinetIRSlot::micB, loadBool(cabPhaseBParameter, false));
}

void SolarisSilverlineAudioProcessor::syncPostEqParameters() noexcept
{
    postEq.setHighPass(loadParameter(eqHpfFrequencyParameter, 70.0f), loadBool(eqHpfBypassParameter, false));
    postEq.setLowPass(loadParameter(eqLpfFrequencyParameter, 18000.0f), loadBool(eqLpfBypassParameter, false));
    for (int i = 0; i < solaris::ParametricEQ::numBands; ++i)
    {
        const auto& band = eqBandParameters[static_cast<std::size_t>(i)];
        postEq.setBand(i,
                       loadParameter(band.frequency, 1000.0f),
                       loadParameter(band.gain, 0.0f),
                       loadParameter(band.q, 0.707f),
                       loadBool(band.bypass, false));
    }
}

void SolarisSilverlineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    tunerEngine.pushSamples(buffer);
    inputGainStage.setGainDecibels(loadParameter(inputGainParameter));
    outputGainStage.setGainDecibels(loadParameter(outputGainParameter));
    ampRegistry.setParameters(readAmpParameters());
    syncCabParameters();
    syncPostEqParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    inputGainStage.process(context);
    preFxStage.process(buffer);
    ampRegistry.process(buffer);
    cabinetEngine.process(buffer);
    postFxStage.process(buffer);
    postEq.process(buffer);
    outputGainStage.process(context);

    if (tunerMuted.load(std::memory_order_relaxed))
        buffer.clear();
}

bool SolarisSilverlineAudioProcessor::requestNeuralAmpModelLoad(const juce::File& modelFile, bool rememberRecent)
{
   #if !SOLARIS_ENABLE_NEURAL_AUDIO
    juce::ignoreUnused(modelFile, rememberRecent);
    return false;
   #else
    const auto path = modelFile.getFullPathName();
    if (path.isEmpty())
        return false;

    {
        const juce::ScopedLock lock(ampStateLock);
        desiredAmpBackend = "neural-nam";
        desiredNeuralPath = path;
        if (rememberRecent)
        {
            recentNeuralModels.removeString(path);
            recentNeuralModels.insert(0, path);
            while (recentNeuralModels.size() > 8)
                recentNeuralModels.remove(recentNeuralModels.size() - 1);
        }
    }

    scheduleDesiredNeuralModel();
    return true;
   #endif
}

void SolarisSilverlineAudioProcessor::scheduleDesiredNeuralModel()
{
   #if SOLARIS_ENABLE_NEURAL_AUDIO
    juce::String path;
    {
        const juce::ScopedLock lock(ampStateLock);
        if (desiredAmpBackend != "neural-nam" || desiredNeuralPath.isEmpty())
            return;
        path = desiredNeuralPath;
    }

    const auto spec = lastAmpSpec;
    const auto params = readAmpParameters();
    neuralModelLoader.request(juce::File(path), spec, params,
        [this](std::unique_ptr<solaris::NeuralAmpModel> candidate, const solaris::NeuralLoadStatus& status)
        {
            completeNeuralModelLoad(std::move(candidate), status);
        });
   #endif
}

void SolarisSilverlineAudioProcessor::completeNeuralModelLoad(std::unique_ptr<solaris::NeuralAmpModel> candidate,
                                                               const solaris::NeuralLoadStatus& status)
{
    if (candidate == nullptr || status.state != solaris::NeuralLoadState::loaded)
        return;

    {
        const juce::ScopedLock lock(ampStateLock);
        if (desiredAmpBackend != "neural-nam" || desiredNeuralPath != status.requestedPath)
            return;
    }

    std::unique_ptr<solaris::IAmpModel> replacement = std::move(candidate);
    {
        const juce::ScopedLock callbackLock(getCallbackLock());
        ampRegistry.swapSelected(replacement);
    }
}

bool SolarisSilverlineAudioProcessor::useAnalogueAmp()
{
    neuralModelLoader.cancelPending();
    auto candidate = std::make_unique<solaris::Silverline68Amp>();
    if (prepared)
        candidate->prepare(lastAmpSpec);
    candidate->setParameters(readAmpParameters());

    std::unique_ptr<solaris::IAmpModel> replacement = std::move(candidate);
    {
        const juce::ScopedLock callbackLock(getCallbackLock());
        ampRegistry.swapSelected(replacement);
    }
    {
        const juce::ScopedLock lock(ampStateLock);
        desiredAmpBackend = "silverline68";
    }
    return true;
}

juce::String SolarisSilverlineAudioProcessor::getActiveAmpModelId() const
{
    const juce::ScopedLock lock(ampStateLock);
    return desiredAmpBackend;
}

juce::StringArray SolarisSilverlineAudioProcessor::getRecentNeuralModels() const
{
    const juce::ScopedLock lock(ampStateLock);
    return recentNeuralModels;
}

juce::ValueTree SolarisSilverlineAudioProcessor::createAmpState() const
{
    juce::ValueTree amp("AMP_STATE");
    const juce::ScopedLock lock(ampStateLock);
    amp.setProperty("backendId", desiredAmpBackend, nullptr);
    amp.setProperty("neuralModelPath", desiredNeuralPath, nullptr);
    juce::ValueTree recent("RECENT_MODELS");
    for (const auto& path : recentNeuralModels)
    {
        juce::ValueTree item("MODEL");
        item.setProperty("path", path, nullptr);
        recent.addChild(item, -1, nullptr);
    }
    amp.addChild(recent, -1, nullptr);
    return amp;
}

void SolarisSilverlineAudioProcessor::restoreAmpStateMetadata(const juce::ValueTree& amp)
{
    const juce::ScopedLock lock(ampStateLock);
    desiredAmpBackend = amp.getProperty("backendId", "silverline68").toString();
    desiredNeuralPath = amp.getProperty("neuralModelPath").toString();
    recentNeuralModels.clear();
    const auto recent = amp.getChildWithName("RECENT_MODELS");
    for (int i = 0; i < recent.getNumChildren() && recentNeuralModels.size() < 8; ++i)
    {
        const auto path = recent.getChild(i).getProperty("path").toString();
        if (path.isNotEmpty() && !recentNeuralModels.contains(path))
            recentNeuralModels.add(path);
    }
    if (desiredNeuralPath.isNotEmpty() && !recentNeuralModels.contains(desiredNeuralPath))
        recentNeuralModels.insert(0, desiredNeuralPath);
}

juce::ValueTree SolarisSilverlineAudioProcessor::capturePresetState()
{
    return solaris::PresetState::create(parameters,
                                        getActiveAmpModelId(),
                                        cabinetEngine.createState(),
                                        postEq.createState(),
                                        {},
                                        createAmpState());
}

void SolarisSilverlineAudioProcessor::applyPresetState(const juce::ValueTree& incoming)
{
    const auto state = solaris::PresetState::normalise(incoming, parameters.state.getType());
    if (!state.isValid())
        return;

    const auto parameterState = solaris::PresetState::parameterState(state, parameters.state.getType());
    if (parameterState.isValid())
        parameters.replaceState(parameterState.createCopy());

    cabinetEngine.restoreState(solaris::PresetState::cabinetState(state));
    postEq.restoreState(solaris::PresetState::eqState(state));

    auto amp = solaris::PresetState::ampState(state);
    if (!amp.isValid())
    {
        amp = juce::ValueTree("AMP_STATE");
        amp.setProperty("backendId", solaris::PresetState::ampModelId(state), nullptr);
        amp.setProperty("neuralModelPath", solaris::PresetState::neuralModelPath(state), nullptr);
    }
    restoreAmpStateMetadata(amp);

    const auto requestedAmp = solaris::PresetState::ampModelId(state);
    if (requestedAmp == "neural-nam")
    {
        // Keep a deterministic analogue path alive while the NAM worker validates/restores.
        auto fallback = std::make_unique<solaris::Silverline68Amp>();
        if (prepared)
            fallback->prepare(lastAmpSpec);
        fallback->setParameters(readAmpParameters());
        std::unique_ptr<solaris::IAmpModel> replacement = std::move(fallback);
        {
            const juce::ScopedLock callbackLock(getCallbackLock());
            ampRegistry.swapSelected(replacement);
        }
        scheduleDesiredNeuralModel();
    }
    else
    {
        useAnalogueAmp();
    }

    syncCabParameters();
    syncPostEqParameters();
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
    const auto state = capturePresetState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SolarisSilverlineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        applyPresetState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout SolarisSilverlineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::inputGain, 1}, "Input Gain", juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f}, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::outputGain, 1}, "Output Gain", juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f}, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{solaris::ParameterIDs::ampEnabled, 1}, "Amp Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{solaris::ParameterIDs::ampChannel, 1}, "Channel", juce::StringArray{"Custom", "Vintage"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampVolume, 1}, "Volume", juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 4.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampBass, 1}, "Bass", juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampTreble, 1}, "Treble", juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampReverb, 1}, "Reverb", juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 2.0f));

    auto tremoloSpeedRange = juce::NormalisableRange<float>{0.5f, 12.0f, 0.01f};
    tremoloSpeedRange.setSkewForCentre(4.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampTremoloSpeed, 1}, "Tremolo Speed", tremoloSpeedRange, 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{solaris::ParameterIDs::ampTremoloIntensity, 1}, "Tremolo Intensity", juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cabWet", 1}, "Cab Wet", juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cabMicBlend", 1}, "Cab Mic B Blend", juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"cabPhaseB", 1}, "Cab Mic B Phase", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"eqHpfBypass", 1}, "EQ HPF Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"eqHpfFrequency", 1}, "EQ HPF Frequency", juce::NormalisableRange<float>{20.0f, 1000.0f, 1.0f, 0.35f}, 70.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"eqLpfBypass", 1}, "EQ LPF Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"eqLpfFrequency", 1}, "EQ LPF Frequency", juce::NormalisableRange<float>{1000.0f, 22000.0f, 1.0f, 0.35f}, 18000.0f));

    const std::array<float, 4> defaults { 100.0f, 400.0f, 1600.0f, 6400.0f };
    for (int i = 0; i < 4; ++i)
    {
        const auto number = juce::String(i + 1);
        const auto prefix = juce::String("eqBand") + number;
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix + "Bypass", 1}, "EQ Band " + number + " Bypass", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix + "Frequency", 1}, "EQ Band " + number + " Frequency", juce::NormalisableRange<float>{20.0f, 22000.0f, 1.0f, 0.3f}, defaults[static_cast<std::size_t>(i)]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix + "Gain", 1}, "EQ Band " + number + " Gain", juce::NormalisableRange<float>{-18.0f, 18.0f, 0.1f}, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix + "Q", 1}, "EQ Band " + number + " Q", juce::NormalisableRange<float>{0.1f, 18.0f, 0.01f, 0.4f}, 0.707f));
    }

    return {params.begin(), params.end()};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolarisSilverlineAudioProcessor();
}
