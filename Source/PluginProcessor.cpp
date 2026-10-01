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

    float measurePeakDb(const juce::AudioBuffer<float>& buffer) noexcept
    {
        float peak = 0.0f;
        const auto samples = buffer.getNumSamples();

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, samples));

        return juce::Decibels::gainToDecibels(peak, -72.0f);
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

    preFxChain.addEffect(preCompressor);
    preFxChain.addEffect(preOverdrive);
    preFxChain.addEffect(preDistortion);
    preFxChain.addEffect(preHardClip);
    preFxChain.addEffect(preFuzz);

    postFxChain.addEffect(postPhaser);
    postFxChain.addEffect(postChorus);
    postFxChain.addEffect(postTremolo);
    postFxChain.addEffect(postDelay);
    postFxChain.addEffect(postReverb);

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

    const auto cachePedal = [this](PedalPointers& target,
                                  const char* enabled,
                                  const char* first,
                                  const char* second,
                                  const char* third)
    {
        target.enabled = parameters.getRawParameterValue(enabled);
        target.first = parameters.getRawParameterValue(first);
        target.second = parameters.getRawParameterValue(second);
        target.third = parameters.getRawParameterValue(third);
    };

    cachePedal(prePedalParameters[0], solaris::ParameterIDs::preCompEnabled,
               solaris::ParameterIDs::preCompSustain, solaris::ParameterIDs::preCompAttack,
               solaris::ParameterIDs::preCompLevel);
    cachePedal(prePedalParameters[1], solaris::ParameterIDs::preDriveEnabled,
               solaris::ParameterIDs::preDriveDrive, solaris::ParameterIDs::preDriveTone,
               solaris::ParameterIDs::preDriveLevel);
    cachePedal(prePedalParameters[2], solaris::ParameterIDs::preDistEnabled,
               solaris::ParameterIDs::preDistGain, solaris::ParameterIDs::preDistContour,
               solaris::ParameterIDs::preDistLevel);
    cachePedal(prePedalParameters[3], solaris::ParameterIDs::preHardEnabled,
               solaris::ParameterIDs::preHardDistortion, solaris::ParameterIDs::preHardFilter,
               solaris::ParameterIDs::preHardLevel);
    cachePedal(prePedalParameters[4], solaris::ParameterIDs::preFuzzEnabled,
               solaris::ParameterIDs::preFuzzSustain, solaris::ParameterIDs::preFuzzTone,
               solaris::ParameterIDs::preFuzzLevel);

    cachePedal(postPedalParameters[0], solaris::ParameterIDs::postPhaserEnabled,
               solaris::ParameterIDs::postPhaserRate, solaris::ParameterIDs::postPhaserDepth,
               solaris::ParameterIDs::postPhaserMix);
    cachePedal(postPedalParameters[1], solaris::ParameterIDs::postChorusEnabled,
               solaris::ParameterIDs::postChorusRate, solaris::ParameterIDs::postChorusDepth,
               solaris::ParameterIDs::postChorusMix);
    cachePedal(postPedalParameters[2], solaris::ParameterIDs::postTremoloEnabled,
               solaris::ParameterIDs::postTremoloRate, solaris::ParameterIDs::postTremoloDepth,
               solaris::ParameterIDs::postTremoloShape);
    cachePedal(postPedalParameters[3], solaris::ParameterIDs::postDelayEnabled,
               solaris::ParameterIDs::postDelayTime, solaris::ParameterIDs::postDelayFeedback,
               solaris::ParameterIDs::postDelayMix);
    cachePedal(postPedalParameters[4], solaris::ParameterIDs::postReverbEnabled,
               solaris::ParameterIDs::postReverbDecay, solaris::ParameterIDs::postReverbTone,
               solaris::ParameterIDs::postReverbMix);

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

void SolarisSilverlineAudioProcessor::collapseGuitarInputToMono(juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() <= 0)
        return;

    if (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        // The official NAM standalone treats the guitar path as mono and catches
        // whichever physical input is actually carrying the instrument. Choosing
        // the hotter block avoids summing an unused interface input full of noise
        // and preserves level when the guitar is plugged into Input 1 or Input 2.
        const auto leftPeak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        const auto rightPeak = buffer.getMagnitude(1, 0, buffer.getNumSamples());
        const auto sourceChannel = rightPeak > leftPeak ? 1 : 0;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto mono = buffer.getSample(sourceChannel, sample);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, mono);
        }

        return;
    }

    // In a DAW a stereo source is collapsed conservatively, matching NAM's
    // mono-internal processing model without doubling correlated inputs.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto mono = 0.5f * (buffer.getSample(0, sample)
                                + buffer.getSample(1, sample));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, mono);
    }
}

