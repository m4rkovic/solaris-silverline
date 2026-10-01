#include "AmpPanel.h"

namespace solaris::ui
{
    AmpPanel::AmpPanel()
    {
        voicingButton.setActive(true);
        voicingButton.onClick = [this]
        {
            vintageMode = !vintageMode;
            voicingButton.setText(vintageMode ? "VINTAGE" : "CUSTOM");
            voicingButton.setActive(true);
        };

        bypassButton.onClick = [this]
        {
            bypassed = !bypassed;
            bypassButton.setActive(bypassed);
        };

        addAndMakeVisible(voicingButton);
        addAndMakeVisible(bypassButton);
        addAndMakeVisible(volume);
        addAndMakeVisible(bass);
        addAndMakeVisible(treble);
        addAndMakeVisible(reverb);
        addAndMakeVisible(tremoloSpeed);
        addAndMakeVisible(tremoloIntensity);
    }

    void AmpPanel::paint(juce::Graphics& g)
    {
        auto content = getLocalBounds().toFloat().reduced(28.0f, 22.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("AMPLIFIER", content.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        auto chassis = content.reduced(6.0f, 2.0f);
        auto shadow = chassis.translated(0.0f, 7.0f);
        g.setColour(juce::Colours::black.withAlpha(0.52f));
        g.fillRoundedRectangle(shadow, 18.0f);

        g.setColour(juce::Colour::fromRGB(10, 10, 10));
        g.fillRoundedRectangle(chassis, 18.0f);
        g.setColour(juce::Colour::fromRGB(55, 54, 51));
        g.drawRoundedRectangle(chassis.reduced(0.5f), 18.0f, 1.0f);

        auto inner = chassis.reduced(18.0f);
        auto faceplate = inner.removeFromTop(230.0f);
        Theme::drawBrushedMetal(g, faceplate);

        g.setColour(juce::Colour::fromRGB(34, 34, 33));
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.drawText("SILVERLINE 68", faceplate.withTrimmedLeft(24.0f).withHeight(44.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        g.setColour(juce::Colour::fromRGB(72, 66, 57));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("AMERICAN CLEAN  /  DYNAMIC RESPONSE",
                   faceplate.withTrimmedLeft(24.0f).withTrimmedTop(33.0f).withHeight(24.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        inner.removeFromTop(14.0f);
        auto grilleArea = inner;
        Theme::drawTextile(g, grilleArea);

        const auto speakerSize = juce::jmin(grilleArea.getHeight() * 0.72f,
                                            grilleArea.getWidth() * 0.22f);
        const auto cy = grilleArea.getCentreY();
        const auto cx1 = grilleArea.getCentreX() - speakerSize * 0.66f;
        const auto cx2 = grilleArea.getCentreX() + speakerSize * 0.66f;

        for (auto cx : { cx1, cx2 })
        {
            juce::Rectangle<float> speaker(cx - speakerSize * 0.5f,
                                           cy - speakerSize * 0.5f,
                                           speakerSize, speakerSize);
            g.setColour(juce::Colours::black.withAlpha(0.64f));
            g.fillEllipse(speaker);
            g.setColour(juce::Colour::fromRGB(53, 51, 47).withAlpha(0.8f));
            g.drawEllipse(speaker.reduced(5.0f), 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillEllipse(speaker.reduced(speakerSize * 0.34f));
        }

        g.setColour(Theme::amberMuted().withAlpha(0.7f));
        g.fillEllipse(grilleArea.getRight() - 24.0f, grilleArea.getBottom() - 24.0f, 6.0f, 6.0f);
    }

    void AmpPanel::resized()
    {
        auto content = getLocalBounds().reduced(28, 22);
        content.removeFromTop(24);
        auto chassis = content.reduced(6, 2);
        auto inner = chassis.reduced(18);
        auto face = inner.removeFromTop(230);

        voicingButton.setBounds(face.getX() + 24, face.getY() + 58, 104, 32);
        bypassButton.setBounds(face.getRight() - 116, face.getY() + 16, 92, 32);

        auto controls = face.withTrimmedTop(90).reduced(20, 8);
        constexpr int count = 6;
        constexpr int gap = 8;
        const int knobW = (controls.getWidth() - gap * (count - 1)) / count;

        std::array<SolarisKnob*, count> knobs {
            &volume, &bass, &treble, &reverb, &tremoloSpeed, &tremoloIntensity
        };

        for (int i = 0; i < count; ++i)
        {
            knobs[(size_t) i]->setBounds(controls.removeFromLeft(knobW));
            if (i < count - 1)
                controls.removeFromLeft(gap);
        }
    }
}
