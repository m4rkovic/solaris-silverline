#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include "Controls.h"

namespace solaris::ui
{
    class NavigationBar final : public juce::Component
    {
    public:
        enum class Page
        {
            preFx = 0,
            amp,
            cab,
            postFx,
            eq
        };

        NavigationBar();

        void setActivePage(Page page);
        void paint(juce::Graphics&) override;
        void resized() override;

        std::function<void(Page)> onPageSelected;
        std::function<void()> onTunerRequested;

    private:
        static constexpr int pageCount = 5;
        std::array<std::unique_ptr<SolarisButton>, pageCount> pageButtons;
        SolarisButton presetButton { "SILVER CLEAN", false };
        SolarisButton tunerButton  { "TUNER", true };
        SolarisButton settingsButton { "SETTINGS", false };

        Page activePage = Page::amp;
    };
}
