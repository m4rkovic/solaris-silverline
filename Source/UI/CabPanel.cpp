#include "CabPanel.h"
#include "../PluginProcessor.h"

namespace solaris::ui
{
    CabPanel::CabPanel(SolarisSilverlineAudioProcessor& owner)
        : processor(owner),
          parameterState(owner.getValueTreeState())
    {
        wetAttachment = std::make_unique<SliderAttachment>(
            parameterState, "cabWet", wet.control());
        blendAttachment = std::make_unique<SliderAttachment>(
            parameterState, "cabMicBlend", blend.control());

        loadIrAButton.onClick = [this]
        {
            loadIr(solaris::CabinetIRSlot::micA);
        };

        loadIrBButton.onClick = [this]
        {
            loadIr(solaris::CabinetIRSlot::micB);
        };

        phaseButton.onClick = [this]
        {
            const auto* value = parameterState.getRawParameterValue("cabPhaseB");
            const auto inverted = value != nullptr && value->load() >= 0.5f;
            setBoolParameter("cabPhaseB", !inverted);
        };

        loadIrAButton.setTooltip("Load an impulse response into Mic A.");
        loadIrBButton.setTooltip("Load an impulse response into Mic B.");
        phaseButton.setTooltip("Invert the polarity of Mic B.");

        for (auto* label : { &irAName, &irBName })
        {
            label->setJustificationType(juce::Justification::centredLeft);
            label->setColour(juce::Label::textColourId, Theme::textMuted());
            label->setFont(juce::FontOptions(10.5f));
            label->setInterceptsMouseClicks(false, false);
        }

        addAndMakeVisible(loadIrAButton);
        addAndMakeVisible(loadIrBButton);
        addAndMakeVisible(phaseButton);
        addAndMakeVisible(wet);
        addAndMakeVisible(blend);
        addAndMakeVisible(irAName);
        addAndMakeVisible(irBName);

        timerCallback();
        startTimerHz(4);
    }

    CabPanel::~CabPanel()
    {
        stopTimer();
    }

    void CabPanel::setBoolParameter(const juce::String& id, bool value)
    {
        if (auto* parameter = parameterState.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(value ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
    }

    void CabPanel::loadIr(solaris::CabinetIRSlot slot)
    {
        irChooser = std::make_unique<juce::FileChooser>(
            slot == solaris::CabinetIRSlot::micA
                ? "Load Mic A Impulse Response"
                : "Load Mic B Impulse Response",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav;*.aif;*.aiff;*.flac");

        auto safeThis = juce::Component::SafePointer<CabPanel>(this);
        irChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis, slot](const juce::FileChooser& chooser)
            {
                if (auto* self = safeThis.getComponent())
                {
                    const auto file = chooser.getResult();
                    if (!file.existsAsFile())
                        return;

                    juce::AudioFormatManager formats;
                    formats.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> reader(
                        formats.createReaderFor(file));

                    const auto stereo = reader != nullptr && reader->numChannels > 1;
                    self->processor.getCabinetEngine().requestImpulseResponseFromFile(
                        slot, file, stereo);
                    self->refreshIrStatus();
                }
            });
    }

    void CabPanel::refreshIrStatus()
    {
        const auto state = processor.getCabinetEngine().createState();

        auto readSlot = [&state](int slotIndex, bool& activeFlag)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                const auto child = state.getChild(i);
                if (!child.hasType("IR_SLOT"))
                    continue;

                if (static_cast<int>(child.getProperty("index", -1)) != slotIndex)
                    continue;

                activeFlag = static_cast<bool>(child.getProperty("active", false));
                const auto path = child.getProperty("sourcePath").toString();

                if (!activeFlag)
                    return juce::String("No IR loaded");

                if (path.isEmpty())
                    return juce::String("Embedded IR");

                return juce::File(path).getFileName();
            }

            activeFlag = false;
            return juce::String("No IR loaded");
        };

