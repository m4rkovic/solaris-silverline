#include "NeuralAmpModel.h"

#if SOLARIS_ENABLE_NEURAL_AUDIO
#include <NeuralAudio/NeuralModel.h>
#else
namespace NeuralAudio { class NeuralModel {}; }
#endif

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>

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
        const auto nextSampleRate = juce::jmax(1.0, spec.sampleRate);
        const auto hostRateChanged = loadedForHostSampleRate > 0.0
            && std::abs(loadedForHostSampleRate - nextSampleRate) > 1.0;

        sampleRate = nextSampleRate;
        maximumBlockSize = juce::jmax<std::size_t>(1u, static_cast<std::size_t>(spec.maximumBlockSize));
        activeChannels = juce::jlimit<std::size_t>(1u, maxChannels, static_cast<std::size_t>(spec.numChannels));

        // NAM is mono internally, matching the reference NeuralAmpModeler plugin.
        // The processor collapses the guitar input before this stage and we
        // broadcast the single model output back to the host channel count.
        inputScratch.setSize(1, static_cast<int>(maximumBlockSize), false, false, true);
        outputScratch.setSize(1, static_cast<int>(maximumBlockSize), false, false, true);

        enabledMix.reset(sampleRate, 0.012);
        enabledMix.setCurrentAndTargetValue(currentParameters.enabled ? 1.0f : 0.0f);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        if (hostRateChanged && models[0] != nullptr)
        {
            models[0].reset();
            models[1].reset();

            loadedForHostSampleRate = 0.0;
            loadError = "NAM needs to be reloaded for the new host sample rate.";
        }
        else
        {
            if (models[0] != nullptr)
                models[0]->SetMaxAudioBufferSize(static_cast<int>(maximumBlockSize));
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
            const auto path = std::filesystem::path(modelFile.getFullPathName().toStdString());

            // Inspect only enough JSON to decide whether the pinned NeuralAudio
            // backend can honour the host rate without a separate realtime SRC.
            std::ifstream metadataStream(path, std::ifstream::binary);
            nlohmann::json modelJson;
            metadataStream >> modelJson;

            const auto architecture = modelJson.value("architecture", std::string {});
            const auto sourceRate = modelJson.contains("sample_rate") && modelJson.at("sample_rate").is_number()
                ? modelJson.at("sample_rate").get<double>()
                : 48000.0;

            const auto hostRate = static_cast<int>(std::lround(sampleRate));
            const auto modelRate = static_cast<int>(std::lround(sourceRate));
            const auto exactRate = std::abs(sourceRate - sampleRate) <= 1.0;
            const auto waveNetIntegerMultiple =
                architecture == "WaveNet"
                && modelRate > 0
                && hostRate >= modelRate
                && (hostRate % modelRate) == 0;

            if (!exactRate && !waveNetIntegerMultiple)
            {
                loadError = "NAM model rate is "
                    + juce::String(sourceRate, 0)
                    + " Hz and the host is "
                    + juce::String(sampleRate, 0)
                    + " Hz. This architecture has no supported load-time rate adaptation; general realtime SRC is intentionally disabled.";
                return false;
            }

            NeuralAudio::NeuralModelLoader loader;
            loader.SetExternalSampleRate(hostRate);
            loader.SetDefaultMaxAudioBufferSize(static_cast<int>(maximumBlockSize));
            loader.SetAudioInputLevelDBu(12.0f);

            std::array<std::unique_ptr<NeuralAudio::NeuralModel>, maxChannels> loaded;
            loaded[0].reset(loader.CreateFromFile(path, true));
            if (loaded[0] == nullptr)
            {
                loadError = "NeuralAudio could not construct this NAM model.";
                return false;
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
            metadata.modelSampleRate = sourceRate;
            metadata.effectiveSampleRate = sampleRate;
            metadata.sampleRateMode = exactRate ? "native" : "wavenet-integer-multiple";
            metadata.receptiveFieldSamples = loaded[0]->GetReceptiveFieldSize();

            if (metadata.displayName.isEmpty())
                metadata.displayName = modelFile.getFileNameWithoutExtension();

            inputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedInputDBAdjustment());
            outputGain = juce::Decibels::decibelsToGain(loaded[0]->GetRecommendedOutputDBAdjustment());

            models = std::move(loaded);
            loadedMetadata = std::move(metadata);
            loadedForHostSampleRate = sampleRate;
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

        const auto* source = buffer.getReadPointer(0);
        auto* modelInput = inputScratch.getWritePointer(0);
        auto* modelOutput = outputScratch.getWritePointer(0);
        juce::FloatVectorOperations::multiply(modelInput, source, inputGain, numSamples);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        models[0]->Process(modelInput, modelOutput, static_cast<std::size_t>(numSamples));
       #endif

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto mix = enabledMix.getNextValue();
            const auto wet = outputScratch.getSample(0, sample) * outputGain;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto dry = buffer.getSample(channel, sample);
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
