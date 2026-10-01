#include "TunerOverlay.h"
#include <cmath>

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

        closeButton.setTooltip("Close tuner.");
        muteButton.setTooltip("Mute plugin output while tuning.");

        addAndMakeVisible(closeButton);
        addAndMakeVisible(muteButton);
    }

    void TunerOverlay::setSnapshot(const solaris::TunerSnapshot& snapshot)
    {
        const auto confident = snapshot.valid
                            && snapshot.confidence >= 0.45f
                            && std::isfinite(snapshot.frequencyHz)
                            && std::isfinite(snapshot.cents);

        if (!confident)
        {
            ++invalidFrames;
            if (invalidFrames >= 6)
            {
                displayedSnapshot.valid = false;
                candidateNote = -1;
                candidateFrames = 0;
            }

            repaint();
            return;
        }

        invalidFrames = 0;

        if (!displayedSnapshot.valid)
        {
            if (candidateNote == snapshot.midiNote)
                ++candidateFrames;
            else
            {
                candidateNote = snapshot.midiNote;
                candidateFrames = 1;
            }

            if (candidateFrames >= 2)
            {
                displayedSnapshot = snapshot;
                displayedSnapshot.cents = juce::jlimit(-50.0f, 50.0f, snapshot.cents);
                candidateNote = -1;
                candidateFrames = 0;
            }

            repaint();
            return;
        }

        if (snapshot.midiNote == displayedSnapshot.midiNote)
        {
            candidateNote = -1;
            candidateFrames = 0;

            displayedSnapshot.valid = true;
            displayedSnapshot.frequencyHz +=
                (snapshot.frequencyHz - displayedSnapshot.frequencyHz) * 0.28f;
            displayedSnapshot.cents +=
                (juce::jlimit(-50.0f, 50.0f, snapshot.cents)
                 - displayedSnapshot.cents) * 0.34f;
            displayedSnapshot.confidence +=
                (snapshot.confidence - displayedSnapshot.confidence) * 0.25f;
        }
        else if (snapshot.confidence >= 0.65f)
        {
            if (candidateNote == snapshot.midiNote)
                ++candidateFrames;
            else
            {
                candidateNote = snapshot.midiNote;
                candidateFrames = 1;
            }

            if (candidateFrames >= 3)
            {
                displayedSnapshot = snapshot;
                displayedSnapshot.cents = juce::jlimit(-50.0f, 50.0f, snapshot.cents);
                candidateNote = -1;
                candidateFrames = 0;
            }
        }

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

    juce::String TunerOverlay::confidenceLabel(float confidence) const
    {
        if (confidence >= 0.82f)
            return "LOCKED";
        if (confidence >= 0.60f)
            return "TRACKING";
        return "LOW SIGNAL";
    }

    void TunerOverlay::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colours::black.withAlpha(0.80f));

        g.setColour(juce::Colours::black.withAlpha(0.62f));
        g.fillRoundedRectangle(cardBounds.translated(0.0f, 8.0f), 18.0f);
        Theme::fillPanel(g, cardBounds, 18.0f);

        auto content = cardBounds.reduced(34.0f, 26.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("CHROMATIC TUNER",
                   content.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centred, false);

        const auto valid = displayedSnapshot.valid;
        const auto note = valid
            ? noteNameForMidi(displayedSnapshot.midiNote)
            : juce::String("--");

        g.setColour(valid ? Theme::text() : Theme::textMuted());
        g.setFont(juce::FontOptions(82.0f, juce::Font::bold));
        g.drawText(note,
                   content.removeFromTop(104.0f).toNearestInt(),
                   juce::Justification::centred, false);

        const auto cents = valid
            ? juce::jlimit(-50.0f, 50.0f, displayedSnapshot.cents)
            : 0.0f;

        g.setColour(valid ? Theme::amber() : Theme::textMuted());
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText(
            valid ? juce::String(displayedSnapshot.cents, 1) + " cents"
                  : juce::String("PLAY A NOTE"),
            content.removeFromTop(32.0f).toNearestInt(),
            juce::Justification::centred, false);

        content.removeFromTop(6.0f);

        g.setColour(valid ? Theme::textMuted() : Theme::border());
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(
            valid
                ? confidenceLabel(displayedSnapshot.confidence)
                    + "   •   " + juce::String(displayedSnapshot.frequencyHz, 2) + " Hz"
                : juce::String("WAITING FOR A STABLE PITCH"),
            content.removeFromTop(18.0f).toNearestInt(),
            juce::Justification::centred, false);

        content.removeFromTop(8.0f);
        auto meter = content.removeFromTop(58.0f).reduced(16.0f, 8.0f);

        g.setColour(Theme::border());
        g.fillRoundedRectangle(
            meter.withHeight(4.0f)
                 .withCentre({ meter.getCentreX(), meter.getCentreY() }),
            2.0f);

        for (int i = -5; i <= 5; ++i)
        {
            const auto x = juce::jmap(
                static_cast<float>(i), -5.0f, 5.0f,
                meter.getX(), meter.getRight());

            g.setColour(i == 0
                ? Theme::amber()
                : Theme::silverMid().withAlpha(0.55f));

            const auto height = i == 0 ? 26.0f : 12.0f;
            g.drawVerticalLine(
                static_cast<int>(x),
                meter.getCentreY() - height * 0.5f,
                meter.getCentreY() + height * 0.5f);
        }

        if (valid)
        {
            const auto markerX = juce::jmap(
                cents, -50.0f, 50.0f,
                meter.getX(), meter.getRight());

            const auto inTune = std::abs(displayedSnapshot.cents) <= 2.0f;
            g.setColour(inTune ? Theme::text() : Theme::amber());
            g.fillEllipse(
                markerX - 5.0f,
                meter.getCentreY() - 5.0f,
                10.0f, 10.0f);
        }

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("-50",
                   meter.withWidth(50.0f)
                        .translated(0.0f, 28.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
        g.drawText("+50",
                   meter.withX(meter.getRight() - 50.0f)
                        .withWidth(50.0f)
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
