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

        presetButton.setEnabled(false);
        presetButton.setTooltip("Preset browser shell. Preset indexing is not connected yet.");

        tunerButton.onClick = [this]
        {
            if (onTunerRequested)
                onTunerRequested();
        };

        addAndMakeVisible(presetButton);
        addAndMakeVisible(tunerButton);

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
        auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient top(juce::Colour::fromRGB(27, 28, 29),
                                 bounds.getCentreX(), bounds.getY(),
                                 Theme::surface(),
                                 bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(top);
        g.fillRect(bounds);

        g.setColour(Theme::border().withAlpha(0.62f));
        g.drawLine(0.0f, bounds.getBottom() - 0.5f,
                   bounds.getRight(), bounds.getBottom() - 0.5f, 1.0f);

        auto brandArea = getLocalBounds().removeFromLeft(176).reduced(18, 7);
        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(17.5f, juce::Font::bold));
        g.drawText("SOLARIS", brandArea.removeFromTop(26),
                   juce::Justification::centredLeft, false);

        g.setColour(Theme::amber());
        g.setFont(juce::FontOptions(11.5f));
        g.drawText("Silverline", brandArea.removeFromTop(17),
                   juce::Justification::centredLeft, false);
    }

    void NavigationBar::resized()
    {
        auto area = getLocalBounds().reduced(12, 9);
        area.removeFromLeft(176);

        auto utility = area.removeFromRight(226);
        tunerButton.setBounds(utility.removeFromRight(72));
        utility.removeFromRight(7);
        presetButton.setBounds(utility);

        area.removeFromRight(14);
        constexpr int tabGap = 5;
        const int buttonWidth = (area.getWidth() - tabGap * (pageCount - 1)) / pageCount;

        for (int i = 0; i < pageCount; ++i)
        {
            pageButtons[(size_t) i]->setBounds(area.removeFromLeft(buttonWidth));
            if (i < pageCount - 1)
                area.removeFromLeft(tabGap);
        }
    }
}
