#pragma once

#include "../../SolarisCore/Amp/IAmpModel.h"
#include "../../SolarisCore/Amp/INonlinearAmpStage.h"
#include <array>
#include <memory>
#include <vector>

namespace solaris
{
    class Silverline68Amp final : public IAmpModel
    {
    public:
        Silverline68Amp();
        ~Silverline68Amp() override = default;

        static AmpMetadata staticMetadata();
        const AmpMetadata& metadata() const noexcept override { return info; }

        void prepare(const AmpPrepareSpec& spec) override;
        void setParameters(const AmpParameters& parameters) noexcept override;
        void process(juce::AudioBuffer<float>& buffer) noexcept override;
        void reset() noexcept override;
        double tailLengthSeconds() const noexcept override { return 1.2; }

    private:
        static constexpr std::size_t maxChannels = 2;

        struct OnePoleLowpass
        {
            void prepare(double sampleRate, float cutoffHz) noexcept;
            float process(std::size_t channel, float input) noexcept;
            void reset() noexcept;

            float coefficient = 0.0f;
            std::array<float, maxChannels> state {};
        };

        struct SpringPrototype
        {
            void prepare(double sampleRate, std::size_t channels);
            void reset() noexcept;
            float process(std::size_t channel, float input) noexcept;
            void advance() noexcept;

            static constexpr std::size_t maxChannels = Silverline68Amp::maxChannels;
            std::array<std::vector<float>, maxChannels> delayBuffer;
            std::array<float, maxChannels> feedback {};
            std::array<float, maxChannels> dampingState {};
            std::size_t writeIndex = 0;
            std::size_t delaySize = 1;
            std::size_t tapA = 1;
            std::size_t tapB = 1;
            std::size_t tapC = 1;
            std::size_t activeChannels = 2;
            float dampingCoefficient = 0.0f;
        };

        AmpMetadata info;
        AmpParameters currentParameters {};
        std::unique_ptr<INonlinearAmpStage> nonlinearStage;

        OnePoleLowpass inputLowTracker;
        OnePoleLowpass preEmphasisTracker;
        OnePoleLowpass outputLowpass;
        SpringPrototype spring;
        juce::AudioBuffer<float> dryBuffer;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enabledMix;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> voicingBlend;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> reverbMix;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> tremoloSpeed;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> tremoloIntensity;

        double sampleRate = 44100.0;
        double tremoloPhase = 0.0;
        std::size_t activeChannels = 2;
        std::size_t maximumBlockSize = 512;
    };
}
