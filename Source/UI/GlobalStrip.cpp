#include "GlobalStrip.h"
#include "../PluginProcessor.h"

namespace solaris::ui
{
    LevelMeter::LevelMeter(juce::String labelText)
        : label(std::move(labelText))
    {
        setInterceptsMouseClicks(false, false);
    }

    void LevelMeter::setLevel(float levelDb, bool clipped)
    {
        const auto target = juce::jlimit(-72.0f, 6.0f, levelDb);
        displayDb = target >= displayDb
            ? target
            : juce::jmax(-72.0f, displayDb - 2.4f);

        if (clipped)
            clipHoldTicks = 30;
        else if (clipHoldTicks > 0)
            --clipHoldTicks;

        repaint();
    }

    void LevelMeter::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto labelArea = bounds.removeFromBottom(16.0f);
        auto clipArea = bounds.removeFromTop(14.0f);
        auto meter = bounds.reduced(5.0f, 2.0f);

        g.setColour(Theme::surface());
        g.fillRoundedRectangle(meter, 4.0f);
        g.setColour(Theme::border().withAlpha(0.75f));
        g.drawRoundedRectangle(meter.reduced(0.5f), 4.0f, 1.0f);

        const auto normalised = juce::jlimit(
            0.0f, 1.0f,
            juce::jmap(displayDb, -72.0f, 6.0f, 0.0f, 1.0f));

        auto fill = meter.reduced(2.0f);
        fill = fill.removeFromBottom(fill.getHeight() * normalised);

        g.setColour(displayDb > -3.0f ? Theme::amber() : Theme::silverLight().withAlpha(0.78f));
        g.fillRoundedRectangle(fill, 2.5f);

        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(clipHoldTicks > 0 ? Theme::amber() : Theme::textMuted().withAlpha(0.45f));
        g.drawText("CLIP", clipArea.toNearestInt(), juce::Justification::centred, false);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(label, labelArea.toNearestInt(), juce::Justification::centred, false);
    }

    GlobalStrip::GlobalStrip(SolarisSilverlineAudioProcessor& owner)
        : processor(owner)
    {
        auto& state = processor.getValueTreeState();
        inputAttachment = std::make_unique<SliderAttachment>(state, "inputGain", inputGain.control());
        outputAttachment = std::make_unique<SliderAttachment>(state, "outputGain", outputGain.control());

        inputGain.control().setSkewFactor(1.0);
        outputGain.control().setSkewFactor(1.0);

        addAndMakeVisible(inputGain);
        addAndMakeVisible(outputGain);
        addAndMakeVisible(inputMeter);
        addAndMakeVisible(outputMeter);

        startTimerHz(30);
    }

    GlobalStrip::~GlobalStrip()
    {
        stopTimer();
    }

    void GlobalStrip::timerCallback()
    {
        inputMeter.setLevel(processor.getInputPeakDb(), processor.consumeInputClip());
        outputMeter.setLevel(processor.getOutputPeakDb(), processor.consumeOutputClip());
    }

    void GlobalStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(Theme::surface().withAlpha(0.92f));
        g.fillRect(bounds);

        g.setColour(Theme::border().withAlpha(0.48f));
        g.drawLine(0.0f, bounds.getBottom() - 0.5f,
                   bounds.getRight(), bounds.getBottom() - 0.5f, 1.0f);

        auto centre = getLocalBounds().reduced(270, 11);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("INPUT  •  PRE FX  •  AMP  •  CAB  •  POST FX  •  EQ  •  OUTPUT",
                   centre, juce::Justification::centred, false);
    }

    void GlobalStrip::resized()
    {
        auto bounds = getLocalBounds().reduced(12, 6);

        auto left = bounds.removeFromLeft(244);
        inputGain.setBounds(left.removeFromLeft(112));
        inputMeter.setBounds(left.removeFromLeft(36).reduced(2, 0));

        auto right = bounds.removeFromRight(244);
        outputMeter.setBounds(right.removeFromRight(36).reduced(2, 0));
        outputGain.setBounds(right.removeFromRight(112));
    }
}
