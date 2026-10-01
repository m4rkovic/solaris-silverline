#pragma once

#include "../../SolarisCore/Amp/IAmpModel.h"

namespace solaris
{
    class Silverline68Amp final : public IAmpModel
    {
    public:
        Silverline68Amp();

        const AmpMetadata& metadata() const noexcept override { return info; }

        void prepare(const AmpPrepareSpec& spec) override;
        void process(juce::AudioBuffer<float>& buffer) noexcept override;
        void reset() noexcept override;

    private:
        AmpMetadata info;
        double sampleRate = 44100.0;
    };
}
