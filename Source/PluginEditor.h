#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NavigationBar.h"
#include "UI/AmpPanel.h"
#include "UI/PedalBoardPanel.h"
#include "UI/CabPanel.h"
#include "UI/EqPanel.h"
#include "UI/TunerOverlay.h"

class SolarisSilverlineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer
{
public:
    explicit SolarisSilverlineAudioProcessorEditor(SolarisSilverlineAudioProcessor&);
    ~SolarisSilverlineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage(solaris::ui::NavigationBar::Page page);
    void setTunerVisible(bool shouldBeVisible);

    SolarisSilverlineAudioProcessor& processor;

    solaris::ui::NavigationBar navigation;
    solaris::ui::PedalBoardPanel preFxPanel { false };
    solaris::ui::AmpPanel ampPanel;
    solaris::ui::CabPanel cabPanel;
    solaris::ui::PedalBoardPanel postFxPanel { true };
    solaris::ui::EqPanel eqPanel;
    solaris::ui::TunerOverlay tunerOverlay;

    solaris::ui::NavigationBar::Page currentPage = solaris::ui::NavigationBar::Page::amp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessorEditor)
};
