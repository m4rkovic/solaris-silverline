#include <JuceHeader.h>
#include "../SolarisCore/Effects/PreEffects.h"
#include "../SolarisCore/Effects/PostEffects.h"
#include "../SolarisCore/Effects/EffectChain.h"
#include "../Silverline/AmpModels/Silverline68Amp.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>

namespace
{
    std::atomic<bool> trackAllocations { false };
    std::atomic<std::size_t> allocationCount { 0 };

    void noteAllocation() noexcept
    {
        if (trackAllocations.load(std::memory_order_relaxed))
            allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void* operator new(std::size_t size)
{
    noteAllocation();
    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    noteAllocation();
    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
    bool isFinite(const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (!std::isfinite(buffer.getSample(channel, sample)))
                    return false;
        return true;
    }

    float peakMagnitude(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = juce::jmax(peak, std::abs(buffer.getSample(channel, sample)));
        return peak;
    }

    void fillSignal(juce::AudioBuffer<float>& buffer, double sampleRate, double& phase)
    {
        const auto increment = juce::MathConstants<double>::twoPi * 437.0 / sampleRate;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto transient = sample == 0 ? 0.32f : 0.0f;
            const auto value = 0.17f * static_cast<float>(std::sin(phase)) + transient;
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, value * (channel == 0 ? 1.0f : 0.93f));
        }
    }

    template <typename Effect, typename Configure>
    bool exerciseEffect(const char* name,
                        double sampleRate,
                        int blockSize,
                        int channels,
                        Configure&& configure)
    {
        Effect effect;
        solaris::EffectPrepareSpec spec {
            sampleRate,
            static_cast<juce::uint32>(blockSize),
            static_cast<juce::uint32>(channels)
        };
        effect.prepare(spec);
        configure(effect);
        effect.setBypassed(false);

        juce::AudioBuffer<float> buffer(channels, blockSize);
        double phase = 0.0;

        for (int warmup = 0; warmup < 3; ++warmup)
        {
            fillSignal(buffer, sampleRate, phase);
            effect.process(buffer);
        }

        fillSignal(buffer, sampleRate, phase);
        allocationCount.store(0, std::memory_order_relaxed);
        trackAllocations.store(true, std::memory_order_relaxed);
        effect.process(buffer);
        trackAllocations.store(false, std::memory_order_relaxed);

        if (allocationCount.load(std::memory_order_relaxed) != 0)
        {
            std::cerr << name << ": realtime allocation detected at "
                      << sampleRate << " Hz / " << blockSize << " / " << channels << "ch\n";
            return false;
        }

        if (!isFinite(buffer) || peakMagnitude(buffer) > 32.0f)
        {
            std::cerr << name << ": invalid or unstable output at "
                      << sampleRate << " Hz / " << blockSize << " / " << channels << "ch\n";
            return false;
        }

        effect.setBypassed(true);
        float previous = buffer.getSample(0, blockSize - 1);
        float largestStep = 0.0f;
        const auto transitionBlocks = juce::jmax(2, static_cast<int>(
            std::ceil(sampleRate * 0.015 / static_cast<double>(blockSize))));

        for (int block = 0; block < transitionBlocks; ++block)
        {
            fillSignal(buffer, sampleRate, phase);
            effect.process(buffer);
            if (!isFinite(buffer))
                return false;

            for (int sample = 0; sample < blockSize; ++sample)
            {
                const auto value = buffer.getSample(0, sample);
                largestStep = juce::jmax(largestStep, std::abs(value - previous));
                previous = value;
            }
        }

        if (largestStep > 2.5f)
        {
            std::cerr << name << ": bypass transition produced a discontinuity\n";
            return false;
        }

        return true;
    }

    bool exerciseAmp(double sampleRate, int blockSize, int channels)
    {
        solaris::Silverline68Amp amp;
        solaris::AmpPrepareSpec spec {
            sampleRate,
            static_cast<juce::uint32>(blockSize),
            static_cast<juce::uint32>(channels)
        };
        amp.prepare(spec);

        solaris::AmpParameters params;
        params.volume = 0.58f;
        params.bass = 0.52f;
        params.treble = 0.62f;
        params.reverb = 0.18f;
        params.tremoloIntensity = 0.2f;
        amp.setParameters(params);

        juce::AudioBuffer<float> buffer(channels, blockSize);
        double phase = 0.0;
        fillSignal(buffer, sampleRate, phase);

        allocationCount.store(0, std::memory_order_relaxed);
        trackAllocations.store(true, std::memory_order_relaxed);
        amp.process(buffer);
        trackAllocations.store(false, std::memory_order_relaxed);

        if (allocationCount.load(std::memory_order_relaxed) != 0)
        {
            std::cerr << "Silverline68Amp: realtime allocation detected\n";
            return false;
        }

        return isFinite(buffer) && peakMagnitude(buffer) < 32.0f;
    }

