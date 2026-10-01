#include "CabPanel.h"

namespace solaris::ui
{
    CabPanel::CabPanel(juce::AudioProcessorValueTreeState& state)
        : parameterState(state)
    {
        blendAttachment = std::make_unique<SliderAttachment>(state, "cabMicBlend", blend.control());

        auto repaintOnChange = [this] { repaint(); };
        micAPosition.onValueChanged = repaintOnChange;
        micADistance.onValueChanged = repaintOnChange;
        micBPosition.onValueChanged = repaintOnChange;
        micBDistance.onValueChanged = repaintOnChange;
        blend.onValueChanged = repaintOnChange;

        // Position/distance need a multi-IR pack or interpolation engine. Keeping
        // them disabled is preferable to pretending they alter the sound.
        micAPosition.setEnabled(false);
        micADistance.setEnabled(false);
        micBPosition.setEnabled(false);
        micBDistance.setEnabled(false);

        phaseButton.onClick = [this]
        {
            const auto inverted = parameterState.getRawParameterValue("cabPhaseB")->load() >= 0.5f;
            setBoolParameter("cabPhaseB", !inverted);
        };

        addAndMakeVisible(micAPosition);
        addAndMakeVisible(micADistance);
        addAndMakeVisible(micBPosition);
        addAndMakeVisible(micBDistance);
        addAndMakeVisible(blend);
        addAndMakeVisible(phaseButton);

        timerCallback();
        startTimerHz(10);
    }

    CabPanel::~CabPanel()
    {
        stopTimer();
    }

    void CabPanel::setBoolParameter(const juce::String& id, bool value)
    {
        if (auto* parameter = parameterState.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(value ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
    }

    void CabPanel::timerCallback()
    {
        const auto inverted = parameterState.getRawParameterValue("cabPhaseB")->load() >= 0.5f;
        phaseButton.setActive(inverted);
        phaseButton.setText(inverted ? "PHASE B INV" : "PHASE B");
    }

    void CabPanel::drawMic(juce::Graphics& g,
                           juce::Rectangle<float> speaker,
                           float edgeAmount,
                           float distanceAmount,
                           juce::String label,
                           juce::Colour colour) const
    {
        const auto centre = speaker.getCentre();
        const auto maxOffset = speaker.getWidth() * 0.36f;
        const auto x = centre.x + edgeAmount * maxOffset;
        const auto y = centre.y - speaker.getHeight() * 0.08f;
        const auto standLength = 28.0f + distanceAmount * 56.0f;

        g.setColour(colour.withAlpha(0.46f));
        g.drawLine(x, y, x, y - standLength, 1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.58f));
        g.fillEllipse(x - 11.0f, y - standLength - 11.0f, 22.0f, 22.0f);
        g.setColour(colour);
        g.drawEllipse(x - 11.0f, y - standLength - 11.0f, 22.0f, 22.0f, 2.0f);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(label, juce::Rectangle<int>(static_cast<int>(x) - 18,
                                               static_cast<int>(y - standLength - 34.0f),
                                               36, 18),
                   juce::Justification::centred, false);
    }

    void CabPanel::paint(juce::Graphics& g)
    {
        auto outer = getLocalBounds().toFloat().reduced(24.0f, 20.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("CABINET / MICROPHONES", outer.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, outer, 16.0f);
        auto body = outer.reduced(22.0f);
        auto visual = body.removeFromLeft(body.getWidth() * 0.64f).reduced(8.0f);

        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.fillRoundedRectangle(visual.translated(0.0f, 5.0f), 14.0f);
        g.setColour(juce::Colour::fromRGB(24, 23, 22));
        g.fillRoundedRectangle(visual, 14.0f);
        g.setColour(juce::Colour::fromRGB(72, 69, 63));
        g.drawRoundedRectangle(visual.reduced(0.5f), 14.0f, 1.2f);

        auto grille = visual.reduced(18.0f);
        Theme::drawTextile(g, grille);

        const auto speakerDiameter = juce::jmin(grille.getHeight() * 0.68f,
                                                grille.getWidth() * 0.36f);
        auto left = juce::Rectangle<float>(0, 0, speakerDiameter, speakerDiameter)
                        .withCentre({ grille.getCentreX() - speakerDiameter * 0.57f,
                                      grille.getCentreY() + 14.0f });
        auto right = juce::Rectangle<float>(0, 0, speakerDiameter, speakerDiameter)
                         .withCentre({ grille.getCentreX() + speakerDiameter * 0.57f,
                                       grille.getCentreY() + 14.0f });

        for (auto speaker : { left, right })
        {
            g.setColour(juce::Colours::black.withAlpha(0.74f));
            g.fillEllipse(speaker);
            g.setColour(juce::Colour::fromRGB(62, 60, 55));
            g.drawEllipse(speaker.reduced(5.0f), 2.0f);
            g.setColour(juce::Colour::fromRGB(17, 17, 16));
            g.fillEllipse(speaker.reduced(speakerDiameter * 0.34f));
        }

        const auto aEdge = static_cast<float>(micAPosition.control().getValue() / 100.0);
        const auto aDist = static_cast<float>(micADistance.control().getValue() / 30.0);
        const auto bEdge = static_cast<float>(micBPosition.control().getValue() / 100.0);
        const auto bDist = static_cast<float>(micBDistance.control().getValue() / 30.0);

        drawMic(g, left, aEdge, aDist, "A", Theme::amber());
        drawMic(g, right, bEdge, bDist, "B", Theme::silverLight());

        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText("2 x 10 CABINET", visual.withTrimmedTop(12.0f).withHeight(26.0f).toNearestInt(),
                   juce::Justification::centred, false);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Position / distance unlock with multi-IR cabinet packs",
                   visual.withTrimmedTop(38.0f).withHeight(20.0f).toNearestInt(),
                   juce::Justification::centred, false);
    }

    void CabPanel::resized()
    {
        auto outer = getLocalBounds().reduced(24, 20);
        outer.removeFromTop(24);
        auto body = outer.reduced(22);
        body.removeFromLeft(static_cast<int>(body.getWidth() * 0.64f));
        auto controls = body.reduced(12, 8);

        auto row1 = controls.removeFromTop(108);
        micAPosition.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(4));
        micADistance.setBounds(row1.reduced(4));

        controls.removeFromTop(8);
        auto row2 = controls.removeFromTop(108);
        micBPosition.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(4));
        micBDistance.setBounds(row2.reduced(4));

        controls.removeFromTop(14);
        blend.setBounds(controls.removeFromTop(138).reduced(14, 0));
        controls.removeFromTop(6);
        phaseButton.setBounds(controls.removeFromTop(36).reduced(20, 0));
    }
}
