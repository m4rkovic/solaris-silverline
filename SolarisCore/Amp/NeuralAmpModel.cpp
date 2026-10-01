#include "NeuralAmpModel.h"

#if SOLARIS_ENABLE_NEURAL_AUDIO
#include <NeuralAudio/NeuralModel.h>
#else
namespace NeuralAudio { class NeuralModel {}; }
#endif

#include <cmath>
#include <filesystem>
#include <exception>

namespace solaris
{
    AmpMetadata NeuralAmpModel::staticMetadata()
    {
        return { "neural-nam", "Neural NAM", "NeuralAudio / Neural Amp Modeler" };
    }

    NeuralAmpModel::NeuralAmpModel() : info(staticMetadata()) {}
    NeuralAmpModel::~NeuralAmpModel() = default;

    void NeuralAmpModel::prepare(const AmpPrepareSpec& spec)
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        maximumBlockSize = juce::jmax<std::size_t>(1u, static_cast<std::size_t>(spec.maximumBlockSize));
        activeChannels = juce::jlimit<std::size_t>(1u, maxChannels, static_cast<std::size_t>(spec.numChannels));

        inputScratch.setSize(static_cast<int>(activeChannels), static_cast<int>(maximumBlockSize), false, false, true);
        outputScratch.setSize(static_cast<int>(activeChannels), static_cast<int>(maximumBlockSize), false, false, true);

        enabledMix.reset(sampleRate, 0.012);
        enabledMix.setCurrentAndTargetValue(currentParameters.enabled ? 1.0f : 0.0f);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        for (auto& model : models)
            if (model != nullptr)
                model->SetMaxAudioBufferSize(static_cast<int>(maximumBlockSize));

        // NeuralAudio performs supported rate adaptation while a model is loaded.
        // If the host changes sample rate afterwards, the model must be rebuilt so
        // the loader can apply the new external sample rate correctly.
        if (models[0] != nullptr && std::abs(static_cast<double>(models[0]->GetSampleRate()) - sampleRate) > 1.0)
        {
            for (auto& model : models)
                model.reset();
            loadError = "NAM needs to be reloaded for the new host sample rate.";
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

       #if !SOLARIS_ENABLE_NEURAL_AUDIO
        juce::ignoreUnused(modelFile);
        loadError = "NeuralAudio support is disabled in this build.";
        return false;
       #else
        if (!modelFile.existsAsFile())
        {
            loadError = "NAM file no longer exists: " + modelFile.getFullPathName();
            return false;
        }

        if (!modelFile.hasFileExtension("nam"))
        {
            loadError = "Select a .nam model file.";
            return false;
        }

        try
        {
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
                    loadError = "NeuralAudio could not construct this NAM model.";
                    return false;
                }

                const auto effectiveRate = static_cast<double>(loaded[channel]->GetSampleRate());
                if (std::abs(effectiveRate - sampleRate) > 1.0)
                {
                    loadError = "NAM effective sample rate is "
                        + juce::String(effectiveRate, 0)
                        + " Hz, but the host is "
                        + juce::String(sampleRate, 0)
                        + " Hz. General realtime SRC is not enabled for this model type.";
                    return false;
                }
            }

            NeuralModelMetadata metadata;
            metadata.filePath = modelFile.getFullPathName();
            metadata.modelVersion = loaded[0]->GetModelVersion();
            metadata.modeledBy = loaded[0]->GetMetadata("modeled_by");
            metadata.gearMake = loaded[0]->GetMetadata("gear_make");
            metadata.gearModel = loaded[0]->GetMetadata("gear_model");
            metadata.gearType = loaded[0]->GetMetadata("gear_type");
            metadata.toneType = loaded[0]->GetMetadata("tone_type");
            metadata.displayName = loaded[0]->GetMetadata("name");
            metadata.effectiveSampleRate = loaded[0]->GetSampleRate();
            metadata.receptiveFieldSamples = loaded[0]->GetReceptiveFieldSize();

            if (metadata.displayName.isEmpty())
                metadata.displayName = modelFile.getFileNameWithoutExtension();

            inputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedInputDBAdjustment());
            outputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedOutputDBAdjustment());

            models = std::move(loaded);
            loadedMetadata = std::move(metadata);
            reset();
            return true;
        }
        catch (const std::exception& e)
        {
            loadError = "Failed to load NAM: " + juce::String(e.what());
        }
        catch (...)
        {
            loadError = "Failed to load NAM because the model loader raised an unknown error.";
        }

        return false;
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

            juce::FloatVectorOperations::multiply(modelInput, source, inputGain, numSamples);

           #if SOLARIS_ENABLE_NEURAL_AUDIO
            models[static_cast<std::size_t>(channel)]->Process(modelInput, modelOutput, static_cast<std::size_t>(numSamples));
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
