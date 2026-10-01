#pragma once
#include <JuceHeader.h>

namespace solaris
{
    class PresetState
    {
    public:
        static constexpr int currentSchemaVersion=1;
        static juce::ValueTree create(juce::AudioProcessorValueTreeState& parameters,
                                      const juce::String& ampModelId,
                                      const juce::ValueTree& cabinetState,
                                      const juce::ValueTree& eqState,
                                      const juce::ValueTree& pedalState={});
        static juce::ValueTree normalise(const juce::ValueTree& incoming,juce::Identifier parameterStateType);
        static juce::ValueTree parameterState(const juce::ValueTree& preset,juce::Identifier parameterStateType);
        static juce::ValueTree cabinetState(const juce::ValueTree& preset);
        static juce::ValueTree eqState(const juce::ValueTree& preset);
        static juce::ValueTree pedalState(const juce::ValueTree& preset);
        static juce::String ampModelId(const juce::ValueTree& preset);
    private:
        static juce::ValueTree migrateToCurrent(juce::ValueTree preset);
    };
}
