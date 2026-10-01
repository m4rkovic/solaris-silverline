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
        applyPendingLoad(CabinetIRSlot::micA);
        applyPendingLoad(CabinetIRSlot::micB);

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
            for (int ch = 0; ch < numChannels; ++ch)
                workA.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            juce::dsp::AudioBlock<float> block(workA);
            auto subBlock = block.getSubBlock(0, static_cast<std::size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            convolutionA.process(context);
        }

        if (bReady)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                workB.copyFrom(ch, 0, buffer, ch, 0, numSamples);
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

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            const auto* a = aReady ? workA.getReadPointer(ch) : nullptr;
            const auto* b = bReady ? workB.getReadPointer(ch) : nullptr;
            for (int n = 0; n < numSamples; ++n)
            {
                float wetSample = 0.0f;
                if (a != nullptr) wetSample += a[n] * gainA * polarityA;
                if (b != nullptr) wetSample += b[n] * gainB * polarityB;
                out[n] = out[n] * dry + wetSample * wet;
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
    void CabinetEngine::setMicBlend(float v) noexcept { micBlend.store(juce::jlimit(0.0f, 1.0f, v), std::memory_order_relaxed); }
    void CabinetEngine::setPhaseInverted(CabinetIRSlot slot, bool v) noexcept { (slot == CabinetIRSlot::micA ? phaseA : phaseB).store(v, std::memory_order_relaxed); }
    void CabinetEngine::setSlotActive(CabinetIRSlot slot, bool v) noexcept { (slot == CabinetIRSlot::micA ? slotAActive : slotBActive).store(v, std::memory_order_relaxed); }

    void CabinetEngine::requestImpulseResponseFromFile(CabinetIRSlot slot, const juce::File& file, bool stereo, bool trim, bool normalise, std::size_t expectedSize)
    {
        auto& pending = pendingFor(slot);
        const juce::SpinLock::ScopedLockType lock(pending.lock);
        pending.type = PendingLoadType::file;
        pending.file = file;
        pending.buffer.setSize(0, 0);
        pending.stereo = stereo;
        pending.trim = trim;
        pending.normalise = normalise;
        pending.expectedSize = expectedSize;
        (slot == CabinetIRSlot::micA ? slotAStereo : slotBStereo).store(stereo, std::memory_order_relaxed);
        {
            const juce::ScopedLock metadata(metadataLock);
            (slot == CabinetIRSlot::micA ? sourcePathA : sourcePathB) = file.getFullPathName();
        }
        setSlotActive(slot, true);
    }

    void CabinetEngine::requestImpulseResponseBuffer(CabinetIRSlot slot, juce::AudioBuffer<float>&& buffer, double sourceSampleRate, bool stereo, bool trim, bool normalise)
    {
        auto& pending = pendingFor(slot);
        const juce::SpinLock::ScopedLockType lock(pending.lock);
        pending.type = PendingLoadType::buffer;
        pending.file = {};
        pending.buffer = std::move(buffer);
        pending.sourceSampleRate = sourceSampleRate;
        pending.stereo = stereo;
        pending.trim = trim;
        pending.normalise = normalise;
        pending.expectedSize = 0;
        (slot == CabinetIRSlot::micA ? slotAStereo : slotBStereo).store(stereo, std::memory_order_relaxed);
        {
            const juce::ScopedLock metadata(metadataLock);
            (slot == CabinetIRSlot::micA ? sourcePathA : sourcePathB).clear();
        }
        setSlotActive(slot, true);
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
        juce::String pathA, pathB;
        { const juce::ScopedLock lock(metadataLock); pathA = sourcePathA; pathB = sourcePathB; }

        juce::ValueTree a("IR_SLOT");
        a.setProperty("index", 0, nullptr);
        a.setProperty("active", slotAActive.load(std::memory_order_relaxed), nullptr);
        a.setProperty("stereo", slotAStereo.load(std::memory_order_relaxed), nullptr);
        a.setProperty("phaseInvert", phaseA.load(std::memory_order_relaxed), nullptr);
        a.setProperty("sourcePath", pathA, nullptr);
        state.addChild(a, -1, nullptr);

        juce::ValueTree b("IR_SLOT");
        b.setProperty("index", 1, nullptr);
        b.setProperty("active", slotBActive.load(std::memory_order_relaxed), nullptr);
        b.setProperty("stereo", slotBStereo.load(std::memory_order_relaxed), nullptr);
        b.setProperty("phaseInvert", phaseB.load(std::memory_order_relaxed), nullptr);
        b.setProperty("sourcePath", pathB, nullptr);
        state.addChild(b, -1, nullptr);
        return state;
    }

    void CabinetEngine::restoreState(const juce::ValueTree& state)
    {
        if (!state.isValid() || !state.hasType("CAB")) return;
        setCabinetModelId(state.getProperty("modelId").toString());
        setWetMix(static_cast<float>(state.getProperty("wet", 1.0f)));
        setMicBlend(static_cast<float>(state.getProperty("blend", 0.0f)));
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            const auto child = state.getChild(i);
            if (!child.hasType("IR_SLOT")) continue;
            const auto slot = static_cast<int>(child.getProperty("index", 0)) == 0 ? CabinetIRSlot::micA : CabinetIRSlot::micB;
            setPhaseInverted(slot, static_cast<bool>(child.getProperty("phaseInvert", false)));
            const auto active = static_cast<bool>(child.getProperty("active", false));
            const auto stereo = static_cast<bool>(child.getProperty("stereo", false));
            const auto path = child.getProperty("sourcePath").toString();
            if (active && path.isNotEmpty() && juce::File(path).existsAsFile())
                requestImpulseResponseFromFile(slot, juce::File(path), stereo);
            else
                setSlotActive(slot, false);
        }
    }

    juce::dsp::Convolution& CabinetEngine::convolverFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? convolutionA : convolutionB; }
    CabinetEngine::PendingLoad& CabinetEngine::pendingFor(CabinetIRSlot slot) noexcept { return slot == CabinetIRSlot::micA ? pendingA : pendingB; }

    void CabinetEngine::applyPendingLoad(CabinetIRSlot slot) noexcept
    {
        auto& pending = pendingFor(slot);
        juce::SpinLock::ScopedTryLockType lock(pending.lock);
        if (!lock.isLocked() || pending.type == PendingLoadType::none) return;

        auto& convolution = convolverFor(slot);
        const auto stereo = pending.stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no;
        const auto trim = pending.trim ? juce::dsp::Convolution::Trim::yes : juce::dsp::Convolution::Trim::no;
        const auto normalise = pending.normalise ? juce::dsp::Convolution::Normalise::yes : juce::dsp::Convolution::Normalise::no;

        if (pending.type == PendingLoadType::file)
            convolution.loadImpulseResponse(pending.file, stereo, trim, pending.expectedSize, normalise);
        else
            convolution.loadImpulseResponse(std::move(pending.buffer), pending.sourceSampleRate, stereo, trim, normalise);

        pending.type = PendingLoadType::none;
    }
}