    bool exerciseEffectChainState()
    {
        solaris::VintageCompressor compressor;
        solaris::AsymmetricOverdrive overdrive;
        solaris::FlexibleDistortion distortion;

        solaris::EffectChain chain;
        if (!chain.addEffect(compressor)
            || !chain.addEffect(overdrive)
            || !chain.addEffect(distortion))
            return false;

        juce::StringArray requestedOrder;
        requestedOrder.add(distortion.id());
        requestedOrder.add(compressor.id());
        requestedOrder.add(overdrive.id());

        if (!chain.setOrder(requestedOrder))
            return false;

        const auto saved = chain.createState("CHAIN");

        juce::StringArray differentOrder;
        differentOrder.add(overdrive.id());
        differentOrder.add(distortion.id());
        differentOrder.add(compressor.id());

        if (!chain.setOrder(differentOrder))
            return false;

        chain.restoreState(saved);

        return chain.size() == 3
            && chain.at(0) == &distortion
            && chain.at(1) == &compressor
            && chain.at(2) == &overdrive;
    }

    bool runConfiguration(double sampleRate, int blockSize, int channels)
    {
        bool ok = true;

        ok &= exerciseEffect<solaris::VintageCompressor>("Leveler", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setSustain(0.6f); effect.setAttack(0.35f); effect.setLevel(0.5f); });
        ok &= exerciseEffect<solaris::AsymmetricOverdrive>("Turbo Drive", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setDrive(0.62f); effect.setTone(0.55f); effect.setLevel(0.45f); });
        ok &= exerciseEffect<solaris::FlexibleDistortion>("Badland Dist", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setGain(0.58f); effect.setContour(0.47f); effect.setLevel(0.43f); });
        ok &= exerciseEffect<solaris::HardClipDistortion>("Vermin Drive", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setDistortion(0.62f); effect.setFilter(0.55f); effect.setLevel(0.42f); });
        ok &= exerciseEffect<solaris::SustainingFuzz>("Void Fuzz", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setSustain(0.68f); effect.setTone(0.52f); effect.setLevel(0.38f); });

        ok &= exerciseEffect<solaris::MultiStagePhaser>("Orbit", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setRate(0.42f); effect.setDepth(0.66f); effect.setMix(0.55f); });
        ok &= exerciseEffect<solaris::AnalogChorus>("Chorus", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setRate(0.34f); effect.setDepth(0.58f); effect.setMix(0.48f); });
        ok &= exerciseEffect<solaris::BiasTremolo>("Pulse", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setRate(0.4f); effect.setDepth(0.72f); effect.setShape(0.35f); });
        ok &= exerciseEffect<solaris::AnalogDelay>("Echo 404", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setTime(0.42f); effect.setFeedback(0.48f); effect.setMix(0.36f); });
        ok &= exerciseEffect<solaris::SpringSpaceReverb>("Sanctum", sampleRate, blockSize, channels,
            [](auto& effect) { effect.setDecay(0.58f); effect.setTone(0.52f); effect.setMix(0.34f); });

        ok &= exerciseAmp(sampleRate, blockSize, channels);
        return ok;
    }
}

int main()
{
    const std::array<double, 3> sampleRates { 44100.0, 48000.0, 96000.0 };
    const std::array<int, 5> blockSizes { 32, 64, 128, 256, 512 };
    const std::array<int, 2> channelCounts { 1, 2 };

    bool ok = exerciseEffectChainState();
    if (!ok)
        std::cerr << "EffectChain: ordering/state round-trip failed\n";

    for (const auto sampleRate : sampleRates)
        for (const auto blockSize : blockSizes)
            for (const auto channels : channelCounts)
                ok &= runConfiguration(sampleRate, blockSize, channels);

    if (!ok)
        return 1;

    std::cout << "Solaris DSP sanity matrix passed: 44.1/48/96 kHz, "
                 "32/64/128/256/512 samples, mono/stereo, bypass, finite output, no process allocations.\n";
    return 0;
}
