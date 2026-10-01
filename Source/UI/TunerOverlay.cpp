#include "TunerOverlay.h"\n#include <cmath>

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

            if (onMuteChanged)
                onMuteChanged(muted);
        };

        addAndMakeVisible(closeButton);
        addAndMakeVisible(muteButton);
    }

    void TunerOverlay::setSnapshot(const solaris::TunerSnapshot& snapshot)
    {
        currentSnapshot = snapshot;
        repaint();
    }

    void TunerOverlay::setMuted(bool shouldBeMuted)
    {
        if (muted == shouldBeMuted)
            return;

        muted = shouldBeMuted;
        muteButton.setActive(muted);
        muteButton.setText(muted ? "MUTED" : "MUTE");
        repaint();
    }

    juce::String TunerOverlay::noteNameForMidi(int midiNote) const
    {
        if (midiNote < 0)
            return "--";

        static constexpr const char* names[] {
            "C", "C#", "D", "D#", "E", "F",
            "F#", "G", "G#", "A", "A#", "B"
        };

        const auto pitchClass = ((midiNote % 12) + 12) % 12;
        const auto octave = midiNote / 12 - 1;
        return juce::String(names[pitchClass]) + juce::String(octave);
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

        const auto valid = currentSnapshot.valid;
        const auto note = valid ? noteNameForMidi(currentSnapshot.midiNote) : juce::String("--");

        g.setColour(valid ? Theme::text() : Theme::textMuted());
        g.setFont(juce::FontOptions(82.0f, juce::Font::bold));
        g.drawText(note, content.removeFromTop(104.0f).toNearestInt(),
                   juce::Justification::centred, false);

        const auto cents = juce::jlimit(-50.0f, 50.0f, currentSnapshot.cents);
        const auto centsText = valid
            ? (juce::String(currentSnapshot.cents, 1) + " cents")
            : juce::String("LISTENING");

        g.setColour(valid ? Theme::amber() : Theme::textMuted());
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText(centsText, content.removeFromTop(32.0f).toNearestInt(),
                   juce::Justification::centred, false);

        content.removeFromTop(8.0f);

        if (valid)
        {
            g.setColour(Theme::textMuted());
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(juce::String(currentSnapshot.frequencyHz, 2) + " Hz   "
                       + juce::String(currentSnapshot.confidence * 100.0f, 0) + "% confidence",
                       content.removeFromTop(18.0f).toNearestInt(),
                       juce::Justification::centred, false);
        }
        else
        {
            content.removeFromTop(18.0f);
        }

        content.removeFromTop(8.0f);
        auto meter = content.removeFromTop(58.0f).reduced(16.0f, 8.0f);

        g.setColour(Theme::border());
        g.fillRoundedRectangle(
            meter.withHeight(4.0f).withCentre({ meter.getCentreX(), meter.getCentreY() }),
            2.0f);

        for (int i = -5; i <= 5; ++i)
        {
            const auto x = juce::jmap(static_cast<float>(i), -5.0f, 5.0f,
                                     meter.getX(), meter.getRight());
            g.setColour(i == 0 ? Theme::amber() : Theme::silverMid().withAlpha(0.55f));
            const auto height = i == 0 ? 26.0f : 12.0f;
            g.drawVerticalLine(static_cast<int>(x),
                               meter.getCentreY() - height * 0.5f,
                               meter.getCentreY() + height * 0.5f);
        }

        if (valid)
        {
            const auto markerX = juce::jmap(cents, -50.0f, 50.0f,
                                           meter.getX(), meter.getRight());
            const auto inTune = std::abs(currentSnapshot.cents) <= 2.0f;
            g.setColour(inTune ? Theme::text() : Theme::amber());
            g.fillEllipse(markerX - 5.0f, meter.getCentreY() - 5.0f, 10.0f, 10.0f);
        }

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("-50",
                   meter.withWidth(50.0f).translated(0.0f, 28.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
        g.drawText("+50",
                   meter.withX(meter.getRight() - 50.0f).withWidth(50.0f)
                        .translated(0.0f, 28.0f).toNearestInt(),
                   juce::Justification::centredRight, false);
    }

    void TunerOverlay::resized()
    {
        const auto width = juce::jmin(560, getWidth() - 80);
        const auto height = juce::jmin(430, getHeight() - 80);

        cardBounds = juce::Rectangle<float>(
            0.0f, 0.0f,
            static_cast<float>(width),
            static_cast<float>(height))
            .withCentre(getLocalBounds().toFloat().getCentre());

        auto card = cardBounds.toNearestInt();
        closeButton.setBounds(card.getRight() - 92, card.getY() + 18, 72, 30);
        muteButton.setBounds(card.getCentreX() - 55, card.getBottom() - 56, 110, 34);
    }

    void TunerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        if (!cardBounds.contains(event.position) && onClose)
            onClose();
    }
}
