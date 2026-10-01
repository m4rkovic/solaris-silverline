#include "NeuralAmpModel.h"

#if SOLARIS_ENABLE_NEURAL_AUDIO
#include <NeuralAudio/NeuralModel.h>
#else
// Keeps the private unique_ptr storage well-formed in lightweight builds where
// the NeuralAudio dependency is intentionally not fetched. No instance of this
// stub is ever created.
namespace NeuralAudio { class NeuralModel {}; }
#endif

#include <cmath>
#include <filesystem>

namespace solaris
{
    AmpMetadata NeuralAmpModel::staticMetadata()
    {
        return {
            "neural-nam",
            "Neural NAM",
            "NeuralAudio / Neural Amp Modeler"
        };
    }

    NeuralAmpModel::NeuralAmpModel()
        : info(staticMetadata())
    {
    }

    NeuralAmpModel::~NeuralAmpModel() = default;

    void NeuralAmpModel::prepare(const AmpPrepareSpec& spec)
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        maximumBlockSize = juce::jmax<std::size_t>(1u, static_cast<std::size_t>(spec.maximumBlockSize));
        activeChannels = juce::jlimit<std::size_t>(1u, maxChannels, static_cast<std::size_t>(spec.numChannels));

        inputScratch.setSize(static_cast<int>(activeChannels),
                             static_cast<int>(maximumBlockSize), false, false, true);
        outputScratch.setSize(static_cast<int>(activeChannels),
                              static_cast<int>(maximumBlockSize), false, false, true);

        enabledMix.reset(sampleRate, 0.012);
        enabledMix.setCurrentAndTargetValue(currentParameters.enabled ? 1.0f : 0.0f);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        for (auto& model : models)
        {
            if (model != nullptr)
                model->SetMaxAudioBufferSize(static_cast<int>(maximumBlockSize));
        }

        if (models[0] != nullptr
            && std::abs(static_cast<double>(models[0]->GetSampleRate()) - sampleRate) > 1.0)
        {
            for (auto& model : models)
                model.reset();

            modelName.clear();
            loadError = "Loaded NAM was released because the host sample rate changed.";
        }
       #endif

        reset();
    }

    void NeuralAmpModel::setParameters(const AmpParameters& parameters) noexcept
    {
        currentParameters = parameters;
        currentParameters.clampToValidRange();
        enabledMix.setTargetValue(currentParameters.enabled ? 1.0f : 0.0f);
    }

    bool NeuralAmpModel::loadFromFile(const juce::File& modelFile)
    {
        loadError.clear();
        modelName.clear();

       #if !SOLARIS_ENABLE_NEURAL_AUDIO
        juce::ignoreUnused(modelFile);
        loadError = "NeuralAudio support is disabled in this build.";
        return false;
       #else
        if (!modelFile.existsAsFile() || !modelFile.hasFileExtension("nam"))
        {
            loadError = "Select a valid .nam model file.";
            return false;
        }

        NeuralAudio::NeuralModelLoader loader;
        loader.SetExternalSampleRate(static_cast<int>(std::lround(sampleRate)));
        loader.SetDefaultMaxAudioBufferSize(static_cast<int>(maximumBlockSize));
        loader.SetAudioInputLevelDBu(12.0f);

        std::array<std::unique_ptr<NeuralAudio::NeuralModel>, maxChannels> loaded;
        const auto path = std::filesystem::path(modelFile.getFullPathName().toStdString());

        for (std::size_t channel = 0; channel < activeChannels; ++channel)
        {
            loaded[channel].reset(loader.CreateFromFile(path, true));
            if (loaded[channel] == nullptr)
            {
                loadError = "NeuralAudio could not load this NAM model.";
                return false;
            }

            // For now the neural path is intentionally exact-rate only. Arbitrary
            // sample-rate conversion will be added as a dedicated realtime stage.
            if (std::abs(static_cast<double>(loaded[channel]->GetSampleRate()) - sampleRate) > 1.0)
            {
                loadError = "NAM sample rate does not match the host sample rate.";
                return false;
            }
        }

        inputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedInputDBAdjustment());
        outputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedOutputDBAdjustment());
        models = std::move(loaded);
        modelName = modelFile.getFileNameWithoutExtension();
        return true;
       #endif
    }

    void NeuralAmpModel::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (models[0] == nullptr)
            return;

        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = juce::jmin<int>(static_cast<int>(activeChannels), buffer.getNumChannels());

        if (numSamples <= 0 || static_cast<std::size_t>(numSamples) > maximumBlockSize)
            return;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* source = buffer.getReadPointer(channel);
            auto* modelInput = inputScratch.getWritePointer(channel);
            auto* modelOutput = outputScratch.getWritePointer(channel);

            for (int sample = 0; sample < numSamples; ++sample)
                modelInput[sample] = source[sample] * inputGain;

           #if SOLARIS_ENABLE_NEURAL_AUDIO
            models[static_cast<std::size_t>(channel)]->Process(
                modelInput, modelOutput, static_cast<std::size_t>(numSamples));
           #endif
        }

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto mix = enabledMix.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto dry = buffer.getSample(channel, sample);
                const auto wet = outputScratch.getSample(channel, sample) * outputGain;
                buffer.setSample(channel, sample, dry + (wet - dry) * mix);
            }
        }
    }

    void NeuralAmpModel::reset() noexcept
    {
        inputScratch.clear();
        outputScratch.clear();
    }
}
