#include "NeuralAmpModel.h"

#if SOLARIS_ENABLE_NEURAL_AUDIO
#include <NeuralAudio/NeuralModel.h>
#include <dsp/ResamplingContainer/ResamplingContainer.h>
#else
namespace NeuralAudio { class NeuralModel {}; }
namespace dsp
{
    template <typename T, int NCHANS, std::size_t A>
    class ResamplingContainer {};
}
#endif

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>

namespace solaris
{
    struct NeuralAmpModel::ResamplerHolder
    {
       #if SOLARIS_ENABLE_NEURAL_AUDIO
        explicit ResamplerHolder(double renderingSampleRate)
            : processor(std::make_unique<dsp::ResamplingContainer<float, 1, 12>>(
                renderingSampleRate))
        {
        }

        std::unique_ptr<dsp::ResamplingContainer<float, 1, 12>> processor;
       #else
        explicit ResamplerHolder(double) {}
       #endif
    };

    namespace
    {
        int modelBlockCapacity(std::size_t hostBlockSize,
                               double hostSampleRate,
                               double modelSampleRate) noexcept
        {
            const auto hostRate = juce::jmax(1.0, hostSampleRate);
            const auto modelRate = juce::jmax(1.0, modelSampleRate);
            const auto scaled = std::ceil(static_cast<double>(hostBlockSize)
                                          * modelRate / hostRate);
            return juce::jmax(1, static_cast<int>(scaled) + 16);
        }
    }

    AmpMetadata NeuralAmpModel::staticMetadata()
    {
        return { "neural-nam", "Neural NAM", "NeuralAudio / Neural Amp Modeler" };
    }

    NeuralAmpModel::NeuralAmpModel() : info(staticMetadata()) {}
    NeuralAmpModel::~NeuralAmpModel() = default;

    void NeuralAmpModel::prepare(const AmpPrepareSpec& spec)
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        maximumBlockSize = juce::jmax<std::size_t>(
            1u, static_cast<std::size_t>(spec.maximumBlockSize));

        inputScratch.setSize(1, static_cast<int>(maximumBlockSize), false, false, true);
        outputScratch.setSize(1, static_cast<int>(maximumBlockSize), false, false, true);

        enabledMix.reset(sampleRate, 0.012);
        enabledMix.setCurrentAndTargetValue(currentParameters.enabled ? 1.0f : 0.0f);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        resampler.reset();
        resamplerLatency = 0;

