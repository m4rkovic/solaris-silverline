#pragma once

#include <JuceHeader.h>

namespace solaris::ui
{
    struct Theme final
    {
        static juce::Colour background()    { return juce::Colour::fromRGB(12, 13, 14); }
        static juce::Colour surface()       { return juce::Colour::fromRGB(20, 21, 22); }
        static juce::Colour raised()        { return juce::Colour::fromRGB(29, 30, 31); }
        static juce::Colour border()        { return juce::Colour::fromRGB(67, 68, 70); }
        static juce::Colour text()          { return juce::Colour::fromRGB(231, 229, 221); }
        static juce::Colour textMuted()     { return juce::Colour::fromRGB(151, 151, 146); }
        static juce::Colour silverLight()   { return juce::Colour::fromRGB(207, 207, 201); }
        static juce::Colour silverMid()     { return juce::Colour::fromRGB(137, 139, 138); }
        static juce::Colour silverDark()    { return juce::Colour::fromRGB(71, 73, 73); }
        static juce::Colour amber()         { return juce::Colour::fromRGB(205, 157, 83); }
        static juce::Colour amberMuted()    { return juce::Colour::fromRGB(126, 94, 52); }
        static juce::Colour grille()        { return juce::Colour::fromRGB(18, 18, 17); }
        static juce::Colour grid()          { return juce::Colour::fromRGB(48, 49, 50); }

        static void fillPanel(juce::Graphics& g,
                              juce::Rectangle<float> area,
                              float radius = 14.0f)
        {
            juce::ColourGradient gradient(raised(), area.getCentreX(), area.getY(),
                                          surface(), area.getCentreX(), area.getBottom(), false);
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(area, radius);

            g.setColour(border().withAlpha(0.65f));
            g.drawRoundedRectangle(area.reduced(0.5f), radius, 1.0f);
        }

        static void drawBrushedMetal(juce::Graphics& g, juce::Rectangle<float> area)
        {
            juce::ColourGradient gradient(silverLight(), area.getCentreX(), area.getY(),
                                          silverMid(), area.getCentreX(), area.getBottom(), false);
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(area, 10.0f);

            g.setColour(juce::Colours::white.withAlpha(0.055f));
            for (int y = (int) area.getY() + 2; y < (int) area.getBottom(); y += 4)
                g.drawHorizontalLine(y, area.getX() + 4.0f, area.getRight() - 4.0f);

            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.0f);
        }

        static void drawTextile(juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.setColour(grille());
            g.fillRoundedRectangle(area, 8.0f);

            g.setColour(juce::Colour::fromRGB(42, 41, 38).withAlpha(0.68f));
            for (int x = (int) area.getX(); x < (int) area.getRight(); x += 7)
                g.drawVerticalLine(x, area.getY(), area.getBottom());

            g.setColour(juce::Colours::white.withAlpha(0.025f));
            for (int y = (int) area.getY(); y < (int) area.getBottom(); y += 6)
                g.drawHorizontalLine(y, area.getX(), area.getRight());
        }
    };
}