void SolarisSilverlineAudioProcessor::resetOutputDcBlocker() noexcept
{
    outputDcPreviousInput.fill(0.0f);
    outputDcPreviousOutput.fill(0.0f);
}

void SolarisSilverlineAudioProcessor::applyOutputSafetyAndDcBlock(juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin(2, buffer.getNumChannels());

    for (int channel = 0; channel < channels; ++channel)
    {
        auto previousInput = outputDcPreviousInput[static_cast<std::size_t>(channel)];
        auto previousOutput = outputDcPreviousOutput[static_cast<std::size_t>(channel)];
        auto* samples = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto x = samples[sample];
            auto y = x - previousInput + outputDcCoefficient * previousOutput;
            previousInput = x;

            if (!std::isfinite(y))
                y = 0.0f;

            if (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
            {
                // A hardware output cannot represent samples beyond full scale.
                // NAM's standalone follows the same principle: protect the device
                // output instead of letting runaway DSP become digital crackle.
                y = juce::jlimit(-0.999f, 0.999f, y);
            }

            previousOutput = y;
            samples[sample] = y;
        }

        outputDcPreviousInput[static_cast<std::size_t>(channel)] = previousInput;
        outputDcPreviousOutput[static_cast<std::size_t>(channel)] = previousOutput;
    }
}

void SolarisSilverlineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec dspSpec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        channels
    };

    inputGainStage.prepare(dspSpec);
    inputGainStage.setRampDurationSeconds(0.020);
    inputGainStage.setGainDecibels(loadParameter(inputGainParameter));

    const solaris::EffectPrepareSpec effectSpec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        channels
    };

    preFxChain.prepare(effectSpec);
    postFxChain.prepare(effectSpec);
    syncEffectParameters();

    lastAmpSpec.sampleRate = sampleRate;
    lastAmpSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    lastAmpSpec.numChannels = channels;
    ampRegistry.prepare(lastAmpSpec);
    ampRegistry.setParameters(readAmpParameters());

    cabinetEngine.prepare(dspSpec);
    syncCabParameters();

    postEq.prepare(sampleRate);
    syncPostEqParameters();

    tunerEngine.prepare(sampleRate);

    outputGainStage.prepare(dspSpec);
    outputGainStage.setRampDurationSeconds(0.020);
    outputGainStage.setGainDecibels(loadParameter(outputGainParameter));

    // Keep the baseline guitar path genuinely low-latency. Fully bypassed
    // effects are now true bypass and contribute no delay.
    setLatencySamples(cabinetEngine.getLatencySamples()
                      + preFxChain.latencySamples()
                      + postFxChain.latencySamples());

    outputDcCoefficient = static_cast<float>(
        std::exp(-2.0 * juce::MathConstants<double>::pi * 5.0 / juce::jmax(1.0, sampleRate)));
    resetOutputDcBlocker();
    prepared = true;
    scheduleDesiredNeuralModel();
}

void SolarisSilverlineAudioProcessor::releaseResources()
{
    prepared = false;
    neuralModelLoader.cancelPending();
    tunerEngine.stop();
    inputGainStage.reset();
    preFxChain.reset();
    ampRegistry.reset();
    cabinetEngine.reset();
    postFxChain.reset();
    postEq.reset();
    outputGainStage.reset();
    resetOutputDcBlocker();
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
    result.channel = loadParameter(ampChannelParameter) >= 0.5f
        ? solaris::AmpChannel::vintage
        : solaris::AmpChannel::custom;
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
    cabinetEngine.setPhaseInverted(solaris::CabinetIRSlot::micB,
                                   loadBool(cabPhaseBParameter, false));
}

