#pragma once

#include "../../SolarisCore/Amp/INonlinearAmpStage.h"
#include <array>
#include <memory>

namespace solaris
{
    class Silverline68AnalogueStage final : public INonlinearAmpStage
    {
    public:
        Silverline68AnalogueStage() = default;
        ~Silverline68AnalogueStage() override = default;

        void prepare(const juce::dsp::ProcessSpec& spec) override;
        void setParameters(const AmpParameters& parameters) noexcept override;
        void process(juce::AudioBuffer<float>& buffer) noexcept override;
        void reset() noexcept override;

    private:
        static constexpr std::size_t maxChannels = 2;
        static constexpr std::size_t oversamplingStages = 2; // 2^2 = 4x

        struct OnePoleLowpass
        {
            void prepare(double sampleRate, float cutoffHz) noexcept;
            float process(std::size_t channel, float input) noexcept;
            void reset() noexcept;

            float coefficient = 0.0f;
            std::array<float, maxChannels> state {};
        };

        struct ChannelState
        {
            float dcState = 0.0f;
            float sagEnvelope = 0.0f;
        };

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        OnePoleLowpass bassBand;
        OnePoleLowpass trebleBand;
        OnePoleLowpass lowTightener;
        OnePoleLowpass antiFizz;
        OnePoleLowpass antiFizz2;
        OnePoleLowpass dcTracker;
        std::array<ChannelState, maxChannels> channelState {};

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> volume;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bass;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> treble;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> channelBlend;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mid;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> presence;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> master;

        double baseSampleRate = 44100.0;
        double oversampledRate = 176400.0;
        std::size_t activeChannels = 2;
    };
}
