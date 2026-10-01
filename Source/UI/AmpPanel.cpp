#include "AmpPanel.h"
#include "../PluginProcessor.h"

namespace solaris::ui
{
    AmpPanel::AmpPanel(SolarisSilverlineAudioProcessor& owner)
        : processor(owner),
          parameterState(owner.getValueTreeState())
    {
        volumeAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampVolume", volume.control());
        bassAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampBass", bass.control());
        trebleAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampTreble", treble.control());
        reverbAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampReverb", reverb.control());
        tremoloSpeedAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampTremoloSpeed", tremoloSpeed.control());
        tremoloIntensityAttachment = std::make_unique<SliderAttachment>(
            parameterState, "ampTremoloIntensity", tremoloIntensity.control());

        analogueModelButton.onClick = [this]
        {
            processor.useAnalogueAmp();
            refreshModelStatus();
        };

        neuralModelButton.onClick = [this]
        {
            if (!processor.isNeuralAudioAvailable())
                return;

            const auto path = parameterState.state.getProperty("neuralModelPath").toString();
            const juce::File modelFile(path);

            if (path.isNotEmpty() && modelFile.existsAsFile())
                processor.loadNeuralAmpModel(modelFile);

            refreshModelStatus();
        };

        loadModelButton.onClick = [this]
        {
            loadNeuralModelFromDialog();
        };

        voicingButton.onClick = [this]
        {
            const auto* value = parameterState.getRawParameterValue("ampChannel");
            const auto vintage = value != nullptr && value->load() >= 0.5f;
            setParameterNormalized("ampChannel", vintage ? 0.0f : 1.0f);
        };

        bypassButton.onClick = [this]
        {
            const auto* value = parameterState.getRawParameterValue("ampEnabled");
            const auto enabled = value == nullptr || value->load() >= 0.5f;
            setParameterNormalized("ampEnabled", enabled ? 0.0f : 1.0f);
        };

        analogueModelButton.setTooltip("Use the built-in Silverline 68 analogue model.");
        neuralModelButton.setTooltip("Use the loaded NAM model.");
        loadModelButton.setTooltip("Load a Neural Amp Model (.nam) file.");

        modelStatusLabel.setJustificationType(juce::Justification::centredRight);
        modelStatusLabel.setColour(juce::Label::textColourId, Theme::amber());
        modelStatusLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        modelStatusLabel.setInterceptsMouseClicks(false, false);

        modelNameLabel.setJustificationType(juce::Justification::centredRight);
        modelNameLabel.setColour(juce::Label::textColourId, Theme::textMuted());
        modelNameLabel.setFont(juce::FontOptions(10.5f));
        modelNameLabel.setInterceptsMouseClicks(false, false);

        addAndMakeVisible(analogueModelButton);
        addAndMakeVisible(neuralModelButton);
        addAndMakeVisible(loadModelButton);
        addAndMakeVisible(voicingButton);
        addAndMakeVisible(bypassButton);
        addAndMakeVisible(modelStatusLabel);
        addAndMakeVisible(modelNameLabel);
        addAndMakeVisible(volume);
        addAndMakeVisible(bass);
        addAndMakeVisible(treble);
        addAndMakeVisible(reverb);
        addAndMakeVisible(tremoloSpeed);
        addAndMakeVisible(tremoloIntensity);

        timerCallback();
        startTimerHz(8);
    }

    AmpPanel::~AmpPanel()
    {
        stopTimer();
    }

    void AmpPanel::setParameterNormalized(const juce::String& id, float normalizedValue)
    {
        if (auto* parameter = parameterState.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
            parameter->endChangeGesture();
        }
    }

