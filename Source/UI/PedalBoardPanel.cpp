#include "PedalBoardPanel.h"

namespace solaris::ui
{
    PedalCard::PedalCard(juce::String displayName,
                         juce::String familyName,
                         juce::Colour finishColour,
                         std::array<juce::String, 3> controlNames)
        : name(std::move(displayName)),
          family(std::move(familyName)),
          finish(finishColour)
    {
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            knobs[i] = std::make_unique<SolarisKnob>(controlNames[i], "", 0.0, 10.0, 0.1,
                                                     i == 0 ? 5.0 : 4.0, true);
            addAndMakeVisible(*knobs[i]);
        }

        bypass.onClick = [this]
        {
            enabled = !enabled;
            bypass.setActive(enabled);
            bypass.setText(enabled ? "ACTIVE" : "ENGAGE");
            repaint();
        };
        addAndMakeVisible(bypass);
    }

    void PedalCard::paint(juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(r.translated(0.0f, 4.0f), 12.0f);

        auto top = finish.interpolatedWith(Theme::raised(), 0.64f);
        auto bottom = finish.interpolatedWith(Theme::surface(), 0.78f);
        juce::ColourGradient body(top, r.getCentreX(), r.getY(),
                                  bottom, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(r, 12.0f);

        g.setColour(finish.brighter(0.14f).withAlpha(enabled ? 0.9f : 0.45f));
        g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, enabled ? 1.4f : 0.8f);

        auto header = r.reduced(12.0f, 10.0f).removeFromTop(48.0f);
        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawFittedText(name, header.removeFromTop(24.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.75f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f));
        g.drawFittedText(family.toUpperCase(), header.toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.8f);

        const auto footY = r.getBottom() - 52.0f;
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillEllipse(r.getCentreX() - 13.0f, footY - 30.0f, 26.0f, 26.0f);
        g.setColour(Theme::silverMid());
        g.drawEllipse(r.getCentreX() - 13.0f, footY - 30.0f, 26.0f, 26.0f, 1.0f);

        g.setColour(enabled ? Theme::amber() : Theme::border());
        g.fillEllipse(r.getCentreX() - 3.0f, r.getY() + 14.0f, 6.0f, 6.0f);
    }

    void PedalCard::resized()
    {
        auto r = getLocalBounds().reduced(10, 10);
        r.removeFromTop(58);

        auto knobArea = r.removeFromTop(120);
        const int gap = 3;
        const int knobW = (knobArea.getWidth() - gap * 2) / 3;
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            knobs[i]->setBounds(knobArea.removeFromLeft(knobW));
            if (i + 1 < knobs.size())
                knobArea.removeFromLeft(gap);
        }

        bypass.setBounds(r.removeFromBottom(34));
    }

    PedalBoardPanel::PedalBoardPanel(bool postEffects)
        : postFx(postEffects)
    {
        if (!postFx)
        {
            pedals.push_back(std::make_unique<PedalCard>("LEVELER", "Optical Compressor",
                juce::Colour::fromRGB(93, 100, 76),
                std::array<juce::String, 3>{ "SUSTAIN", "ATTACK", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>("TURBO DRIVE", "Dual Voice Overdrive",
                juce::Colour::fromRGB(104, 91, 61),
                std::array<juce::String, 3>{ "DRIVE", "TONE", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>("BADLAND DIST", "Three Band Distortion",
                juce::Colour::fromRGB(104, 73, 57),
                std::array<juce::String, 3>{ "GAIN", "CONTOUR", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>("VERMIN DRIVE", "Filter Drive",
                juce::Colour::fromRGB(72, 70, 68),
                std::array<juce::String, 3>{ "DIST", "FILTER", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>("VOID FUZZ", "Sustaining Fuzz",
                juce::Colour::fromRGB(66, 58, 67),
                std::array<juce::String, 3>{ "SUSTAIN", "TONE", "VOLUME" }));
        }
        else
        {
            pedals.push_back(std::make_unique<PedalCard>("ORBIT", "Phase Shifter",
                juce::Colour::fromRGB(72, 86, 82),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>("CHORUS", "Analog Chorus",
                juce::Colour::fromRGB(72, 82, 96),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>("PULSE", "Bias Tremolo",
                juce::Colour::fromRGB(92, 77, 64),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "SHAPE" }));
            pedals.push_back(std::make_unique<PedalCard>("ECHO 404", "Analog Delay",
                juce::Colour::fromRGB(78, 65, 57),
                std::array<juce::String, 3>{ "TIME", "REGEN", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>("SANCTUM", "Spring / Hall Reverb",
                juce::Colour::fromRGB(75, 78, 73),
                std::array<juce::String, 3>{ "DECAY", "TONE", "MIX" }));
        }

        for (auto& pedal : pedals)
            addAndMakeVisible(*pedal);
    }

    void PedalBoardPanel::paint(juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced(20.0f, 18.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(postFx ? "POST EFFECTS" : "PRE EFFECTS",
                   r.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, r, 16.0f);

        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(r.reduced(18.0f, 28.0f), 8.0f);

        g.setColour(Theme::border().withAlpha(0.5f));
        const auto y = r.getBottom() - 22.0f;
        g.drawLine(r.getX() + 18.0f, y, r.getRight() - 18.0f, y, 1.0f);
    }

    void PedalBoardPanel::resized()
    {
        auto r = getLocalBounds().reduced(20, 18);
        r.removeFromTop(24);
        r = r.reduced(18, 28);

        constexpr int gap = 10;
        const int count = (int) pedals.size();
        const int w = (r.getWidth() - gap * (count - 1)) / count;
        const int h = juce::jmin(r.getHeight(), 360);
        const int y = r.getCentreY() - h / 2;

        for (int i = 0; i < count; ++i)
        {
            pedals[(size_t) i]->setBounds(r.getX() + i * (w + gap), y, w, h);
        }
    }
}
