#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "Controls.h"
#include "Theme.h"

namespace solaris::ui
{
    class PedalCard final : public juce::Component
    {
    public:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

        PedalCard(juce::AudioProcessorValueTreeState& state,
                  int signalOrder,
                  juce::String displayName,
                  juce::String familyName,
                  juce::Colour finishColour,
                  juce::String enabledParameter,
                  std::array<juce::String, 3> controlNames,
                  std::array<juce::String, 3> parameterIds);
        ~PedalCard() override = default;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        int order = 0;
        juce::String name;
        juce::String family;
        juce::Colour finish;

        SolarisButton bypassButton { "OFF" };
        std::array<std::unique_ptr<SolarisKnob>, 3> knobs;
        std::unique_ptr<ButtonAttachment> bypassAttachment;
        std::array<std::unique_ptr<SliderAttachment>, 3> knobAttachments;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalCard)
    };

    class PedalBoardPanel final : public juce::Component
    {
    public:
        PedalBoardPanel(juce::AudioProcessorValueTreeState& state, bool postEffects);

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        bool postFx = false;
        std::vector<std::unique_ptr<PedalCard>> pedals;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalBoardPanel)
    };
}
