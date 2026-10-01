#pragma once

#include <JuceHeader.h>
#include <memory>
#include "Controls.h"
#include "../../SolarisCore/Cab/CabinetEngine.h"

class SolarisSilverlineAudioProcessor;

namespace solaris::ui
{
    class CabPanel final : public juce::Component,
                           private juce::Timer
    {
    public:
        explicit CabPanel(SolarisSilverlineAudioProcessor& processor);
        ~CabPanel() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        void timerCallback() override;
        void setBoolParameter(const juce::String& id, bool value);
        void loadIr(solaris::CabinetIRSlot slot);
        void refreshIrStatus();

        SolarisSilverlineAudioProcessor& processor;
        juce::AudioProcessorValueTreeState& parameterState;

        SolarisButton loadIrAButton { "LOAD IR A", false };
        SolarisButton loadIrBButton { "LOAD IR B", false };
        SolarisButton phaseButton { "PHASE B", true };
        SolarisKnob wet { "CAB WET", "%", 0.0, 100.0, 0.1, 100.0 };
        SolarisKnob blend { "MIC B BLEND", "% B", 0.0, 100.0, 0.1, 50.0 };

        juce::Label irAName;
        juce::Label irBName;

        std::unique_ptr<SliderAttachment> wetAttachment;
        std::unique_ptr<SliderAttachment> blendAttachment;
        std::unique_ptr<juce::FileChooser> irChooser;

        bool slotAActive = false;
        bool slotBActive = false;
    };
}
