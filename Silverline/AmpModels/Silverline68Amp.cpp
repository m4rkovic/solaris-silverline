#include "Silverline68Amp.h"
#include "Silverline68AnalogueStage.h"
#include <algorithm>
#include <cmath>

namespace solaris
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        std::size_t wrappedReadIndex(std::size_t writeIndex,
                                     std::size_t delaySamples,
                                     std::size_t size) noexcept
        {
            return (writeIndex + size - (delaySamples % size)) % size;
        }
    }

    AmpMetadata Silverline68Amp::staticMetadata()
    {
        return {
            "silverline68",
            "Silverline 68",
            "American vintage clean / edge-of-breakup"
        };
    }

    Silverline68Amp::Silverline68Amp()
        : info(staticMetadata()),
          nonlinearStage(std::make_unique<Silverline68AnalogueStage>())
    {
    }

    void Silverline68Amp::OnePoleLowpass::prepare(double targetSampleRate,
                                                   float cutoffHz) noexcept
    {
        const auto safeRate = std::max(1.0, targetSampleRate);
        const auto safeCutoff = std::clamp(cutoffHz, 1.0f,
                                           static_cast<float>(safeRate * 0.45));
        coefficient = 1.0f - std::exp(-2.0f * pi * safeCutoff
                                      / static_cast<float>(safeRate));
    }

    float Silverline68Amp::OnePoleLowpass::process(std::size_t channel,
                                                    float input) noexcept
    {
        auto& z = state[channel];
        z += coefficient * (input - z);
        return z;
    }

    void Silverline68Amp::OnePoleLowpass::reset() noexcept
    {
        state.fill(0.0f);
    }

    void Silverline68Amp::SpringPrototype::prepare(double targetSampleRate,
                                                    std::size_t channels)
    {
        activeChannels = std::clamp<std::size_t>(channels, 1u, maxChannels);
        delaySize = std::max<std::size_t>(2u,
            static_cast<std::size_t>(std::ceil(targetSampleRate * 0.095)));

        for (auto& channelBuffer : delayBuffer)
            channelBuffer.assign(delaySize, 0.0f);

        tapA = std::clamp<std::size_t>(static_cast<std::size_t>(targetSampleRate * 0.029), 1u, delaySize - 1u);
        tapB = std::clamp<std::size_t>(static_cast<std::size_t>(targetSampleRate * 0.043), 1u, delaySize - 1u);
        tapC = std::clamp<std::size_t>(static_cast<std::size_t>(targetSampleRate * 0.071), 1u, delaySize - 1u);

        const auto safeRate = std::max(1.0, targetSampleRate);
        dampingCoefficient = 1.0f - std::exp(-2.0f * pi * 4200.0f
                                             / static_cast<float>(safeRate));
        reset();
    }

    void Silverline68Amp::SpringPrototype::reset() noexcept
    {
        for (auto& channelBuffer : delayBuffer)
            std::fill(channelBuffer.begin(), channelBuffer.end(), 0.0f);

        feedback.fill(0.0f);
        dampingState.fill(0.0f);
        writeIndex = 0;
    }

    float Silverline68Amp::SpringPrototype::process(std::size_t channel,
                                                    float input) noexcept
    {
        auto& line = delayBuffer[channel];
        if (line.empty())
            return 0.0f;

        line[writeIndex] = input + feedback[channel] * 0.52f;

        const auto a = line[wrappedReadIndex(writeIndex, tapA, delaySize)];
        const auto b = line[wrappedReadIndex(writeIndex, tapB, delaySize)];
        const auto c = line[wrappedReadIndex(writeIndex, tapC, delaySize)];

        // Alternating polarity and damping produce a compact, splashy spring-like tail.
        const auto diffuse = a * 0.58f - b * 0.34f + c * 0.27f;
        auto& damped = dampingState[channel];
        damped += dampingCoefficient * (diffuse - damped);
        feedback[channel] = damped;
        return damped;
    }

    void Silverline68Amp::SpringPrototype::advance() noexcept
    {
        writeIndex = (writeIndex + 1u) % delaySize;
    }

    void Silverline68Amp::prepare(const AmpPrepareSpec& spec)
    {
        sampleRate = std::max(1.0, spec.sampleRate);
        maximumBlockSize = std::max<std::size_t>(1u, static_cast<std::size_t>(spec.maximumBlockSize));
        activeChannels = std::clamp<std::size_t>(static_cast<std::size_t>(spec.numChannels),
                                                 1u, maxChannels);

        const juce::dsp::ProcessSpec processSpec {
            sampleRate,
            spec.maximumBlockSize,
            static_cast<juce::uint32>(activeChannels)
        };

        nonlinearStage->prepare(processSpec);

        inputLowTracker.prepare(sampleRate, 34.0f);
        preEmphasisTracker.prepare(sampleRate, 1650.0f);
        outputLowpass.prepare(sampleRate, 11800.0f);
        spring.prepare(sampleRate, activeChannels);

        dryBuffer.setSize(static_cast<int>(activeChannels),
                          static_cast<int>(maximumBlockSize),
                          false, false, true);

        enabledMix.reset(sampleRate, 0.012);
        voicingBlend.reset(sampleRate, 0.025);
        reverbMix.reset(sampleRate, 0.035);
        tremoloSpeed.reset(sampleRate, 0.040);
        tremoloIntensity.reset(sampleRate, 0.030);

        enabledMix.setCurrentAndTargetValue(currentParameters.enabled ? 1.0f : 0.0f);
        voicingBlend.setCurrentAndTargetValue(currentParameters.channel == AmpChannel::vintage ? 1.0f : 0.0f);
        reverbMix.setCurrentAndTargetValue(currentParameters.reverb);
        tremoloSpeed.setCurrentAndTargetValue(currentParameters.tremoloSpeedHz);
        tremoloIntensity.setCurrentAndTargetValue(currentParameters.tremoloIntensity);

        nonlinearStage->setParameters(currentParameters);
        reset();
    }

    void Silverline68Amp::setParameters(const AmpParameters& parameters) noexcept
    {
        currentParameters = parameters;
        currentParameters.clampToValidRange();

        enabledMix.setTargetValue(currentParameters.enabled ? 1.0f : 0.0f);
        voicingBlend.setTargetValue(currentParameters.channel == AmpChannel::vintage ? 1.0f : 0.0f);
        reverbMix.setTargetValue(currentParameters.reverb);
        tremoloSpeed.setTargetValue(currentParameters.tremoloSpeedHz);
        tremoloIntensity.setTargetValue(currentParameters.tremoloIntensity);
        nonlinearStage->setParameters(currentParameters);
    }

    void Silverline68Amp::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = std::min<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), activeChannels);

        if (numSamples <= 0 || numChannels == 0)
            return;

        const auto canCrossfadeBypass = static_cast<std::size_t>(numSamples) <= maximumBlockSize;
        jassert(canCrossfadeBypass);

        if (canCrossfadeBypass)
        {
            for (std::size_t channel = 0; channel < numChannels; ++channel)
                dryBuffer.copyFrom(static_cast<int>(channel), 0, buffer,
                                   static_cast<int>(channel), 0, numSamples);
        }
        else if (!currentParameters.enabled)
        {
            // Real-time safety wins over resizing a scratch buffer in the callback.
            return;
        }

        // Analogue front-end: remove subsonic energy, then add a subtle frequency-
        // dependent emphasis. Custom is brighter/tighter; Vintage is more restrained.
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto vintage = voicingBlend.getNextValue();
            const auto emphasisAmount = 0.115f - 0.055f * vintage;

            for (std::size_t channel = 0; channel < numChannels; ++channel)
            {
                auto x = buffer.getSample(static_cast<int>(channel), sample);
                const auto sub = inputLowTracker.process(channel, x);
                x -= sub;

                const auto lowMid = preEmphasisTracker.process(channel, x);
                const auto upper = x - lowMid;
                buffer.setSample(static_cast<int>(channel), sample,
                                 x + upper * emphasisAmount);
            }
        }

        nonlinearStage->process(buffer);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto speedHz = tremoloSpeed.getNextValue();
            const auto tremDepth = tremoloIntensity.getNextValue();
            const auto reverbAmount = reverbMix.getNextValue();
            const auto ampMix = enabledMix.getNextValue();

            const auto lfo = 0.5f + 0.5f * static_cast<float>(std::sin(tremoloPhase));
            const auto tremGain = 1.0f - tremDepth * 0.78f * lfo;

            tremoloPhase += twoPi * static_cast<double>(speedHz) / sampleRate;
            if (tremoloPhase >= twoPi)
                tremoloPhase -= twoPi;

            for (std::size_t channel = 0; channel < numChannels; ++channel)
            {
                auto wet = buffer.getSample(static_cast<int>(channel), sample);
                wet = outputLowpass.process(channel, wet) * tremGain;

                const auto springWet = spring.process(channel, wet);
                wet += springWet * reverbAmount * 0.52f;

                // Conservative trim leaves headroom for CAB/EQ stages that will follow.
                wet *= 0.92f;

                if (canCrossfadeBypass)
                {
                    const auto dry = dryBuffer.getSample(static_cast<int>(channel), sample);
                    wet = dry + (wet - dry) * ampMix;
                }

                buffer.setSample(static_cast<int>(channel), sample, wet);
            }

            spring.advance();
        }
    }

    void Silverline68Amp::reset() noexcept
    {
        nonlinearStage->reset();
        inputLowTracker.reset();
        preEmphasisTracker.reset();
        outputLowpass.reset();
        spring.reset();
        tremoloPhase = 0.0;
    }
}
