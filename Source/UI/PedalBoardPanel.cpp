#include "PedalBoardPanel.h"
#include "../Parameters.h"

namespace solaris::ui
{
    PedalCard::PedalCard(juce::AudioProcessorValueTreeState& state,
                         int signalOrder,
                         juce::String displayName,
                         juce::String familyName,
                         juce::Colour finishColour,
                         juce::String enabledParameter,
                         std::array<juce::String, 3> controlNames,
                         std::array<juce::String, 3> parameterIds)
        : order(signalOrder),
          name(std::move(displayName)),
          family(std::move(familyName)),
          finish(finishColour)
    {
        bypassButton.setClickingTogglesState(true);
        bypassButton.setTooltip("Enable or bypass " + name);
        bypassButton.onStateChange = [this] { repaint(); };
        addAndMakeVisible(bypassButton);
        bypassAttachment = std::make_unique<ButtonAttachment>(state, enabledParameter, bypassButton);

        for (std::size_t i = 0; i < knobs.size(); ++i)
        {
            knobs[i] = std::make_unique<SolarisKnob>(
                controlNames[i], "", 0.0, 10.0, 0.01, 5.0, true);
            knobs[i]->control().setTooltip(name + " " + controlNames[i]);
            addAndMakeVisible(*knobs[i]);
            knobAttachments[i] = std::make_unique<SliderAttachment>(
                state, parameterIds[i], knobs[i]->control());
        }
    }

