#pragma once

#include <JuceHeader.h>
#include <array>
#include "Controls.h"

namespace solaris::ui
{
    class AmpPanel final : public juce::Component
    {
    public:
        AmpPanel();

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        SolarisButton voicingButton { "CUSTOM", true };
        SolarisButton bypassButton { "BYPASS", true };

        SolarisKnob volume { "VOLUME", "", 0.0, 10.0, 0.1, 4.5 };
        SolarisKnob bass { "BASS", "", 0.0, 10.0, 0.1, 5.0 };
        SolarisKnob treble { "TREBLE", "", 0.0, 10.0, 0.1, 5.7 };
        SolarisKnob reverb { "REVERB", "", 0.0, 10.0, 0.1, 2.8 };
        SolarisKnob tremoloSpeed { "TREM SPEED", " Hz", 1.0, 12.0, 0.1, 4.2 };
        SolarisKnob tremoloIntensity { "TREM INT", "", 0.0, 10.0, 0.1, 0.0 };

        bool vintageMode = false;
        bool bypassed = false;
    };
}
