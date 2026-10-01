#include "Controls.h"
#include <cmath>

namespace solaris::ui
{
    SolarisButton::SolarisButton(juce::String textToShow, bool useAccentWhenActive)
        : juce::Button(textToShow),
          text(std::move(textToShow)),
          accentWhenActive(useAccentWhenActive)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void SolarisButton::setActive(bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;

        active = shouldBeActive;
        repaint();
    }

    void SolarisButton::setText(juce::String newText)
    {
        text = std::move(newText);
        repaint();
    }

    void SolarisButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const auto radius = juce::jmin(9.0f, r.getHeight() * 0.22f);

        auto fill = Theme::surface();
        if (highlighted)
            fill = Theme::raised().brighter(0.08f);
        if (down)
            fill = Theme::raised().darker(0.12f);
        if (active)
            fill = Theme::raised().brighter(0.11f);

        g.setColour(fill);
        g.fillRoundedRectangle(r, radius);

        g.setColour(active ? Theme::amber().withAlpha(0.9f)
                           : Theme::border().withAlpha(highlighted ? 0.9f : 0.55f));
        g.drawRoundedRectangle(r, radius, active ? 1.2f : 0.8f);

        if (active && accentWhenActive)
        {
            g.setColour(Theme::amber());
            auto line = r.removeFromBottom(2.0f).reduced(r.getWidth() * 0.25f, 0.0f);
            g.fillRoundedRectangle(line, 1.0f);
        }

        g.setColour(active ? Theme::text() : Theme::textMuted());
        g.setFont(juce::FontOptions(12.0f, active ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText(text, getLocalBounds().reduced(8, 2),
                         juce::Justification::centred, 1, 0.8f);
    }

    void KnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider&)
    {
        auto bounds = juce::Rectangle<float>((float) x, (float) y,
                                             (float) width, (float) height).reduced(5.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle
                         + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto arcRadius = radius - 2.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                    rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(Theme::amber());
        g.strokePath(valueArc, juce::PathStrokeType(2.2f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto shadow = bounds.reduced(radius * 0.13f);
        shadow.translate(0.0f, 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.fillEllipse(shadow);

        auto knob = bounds.reduced(radius * 0.16f);
        juce::ColourGradient metal(Theme::silverLight(),
                                   knob.getX() + knob.getWidth() * 0.25f,
                                   knob.getY() + knob.getHeight() * 0.18f,
                                   Theme::silverDark(),
                                   knob.getRight(), knob.getBottom(), true);
        metal.addColour(0.52, Theme::silverMid());
        g.setGradientFill(metal);
        g.fillEllipse(knob);

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawEllipse(knob.reduced(1.0f), 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.drawEllipse(knob, 1.0f);

        const auto pointerLength = radius * 0.52f;
        const auto pointerStart = radius * 0.13f;
        juce::Point<float> p1(centre.x + std::sin(angle) * pointerStart,
                              centre.y - std::cos(angle) * pointerStart);
        juce::Point<float> p2(centre.x + std::sin(angle) * pointerLength,
                              centre.y - std::cos(angle) * pointerLength);

        g.setColour(juce::Colour::fromRGB(24, 24, 23));
        g.drawLine({p1, p2}, 2.1f);

        g.setColour(Theme::amber().withAlpha(0.85f));
        g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }

    SolarisKnob::SolarisKnob(juce::String labelText,
                             juce::String suffixText,
                             double minimum,
                             double maximum,
                             double interval,
                             double initialValue,
                             bool compactStyle)
        : suffix(std::move(suffixText)),
          compact(compactStyle)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(minimum, maximum, interval);
        slider.setValue(initialValue, juce::dontSendNotification);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.20f,
                                   juce::MathConstants<float>::pi * 2.80f, true);
        slider.setMouseDragSensitivity(190);
        slider.setDoubleClickReturnValue(true, initialValue);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this]
        {
            updateValueLabel();
            if (onValueChanged)
                onValueChanged();
        };
        addAndMakeVisible(slider);

        label.setText(std::move(labelText), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, Theme::text());
        label.setFont(juce::FontOptions(compact ? 10.5f : 12.0f, juce::Font::bold));
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);

        value.setJustificationType(juce::Justification::centred);
        value.setColour(juce::Label::textColourId, Theme::textMuted());
        value.setFont(juce::FontOptions(compact ? 9.5f : 10.5f));
        value.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(value);

        updateValueLabel();
    }

    SolarisKnob::~SolarisKnob()
    {
        slider.setLookAndFeel(nullptr);
    }

    void SolarisKnob::updateValueLabel()
    {
        const auto decimals = slider.getInterval() >= 1.0 ? 0 : 1;
        value.setText(juce::String(slider.getValue(), decimals) + suffix,
                      juce::dontSendNotification);
    }

    void SolarisKnob::resized()
    {
        auto r = getLocalBounds();
        const auto labelH = compact ? 17 : 20;
        const auto valueH = compact ? 15 : 18;

        label.setBounds(r.removeFromTop(labelH));
        value.setBounds(r.removeFromBottom(valueH));
        slider.setBounds(r.reduced(compact ? 1 : 2));
    }
}