    void AmpPanel::loadNeuralModelFromDialog()
    {
        if (!processor.isNeuralAudioAvailable())
        {
            refreshModelStatus();
            return;
        }

        modelChooser = std::make_unique<juce::FileChooser>(
            "Load Neural Amp Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.nam");

        auto safeThis = juce::Component::SafePointer<AmpPanel>(this);
        modelChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (auto* self = safeThis.getComponent())
                {
                    const auto file = chooser.getResult();
                    if (file.existsAsFile())
                    {
                        const auto loaded = self->processor.loadNeuralAmpModel(file);
                        self->refreshModelStatus();

                        if (!loaded)
                            self->modelStatusLabel.setText(
                                "NAM • LOAD FAILED", juce::dontSendNotification);
                    }
                }
            });
    }

    void AmpPanel::refreshModelStatus()
    {
        const auto activeId = processor.getActiveAmpModelId();
        const auto neuralAvailable = processor.isNeuralAudioAvailable();
        const auto path = parameterState.state.getProperty("neuralModelPath").toString();
        const juce::File modelFile(path);
        const auto modelReady = path.isNotEmpty() && modelFile.existsAsFile();
        const auto neuralActive = activeId == "neural-nam";

        analogueModelButton.setActive(activeId == "silverline68");
        neuralModelButton.setActive(neuralActive);
        neuralModelButton.setEnabled(neuralAvailable);
        loadModelButton.setEnabled(neuralAvailable);

        if (!neuralAvailable)
        {
            modelStatusLabel.setText("NAM • BACKEND OFF", juce::dontSendNotification);
            modelNameLabel.setText(
                "NeuralAudio disabled in this build", juce::dontSendNotification);
        }
        else if (neuralActive)
        {
            modelStatusLabel.setText("NAM • ACTIVE", juce::dontSendNotification);
            modelNameLabel.setText(
                modelReady ? modelFile.getFileNameWithoutExtension()
                           : juce::String("Neural model"),
                juce::dontSendNotification);
        }
        else if (modelReady)
        {
            modelStatusLabel.setText("NAM • READY", juce::dontSendNotification);
            modelNameLabel.setText(
                modelFile.getFileNameWithoutExtension(), juce::dontSendNotification);
        }
        else
        {
            modelStatusLabel.setText("NAM • EMPTY", juce::dontSendNotification);
            modelNameLabel.setText(
                "Load a .nam model to enable Neural Model",
                juce::dontSendNotification);
        }
    }

    void AmpPanel::timerCallback()
    {
        const auto* channel = parameterState.getRawParameterValue("ampChannel");
        const auto* enabledValue = parameterState.getRawParameterValue("ampEnabled");

        const auto vintage = channel != nullptr && channel->load() >= 0.5f;
        const auto enabled = enabledValue == nullptr || enabledValue->load() >= 0.5f;

        voicingButton.setText(vintage ? "VINTAGE" : "CUSTOM");
        voicingButton.setActive(true);

        bypassButton.setText(enabled ? "AMP ON" : "BYPASS");
        bypassButton.setActive(!enabled);

        refreshModelStatus();
    }

    void AmpPanel::paint(juce::Graphics& g)
    {
        auto content = getLocalBounds().toFloat().reduced(28.0f, 18.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("AMPLIFIER", content.removeFromTop(22.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        auto chassis = content.reduced(6.0f, 2.0f);
        auto shadow = chassis.translated(0.0f, 7.0f);
        g.setColour(juce::Colours::black.withAlpha(0.52f));
        g.fillRoundedRectangle(shadow, 18.0f);

        g.setColour(juce::Colour::fromRGB(10, 10, 10));
        g.fillRoundedRectangle(chassis, 18.0f);
        g.setColour(juce::Colour::fromRGB(55, 54, 51));
        g.drawRoundedRectangle(chassis.reduced(0.5f), 18.0f, 1.0f);

        auto inner = chassis.reduced(18.0f);
        auto faceplate = inner.removeFromTop(252.0f);
        Theme::drawBrushedMetal(g, faceplate);

        g.setColour(juce::Colour::fromRGB(34, 34, 33));
        g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
        g.drawText("SILVERLINE",
                   faceplate.withTrimmedLeft(24.0f).withHeight(38.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        g.setColour(juce::Colour::fromRGB(72, 66, 57));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("BOUTIQUE AMPLIFIER PLATFORM",
                   faceplate.withTrimmedLeft(24.0f).withTrimmedTop(29.0f)
                            .withHeight(18.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        inner.removeFromTop(12.0f);
        auto grilleArea = inner;
        Theme::drawTextile(g, grilleArea);

        const auto speakerSize = juce::jmin(grilleArea.getHeight() * 0.70f,
                                            grilleArea.getWidth() * 0.22f);
        const auto cy = grilleArea.getCentreY();
        const auto cx1 = grilleArea.getCentreX() - speakerSize * 0.66f;
        const auto cx2 = grilleArea.getCentreX() + speakerSize * 0.66f;

        for (auto cx : { cx1, cx2 })
        {
            juce::Rectangle<float> speaker(cx - speakerSize * 0.5f,
                                           cy - speakerSize * 0.5f,
                                           speakerSize, speakerSize);
            g.setColour(juce::Colours::black.withAlpha(0.64f));
            g.fillEllipse(speaker);
            g.setColour(juce::Colour::fromRGB(53, 51, 47).withAlpha(0.8f));
            g.drawEllipse(speaker.reduced(5.0f), 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillEllipse(speaker.reduced(speakerSize * 0.34f));
        }

        g.setColour(Theme::amberMuted().withAlpha(0.7f));
        g.fillEllipse(grilleArea.getRight() - 24.0f,
                      grilleArea.getBottom() - 24.0f, 6.0f, 6.0f);
    }

    void AmpPanel::resized()
    {
        auto content = getLocalBounds().reduced(28, 18);
        content.removeFromTop(22);
        auto chassis = content.reduced(6, 2);
        auto inner = chassis.reduced(18);
        auto face = inner.removeFromTop(252);

        auto modelRow = face.withTrimmedLeft(24).withTrimmedRight(24).removeFromTop(48);
        modelRow.removeFromTop(9);
        analogueModelButton.setBounds(modelRow.removeFromLeft(118));
        modelRow.removeFromLeft(6);
        neuralModelButton.setBounds(modelRow.removeFromLeft(112));
        modelRow.removeFromLeft(6);
        loadModelButton.setBounds(modelRow.removeFromLeft(100));

        auto modelText = modelRow.reduced(8, 0);
        modelStatusLabel.setBounds(modelText.removeFromTop(17));
        modelNameLabel.setBounds(modelText.removeFromTop(21));

        voicingButton.setBounds(face.getX() + 24, face.getY() + 52, 104, 30);
        bypassButton.setBounds(face.getRight() - 116, face.getY() + 52, 92, 30);

        auto controls = face.withTrimmedTop(88).reduced(20, 8);
        constexpr int count = 6;
        constexpr int gap = 8;
        const int knobWidth = (controls.getWidth() - gap * (count - 1)) / count;

        std::array<SolarisKnob*, count> knobs {
            &volume, &bass, &treble, &reverb, &tremoloSpeed, &tremoloIntensity
        };

        for (int i = 0; i < count; ++i)
        {
            knobs[static_cast<std::size_t>(i)]->setBounds(
                controls.removeFromLeft(knobWidth));
            if (i < count - 1)
                controls.removeFromLeft(gap);
        }
    }
}
