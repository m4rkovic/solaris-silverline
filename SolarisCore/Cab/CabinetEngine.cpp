#include "CabinetEngine.h"

namespace solaris
{
    CabinetEngine::CabinetEngine() = default;

    void CabinetEngine::prepare(const juce::dsp::ProcessSpec& spec)
    {
        maximumBlockSize = static_cast<int>(spec.maximumBlockSize);
        preparedChannels = juce::jlimit(1, 2, static_cast<int>(spec.numChannels));
        convolutionA.prepare(spec);
        convolutionB.prepare(spec);
        workA.setSize(preparedChannels, maximumBlockSize, false, false, true);
        workB.setSize(preparedChannels, maximumBlockSize, false, false, true);
        reset();
    }

    void CabinetEngine::reset() noexcept
    {
        convolutionA.reset();
        convolutionB.reset();
        workA.clear();
        workB.clear();
    }

    void CabinetEngine::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = juce::jmin(buffer.getNumChannels(), preparedChannels);
        if (numSamples <= 0 || numChannels <= 0 || numSamples > maximumBlockSize)
            return;

        const bool aReady = slotAActive.load(std::memory_order_relaxed) && convolutionA.getCurrentIRSize() > 0;
        const bool bReady = slotBActive.load(std::memory_order_relaxed) && convolutionB.getCurrentIRSize() > 0;
        if (!aReady && !bReady)
            return;