    void PedalCard::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), 12.0f);

        const auto top = finish.interpolatedWith(Theme::raised(), 0.76f);
        const auto bottom = finish.interpolatedWith(Theme::surface(), 0.86f);
        juce::ColourGradient body(top, bounds.getCentreX(), bounds.getY(),
                                  bottom, bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(bounds, 12.0f);

        g.setColour(finish.withAlpha(bypassButton.getToggleState() ? 0.62f : 0.30f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f,
                               bypassButton.getToggleState() ? 1.4f : 0.9f);

        auto header = bounds.reduced(13.0f, 11.0f).removeFromTop(62.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        const auto orderText = order < 10
            ? juce::String("0") + juce::String(order)
            : juce::String(order);
        g.drawText(orderText, header.removeFromTop(16.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawFittedText(name, header.removeFromTop(25.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.75f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.0f));
        g.drawFittedText(family.toUpperCase(), header.toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.8f);

        if (bypassButton.getToggleState())
        {
            g.setColour(Theme::amber().withAlpha(0.86f));
            g.fillEllipse(bounds.getRight() - 21.0f, bounds.getY() + 14.0f, 6.0f, 6.0f);
        }
    }

    void PedalCard::resized()
    {
        auto bounds = getLocalBounds().reduced(12, 10);
        bounds.removeFromTop(66);

        auto knobArea = bounds.removeFromTop(118);
        constexpr int gap = 3;
        const auto width = (knobArea.getWidth() - gap * 2) / 3;

        for (std::size_t i = 0; i < knobs.size(); ++i)
        {
            knobs[i]->setBounds(knobArea.removeFromLeft(width));
            if (i + 1 < knobs.size())
                knobArea.removeFromLeft(gap);
        }

        bounds.removeFromTop(8);
        bypassButton.setBounds(bounds.removeFromBottom(32));
    }

    PedalBoardPanel::PedalBoardPanel(juce::AudioProcessorValueTreeState& state, bool postEffects)
        : postFx(postEffects)
    {
        using namespace solaris::ParameterIDs;

        const auto add = [this, &state](int order,
                                        const char* name,
                                        const char* family,
                                        juce::Colour finish,
                                        const char* enabled,
                                        std::array<juce::String, 3> labels,
                                        std::array<juce::String, 3> ids)
        {
            pedals.push_back(std::make_unique<PedalCard>(
                state, order, name, family, finish, enabled,
                std::move(labels), std::move(ids)));
        };

        if (!postFx)
        {
            add(1, "LEVELER", "Compressor", juce::Colour::fromRGB(93, 100, 76),
                preCompEnabled, { "SUSTAIN", "ATTACK", "LEVEL" },
                { preCompSustain, preCompAttack, preCompLevel });
            add(2, "TURBO DRIVE", "Overdrive", juce::Colour::fromRGB(104, 91, 61),
                preDriveEnabled, { "DRIVE", "TONE", "LEVEL" },
                { preDriveDrive, preDriveTone, preDriveLevel });
            add(3, "BADLAND DIST", "Distortion", juce::Colour::fromRGB(104, 73, 57),
                preDistEnabled, { "GAIN", "CONTOUR", "LEVEL" },
                { preDistGain, preDistContour, preDistLevel });
            add(4, "VERMIN DRIVE", "Hard Clip Drive", juce::Colour::fromRGB(72, 70, 68),
                preHardEnabled, { "DIST", "FILTER", "LEVEL" },
                { preHardDistortion, preHardFilter, preHardLevel });
            add(5, "VOID FUZZ", "Sustaining Fuzz", juce::Colour::fromRGB(66, 58, 67),
                preFuzzEnabled, { "SUSTAIN", "TONE", "LEVEL" },
                { preFuzzSustain, preFuzzTone, preFuzzLevel });
        }
        else
        {
            add(1, "ORBIT", "Phaser", juce::Colour::fromRGB(72, 86, 82),
                postPhaserEnabled, { "RATE", "DEPTH", "MIX" },
                { postPhaserRate, postPhaserDepth, postPhaserMix });
            add(2, "CHORUS", "Analog Chorus", juce::Colour::fromRGB(72, 82, 96),
                postChorusEnabled, { "RATE", "DEPTH", "MIX" },
                { postChorusRate, postChorusDepth, postChorusMix });
            add(3, "PULSE", "Tremolo", juce::Colour::fromRGB(92, 77, 64),
                postTremoloEnabled, { "RATE", "DEPTH", "SHAPE" },
                { postTremoloRate, postTremoloDepth, postTremoloShape });
            add(4, "ECHO 404", "Analog Delay", juce::Colour::fromRGB(78, 65, 57),
                postDelayEnabled, { "TIME", "REGEN", "MIX" },
                { postDelayTime, postDelayFeedback, postDelayMix });
            add(5, "SANCTUM", "Reverb", juce::Colour::fromRGB(75, 78, 73),
                postReverbEnabled, { "DECAY", "TONE", "MIX" },
                { postReverbDecay, postReverbTone, postReverbMix });
        }

        for (auto& pedal : pedals)
            addAndMakeVisible(*pedal);
    }

    void PedalBoardPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(20.0f, 16.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(postFx ? "POST EFFECTS" : "PRE EFFECTS",
                   bounds.removeFromTop(22.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, bounds, 16.0f);

        auto notice = bounds.reduced(18.0f, 15.0f).removeFromTop(34.0f);
        g.setColour(Theme::surface().withAlpha(0.88f));
        g.fillRoundedRectangle(notice, 7.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(postFx
                       ? "CAB → MODULATION → DELAY / SPACE → EQ"
                       : "INPUT → LEVEL / DRIVE / DISTORTION → AMP",
                   notice.reduced(10.0f, 0.0f).toNearestInt(),
                   juce::Justification::centred, false);

        g.setColour(Theme::border().withAlpha(0.56f));
        const auto flowY = bounds.getBottom() - 23.0f;
        g.drawLine(bounds.getX() + 24.0f, flowY,
                   bounds.getRight() - 24.0f, flowY, 1.0f);
    }

    void PedalBoardPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(20, 16);
        bounds.removeFromTop(22);
        bounds = bounds.reduced(18, 15);
        bounds.removeFromTop(44);

        constexpr int gap = 10;
        const int count = static_cast<int>(pedals.size());
        const int width = (bounds.getWidth() - gap * (count - 1)) / count;
        const int height = juce::jmin(bounds.getHeight() - 18, 300);
        const int y = bounds.getCentreY() - height / 2 - 4;

        for (int i = 0; i < count; ++i)
        {
            pedals[static_cast<std::size_t>(i)]->setBounds(
                bounds.getX() + i * (width + gap), y, width, height);
        }
    }
}
