#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Controls.h"

namespace solaris::ui
{
    class TunerOverlay final : public juce::Component
    {
    public:
        TunerOverlay();

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;

        std::function<void()> onClose;

    private:
        juce::Rectangle<float> cardBounds;
        SolarisButton closeButton { "CLOSE", false };
        SolarisButton muteButton { "MUTE", true };
        bool muted = false;
    };
}
