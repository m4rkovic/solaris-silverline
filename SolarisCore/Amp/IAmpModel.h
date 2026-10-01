#pragma once

#include <JuceHeader.h>
#include <string>

namespace solaris
{
    struct AmpPrepareSpec
    {
        double sampleRate = 44100.0;
        juce::uint32 maximumBlockSize = 512;
        juce::uint32 numChannels = 2;
    };

    struct AmpMetadata
    {
        std::string id;
        std::string displayName;
        std::string family;
    };

    class IAmpModel
    {
    public:
        virtual ~IAmpModel() = default;

        virtual const AmpMetadata& metadata() const noexcept = 0;
        virtual void prepare(const AmpPrepareSpec& spec) = 0;
        virtual void process(juce::AudioBuffer<float>& buffer) noexcept = 0;
        virtual void reset() noexcept = 0;
    };
}
