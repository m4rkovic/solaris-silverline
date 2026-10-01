#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "Theme.h"

namespace solaris::ui
{
    class PedalCard final : public juce::Component
    {
    public:
        PedalCard(int signalOrder,
                  juce::String displayName,
                  juce::String familyName,
                  juce::Colour finishColour,
                  std::array<juce::String, 3> controlNames);

        void paint(juce::Graphics&) override;

    private:
        int order = 0;
        juce::String name;
        juce::String family;
        juce::Colour finish;
        std::array<juce::String, 3> controls;
    };

    class PedalBoardPanel final : public juce::Component
    {
    public:
        explicit PedalBoardPanel(bool postEffects);

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        bool postFx = false;
        std::vector<std::unique_ptr<PedalCard>> pedals;
    };
}
