#include "PostEffects.h"
#include <cmath>

namespace solaris
{
    namespace
    {
        void prepareSmoother(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& smoother,
                             double sampleRate,
                             float initialValue,
                             double seconds = 0.035)
        {
            smoother.reset(sampleRate, seconds);
            smoother.setCurrentAndTargetValue(initialValue);
        }

        float mapRate(float normalized, float minimum, float maximum) noexcept
        {
            return minimum * std::pow(maximum / minimum, normalized);
        }

        float triangleFromPhase(double phase) noexcept
        {
            const auto cycle = static_cast<float>(phase / effectdsp::twoPi);
            const auto centred = cycle - std::floor(cycle + 0.5f);
            return 4.0f * std::abs(centred) - 1.0f;
        }
    }

    void AnalogChorus::prepareEffect(const EffectPrepareSpec& spec)
    {
        delay.prepare(spec.sampleRate, 0.050f, static_cast<std::size_t>(spec.numChannels));
        prepareSmoother(rate, spec.sampleRate, 0.35f);
        prepareSmoother(depth, spec.sampleRate, 0.45f);
        prepareSmoother(mix, spec.sampleRate, 0.45f, 0.025);
        resetEffect();
    }

    void AnalogChorus::resetEffect() noexcept
    {
        delay.reset();
        feedback.fill(0.0f);
        phase = 0.0;
    }

