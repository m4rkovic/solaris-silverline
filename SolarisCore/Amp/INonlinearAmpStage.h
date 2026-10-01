#pragma once

#include "AmpParameters.h"
#include <JuceHeader.h>

namespace solaris
{
    // Replaceable nonlinear backend used by an amp model. The current Silverline 68
    // implementation is analogue DSP; a future neural backend can implement the same
    // contract without changing IAmpModel or the plugin shell.
    class INonlinearAmpStage
    {
    public:
        virtual ~INonlinearAmpStage() = default;

        virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
        virtual void setParameters(const AmpParameters& parameters) noexcept = 0;
        virtual void process(juce::AudioBuffer<float>& buffer) noexcept = 0;
        virtual void reset() noexcept = 0;
    };
}
