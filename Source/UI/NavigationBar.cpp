#include "NavigationBar.h"

namespace solaris::ui
{
    NavigationBar::NavigationBar()
    {
        const std::array<juce::String, pageCount> names {
            "PRE FX", "AMP", "CAB", "POST FX", "EQ"
        };

        for (int i = 0; i < pageCount; ++i)
        {
            pageButtons[(size_t) i] = std::make_unique<SolarisButton>(names[(size_t) i]);
            auto* button = pageButtons[(size_t) i].get();
            button->onClick = [this, i]
            {
                setActivePage(static_cast<Page>(i));
                if (onPageSelected)
                    onPageSelected(static_cast<Page>(i));
            };
            addAndMakeVisible(*button);
        }

        presetButton.onClick = [] {};
        tunerButton.onClick = [this]
        {
            if (onTunerRequested)
                onTunerRequested();
        };
        settingsButton.onClick = [] {};

        addAndMakeVisible(presetButton);
        addAndMakeVisible(tunerButton);
        addAndMakeVisible(settingsButton);

        setActivePage(Page::amp);
    }

    void NavigationBar::setActivePage(Page page)
    {
        activePage = page;
        for (int i = 0; i < pageCount; ++i)
            pageButtons[(size_t) i]->setActive(i == static_cast<int>(activePage));
    }

    void NavigationBar::paint(juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();

        juce::ColourGradient top(juce::Colour::fromRGB(27, 28, 29),
                                 r.getCentreX(), r.getY(),
                                 Theme::surface(),
                                 r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(top);
        g.fillRect(r);

        g.setColour(Theme::border().withAlpha(0.62f));
        g.drawLine(0.0f, r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.0f);

        auto brandArea = getLocalBounds().removeFromLeft(176).reduced(18, 8);
        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(17.5f, juce::Font::bold));
        g.drawText("SOLARIS", brandArea.removeFromTop(27),
                   juce::Justification::centredLeft, false);

        g.setColour(Theme::amber());
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Silverline", brandArea.removeFromTop(18),
                   juce::Justification::centredLeft, false);
    }

    void NavigationBar::resized()
    {
        auto r = getLocalBounds().reduced(12, 10);
        r.removeFromLeft(176);

        const int utilityWidth = juce::jmin(326, juce::jmax(258, getWidth() / 4));
        auto utility = r.removeFromRight(utilityWidth);

        const int gap = 6;
        settingsButton.setBounds(utility.removeFromRight(78));
        utility.removeFromRight(gap);
        tunerButton.setBounds(utility.removeFromRight(70));
        utility.removeFromRight(gap);
        presetButton.setBounds(utility);

        r.removeFromRight(16);
        const int tabGap = 5;
        const int buttonWidth = (r.getWidth() - tabGap * (pageCount - 1)) / pageCount;

        for (int i = 0; i < pageCount; ++i)
        {
            pageButtons[(size_t) i]->setBounds(r.removeFromLeft(buttonWidth));
            if (i < pageCount - 1)
                r.removeFromLeft(tabGap);
        }
    }
}
