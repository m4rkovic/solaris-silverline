#include "PedalBoardPanel.h"

namespace solaris::ui
{
    PedalCard::PedalCard(int signalOrder,
                         juce::String displayName,
                         juce::String familyName,
                         juce::Colour finishColour,
                         std::array<juce::String, 3> controlNames)
        : order(signalOrder),
          name(std::move(displayName)),
          family(std::move(familyName)),
          finish(finishColour),
          controls(std::move(controlNames))
    {
        setInterceptsMouseClicks(false, false);
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

        g.setColour(finish.withAlpha(0.38f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

        auto content = bounds.reduced(13.0f, 12.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        const auto orderText = order < 10
            ? juce::String("0") + juce::String(order)
            : juce::String(order);
        g.drawText(orderText,
                   content.removeFromTop(18.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawFittedText(name,
                         content.removeFromTop(26.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.78f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f));
        g.drawFittedText(family.toUpperCase(),
                         content.removeFromTop(22.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.8f);

        content.removeFromTop(18.0f);

        for (const auto& control : controls)
        {
            auto row = content.removeFromTop(28.0f);
            g.setColour(Theme::border().withAlpha(0.5f));
            g.fillRoundedRectangle(row.reduced(0.0f, 3.0f), 5.0f);
            g.setColour(Theme::textMuted().withAlpha(0.72f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText(control, row.reduced(8, 0).toNearestInt(),
                       juce::Justification::centredLeft, false);
        }

        auto status = bounds.reduced(12.0f).removeFromBottom(34.0f);
        g.setColour(Theme::surface().withAlpha(0.84f));
        g.fillRoundedRectangle(status, 6.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("DSP NOT CONNECTED",
                   status.toNearestInt(),
                   juce::Justification::centred, false);
    }

    PedalBoardPanel::PedalBoardPanel(bool postEffects)
        : postFx(postEffects)
    {
        if (!postFx)
        {
            pedals.push_back(std::make_unique<PedalCard>(
                1, "LEVELER", "Compressor",
                juce::Colour::fromRGB(93, 100, 76),
                std::array<juce::String, 3>{ "SUSTAIN", "ATTACK", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>(
                2, "TURBO DRIVE", "Overdrive",
                juce::Colour::fromRGB(104, 91, 61),
                std::array<juce::String, 3>{ "DRIVE", "TONE", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>(
                3, "BADLAND DIST", "Distortion",
                juce::Colour::fromRGB(104, 73, 57),
                std::array<juce::String, 3>{ "GAIN", "CONTOUR", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>(
                4, "VERMIN DRIVE", "Filter Drive",
                juce::Colour::fromRGB(72, 70, 68),
                std::array<juce::String, 3>{ "DIST", "FILTER", "LEVEL" }));
            pedals.push_back(std::make_unique<PedalCard>(
                5, "VOID FUZZ", "Sustaining Fuzz",
                juce::Colour::fromRGB(66, 58, 67),
                std::array<juce::String, 3>{ "SUSTAIN", "TONE", "VOLUME" }));
        }
        else
        {
            pedals.push_back(std::make_unique<PedalCard>(
                1, "ORBIT", "Phaser",
                juce::Colour::fromRGB(72, 86, 82),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>(
                2, "CHORUS", "Analog Chorus",
                juce::Colour::fromRGB(72, 82, 96),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>(
                3, "PULSE", "Tremolo",
                juce::Colour::fromRGB(92, 77, 64),
                std::array<juce::String, 3>{ "RATE", "DEPTH", "SHAPE" }));
            pedals.push_back(std::make_unique<PedalCard>(
                4, "ECHO 404", "Analog Delay",
                juce::Colour::fromRGB(78, 65, 57),
                std::array<juce::String, 3>{ "TIME", "REGEN", "MIX" }));
            pedals.push_back(std::make_unique<PedalCard>(
                5, "SANCTUM", "Reverb",
                juce::Colour::fromRGB(75, 78, 73),
                std::array<juce::String, 3>{ "DECAY", "TONE", "MIX" }));
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

        auto notice = bounds.reduced(18.0f, 15.0f).removeFromTop(38.0f);
        g.setColour(Theme::surface().withAlpha(0.88f));
        g.fillRoundedRectangle(notice, 7.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(
            "EFFECTS ENGINE NOT AVAILABLE ON LATEST MAIN • CONTROLS STAY LOCKED UNTIL REAL PARAMETERS EXIST",
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
        bounds.removeFromTop(48);

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
