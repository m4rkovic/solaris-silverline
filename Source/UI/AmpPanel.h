#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include "Controls.h"

namespace solaris::ui
{
    class AmpPanel final : public juce::Component,
                           private juce::Timer
    {
    public:
        explicit AmpPanel(juce::AudioProcessorValueTreeState& state);
        ~AmpPanel() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        void timerCallback() override;
        void setParameterNormalized(const juce::String& id, float normalizedValue);

        juce::AudioProcessorValueTreeState& parameterState;

        SolarisButton voicingButton { "CUSTOM", true };
        SolarisButton bypassButton { "AMP ON", true };

        SolarisKnob volume { "VOLUME", "", 0.0, 10.0, 0.1, 4.5 };
        SolarisKnob bass { "BASS", "", 0.0, 10.0, 0.1, 5.0 };
        SolarisKnob treble { "TREBLE", "", 0.0, 10.0, 0.1, 5.5 };
        SolarisKnob reverb { "REVERB", "", 0.0, 10.0, 0.1, 2.0 };
        SolarisKnob tremoloSpeed { "TREM SPEED", " Hz", 0.5, 12.0, 0.1, 4.0 };
        SolarisKnob tremoloIntensity { "TREM INT", "", 0.0, 10.0, 0.1, 0.0 };

        std::unique_ptr<SliderAttachment> volumeAttachment;
        std::unique_ptr<SliderAttachment> bassAttachment;
        std::unique_ptr<SliderAttachment> trebleAttachment;
        std::unique_ptr<SliderAttachment> reverbAttachment;
        std::unique_ptr<SliderAttachment> tremoloSpeedAttachment;
        std::unique_ptr<SliderAttachment> tremoloIntensityAttachment;
    };
}
