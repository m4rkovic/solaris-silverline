#pragma once

#include "EffectDSP.h"
#include "IEffect.h"
#include <array>
#include <vector>

namespace solaris
{
    class AnalogChorus final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "chorus"; }
        void setRate(float value) noexcept { rate.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setDepth(float value) noexcept { depth.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setMix(float value) noexcept { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;

        effectdsp::CubicDelayLine delay;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rate, depth, mix;
        std::array<float, effectdsp::maxChannels> feedback {};
        double phase = 0.0;
    };

    class MultiStagePhaser final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "orbit"; }
        void setRate(float value) noexcept { rate.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setDepth(float value) noexcept { depth.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setMix(float value) noexcept { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        static constexpr std::size_t numStages = 6;
        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;
        void updateCoefficients(float centreHz) noexcept;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rate, depth, mix;
        std::array<std::array<float, numStages>, effectdsp::maxChannels> state {};
        std::array<float, numStages> coefficients {};
        std::array<float, effectdsp::maxChannels> feedbackState {};
        double phase = 0.0;
        int coefficientCountdown = 0;
    };

    class BiasTremolo final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "pulse"; }
        void setRate(float value) noexcept { rate.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setDepth(float value) noexcept { depth.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setShape(float value) noexcept { shape.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> rate, depth, shape;
        double phase = 0.0;
    };

    class AnalogDelay final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "echo-404"; }
        void setTime(float value) noexcept { time.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setFeedback(float value) noexcept { regeneration.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setMix(float value) noexcept { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        double tailLengthSeconds() const noexcept override { return 5.0; }

    private:
        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;

        effectdsp::CubicDelayLine delay;
        effectdsp::OnePoleLowpass feedbackLowpass;
        effectdsp::OnePoleHighpass feedbackHighpass;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> time, regeneration, mix;
        std::array<float, effectdsp::maxChannels> previousDelayed {};
        double modulationPhase = 0.0;
    };

    class SpringSpaceReverb final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "sanctum"; }
        void setDecay(float value) noexcept { decay.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setTone(float value) noexcept { tone.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setMix(float value) noexcept { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        double tailLengthSeconds() const noexcept override { return 6.0; }

    private:
        struct AllpassLine
        {
            void prepare(double sampleRate, float delayMs);
            void reset() noexcept;
            float process(float input, float feedback) noexcept;
            std::vector<float> buffer;
            std::size_t index = 0;
        };

        struct DampedComb
        {
            void prepare(double sampleRate, float delayMs);
            void reset() noexcept;
            float process(float input, float feedback, float damping) noexcept;
            std::vector<float> buffer;
            std::size_t index = 0;
            float dampingState = 0.0f;
        };

        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;

        static constexpr std::size_t diffuserCount = 4;
        static constexpr std::size_t combCount = 4;
        std::array<std::array<AllpassLine, diffuserCount>, effectdsp::maxChannels> diffusers;
        std::array<std::array<DampedComb, combCount>, effectdsp::maxChannels> combs;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> decay, tone, mix;
        float dampingCoefficient = 0.25f;
        int dampingCountdown = 0;
    };
}