void SolarisSilverlineAudioProcessor::syncEffectParameters() noexcept
{
    const auto normalized = [](const std::atomic<float>* value, float fallback = 5.0f) noexcept
    {
        return juce::jlimit(0.0f, 1.0f, loadParameter(value, fallback) * 0.1f);
    };

    preCompressor.setBypassed(!loadBool(prePedalParameters[0].enabled));
    preCompressor.setSustain(normalized(prePedalParameters[0].first, 4.5f));
    preCompressor.setAttack(normalized(prePedalParameters[0].second, 3.5f));
    preCompressor.setLevel(normalized(prePedalParameters[0].third, 5.0f));

    preOverdrive.setBypassed(!loadBool(prePedalParameters[1].enabled));
    preOverdrive.setDrive(normalized(prePedalParameters[1].first, 3.5f));
    preOverdrive.setTone(normalized(prePedalParameters[1].second, 5.0f));
    preOverdrive.setLevel(normalized(prePedalParameters[1].third, 5.0f));

    preDistortion.setBypassed(!loadBool(prePedalParameters[2].enabled));
    preDistortion.setGain(normalized(prePedalParameters[2].first, 4.5f));
    preDistortion.setContour(normalized(prePedalParameters[2].second, 5.0f));
    preDistortion.setLevel(normalized(prePedalParameters[2].third, 5.0f));

    preHardClip.setBypassed(!loadBool(prePedalParameters[3].enabled));
    preHardClip.setDistortion(normalized(prePedalParameters[3].first, 4.5f));
    preHardClip.setFilter(normalized(prePedalParameters[3].second, 4.5f));
    preHardClip.setLevel(normalized(prePedalParameters[3].third, 5.0f));

    preFuzz.setBypassed(!loadBool(prePedalParameters[4].enabled));
    preFuzz.setSustain(normalized(prePedalParameters[4].first, 5.5f));
    preFuzz.setTone(normalized(prePedalParameters[4].second, 5.0f));
    preFuzz.setLevel(normalized(prePedalParameters[4].third, 5.0f));

    postPhaser.setBypassed(!loadBool(postPedalParameters[0].enabled));
    postPhaser.setRate(normalized(postPedalParameters[0].first, 3.5f));
    postPhaser.setDepth(normalized(postPedalParameters[0].second, 5.5f));
    postPhaser.setMix(normalized(postPedalParameters[0].third, 5.0f));

    postChorus.setBypassed(!loadBool(postPedalParameters[1].enabled));
    postChorus.setRate(normalized(postPedalParameters[1].first, 3.5f));
    postChorus.setDepth(normalized(postPedalParameters[1].second, 4.5f));
    postChorus.setMix(normalized(postPedalParameters[1].third, 4.5f));

    postTremolo.setBypassed(!loadBool(postPedalParameters[2].enabled));
    postTremolo.setRate(normalized(postPedalParameters[2].first, 3.5f));
    postTremolo.setDepth(normalized(postPedalParameters[2].second, 4.5f));
    postTremolo.setShape(normalized(postPedalParameters[2].third, 2.5f));

    postDelay.setBypassed(!loadBool(postPedalParameters[3].enabled));
    postDelay.setTime(normalized(postPedalParameters[3].first, 3.8f));
    postDelay.setFeedback(normalized(postPedalParameters[3].second, 3.5f));
    postDelay.setMix(normalized(postPedalParameters[3].third, 3.2f));

    postReverb.setBypassed(!loadBool(postPedalParameters[4].enabled));
    postReverb.setDecay(normalized(postPedalParameters[4].first, 4.6f));
    postReverb.setTone(normalized(postPedalParameters[4].second, 5.2f));
    postReverb.setMix(normalized(postPedalParameters[4].third, 2.8f));
}