        irAName.setText(readSlot(0, slotAActive), juce::dontSendNotification);
        irBName.setText(readSlot(1, slotBActive), juce::dontSendNotification);
    }

    void CabPanel::timerCallback()
    {
        const auto* phase = parameterState.getRawParameterValue("cabPhaseB");
        const auto inverted = phase != nullptr && phase->load() >= 0.5f;

        phaseButton.setActive(inverted);
        phaseButton.setText(inverted ? "PHASE B INV" : "PHASE B");
        refreshIrStatus();
    }

    void CabPanel::paint(juce::Graphics& g)
    {
        auto outer = getLocalBounds().toFloat().reduced(24.0f, 18.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("CABINET / IMPULSE RESPONSES",
                   outer.removeFromTop(22.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, outer, 16.0f);

        auto body = outer.reduced(22.0f);
        auto visual = body.removeFromLeft(body.getWidth() * 0.57f).reduced(8.0f);

        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.fillRoundedRectangle(visual.translated(0.0f, 5.0f), 14.0f);

        g.setColour(juce::Colour::fromRGB(24, 23, 22));
        g.fillRoundedRectangle(visual, 14.0f);
        g.setColour(juce::Colour::fromRGB(72, 69, 63));
        g.drawRoundedRectangle(visual.reduced(0.5f), 14.0f, 1.2f);

        auto grille = visual.reduced(18.0f);
        Theme::drawTextile(g, grille);

        const auto diameter = juce::jmin(
            grille.getHeight() * 0.62f, grille.getWidth() * 0.34f);

        const auto left = juce::Rectangle<float>(0, 0, diameter, diameter)
            .withCentre({ grille.getCentreX() - diameter * 0.57f,
                          grille.getCentreY() + 12.0f });
        const auto right = juce::Rectangle<float>(0, 0, diameter, diameter)
            .withCentre({ grille.getCentreX() + diameter * 0.57f,
                          grille.getCentreY() + 12.0f });

        int index = 0;
        for (auto speaker : { left, right })
        {
            g.setColour(juce::Colours::black.withAlpha(0.74f));
            g.fillEllipse(speaker);
            g.setColour(juce::Colour::fromRGB(62, 60, 55));
            g.drawEllipse(speaker.reduced(5.0f), 2.0f);
            g.setColour(juce::Colour::fromRGB(17, 17, 16));
            g.fillEllipse(speaker.reduced(diameter * 0.34f));

            const auto active = index == 0 ? slotAActive : slotBActive;
            g.setColour(active ? Theme::amber() : Theme::textMuted());
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(index == 0 ? "MIC A IR" : "MIC B IR",
                       speaker.withY(speaker.getY() - 28.0f)
                              .withHeight(20.0f).toNearestInt(),
                       juce::Justification::centred, false);
            ++index;
        }

        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText("SILVERLINE 2 × 10",
                   visual.withTrimmedTop(12.0f).withHeight(26.0f).toNearestInt(),
                   juce::Justification::centred, false);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f));
        g.drawText("Dual IR slots • blend • wet mix • Mic B polarity",
                   visual.withTrimmedTop(38.0f).withHeight(18.0f).toNearestInt(),
                   juce::Justification::centred, false);
    }

    void CabPanel::resized()
    {
        auto outer = getLocalBounds().reduced(24, 18);
        outer.removeFromTop(22);
        auto body = outer.reduced(22);
        body.removeFromLeft(static_cast<int>(body.getWidth() * 0.57f));
        auto controls = body.reduced(14, 10);

        auto slotA = controls.removeFromTop(58);
        loadIrAButton.setBounds(slotA.removeFromLeft(102).withSizeKeepingCentre(102, 32));
        slotA.removeFromLeft(8);
        irAName.setBounds(slotA);

        controls.removeFromTop(6);
        auto slotB = controls.removeFromTop(58);
        loadIrBButton.setBounds(slotB.removeFromLeft(102).withSizeKeepingCentre(102, 32));
        slotB.removeFromLeft(8);
        irBName.setBounds(slotB);

        controls.removeFromTop(12);
        auto knobs = controls.removeFromTop(154);
        wet.setBounds(knobs.removeFromLeft(knobs.getWidth() / 2).reduced(6, 0));
        blend.setBounds(knobs.reduced(6, 0));

        controls.removeFromTop(8);
        phaseButton.setBounds(controls.removeFromTop(34).reduced(26, 0));
    }
}
