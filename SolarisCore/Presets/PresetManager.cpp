#include "PresetManager.h"
#include <algorithm>

namespace solaris
{
    PresetManager::PresetManager() : PresetManager(defaultUserPresetDirectory()) {}
    PresetManager::PresetManager(juce::File directory) : userDirectory(std::move(directory)) {}

    juce::File PresetManager::defaultUserPresetDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Solaris Audio")
            .getChildFile("Silverline")
            .getChildFile("Presets");
    }

    void PresetManager::registerFactoryPreset(juce::String name, juce::ValueTree state)
    {
        if (!state.isValid())
            return;
        factoryPresets.push_back({ std::move(name), PresetKind::factory, {}, state.createCopy() });
    }

    std::vector<PresetDescriptor> PresetManager::listPresets() const
    {
        auto result = factoryPresets;
        if (userDirectory.isDirectory())
        {
            for (const auto& file : userDirectory.findChildFiles(juce::File::findFiles, false, "*" + juce::String(extension)))
                result.push_back({ file.getFileNameWithoutExtension(), PresetKind::user, file, {} });
        }

        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b)
        {
            if (a.kind != b.kind)
                return a.kind == PresetKind::factory;
            return a.name.compareNatural(b.name) < 0;
        });
        return result;
    }

    juce::Result PresetManager::saveUserPreset(const juce::String& name, const juce::ValueTree& state)
    {
        if (!state.isValid() || !state.hasType("SOLARIS_PRESET"))
            return juce::Result::fail("Preset state is invalid.");

        const auto legalName = juce::File::createLegalFileName(name.trim());
        if (legalName.isEmpty())
            return juce::Result::fail("Preset name is empty.");

        if (userDirectory.createDirectory().failed())
            return juce::Result::fail("Could not create user preset directory: " + userDirectory.getFullPathName());

        auto xml = state.createXml();
        if (xml == nullptr)
            return juce::Result::fail("Could not serialize preset state.");

        const auto target = userDirectory.getChildFile(legalName + extension);
        juce::TemporaryFile temporary(target);
        if (!temporary.getFile().replaceWithText(xml->toString()))
            return juce::Result::fail("Could not write temporary preset file.");
        if (!temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail("Could not atomically replace preset file.");
        return juce::Result::ok();
    }

    juce::ValueTree PresetManager::loadPreset(const PresetDescriptor& descriptor,
                                               juce::Identifier parameterStateType,
                                               juce::String& error) const
    {
        error.clear();
        juce::ValueTree raw;

        if (descriptor.kind == PresetKind::factory)
        {
            raw = descriptor.factoryState.createCopy();
        }
        else
        {
            if (!descriptor.file.existsAsFile())
            {
                error = "Preset file is missing: " + descriptor.file.getFullPathName();
                return {};
            }
            auto xml = juce::XmlDocument::parse(descriptor.file);
            if (xml == nullptr)
            {
                error = "Preset XML could not be parsed.";
                return {};
            }
            raw = juce::ValueTree::fromXml(*xml);
        }

        auto normalised = PresetState::normalise(raw, parameterStateType);
        if (!normalised.isValid())
            error = "Preset schema is newer than this plugin or the file is invalid.";
        return normalised;
    }
}
