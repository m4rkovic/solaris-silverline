#include "PluginEditor.h"

SolarisSilverlineAudioProcessorEditor::SolarisSilverlineAudioProcessorEditor(
    SolarisSilverlineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(900, 560, 1800, 1120);
    setSize(1200, 720);

    title.setText("SOLARIS", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    addAndMakeVisible(title);

    subtitle.setText("Silverline", juce::dontSendNotification);
    subtitle.setJustificationType(juce::Justification::centred);
    subtitle.setFont(juce::FontOptions(20.0f, juce::Font::plain));
    addAndMakeVisible(subtitle);
}

void SolarisSilverlineAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Deliberately simple placeholder UI. Real visual system comes next.
    g.fillAll(juce::Colour::fromRGB(20, 20, 22));

    auto area = getLocalBounds().toFloat().reduced(24.0f);

    g.setColour(juce::Colour::fromRGB(48, 49, 52));
    g.drawRoundedRectangle(area, 18.0f, 1.0f);

    g.setColour(juce::Colour::fromRGB(188, 190, 194));
    g.setFont(16.0f);
    g.drawFittedText(
        "PRE FX     AMP     CAB     POST FX     EQ",
        getLocalBounds().withTrimmedTop(116).withHeight(36),
        juce::Justification::centred,
        1);

    g.setColour(juce::Colour::fromRGB(116, 117, 122));
    g.setFont(14.0f);
    g.drawFittedText(
        "v0.1  •  audio passthrough skeleton  •  modular amp engine ready",
        getLocalBounds().withTrimmedTop(getHeight() - 64).withHeight(28),
        juce::Justification::centred,
        1);
}

void SolarisSilverlineAudioProcessorEditor::resized()
{
    const auto centreX = getWidth() / 2;
    title.setBounds(centreX - 160, 38, 320, 44);
    subtitle.setBounds(centreX - 120, 78, 240, 30);
}
