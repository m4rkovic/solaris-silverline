#pragma once

#include <JuceHeader.h>
#include <memory>
#include "Controls.h"

class SolarisSilverlineAudioProcessor;

namespace solaris::ui
{
    class LevelMeter final : public juce::Component
    {
    public:
        explicit LevelMeter(juce::String labelText);
        void setLevel(float levelDb, bool clipped);
        void paint(juce::Graphics&) override;

    private:
        juce::String label;
        float displayDb = -72.0f;
        int clipHoldTicks = 0;
    };

    class GlobalStrip final : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit GlobalStrip(SolarisSilverlineAudioProcessor& processor);
        ~GlobalStrip() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        void timerCallback() override;

        SolarisSilverlineAudioProcessor& processor;
        SolarisKnob inputGain { "INPUT", " dB", -24.0, 24.0, 0.1, 0.0, true };
        SolarisKnob outputGain { "OUTPUT", " dB", -24.0, 24.0, 0.1, 0.0, true };
        LevelMeter inputMeter { "IN" };
        LevelMeter outputMeter { "OUT" };

        std::unique_ptr<SliderAttachment> inputAttachment;
        std::unique_ptr<SliderAttachment> outputAttachment;
    };
}