void SolarisSilverlineAudioProcessor::syncPostEqParameters() noexcept
{
    postEq.setHighPass(loadParameter(eqHpfFrequencyParameter, 70.0f),
                       loadBool(eqHpfBypassParameter, false));
    postEq.setLowPass(loadParameter(eqLpfFrequencyParameter, 18000.0f),
                      loadBool(eqLpfBypassParameter, false));

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

void SolarisSilverlineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    collapseGuitarInputToMono(buffer);

    const auto inputPeak = measurePeakDb(buffer);
    inputPeakDb.store(inputPeak, std::memory_order_relaxed);
    if (inputPeak >= -0.01f)
        inputClip.store(true, std::memory_order_relaxed);

    tunerEngine.pushSamples(buffer);

    inputGainStage.setGainDecibels(loadParameter(inputGainParameter));
    outputGainStage.setGainDecibels(loadParameter(outputGainParameter));
    ampRegistry.setParameters(readAmpParameters());
    syncCabParameters();
    syncEffectParameters();
    syncPostEqParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    inputGainStage.process(context);
    preFxChain.process(buffer);
    ampRegistry.process(buffer);
    cabinetEngine.process(buffer);
    postFxChain.process(buffer);
    postEq.process(buffer);
    outputGainStage.process(context);
    applyOutputSafetyAndDcBlock(buffer);

    if (tunerMuted.load(std::memory_order_relaxed))
        buffer.clear();

    const auto outputPeak = measurePeakDb(buffer);
    outputPeakDb.store(outputPeak, std::memory_order_relaxed);
    if (outputPeak >= -0.01f)
        outputClip.store(true, std::memory_order_relaxed);
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
    if (!prepared)
        return;

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
        const juce::ScopedLock hostCallbackGuard(getCallbackLock());
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
        const juce::ScopedLock hostCallbackGuard(getCallbackLock());
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
    juce::ValueTree pedalState("PEDALS");
    pedalState.addChild(preFxChain.createState("PRE"), -1, nullptr);
    pedalState.addChild(postFxChain.createState("POST"), -1, nullptr);

    return solaris::PresetState::create(parameters,
                                        getActiveAmpModelId(),
                                        cabinetEngine.createState(),
                                        postEq.createState(),
                                        pedalState,
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

    const auto pedalState = solaris::PresetState::pedalState(state);
    if (pedalState.isValid())
    {
        preFxChain.restoreState(pedalState.getChildWithName("PRE"));
        postFxChain.restoreState(pedalState.getChildWithName("POST"));
    }

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
        auto fallback = std::make_unique<solaris::Silverline68Amp>();
        if (prepared)
            fallback->prepare(lastAmpSpec);
        fallback->setParameters(readAmpParameters());

        std::unique_ptr<solaris::IAmpModel> replacement = std::move(fallback);
        {
            const juce::ScopedLock hostCallbackGuard(getCallbackLock());
            ampRegistry.swapSelected(replacement);
        }
        scheduleDesiredNeuralModel();
    }
    else
    {
        useAnalogueAmp();
    }

    syncCabParameters();
    syncEffectParameters();
    syncPostEqParameters();
}

juce::AudioProcessorEditor* SolarisSilverlineAudioProcessor::createEditor()
{
    return new SolarisSilverlineAudioProcessorEditor(*this);
}

