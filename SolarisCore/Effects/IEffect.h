#pragma once

#include <JuceHeader.h>

namespace solaris
{
    struct EffectPrepareSpec
    {
        double sampleRate = 44100.0;
        juce::uint32 maximumBlockSize = 512;
        juce::uint32 numChannels = 2;

        juce::dsp::ProcessSpec asJuceSpec() const noexcept
        {
            return { sampleRate, maximumBlockSize, numChannels };
        }
    };

    class IEffect
    {
    public:
        virtual ~IEffect() = default;

        virtual const char* id() const noexcept = 0;
        virtual void prepare(const EffectPrepareSpec& spec) = 0;
        virtual void reset() noexcept = 0;
        virtual void process(juce::AudioBuffer<float>& buffer) noexcept = 0;

        virtual void setBypassed(bool shouldBeBypassed) noexcept = 0;
        virtual bool isBypassed() const noexcept = 0;
        virtual double tailLengthSeconds() const noexcept { return 0.0; }
    };

    // Shared click-free bypass wrapper. Scratch storage is allocated only in prepare().
    class EffectBase : public IEffect
    {
    public:
        void prepare(const EffectPrepareSpec& spec) final
        {
            preparedSampleRate = juce::jmax(1.0, spec.sampleRate);
            maximumBlockSize = juce::jmax<juce::uint32>(1u, spec.maximumBlockSize);
            preparedChannels = juce::jlimit<juce::uint32>(1u, 2u, spec.numChannels);

            dryScratch.setSize(static_cast<int>(preparedChannels),
                               static_cast<int>(maximumBlockSize),
                               false, false, true);

            bypassMix.reset(preparedSampleRate, 0.008);
            bypassMix.setCurrentAndTargetValue(bypassed ? 0.0f : 1.0f);

            prepareEffect(spec);
            resetEffect();
        }

        void reset() noexcept final
        {
            bypassMix.setCurrentAndTargetValue(bypassed ? 0.0f : 1.0f);
            resetEffect();
        }

        void setBypassed(bool shouldBeBypassed) noexcept final
        {
            if (bypassed == shouldBeBypassed)
                return;

            bypassed = shouldBeBypassed;
            bypassMix.setTargetValue(bypassed ? 0.0f : 1.0f);
        }

        bool isBypassed() const noexcept final { return bypassed; }

        void process(juce::AudioBuffer<float>& buffer) noexcept final
        {
            const auto numSamples = buffer.getNumSamples();
            const auto numChannels = juce::jmin(buffer.getNumChannels(),
                                                static_cast<int>(preparedChannels));

            if (numSamples <= 0 || numChannels <= 0)
                return;

            if (bypassMix.getCurrentValue() == 0.0f
                && bypassMix.getTargetValue() == 0.0f)
                return;

            const auto canCrossfade = numSamples <= dryScratch.getNumSamples();
            jassert(canCrossfade);

            if (!canCrossfade)
            {
                // Hosts are expected to honour maximumBlockSize. Avoid allocating in the
                // callback if a hostile host violates the contract.
                if (!bypassed)
                    processEffect(buffer);
                return;
            }

            for (int channel = 0; channel < numChannels; ++channel)
                dryScratch.copyFrom(channel, 0, buffer, channel, 0, numSamples);

            processEffect(buffer);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto wet = bypassMix.getNextValue();
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto dry = dryScratch.getSample(channel, sample);
                    const auto effected = buffer.getSample(channel, sample);
                    buffer.setSample(channel, sample, dry + (effected - dry) * wet);
                }
            }
        }

    protected:
        virtual void prepareEffect(const EffectPrepareSpec& spec) = 0;
        virtual void resetEffect() noexcept = 0;
        virtual void processEffect(juce::AudioBuffer<float>& buffer) noexcept = 0;

        double preparedSampleRate = 44100.0;
        juce::uint32 maximumBlockSize = 512;
        juce::uint32 preparedChannels = 2;

    private:
        juce::AudioBuffer<float> dryScratch;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
        bool bypassed = true;
    };
}