        if (model != nullptr)
        {
            model->SetMaxAudioBufferSize(
                modelBlockCapacity(maximumBlockSize, sampleRate, nativeModelSampleRate));

            if (std::abs(nativeModelSampleRate - sampleRate) > 1.0)
            {
                resampler = std::make_unique<ResamplerHolder>(nativeModelSampleRate);
                resampler->processor->Reset(sampleRate, static_cast<int>(maximumBlockSize));
                resamplerLatency = resampler->processor->GetLatency();
            }

            loadedMetadata.effectiveSampleRate = sampleRate;
            loadedMetadata.sampleRateMode = resampler != nullptr ? "lanczos-src" : "native";
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

            std::ifstream metadataStream(path, std::ifstream::binary);
            if (!metadataStream.good())
            {
                loadError = "NAM file could not be opened.";
                return false;
            }

            nlohmann::json modelJson;
            metadataStream >> modelJson;

            // Old NAM files did not always declare a rate. The reference NAM plug-in
            // makes the same 48 kHz assumption for those captures.
            const auto sourceRate = modelJson.contains("sample_rate")
                                 && modelJson.at("sample_rate").is_number()
                ? modelJson.at("sample_rate").get<double>()
                : 48000.0;

            if (sourceRate < 8000.0 || sourceRate > 384000.0)
            {
                loadError = "NAM reports an invalid model sample rate.";
                return false;
            }

            const auto nativeRate = static_cast<int>(std::lround(sourceRate));
            const auto internalCapacity =
                modelBlockCapacity(maximumBlockSize, sampleRate, sourceRate);

            NeuralAudio::NeuralModelLoader loader;

            // Keep the neural model at its native capture rate. Any host/model
            // mismatch is handled by the same style of realtime Lanczos wrapper
            // used by the official Neural Amp Modeler plug-in, rather than by
            // running a model at the wrong rate or simply resampling its output.
            loader.SetExternalSampleRate(nativeRate);
            loader.SetDefaultMaxAudioBufferSize(internalCapacity);
            loader.SetAudioInputLevelDBu(12.0f);

            std::unique_ptr<NeuralAudio::NeuralModel> loaded(
                loader.CreateFromFile(path, true));
            if (loaded == nullptr)
            {
                loadError = "NeuralAudio could not construct this NAM model.";
                return false;
            }

            std::unique_ptr<ResamplerHolder> loadedResampler;
            int loadedResamplerLatency = 0;
            if (std::abs(sourceRate - sampleRate) > 1.0)
            {
                loadedResampler = std::make_unique<ResamplerHolder>(sourceRate);
                loadedResampler->processor->Reset(
                    sampleRate, static_cast<int>(maximumBlockSize));
                loadedResamplerLatency = loadedResampler->processor->GetLatency();
            }

            NeuralModelMetadata metadata;
            metadata.filePath = modelFile.getFullPathName();
            metadata.modelVersion = loaded->GetModelVersion();
            metadata.modeledBy = loaded->GetMetadata("modeled_by");
            metadata.gearMake = loaded->GetMetadata("gear_make");
            metadata.gearModel = loaded->GetMetadata("gear_model");
            metadata.gearType = loaded->GetMetadata("gear_type");
            metadata.toneType = loaded->GetMetadata("tone_type");
            metadata.displayName = loaded->GetMetadata("name");
            metadata.modelSampleRate = sourceRate;
            metadata.effectiveSampleRate = sampleRate;
            metadata.sampleRateMode = loadedResampler != nullptr ? "lanczos-src" : "native";
            metadata.receptiveFieldSamples = loaded->GetReceptiveFieldSize();

            if (metadata.displayName.isEmpty())
                metadata.displayName = modelFile.getFileNameWithoutExtension();

            const auto recommendedInput =
                juce::jlimit(-24.0f, 24.0f, loaded->GetRecommendedInputDBAdjustment());
            const auto recommendedOutput =
                juce::jlimit(-36.0f, 36.0f, loaded->GetRecommendedOutputDBAdjustment());

            inputGain = juce::Decibels::decibelsToGain(recommendedInput);
            outputGain = juce::Decibels::decibelsToGain(recommendedOutput);

            model = std::move(loaded);
            resampler = std::move(loadedResampler);
            resamplerLatency = loadedResamplerLatency;
            nativeModelSampleRate = sourceRate;
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
        if (model == nullptr)
            return;

        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0
            || static_cast<std::size_t>(numSamples) > maximumBlockSize)
            return;

        const auto* source = buffer.getReadPointer(0);
        auto* modelInput = inputScratch.getWritePointer(0);
        auto* modelOutput = outputScratch.getWritePointer(0);
        juce::FloatVectorOperations::multiply(modelInput, source, inputGain, numSamples);

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        if (resampler != nullptr)
        {
            float* inputPointers[] { modelInput };
            float* outputPointers[] { modelOutput };

            try
            {
                resampler->processor->ProcessBlock(
                    inputPointers,
                    outputPointers,
                    numSamples,
                    [this](float** input, float** output, int frames)
                    {
                        model->Process(input[0], output[0],
                                       static_cast<std::size_t>(frames));
                    });
            }
            catch (...)
            {
                // Audio callbacks are noexcept. A defensive dry fallback is much
                // safer than terminating the host if an SRC invariant is violated.
                juce::FloatVectorOperations::copy(modelOutput, modelInput, numSamples);
            }
        }
        else
        {
            model->Process(modelInput, modelOutput,
                           static_cast<std::size_t>(numSamples));
        }
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

       #if SOLARIS_ENABLE_NEURAL_AUDIO
        if (resampler != nullptr)
        {
            try
            {
                resampler->processor->Reset(
                    sampleRate, static_cast<int>(maximumBlockSize));
                resamplerLatency = resampler->processor->GetLatency();
            }
            catch (...)
            {
                resampler.reset();
                resamplerLatency = 0;
            }
        }
       #endif
    }
}
