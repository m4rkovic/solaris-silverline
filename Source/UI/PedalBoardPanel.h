#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "Controls.h"

namespace solaris::ui
{
    class PedalCard final : public juce::Component
    {
    public:
        PedalCard(juce::String displayName,
                  juce::String familyName,
                  juce::Colour finishColour,
                  std::array<juce::String, 3> controlNames);

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        juce::String name;
        juce::String family;
        juce::Colour finish;
        std::array<std::unique_ptr<SolarisKnob>, 3> knobs;
        SolarisButton bypass { "ENGAGE", true };
        bool enabled = false;
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
