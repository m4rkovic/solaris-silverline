#pragma once

#include "IAmpModel.h"
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace NeuralAudio
{
    class NeuralModel;
}

namespace solaris
{
    struct NeuralModelMetadata
    {
        juce::String filePath;
        juce::String displayName;
        juce::String modelVersion;
        juce::String modeledBy;
        juce::String gearMake;
        juce::String gearModel;
        juce::String gearType;
        juce::String toneType;
        double effectiveSampleRate = 0.0;
        int receptiveFieldSamples = -1;
    };

    class NeuralAmpModel final : public IAmpModel
    {
    public:
        NeuralAmpModel();
        ~NeuralAmpModel() override;

        static AmpMetadata staticMetadata();
        const AmpMetadata& metadata() const noexcept override { return info; }

        void prepare(const AmpPrepareSpec& spec) override;
        void setParameters(const AmpParameters& parameters) noexcept override;
        void process(juce::AudioBuffer<float>& buffer) noexcept override;
        void reset() noexcept override;

        // Non realtime API. File parsing, model construction and prewarming happen here.
        bool loadFromFile(const juce::File& modelFile);

        bool isLoaded() const noexcept { return models[0] != nullptr; }
        const NeuralModelMetadata& modelMetadata() const noexcept { return loadedMetadata; }
        const juce::String& loadedModelName() const noexcept { return loadedMetadata.displayName; }
        const juce::String& lastLoadError() const noexcept { return loadError; }
        double hostSampleRate() const noexcept { return sampleRate; }

    private:
        static constexpr std::size_t maxChannels = 2;

        AmpMetadata info;
        AmpParameters currentParameters {};
        std::array<std::unique_ptr<NeuralAudio::NeuralModel>, maxChannels> models;

        juce::AudioBuffer<float> inputScratch;
        juce::AudioBuffer<float> outputScratch;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enabledMix;

        double sampleRate = 48000.0;
        std::size_t maximumBlockSize = 512;
        std::size_t activeChannels = 2;
        float inputGain = 1.0f;
        float outputGain = 1.0f;
        NeuralModelMetadata loadedMetadata;
        juce::String loadError;
    };
}
