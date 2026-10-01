#include "PresetState.h"

namespace solaris
{
    juce::ValueTree PresetState::create(juce::AudioProcessorValueTreeState& parameters,
                                        const juce::String& ampModelId,
                                        const juce::ValueTree& cabinetState,
                                        const juce::ValueTree& eqState,
                                        const juce::ValueTree& pedalState,
                                        const juce::ValueTree& suppliedAmpState)
    {
        juce::ValueTree preset("SOLARIS_PRESET");
        preset.setProperty("schemaVersion", currentSchemaVersion, nullptr);
        preset.setProperty("productId", "solaris-silverline", nullptr);
        preset.setProperty("ampModelId", ampModelId, nullptr);
        preset.addChild(parameters.copyState(), -1, nullptr);

        juce::ValueTree globals("GLOBALS");
        if (auto* value = parameters.getRawParameterValue("inputGain"))
            globals.setProperty("inputGainDb", value->load(), nullptr);
        if (auto* value = parameters.getRawParameterValue("outputGain"))
            globals.setProperty("outputGainDb", value->load(), nullptr);
        preset.addChild(globals, -1, nullptr);

        juce::ValueTree amp = suppliedAmpState.isValid() ? suppliedAmpState.createCopy()
                                                        : juce::ValueTree("AMP_STATE");
        if (!amp.hasType("AMP_STATE"))
            amp = juce::ValueTree("AMP_STATE");
        amp.setProperty("backendId", ampModelId, nullptr);
        preset.addChild(amp, -1, nullptr);

        preset.addChild(cabinetState.isValid() ? cabinetState.createCopy() : juce::ValueTree("CAB"), -1, nullptr);
        preset.addChild(eqState.isValid() ? eqState.createCopy() : juce::ValueTree("EQ"), -1, nullptr);
        preset.addChild(pedalState.isValid() ? pedalState.createCopy() : juce::ValueTree("PEDALS"), -1, nullptr);
        return preset;
    }

    juce::ValueTree PresetState::normalise(const juce::ValueTree& incoming,
                                           juce::Identifier parameterStateType)
    {
        if (!incoming.isValid())
            return {};

        if (incoming.hasType("SOLARIS_PRESET"))
        {
            if (schemaVersion(incoming) > currentSchemaVersion)
                return {};
            return migrateToCurrent(incoming.createCopy(), parameterStateType);
        }

        if (incoming.hasType(parameterStateType))
        {
            juce::ValueTree wrapped("SOLARIS_PRESET");
            wrapped.setProperty("schemaVersion", 0, nullptr);
            wrapped.setProperty("productId", "solaris-silverline", nullptr);
            wrapped.setProperty("ampModelId", "silverline68", nullptr);
            wrapped.addChild(incoming.createCopy(), -1, nullptr);
            return migrateToCurrent(std::move(wrapped), parameterStateType);
        }

        return {};
    }

    juce::ValueTree PresetState::parameterState(const juce::ValueTree& preset, juce::Identifier type) { return preset.getChildWithName(type); }
    juce::ValueTree PresetState::ampState(const juce::ValueTree& preset) { return preset.getChildWithName("AMP_STATE"); }
    juce::ValueTree PresetState::cabinetState(const juce::ValueTree& preset) { return preset.getChildWithName("CAB"); }
    juce::ValueTree PresetState::eqState(const juce::ValueTree& preset) { return preset.getChildWithName("EQ"); }
    juce::ValueTree PresetState::pedalState(const juce::ValueTree& preset) { return preset.getChildWithName("PEDALS"); }

    juce::String PresetState::ampModelId(const juce::ValueTree& preset)
    {
        const auto amp = ampState(preset);
        if (amp.isValid())
            return amp.getProperty("backendId", preset.getProperty("ampModelId", "silverline68")).toString();
        return preset.getProperty("ampModelId", "silverline68").toString();
    }

    juce::String PresetState::neuralModelPath(const juce::ValueTree& preset)
    {
        const auto amp = ampState(preset);
        return amp.isValid() ? amp.getProperty("neuralModelPath").toString() : juce::String();
    }

    int PresetState::schemaVersion(const juce::ValueTree& preset) noexcept
    {
        return preset.isValid() ? static_cast<int>(preset.getProperty("schemaVersion", 0)) : -1;
    }

    juce::ValueTree PresetState::migrateToCurrent(juce::ValueTree preset,
                                                  juce::Identifier parameterStateType)
    {
        auto version = schemaVersion(preset);

        if (version < 1)
        {
            if (!preset.getChildWithName("GLOBALS").isValid()) preset.addChild(juce::ValueTree("GLOBALS"), -1, nullptr);
            if (!preset.getChildWithName("CAB").isValid()) preset.addChild(juce::ValueTree("CAB"), -1, nullptr);
            if (!preset.getChildWithName("EQ").isValid()) preset.addChild(juce::ValueTree("EQ"), -1, nullptr);
            if (!preset.getChildWithName("PEDALS").isValid()) preset.addChild(juce::ValueTree("PEDALS"), -1, nullptr);
            version = 1;
        }

        if (version < 2)
        {
            juce::ValueTree amp("AMP_STATE");
            amp.setProperty("backendId", preset.getProperty("ampModelId", "silverline68"), nullptr);

            const auto params = preset.getChildWithName(parameterStateType);
            if (params.isValid() && params.hasProperty("neuralModelPath"))
                amp.setProperty("neuralModelPath", params.getProperty("neuralModelPath"), nullptr);

            preset.addChild(amp, -1, nullptr);
            version = 2;
        }

        preset.setProperty("schemaVersion", version, nullptr);
        return preset;
    }
}
