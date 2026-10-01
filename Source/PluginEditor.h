#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SolarisSilverlineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SolarisSilverlineAudioProcessorEditor(SolarisSilverlineAudioProcessor&);
    ~SolarisSilverlineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SolarisSilverlineAudioProcessor& processor;

    juce::Label title;
    juce::Label subtitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessorEditor)
};
