#pragma once

#include "IEffect.h"
#include <cmath>
#include <memory>

namespace solaris
{
    class OversampledEffectBase : public EffectBase
    {
    public:
        int latencySamples() const noexcept override { return latency; }

    protected:
        static constexpr std::size_t oversamplingStages = 2;

        void prepareEffect(const EffectPrepareSpec& spec) final
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                static_cast<std::size_t>(juce::jlimit<juce::uint32>(1u, 2u, spec.numChannels)),
                oversamplingStages,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true,
                true);

            oversampling->initProcessing(static_cast<std::size_t>(spec.maximumBlockSize));
            latency = static_cast<int>(std::lround(oversampling->getLatencyInSamples()));
            oversampledRate = spec.sampleRate
                            * static_cast<double>(oversampling->getOversamplingFactor());

            prepareOversampledEffect(oversampledRate,
                                     static_cast<std::size_t>(spec.numChannels));
        }

        void resetEffect() noexcept final
        {
            if (oversampling != nullptr)
                oversampling->reset();
            resetOversampledEffect();
        }

        void processEffect(juce::AudioBuffer<float>& buffer) noexcept final
        {
            if (oversampling == nullptr)
                return;

            juce::dsp::AudioBlock<float> block(buffer);
            auto upsampled = oversampling->processSamplesUp(block);
            processOversampledEffect(upsampled);
            oversampling->processSamplesDown(block);
        }

        virtual void prepareOversampledEffect(double sampleRate, std::size_t channels) = 0;
        virtual void resetOversampledEffect() noexcept = 0;
        virtual void processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept = 0;

        double oversampledRate = 176400.0;

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        int latency = 0;
    };
}
