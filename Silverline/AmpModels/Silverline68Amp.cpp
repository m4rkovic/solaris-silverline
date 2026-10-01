#include "Silverline68Amp.h"

namespace solaris
{
    Silverline68Amp::Silverline68Amp()
        : info{
            "silverline68",
            "Silverline 68",
            "American vintage clean / edge-of-breakup"
        }
    {
    }

    void Silverline68Amp::prepare(const AmpPrepareSpec& spec)
    {
        sampleRate = spec.sampleRate;
        juce::ignoreUnused(sampleRate);
    }

    void Silverline68Amp::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        // V0.1 is intentionally transparent.
        // Real amp DSP / neural inference plugs in here without touching the host shell.
        juce::ignoreUnused(buffer);
    }

    void Silverline68Amp::reset() noexcept
    {
    }
}
