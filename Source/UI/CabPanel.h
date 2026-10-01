#pragma once

#include <JuceHeader.h>
#include <memory>
#include "Controls.h"

namespace solaris::ui
{
    class CabPanel final : public juce::Component,
                           private juce::Timer
    {
    public:
        explicit CabPanel(juce::AudioProcessorValueTreeState& state);
        ~CabPanel() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        void timerCallback() override;
        void drawMic(juce::Graphics&, juce::Rectangle<float> speaker,
                     float edgeAmount, float distanceAmount,
                     juce::String label, juce::Colour colour) const;
        void setBoolParameter(const juce::String& id, bool value);

        juce::AudioProcessorValueTreeState& parameterState;

        SolarisKnob micAPosition { "A CENTER/EDGE", "%", 0.0, 100.0, 1.0, 28.0, true };
        SolarisKnob micADistance { "A DISTANCE", " cm", 0.0, 30.0, 1.0, 5.0, true };
        SolarisKnob micBPosition { "B CENTER/EDGE", "%", 0.0, 100.0, 1.0, 66.0, true };
        SolarisKnob micBDistance { "B DISTANCE", " cm", 0.0, 30.0, 1.0, 12.0, true };
        SolarisKnob blend { "MIC B BLEND", "% B", 0.0, 100.0, 1.0, 50.0 };
        SolarisButton phaseButton { "PHASE B", true };

        std::unique_ptr<SliderAttachment> blendAttachment;
    };
}
