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

    class FactoryIRCatalog
    {
    public:
        static const std::vector<FactoryIRDescriptor>& entries()
        {
            static const std::vector<FactoryIRDescriptor> catalog {
                {
                    "factory-open-back-1x12",
                    "Factory Open Back 1x12",
                    "embedded:0xfx-cc0-open-back-1x12",
                    "CC0-1.0",
                    true
                }
            };
            return catalog;
        }
    };
}
