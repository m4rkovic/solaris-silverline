#include "PluginEditor.h"
#include "UI/Theme.h"

SolarisSilverlineAudioProcessorEditor::SolarisSilverlineAudioProcessorEditor(
    SolarisSilverlineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    juce::ignoreUnused(processor);

    setResizable(true, true);
    setResizeLimits(960, 600, 1920, 1200);
    setSize(1280, 800);

    addAndMakeVisible(navigation);
    addAndMakeVisible(preFxPanel);
    addAndMakeVisible(ampPanel);
    addAndMakeVisible(cabPanel);
    addAndMakeVisible(postFxPanel);
    addAndMakeVisible(eqPanel);
    addChildComponent(tunerOverlay);

    navigation.onPageSelected = [this](solaris::ui::NavigationBar::Page page)
    {
        showPage(page);
    };

    navigation.onTunerRequested = [this]
    {
        setTunerVisible(!tunerOverlay.isVisible());
    };

    tunerOverlay.onClose = [this]
    {
        setTunerVisible(false);
    };

    showPage(currentPage);
}

void SolarisSilverlineAudioProcessorEditor::paint(juce::Graphics& g)
{
    using solaris::ui::Theme;

    const auto r = getLocalBounds().toFloat();
    juce::ColourGradient background(juce::Colour::fromRGB(18, 19, 20),
                                    r.getCentreX(), r.getY(),
                                    Theme::background(),
                                    r.getCentreX(), r.getBottom(), false);
    g.setGradientFill(background);
    g.fillRect(r);

    g.setColour(juce::Colours::white.withAlpha(0.018f));
    for (int y = 72; y < getHeight(); y += 8)
        g.drawHorizontalLine(y, 0.0f, (float) getWidth());

    g.setColour(Theme::border().withAlpha(0.35f));
    g.drawRect(getLocalBounds().toFloat().reduced(0.5f), 1.0f);
}

void SolarisSilverlineAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    navigation.setBounds(r.removeFromTop(68));

    auto content = r.reduced(8, 6);
    preFxPanel.setBounds(content);
    ampPanel.setBounds(content);
    cabPanel.setBounds(content);
    postFxPanel.setBounds(content);
    eqPanel.setBounds(content);
    tunerOverlay.setBounds(getLocalBounds());
}

void SolarisSilverlineAudioProcessorEditor::showPage(solaris::ui::NavigationBar::Page page)
{
    currentPage = page;
    navigation.setActivePage(page);

    preFxPanel.setVisible(page == solaris::ui::NavigationBar::Page::preFx);
    ampPanel.setVisible(page == solaris::ui::NavigationBar::Page::amp);
    cabPanel.setVisible(page == solaris::ui::NavigationBar::Page::cab);
    postFxPanel.setVisible(page == solaris::ui::NavigationBar::Page::postFx);
    eqPanel.setVisible(page == solaris::ui::NavigationBar::Page::eq);

    setTunerVisible(false);
}

void SolarisSilverlineAudioProcessorEditor::setTunerVisible(bool shouldBeVisible)
{
    tunerOverlay.setVisible(shouldBeVisible);
    if (shouldBeVisible)
        tunerOverlay.toFront(true);
}