double SolarisSilverlineAudioProcessor::getTailLengthSeconds() const
{
    return ampRegistry.tailLengthSeconds() + postFxChain.tailLengthSeconds();
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

juce::AudioProcessorValueTreeState::ParameterLayout
SolarisSilverlineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::inputGain, 1}, "Input Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f}, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::outputGain, 1}, "Output Gain",
        juce::NormalisableRange<float>{-24.0f, 24.0f, 0.1f}, -6.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{solaris::ParameterIDs::ampEnabled, 1}, "Amp Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{solaris::ParameterIDs::ampChannel, 1}, "Channel",
        juce::StringArray{"Custom", "Vintage"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampVolume, 1}, "Volume",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 3.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampBass, 1}, "Bass",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTreble, 1}, "Treble",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampReverb, 1}, "Reverb",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 0.0f));

    auto tremoloSpeedRange = juce::NormalisableRange<float>{0.5f, 12.0f, 0.01f};
    tremoloSpeedRange.setSkewForCentre(4.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTremoloSpeed, 1}, "Tremolo Speed",
        tremoloSpeedRange, 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{solaris::ParameterIDs::ampTremoloIntensity, 1}, "Tremolo Intensity",
        juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, 0.0f));


    const auto addPedal = [&params](const char* enabledId,
                                    const juce::String& displayName,
                                    const char* firstId,
                                    const juce::String& firstName,
                                    float firstDefault,
                                    const char* secondId,
                                    const juce::String& secondName,
                                    float secondDefault,
                                    const char* thirdId,
                                    const juce::String& thirdName,
                                    float thirdDefault)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{enabledId, 1}, displayName + " Enabled", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{firstId, 1}, displayName + " " + firstName,
            juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, firstDefault));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{secondId, 1}, displayName + " " + secondName,
            juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, secondDefault));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{thirdId, 1}, displayName + " " + thirdName,
            juce::NormalisableRange<float>{0.0f, 10.0f, 0.01f}, thirdDefault));
    };

    addPedal(solaris::ParameterIDs::preCompEnabled, "Leveler",
             solaris::ParameterIDs::preCompSustain, "Sustain", 4.5f,
             solaris::ParameterIDs::preCompAttack, "Attack", 3.5f,
             solaris::ParameterIDs::preCompLevel, "Level", 5.0f);
    addPedal(solaris::ParameterIDs::preDriveEnabled, "Turbo Drive",
             solaris::ParameterIDs::preDriveDrive, "Drive", 3.5f,
             solaris::ParameterIDs::preDriveTone, "Tone", 5.0f,
             solaris::ParameterIDs::preDriveLevel, "Level", 5.0f);
    addPedal(solaris::ParameterIDs::preDistEnabled, "Badland Dist",
             solaris::ParameterIDs::preDistGain, "Gain", 4.5f,
             solaris::ParameterIDs::preDistContour, "Contour", 5.0f,
             solaris::ParameterIDs::preDistLevel, "Level", 5.0f);
    addPedal(solaris::ParameterIDs::preHardEnabled, "Vermin Drive",
             solaris::ParameterIDs::preHardDistortion, "Distortion", 4.5f,
             solaris::ParameterIDs::preHardFilter, "Filter", 4.5f,
             solaris::ParameterIDs::preHardLevel, "Level", 5.0f);
    addPedal(solaris::ParameterIDs::preFuzzEnabled, "Void Fuzz",
             solaris::ParameterIDs::preFuzzSustain, "Sustain", 5.5f,
             solaris::ParameterIDs::preFuzzTone, "Tone", 5.0f,
             solaris::ParameterIDs::preFuzzLevel, "Level", 5.0f);

    addPedal(solaris::ParameterIDs::postPhaserEnabled, "Orbit",
             solaris::ParameterIDs::postPhaserRate, "Rate", 3.5f,
             solaris::ParameterIDs::postPhaserDepth, "Depth", 5.5f,
             solaris::ParameterIDs::postPhaserMix, "Mix", 5.0f);
    addPedal(solaris::ParameterIDs::postChorusEnabled, "Chorus",
             solaris::ParameterIDs::postChorusRate, "Rate", 3.5f,
             solaris::ParameterIDs::postChorusDepth, "Depth", 4.5f,
             solaris::ParameterIDs::postChorusMix, "Mix", 4.5f);
    addPedal(solaris::ParameterIDs::postTremoloEnabled, "Pulse",
             solaris::ParameterIDs::postTremoloRate, "Rate", 3.5f,
             solaris::ParameterIDs::postTremoloDepth, "Depth", 4.5f,
             solaris::ParameterIDs::postTremoloShape, "Shape", 2.5f);
    addPedal(solaris::ParameterIDs::postDelayEnabled, "Echo 404",
             solaris::ParameterIDs::postDelayTime, "Time", 3.8f,
             solaris::ParameterIDs::postDelayFeedback, "Regeneration", 3.5f,
             solaris::ParameterIDs::postDelayMix, "Mix", 3.2f);
    addPedal(solaris::ParameterIDs::postReverbEnabled, "Sanctum",
             solaris::ParameterIDs::postReverbDecay, "Decay", 4.6f,
             solaris::ParameterIDs::postReverbTone, "Tone", 5.2f,
             solaris::ParameterIDs::postReverbMix, "Mix", 2.8f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"cabWet", 1}, "Cab Wet",
        juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"cabMicBlend", 1}, "Cab Mic B Blend",
        juce::NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"cabPhaseB", 1}, "Cab Mic B Phase", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"eqHpfBypass", 1}, "EQ HPF Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"eqHpfFrequency", 1}, "EQ HPF Frequency",
        juce::NormalisableRange<float>{20.0f, 1000.0f, 1.0f, 0.35f}, 70.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"eqLpfBypass", 1}, "EQ LPF Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"eqLpfFrequency", 1}, "EQ LPF Frequency",
        juce::NormalisableRange<float>{1000.0f, 22000.0f, 1.0f, 0.35f}, 18000.0f));

    const std::array<float, 4> defaults { 100.0f, 400.0f, 1600.0f, 6400.0f };
    for (int i = 0; i < 4; ++i)
    {
        const auto number = juce::String(i + 1);
        const auto prefix = juce::String("eqBand") + number;
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "Bypass", 1}, "EQ Band " + number + " Bypass", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Frequency", 1}, "EQ Band " + number + " Frequency",
            juce::NormalisableRange<float>{20.0f, 22000.0f, 1.0f, 0.3f},
            defaults[static_cast<std::size_t>(i)]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Gain", 1}, "EQ Band " + number + " Gain",
            juce::NormalisableRange<float>{-18.0f, 18.0f, 0.1f}, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Q", 1}, "EQ Band " + number + " Q",
            juce::NormalisableRange<float>{0.1f, 18.0f, 0.01f, 0.4f}, 0.707f));
    }

    return {params.begin(), params.end()};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SolarisSilverlineAudioProcessor();
}