    void AnalogChorus::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);
        const auto sampleRate = static_cast<float>(preparedSampleRate);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto rateHz = mapRate(rate.getNextValue(), 0.08f, 4.2f);
            const auto depthValue = depth.getNextValue();
            const auto mixValue = mix.getNextValue();

            std::array<float, effectdsp::maxChannels> delayed {};
            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto phaseOffset = channel == 0 ? 0.0 : effectdsp::twoPi * 0.25;
                const auto lfo = 0.5f + 0.5f * static_cast<float>(std::sin(phase + phaseOffset));
                const auto delayMs = 8.0f + depthValue * (2.0f + 9.0f * lfo);
                delayed[channel] = delay.read(channel, delayMs * 0.001f * sampleRate);
            }

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto x = buffer.getSample(static_cast<int>(channel), sample);
                delay.write(channel, x + delayed[channel] * (0.035f + 0.055f * depthValue));
                feedback[channel] = delayed[channel];
                const auto y = x * (1.0f - 0.55f * mixValue) + delayed[channel] * mixValue;
                buffer.setSample(static_cast<int>(channel), sample, y);
            }

            delay.advance();
            phase += effectdsp::twoPi * static_cast<double>(rateHz) / preparedSampleRate;
            if (phase >= effectdsp::twoPi)
                phase -= effectdsp::twoPi;
        }
    }

    void MultiStagePhaser::prepareEffect(const EffectPrepareSpec& spec)
    {
        prepareSmoother(rate, spec.sampleRate, 0.35f);
        prepareSmoother(depth, spec.sampleRate, 0.55f);
        prepareSmoother(mix, spec.sampleRate, 0.5f, 0.025);
        updateCoefficients(700.0f);
        resetEffect();
    }

    void MultiStagePhaser::resetEffect() noexcept
    {
        for (auto& channel : state)
            channel.fill(0.0f);
        feedbackState.fill(0.0f);
        phase = 0.0;
        coefficientCountdown = 0;
    }

    void MultiStagePhaser::updateCoefficients(float centreHz) noexcept
    {
        static constexpr std::array<float, numStages> spread {
            0.46f, 0.68f, 0.92f, 1.22f, 1.58f, 2.05f
        };

        for (std::size_t stage = 0; stage < numStages; ++stage)
        {
            const auto frequency = juce::jlimit(35.0f,
                static_cast<float>(preparedSampleRate * 0.42),
                centreHz * spread[stage]);
            coefficients[stage] = std::exp(
                -2.0f * effectdsp::pi * frequency / static_cast<float>(preparedSampleRate));
        }
    }

    void MultiStagePhaser::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto rateHz = mapRate(rate.getNextValue(), 0.06f, 4.8f);
            const auto depthValue = depth.getNextValue();
            const auto mixValue = mix.getNextValue();
            const auto lfo = 0.5f + 0.5f * static_cast<float>(std::sin(phase));
            const auto centre = 180.0f + (380.0f + 1550.0f * depthValue) * lfo;

            if (--coefficientCountdown <= 0)
            {
                updateCoefficients(centre);
                coefficientCountdown = 16;
            }

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto dry = buffer.getSample(static_cast<int>(channel), sample);
                auto x = dry + feedbackState[channel] * 0.18f;

                for (std::size_t stage = 0; stage < numStages; ++stage)
                {
                    const auto a = coefficients[stage];
                    const auto y = -a * x + state[channel][stage];
                    state[channel][stage] = x + a * y;
                    x = y;
                }

                feedbackState[channel] = x;
                buffer.setSample(static_cast<int>(channel), sample,
                                 dry + (x - dry) * mixValue);
            }

            phase += effectdsp::twoPi * static_cast<double>(rateHz) / preparedSampleRate;
            if (phase >= effectdsp::twoPi)
                phase -= effectdsp::twoPi;
        }
    }

    void BiasTremolo::prepareEffect(const EffectPrepareSpec& spec)
    {
        prepareSmoother(rate, spec.sampleRate, 0.35f);
        prepareSmoother(depth, spec.sampleRate, 0.45f);
        prepareSmoother(shape, spec.sampleRate, 0.25f);
        resetEffect();
    }

    void BiasTremolo::resetEffect() noexcept
    {
        phase = 0.0;
    }

    void BiasTremolo::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto rateHz = mapRate(rate.getNextValue(), 0.35f, 12.0f);
            const auto depthValue = depth.getNextValue();
            const auto shapeValue = shape.getNextValue();

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto phaseOffset = channels > 1 && channel == 1 ? 0.09 : 0.0;
                auto localPhase = phase + phaseOffset;
                if (localPhase >= effectdsp::twoPi)
                    localPhase -= effectdsp::twoPi;

                const auto sine = static_cast<float>(std::sin(localPhase));
                const auto triangle = triangleFromPhase(localPhase);
                const auto wave = sine + (triangle - sine) * shapeValue;
                const auto unipolar = 0.5f + 0.5f * wave;
                const auto gain = 1.0f - depthValue * (0.92f * unipolar);
                buffer.setSample(static_cast<int>(channel), sample,
                                 buffer.getSample(static_cast<int>(channel), sample) * gain);
            }

            phase += effectdsp::twoPi * static_cast<double>(rateHz) / preparedSampleRate;
            if (phase >= effectdsp::twoPi)
                phase -= effectdsp::twoPi;
        }
    }

    void AnalogDelay::prepareEffect(const EffectPrepareSpec& spec)
    {
        delay.prepare(spec.sampleRate, 1.35f, static_cast<std::size_t>(spec.numChannels));
        feedbackLowpass.prepare(spec.sampleRate, 4300.0f);
        feedbackHighpass.prepare(spec.sampleRate, 85.0f);
        prepareSmoother(time, spec.sampleRate, 0.38f, 0.060);
        prepareSmoother(regeneration, spec.sampleRate, 0.35f, 0.035);
        prepareSmoother(mix, spec.sampleRate, 0.32f, 0.030);
        resetEffect();
    }

    void AnalogDelay::resetEffect() noexcept
    {
        delay.reset();
        feedbackLowpass.reset();
        feedbackHighpass.reset();
        previousDelayed.fill(0.0f);
        modulationPhase = 0.0;
    }

    void AnalogDelay::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);
        const auto sr = static_cast<float>(preparedSampleRate);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto timeValue = time.getNextValue();
            const auto regenValue = regeneration.getNextValue();
            const auto mixValue = mix.getNextValue();
            const auto baseMs = 35.0f + timeValue * 965.0f;
            const auto feedbackGain = 0.08f + regenValue * 0.82f;

            std::array<float, effectdsp::maxChannels> delayed {};
            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto offset = channel == 0 ? 0.0 : effectdsp::twoPi * 0.37;
                const auto mod = static_cast<float>(std::sin(modulationPhase + offset));
                const auto modMs = (0.35f + 1.65f * regenValue) * mod;
                delayed[channel] = delay.read(channel, (baseMs + modMs) * 0.001f * sr);
            }

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto dry = buffer.getSample(static_cast<int>(channel), sample);
                const auto other = channels > 1 ? delayed[1u - channel] : delayed[channel];
                auto feedbackSample = delayed[channel] + 0.07f * other;
                feedbackSample = feedbackLowpass.process(channel, feedbackSample);
                feedbackSample = feedbackHighpass.process(channel, feedbackSample);
                feedbackSample = effectdsp::cubicSoftClip(feedbackSample * 1.18f) * 1.12f;

                delay.write(channel, dry + feedbackSample * feedbackGain);
                previousDelayed[channel] = delayed[channel];
                buffer.setSample(static_cast<int>(channel), sample,
                                 dry + (delayed[channel] - dry) * mixValue);
            }

            delay.advance();
            modulationPhase += effectdsp::twoPi * 0.17 / preparedSampleRate;
            if (modulationPhase >= effectdsp::twoPi)
                modulationPhase -= effectdsp::twoPi;
        }
    }

    void SpringSpaceReverb::AllpassLine::prepare(double sampleRate, float delayMs)
    {
        const auto samples = juce::jmax<std::size_t>(
            2u, static_cast<std::size_t>(std::lround(sampleRate * delayMs * 0.001)));
        buffer.assign(samples, 0.0f);
        index = 0;
    }

    void SpringSpaceReverb::AllpassLine::reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        index = 0;
    }

    float SpringSpaceReverb::AllpassLine::process(float input, float feedback) noexcept
    {
        if (buffer.empty())
            return input;

        const auto delayed = buffer[index];
        const auto output = delayed - feedback * input;
        buffer[index] = input + feedback * output;
        index = (index + 1u) % buffer.size();
        return output;
    }

    void SpringSpaceReverb::DampedComb::prepare(double sampleRate, float delayMs)
    {
        const auto samples = juce::jmax<std::size_t>(
            2u, static_cast<std::size_t>(std::lround(sampleRate * delayMs * 0.001)));
        buffer.assign(samples, 0.0f);
        index = 0;
        dampingState = 0.0f;
    }

    void SpringSpaceReverb::DampedComb::reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        index = 0;
        dampingState = 0.0f;
    }

    float SpringSpaceReverb::DampedComb::process(float input,
                                                  float feedback,
                                                  float damping) noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const auto delayed = buffer[index];
        dampingState += damping * (delayed - dampingState);
        buffer[index] = input + dampingState * feedback;
        index = (index + 1u) % buffer.size();
        return delayed;
    }

    void SpringSpaceReverb::prepareEffect(const EffectPrepareSpec& spec)
    {
        prepareSmoother(decay, spec.sampleRate, 0.46f, 0.050);
        prepareSmoother(tone, spec.sampleRate, 0.52f, 0.040);
        prepareSmoother(mix, spec.sampleRate, 0.28f, 0.035);

        static constexpr std::array<float, diffuserCount> diffuserMs {
            2.7f, 4.1f, 6.3f, 9.1f
        };
        static constexpr std::array<float, combCount> combMs {
            29.7f, 37.1f, 41.1f, 43.7f
        };

        for (std::size_t channel = 0; channel < effectdsp::maxChannels; ++channel)
        {
            const auto stereoScale = channel == 0 ? 1.0f : 1.071f;
            for (std::size_t i = 0; i < diffuserCount; ++i)
                diffusers[channel][i].prepare(spec.sampleRate, diffuserMs[i] * stereoScale);
            for (std::size_t i = 0; i < combCount; ++i)
                combs[channel][i].prepare(spec.sampleRate, combMs[i] * stereoScale);
        }

        dampingCoefficient = effectdsp::onePoleCoefficient(spec.sampleRate, 4800.0f);
        resetEffect();
    }

    void SpringSpaceReverb::resetEffect() noexcept
    {
        for (auto& channel : diffusers)
            for (auto& diffuser : channel)
                diffuser.reset();

        for (auto& channel : combs)
            for (auto& comb : channel)
                comb.reset();

        dampingCountdown = 0;
    }

    void SpringSpaceReverb::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto decayValue = decay.getNextValue();
            const auto toneValue = tone.getNextValue();
            const auto mixValue = mix.getNextValue();
            const auto feedback = 0.58f + 0.355f * decayValue;

            if (--dampingCountdown <= 0)
            {
                const auto cutoff = 1600.0f + 8400.0f * toneValue;
                dampingCoefficient = effectdsp::onePoleCoefficient(preparedSampleRate, cutoff);
                dampingCountdown = 16;
            }

            std::array<float, effectdsp::maxChannels> dry {};
            std::array<float, effectdsp::maxChannels> diffused {};
            std::array<float, effectdsp::maxChannels> wet {};

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                dry[channel] = buffer.getSample(static_cast<int>(channel), sample);
                auto x = dry[channel];
                x = diffusers[channel][0].process(x, 0.56f);
                x = diffusers[channel][1].process(x, -0.47f);
                diffused[channel] = x;
            }

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto other = channels > 1 ? diffused[1u - channel] : diffused[channel];
                const auto injected = diffused[channel] + 0.14f * other;
                auto sum = 0.0f;
                for (auto& comb : combs[channel])
                    sum += comb.process(injected, feedback, dampingCoefficient);
                auto tail = sum * 0.25f;
                tail = diffusers[channel][2].process(tail, 0.51f);
                tail = diffusers[channel][3].process(tail, -0.43f);
                wet[channel] = tail + 0.10f * diffused[channel];
            }

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto other = channels > 1 ? wet[1u - channel] : wet[channel];
                const auto stereoWet = wet[channel] + 0.16f * other;
                buffer.setSample(static_cast<int>(channel), sample,
                                 dry[channel] + (stereoWet - dry[channel]) * mixValue);
            }
        }
    }
}
