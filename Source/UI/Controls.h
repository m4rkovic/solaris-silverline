#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Theme.h"

namespace solaris::ui
{
    class SolarisButton final : public juce::Button
    {
    public:
        explicit SolarisButton(juce::String textToShow, bool useAccentWhenActive = true);

        void setActive(bool shouldBeActive);
        bool isActive() const noexcept { return active; }
        void setText(juce::String newText);

        void paintButton(juce::Graphics&, bool highlighted, bool down) override;

    private:
        juce::String text;
        bool active = false;
        bool accentWhenActive = true;
    };

    class KnobLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional,
                              float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider&) override;
    };

    class SolarisKnob final : public juce::Component
    {
    public:
        SolarisKnob(juce::String labelText,
                    juce::String suffixText,
                    double minimum,
                    double maximum,
                    double interval,
                    double initialValue,
                    bool compactStyle = false);
        ~SolarisKnob() override;

        juce::Slider& control() noexcept { return slider; }
        std::function<void()> onValueChanged;

        void resized() override;

    private:
        void updateValueLabel();

        KnobLookAndFeel lookAndFeel;
        juce::Slider slider;
        juce::Label label;
        juce::Label value;
        juce::String suffix;
        bool compact = false;
    };
}
