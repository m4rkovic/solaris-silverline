#pragma once
#include <JuceHeader.h>

namespace solaris
{
    class PresetState
    {
    public:
        static constexpr int currentSchemaVersion = 2;

        static juce::ValueTree create(juce::AudioProcessorValueTreeState& parameters,
                                      const juce::String& ampModelId,
                                      const juce::ValueTree& cabinetState,
                                      const juce::ValueTree& eqState,
                                      const juce::ValueTree& pedalState = {},
                                      const juce::ValueTree& ampState = {});

        static juce::ValueTree normalise(const juce::ValueTree& incoming,
                                         juce::Identifier parameterStateType);
        static juce::ValueTree parameterState(const juce::ValueTree& preset,
                                              juce::Identifier parameterStateType);
        static juce::ValueTree ampState(const juce::ValueTree& preset);
        static juce::ValueTree cabinetState(const juce::ValueTree& preset);
        static juce::ValueTree eqState(const juce::ValueTree& preset);
        static juce::ValueTree pedalState(const juce::ValueTree& preset);
        static juce::String ampModelId(const juce::ValueTree& preset);
        static juce::String neuralModelPath(const juce::ValueTree& preset);
        static int schemaVersion(const juce::ValueTree& preset) noexcept;

    private:
        static juce::ValueTree migrateToCurrent(juce::ValueTree preset,
                                                juce::Identifier parameterStateType);
    };
}
