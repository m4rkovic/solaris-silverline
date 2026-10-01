#pragma once

#include <JuceHeader.h>
#include <vector>

namespace solaris
{
    struct FactoryIRDescriptor
    {
        juce::String id;
        juce::String displayName;
        juce::String binaryResourceName;
        juce::String licenseId;
        bool stereo = false;
    };

    // Intentionally empty until every bundled IR has explicit redistribution rights.
    // This gives the release pack a stable manifest shape without shipping mystery audio.
    class FactoryIRCatalog
    {
    public:
        static const std::vector<FactoryIRDescriptor>& entries()
        {
            static const std::vector<FactoryIRDescriptor> catalog;
            return catalog;
        }
    };
}