        if (aReady)
        {
            for (int channel = 0; channel < numChannels; ++channel)
                workA.copyFrom(channel, 0, buffer, channel, 0, numSamples);
            juce::dsp::AudioBlock<float> block(workA);
            auto subBlock = block.getSubBlock(0, static_cast<std::size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            convolutionA.process(context);
        }

        if (bReady)
        {
            for (int channel = 0; channel < numChannels; ++channel)
                workB.copyFrom(channel, 0, buffer, channel, 0, numSamples);
            juce::dsp::AudioBlock<float> block(workB);
            auto subBlock = block.getSubBlock(0, static_cast<std::size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            convolutionB.process(context);
        }

        const auto wet = juce::jlimit(0.0f, 1.0f, wetMix.load(std::memory_order_relaxed));
        const auto dry = 1.0f - wet;
        const auto blend = juce::jlimit(0.0f, 1.0f, micBlend.load(std::memory_order_relaxed));
        const auto gainA = aReady ? (bReady ? 1.0f - blend : 1.0f) : 0.0f;
        const auto gainB = bReady ? (aReady ? blend : 1.0f) : 0.0f;
        const auto polarityA = phaseA.load(std::memory_order_relaxed) ? -1.0f : 1.0f;
        const auto polarityB = phaseB.load(std::memory_order_relaxed) ? -1.0f : 1.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* out = buffer.getWritePointer(channel);
            const auto* a = aReady ? workA.getReadPointer(channel) : nullptr;
            const auto* b = bReady ? workB.getReadPointer(channel) : nullptr;
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float wetSample = 0.0f;
                if (a != nullptr) wetSample += a[sample] * gainA * polarityA;
                if (b != nullptr) wetSample += b[sample] * gainB * polarityB;
                out[sample] = out[sample] * dry + wetSample * wet;
            }
        }
    }

    void CabinetEngine::setCabinetModelId(juce::String modelId)
    {
        const juce::ScopedLock lock(metadataLock);
        cabinetModelId = std::move(modelId);
    }

    juce::String CabinetEngine::getCabinetModelId() const
    {
        const juce::ScopedLock lock(metadataLock);
        return cabinetModelId;
    }

    void CabinetEngine::setWetMix(float wet) noexcept { wetMix.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed); }
    void CabinetEngine::setMicBlend(float value) noexcept { micBlend.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed); }
    void CabinetEngine::setPhaseInverted(CabinetIRSlot slot, bool value) noexcept { phaseFor(slot).store(value, std::memory_order_relaxed); }
    void CabinetEngine::setSlotActive(CabinetIRSlot slot, bool value) noexcept { activeFor(slot).store(value, std::memory_order_relaxed); }

    CabinetEngine::Validation CabinetEngine::validateWavIR(const juce::File& file)
    {
        Validation validation;
        if (!file.existsAsFile())
        {
            validation.result = juce::Result::fail("IR file does not exist: " + file.getFullPathName());
            return validation;
        }
        if (!file.hasFileExtension("wav"))
        {
            validation.result = juce::Result::fail("Cabinet IR must be a WAV file.");
            return validation;
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
        {
            validation.result = juce::Result::fail("WAV file could not be decoded as an audio impulse response.");
            return validation;
        }

        validation.sampleRate = reader->sampleRate;
        validation.lengthInSamples = reader->lengthInSamples;
        validation.channels = static_cast<int>(reader->numChannels);

        if (validation.channels < 1 || validation.channels > 2)
            validation.result = juce::Result::fail("Cabinet IR must contain one or two channels.");
        else if (validation.lengthInSamples <= 0)
            validation.result = juce::Result::fail("Cabinet IR contains no audio samples.");
        else if (validation.sampleRate < 8000.0 || validation.sampleRate > 384000.0)
            validation.result = juce::Result::fail("Cabinet IR reports an unsupported sample rate.");

        return validation;
    }

    juce::Result CabinetEngine::loadImpulseResponseFromFile(CabinetIRSlot slot,
                                                             const juce::File& file,
                                                             bool trim,
                                                             bool normalise,
                                                             std::size_t expectedSize)
    {
        const auto validation = validateWavIR(file);
        if (validation.result.failed())
        {
            const juce::ScopedLock lock(metadataLock);
            auto& status = statusFor(slot);
            status.error = validation.result.getErrorMessage();
            status.missing = !file.existsAsFile();
            return validation.result;
        }

        const bool stereo = validation.channels == 2;
        convolverFor(slot).loadImpulseResponse(
            file,
            stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
            trim ? juce::dsp::Convolution::Trim::yes : juce::dsp::Convolution::Trim::no,
            expectedSize,
            normalise ? juce::dsp::Convolution::Normalise::yes : juce::dsp::Convolution::Normalise::no);

        stereoFor(slot).store(stereo, std::memory_order_relaxed);
        activeFor(slot).store(true, std::memory_order_relaxed);

        {
            const juce::ScopedLock lock(metadataLock);
            auto& status = statusFor(slot);
            status.sourcePath = file.getFullPathName();
            status.error.clear();
            status.sourceSampleRate = validation.sampleRate;
            status.lengthInSamples = validation.lengthInSamples;
            status.channels = validation.channels;
            status.active = true;
            status.missing = false;
        }

        return juce::Result::ok();
    }

    void CabinetEngine::requestImpulseResponseFromFile(CabinetIRSlot slot,
                                                        const juce::File& file,
                                                        bool stereo,
                                                        bool trim,
                                                        bool normalise,
                                                        std::size_t expectedSize)
    {
        juce::ignoreUnused(stereo);
        (void) loadImpulseResponseFromFile(slot, file, trim, normalise, expectedSize);
    }

    void CabinetEngine::requestImpulseResponseBuffer(CabinetIRSlot slot,
                                                      juce::AudioBuffer<float>&& buffer,
                                                      double sourceSampleRate,
                                                      bool stereo,
                                                      bool trim,
                                                      bool normalise)
    {
        if (buffer.getNumChannels() < 1 || buffer.getNumChannels() > 2 || buffer.getNumSamples() <= 0)
            return;

        const bool actualStereo = stereo && buffer.getNumChannels() >= 2;
        const auto length = buffer.getNumSamples();
        convolverFor(slot).loadImpulseResponse(
            std::move(buffer), sourceSampleRate,
            actualStereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
            trim ? juce::dsp::Convolution::Trim::yes : juce::dsp::Convolution::Trim::no,
            normalise ? juce::dsp::Convolution::Normalise::yes : juce::dsp::Convolution::Normalise::no);

        stereoFor(slot).store(actualStereo, std::memory_order_relaxed);
        activeFor(slot).store(true, std::memory_order_relaxed);
        const juce::ScopedLock lock(metadataLock);
        auto& status = statusFor(slot);
        status = {};
        status.sourceSampleRate = sourceSampleRate;
        status.lengthInSamples = length;
        status.channels = actualStereo ? 2 : 1;
        status.active = true;
    }

    CabinetIRStatus CabinetEngine::getSlotStatus(CabinetIRSlot slot) const
    {
        const juce::ScopedLock lock(metadataLock);
        auto status = statusFor(slot);
        status.active = slot == CabinetIRSlot::micA ? slotAActive.load(std::memory_order_relaxed)
                                                     : slotBActive.load(std::memory_order_relaxed);
        return status;
    }

    int CabinetEngine::getLatencySamples() const noexcept
    {
        int latency = 0;
        if (slotAActive.load(std::memory_order_relaxed)) latency = juce::jmax(latency, convolutionA.getLatency());
        if (slotBActive.load(std::memory_order_relaxed)) latency = juce::jmax(latency, convolutionB.getLatency());
        return latency;
    }

    juce::ValueTree CabinetEngine::createState() const
    {
        juce::ValueTree state("CAB");
        state.setProperty("modelId", getCabinetModelId(), nullptr);
        state.setProperty("wet", wetMix.load(std::memory_order_relaxed), nullptr);
        state.setProperty("blend", micBlend.load(std::memory_order_relaxed), nullptr);

        for (const auto slot : { CabinetIRSlot::micA, CabinetIRSlot::micB })
        {
            const auto status = getSlotStatus(slot);
            juce::ValueTree child("IR_SLOT");
            child.setProperty("index", slot == CabinetIRSlot::micA ? 0 : 1, nullptr);
            child.setProperty("active", status.active, nullptr);
            child.setProperty("stereo", status.channels == 2, nullptr);
            child.setProperty("phaseInvert", (slot == CabinetIRSlot::micA ? phaseA : phaseB).load(std::memory_order_relaxed), nullptr);
            child.setProperty("sourcePath", status.sourcePath, nullptr);
            child.setProperty("sourceSampleRate", status.sourceSampleRate, nullptr);
            child.setProperty("channels", status.channels, nullptr);
            child.setProperty("missing", status.missing, nullptr);
            state.addChild(child, -1, nullptr);
        }
        return state;
    }

    void CabinetEngine::restoreState(const juce::ValueTree& state)
    {
        if (!state.isValid() || !state.hasType("CAB"))
            return;

        setCabinetModelId(state.getProperty("modelId").toString());
        setWetMix(static_cast<float>(state.getProperty("wet", 1.0f)));
        setMicBlend(static_cast<float>(state.getProperty("blend", 0.5f)));

        for (int index = 0; index < state.getNumChildren(); ++index)
        {
            const auto child = state.getChild(index);
            if (!child.hasType("IR_SLOT"))
                continue;

            const auto slot = static_cast<int>(child.getProperty("index", 0)) == 0 ? CabinetIRSlot::micA : CabinetIRSlot::micB;
            setPhaseInverted(slot, static_cast<bool>(child.getProperty("phaseInvert", false)));
            const auto active = static_cast<bool>(child.getProperty("active", false));
            const auto path = child.getProperty("sourcePath").toString();

            if (!active)
            {
                setSlotActive(slot, false);
                continue;
            }

            if (path.isEmpty())
            {
                setSlotActive(slot, false);
                continue;
            }

            const auto file = juce::File(path);
            const auto result = loadImpulseResponseFromFile(slot, file);
            if (result.failed())
            {
                setSlotActive(slot, false);
                const juce::ScopedLock lock(metadataLock);
                auto& status = statusFor(slot);
                status.sourcePath = path;
                status.active = false;
                status.missing = !file.existsAsFile();
            }
        }
    }

    juce::dsp::Convolution& CabinetEngine::convolverFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? convolutionA : convolutionB; }
    std::atomic<bool>& CabinetEngine::activeFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? slotAActive : slotBActive; }
    std::atomic<bool>& CabinetEngine::stereoFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? slotAStereo : slotBStereo; }
    std::atomic<bool>& CabinetEngine::phaseFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? phaseA : phaseB; }
    CabinetIRStatus& CabinetEngine::statusFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? statusA : statusB; }
    const CabinetIRStatus& CabinetEngine::statusFor(CabinetIRSlot slot) const noexcept { return slot == CabinetIRSlot::micA ? statusA : statusB; }
}
