#pragma once

#include <JuceHeader.h>
#include <cmath>

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
        virtual int latencySamples() const noexcept { return 0; }
        virtual double tailLengthSeconds() const noexcept { return 0.0; }
    };

    // Shared click-free bypass wrapper. Scratch/delay storage is allocated only in prepare().
    // Effects with internal oversampling can declare latency and receive a time-aligned dry path.
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

            prepareEffect(spec);

            dryDelaySamples = juce::jmax(0, latencySamples());
            dryDelay.setSize(static_cast<int>(preparedChannels),
                             juce::jmax(1, dryDelaySamples + 1),
                             false, true, true);
            dryWriteIndex = 0;

            bypassMix.reset(preparedSampleRate, 0.008);
            bypassMix.setCurrentAndTargetValue(bypassed ? 0.0f : 1.0f);
            resetEffect();
        }

        void reset() noexcept final
        {
            bypassMix.setCurrentAndTargetValue(bypassed ? 0.0f : 1.0f);
            dryDelay.clear();
            dryWriteIndex = 0;
            resetEffect();
        }

        void setBypassed(bool shouldBeBypassed) noexcept final
        {
            if (bypassed == shouldBeBypassed)
                return;

            bypassed = shouldBeBypassed;
            bypassMix.setTargetValue(bypassed ? 0.0f : 1.0f);

            if (!bypassed)
            {
                // Re-arm the latency alignment path from silence so an effect that
                // is enabled after a long bypass cannot crossfade against stale dry samples.
                dryDelay.clear();
                dryWriteIndex = 0;
                resetEffect();
            }
        }

        bool isBypassed() const noexcept final { return bypassed; }

        void process(juce::AudioBuffer<float>& buffer) noexcept final
        {
            const auto numSamples = buffer.getNumSamples();
            const auto numChannels = juce::jmin(buffer.getNumChannels(),
                                                static_cast<int>(preparedChannels));

            if (numSamples <= 0 || numChannels <= 0)
                return;

            const auto canCrossfade = numSamples <= dryScratch.getNumSamples();
            jassert(canCrossfade);

            if (!canCrossfade)
            {
                if (!bypassed)
                    processEffect(buffer);
                return;
            }

            const auto fullyBypassed = std::abs(bypassMix.getCurrentValue()) <= 1.0e-7f
                                    && std::abs(bypassMix.getTargetValue()) <= 1.0e-7f;

            // A fully bypassed effect must be a true wire. In particular, do not
            // run the latency-alignment delay for oversampled effects when they are
            // off; doing so needlessly adds latency to an all-bypassed pedalboard.
            if (fullyBypassed)
                return;

            captureAlignedDry(buffer, numChannels, numSamples);
            processEffect(buffer);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto wet = fullyBypassed ? 0.0f : bypassMix.getNextValue();

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto dry = dryScratch.getSample(channel, sample);
                    const auto effected = fullyBypassed ? dry : buffer.getSample(channel, sample);
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
        void captureAlignedDry(const juce::AudioBuffer<float>& input,
                               int numChannels,
                               int numSamples) noexcept
        {
            if (dryDelaySamples == 0)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                    dryScratch.copyFrom(channel, 0, input, channel, 0, numSamples);
                return;
            }

            const auto delaySize = dryDelay.getNumSamples();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto readIndex = (dryWriteIndex + delaySize - dryDelaySamples) % delaySize;

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const auto x = input.getSample(channel, sample);
                    const auto delayed = dryDelay.getSample(channel, readIndex);
                    dryDelay.setSample(channel, dryWriteIndex, x);
                    dryScratch.setSample(channel, sample, delayed);
                }

                dryWriteIndex = (dryWriteIndex + 1) % delaySize;
            }
        }

        juce::AudioBuffer<float> dryScratch;
        juce::AudioBuffer<float> dryDelay;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
        int dryDelaySamples = 0;
        int dryWriteIndex = 0;
        bool bypassed = true;
    };
}
