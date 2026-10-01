#pragma once

#include "PresetState.h"
#include <JuceHeader.h>
#include <vector>

namespace solaris
{
    enum class PresetKind { factory, user };

    struct PresetDescriptor
    {
        juce::String name;
        PresetKind kind = PresetKind::user;
        juce::File file;
        juce::ValueTree factoryState;
    };

    class PresetManager
    {
    public:
        PresetManager();
        explicit PresetManager(juce::File userDirectory);

        static juce::File defaultUserPresetDirectory();
        const juce::File& userPresetDirectory() const noexcept { return userDirectory; }

        void registerFactoryPreset(juce::String name, juce::ValueTree state);
        std::vector<PresetDescriptor> listPresets() const;

        juce::Result saveUserPreset(const juce::String& name, const juce::ValueTree& state);
        juce::ValueTree loadPreset(const PresetDescriptor& descriptor,
                                   juce::Identifier parameterStateType,
                                   juce::String& error) const;

    private:
        static constexpr auto extension = ".silverlinepreset";
        juce::File userDirectory;
        std::vector<PresetDescriptor> factoryPresets;
    };
}
