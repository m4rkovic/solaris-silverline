#include "TunerOverlay.h"

namespace solaris::ui
{
    TunerOverlay::TunerOverlay()
    {
        closeButton.onClick = [this]
        {
            if (onClose)
                onClose();
        };

        muteButton.onClick = [this]
        {
            muted = !muted;
            muteButton.setActive(muted);
            muteButton.setText(muted ? "MUTED" : "MUTE");
        };

        addAndMakeVisible(closeButton);
        addAndMakeVisible(muteButton);
    }

    void TunerOverlay::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colours::black.withAlpha(0.78f));

        g.setColour(juce::Colours::black.withAlpha(0.62f));
        g.fillRoundedRectangle(cardBounds.translated(0.0f, 8.0f), 18.0f);
        Theme::fillPanel(g, cardBounds, 18.0f);

        auto content = cardBounds.reduced(34.0f, 26.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("CHROMATIC TUNER", content.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centred, false);

        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(82.0f, juce::Font::bold));
        g.drawText("A", content.removeFromTop(104.0f).toNearestInt(),
                   juce::Justification::centred, false);

        g.setColour(Theme::amber());
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText("0.0 cents", content.removeFromTop(32.0f).toNearestInt(),
                   juce::Justification::centred, false);

        content.removeFromTop(18.0f);
        auto meter = content.removeFromTop(58.0f).reduced(16.0f, 8.0f);
        const auto centreX = meter.getCentreX();

        g.setColour(Theme::border());
        g.fillRoundedRectangle(meter.withHeight(4.0f).withCentre({ meter.getCentreX(), meter.getCentreY() }), 2.0f);

        for (int i = -5; i <= 5; ++i)
        {
            const auto x = juce::jmap((float) i, -5.0f, 5.0f, meter.getX(), meter.getRight());
            g.setColour(i == 0 ? Theme::amber() : Theme::silverMid().withAlpha(0.55f));
            const auto h = i == 0 ? 26.0f : 12.0f;
            g.drawVerticalLine((int) x, meter.getCentreY() - h * 0.5f, meter.getCentreY() + h * 0.5f);
        }

        g.setColour(Theme::amber());
        g.fillEllipse(centreX - 5.0f, meter.getCentreY() - 5.0f, 10.0f, 10.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("-50", meter.withWidth(50.0f).translated(0.0f, 28.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
        g.drawText("+50", meter.withX(meter.getRight() - 50.0f).withWidth(50.0f).translated(0.0f, 28.0f).toNearestInt(),
                   juce::Justification::centredRight, false);
    }

    void TunerOverlay::resized()
    {
        const auto w = juce::jmin(560, getWidth() - 80);
        const auto h = juce::jmin(410, getHeight() - 80);
        cardBounds = juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h)
                         .withCentre(getLocalBounds().toFloat().getCentre());

        auto card = cardBounds.toNearestInt();
        closeButton.setBounds(card.getRight() - 92, card.getY() + 18, 72, 30);
        muteButton.setBounds(card.getCentreX() - 55, card.getBottom() - 56, 110, 34);
    }

    void TunerOverlay::mouseDown(const juce::MouseEvent& e)
    {
        if (!cardBounds.contains(e.position) && onClose)
            onClose();
    }
}
