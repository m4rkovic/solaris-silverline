#pragma once

#include <JuceHeader.h>
#include "Controls.h"

namespace solaris::ui
{
    class CabPanel final : public juce::Component
    {
    public:
        CabPanel();

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        void drawMic(juce::Graphics&, juce::Rectangle<float> speaker,
                     float edgeAmount, float distanceAmount,
                     juce::String label, juce::Colour colour) const;

        SolarisKnob micAPosition { "A CENTER/EDGE", "%", 0.0, 100.0, 1.0, 28.0, true };
        SolarisKnob micADistance { "A DISTANCE", " cm", 0.0, 30.0, 1.0, 5.0, true };
        SolarisKnob micBPosition { "B CENTER/EDGE", "%", 0.0, 100.0, 1.0, 66.0, true };
        SolarisKnob micBDistance { "B DISTANCE", " cm", 0.0, 30.0, 1.0, 12.0, true };
        SolarisKnob blend { "MIC BLEND", "% A", 0.0, 100.0, 1.0, 55.0 };
        SolarisButton phaseButton { "PHASE B", true };
        bool phaseInverted = false;
    };
}
