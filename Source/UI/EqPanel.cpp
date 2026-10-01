#include "EqPanel.h"
#include <cmath>

namespace solaris::ui
{
    namespace
    {
        constexpr float minFrequency = 20.0f;
        constexpr float maxFrequency = 22000.0f;
        constexpr float gainSpan = 36.0f;

        float frequencyToX(float frequency) noexcept
        {
            const auto safe = juce::jlimit(minFrequency, maxFrequency, frequency);
            return static_cast<float>(
                std::log(safe / minFrequency) / std::log(maxFrequency / minFrequency));
        }

        float xToFrequency(float x) noexcept
        {
            return minFrequency * static_cast<float>(
                std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, x)));
        }
    }

    EqPanel::EqPanel(juce::AudioProcessorValueTreeState& state)
        : parameterState(state)
    {
        highPass.control().setSkewFactorFromMidPoint(100.0);
        lowPass.control().setSkewFactorFromMidPoint(8000.0);
        bandFrequency.control().setSkewFactorFromMidPoint(1000.0);
        bandQ.control().setSkewFactorFromMidPoint(1.0);

        highPassAttachment = std::make_unique<SliderAttachment>(
            state, "eqHpfFrequency", highPass.control());
        lowPassAttachment = std::make_unique<SliderAttachment>(
            state, "eqLpfFrequency", lowPass.control());

        hpfBypassButton.onClick = [this]
        {
            const auto* value = parameterState.getRawParameterValue("eqHpfBypass");
            const auto bypassed = value != nullptr && value->load() >= 0.5f;
            setBoolParameter("eqHpfBypass", !bypassed);
        };

        lpfBypassButton.onClick = [this]
        {
            const auto* value = parameterState.getRawParameterValue("eqLpfBypass");
            const auto bypassed = value != nullptr && value->load() >= 0.5f;
            setBoolParameter("eqLpfBypass", !bypassed);
        };

        bandBypassButton.onClick = [this]
        {
            const auto prefix = juce::String("eqBand") + juce::String(selectedBand + 1);
            const auto* value = parameterState.getRawParameterValue(prefix + "Bypass");
            const auto bypassed = value != nullptr && value->load() >= 0.5f;
            setBoolParameter(prefix + "Bypass", !bypassed);
        };

        addAndMakeVisible(highPass);
        addAndMakeVisible(lowPass);
        addAndMakeVisible(hpfBypassButton);
        addAndMakeVisible(lpfBypassButton);
        addAndMakeVisible(bandFrequency);
        addAndMakeVisible(bandGain);
        addAndMakeVisible(bandQ);
        addAndMakeVisible(bandBypassButton);

        syncNodesFromParameters();
        selectBand(0);
        refreshBypassButtons();
        startTimerHz(12);
    }

    EqPanel::~EqPanel()
    {
        stopTimer();
    }

    void EqPanel::timerCallback()
    {
        if (draggingBand < 0)
            syncNodesFromParameters();

        refreshBypassButtons();
    }

    void EqPanel::syncNodesFromParameters()
    {
        for (int i = 0; i < 4; ++i)
        {
            const auto prefix = juce::String("eqBand") + juce::String(i + 1);
            const auto* frequency = parameterState.getRawParameterValue(prefix + "Frequency");
            const auto* gain = parameterState.getRawParameterValue(prefix + "Gain");

            if (frequency == nullptr || gain == nullptr)
                continue;

            auto& node = nodes[static_cast<std::size_t>(i)];
            node.x = frequencyToX(frequency->load(std::memory_order_relaxed));
            node.y = juce::jlimit(
                0.02f, 0.98f,
                0.5f - gain->load(std::memory_order_relaxed) / gainSpan);
        }

        repaint(graphBounds.toNearestInt().expanded(14));
    }

    juce::Point<float> EqPanel::nodeToPoint(const Node& node) const
    {
        return {
            graphBounds.getX() + node.x * graphBounds.getWidth(),
            graphBounds.getY() + node.y * graphBounds.getHeight()
        };
    }

    void EqPanel::setRealParameter(const juce::String& id, float realValue)
    {
        if (auto* parameter = parameterState.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(realValue));
    }

    void EqPanel::setBoolParameter(const juce::String& id, bool value)
    {
        if (auto* parameter = parameterState.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(value ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
    }

    void EqPanel::selectBand(int index)
    {
        if (!juce::isPositiveAndBelow(index, 4))
            return;

        selectedBand = index;
        updateSelectedBandAttachments();
        refreshBypassButtons();
        repaint();
    }

    void EqPanel::updateSelectedBandAttachments()
    {
        bandFrequencyAttachment.reset();
        bandGainAttachment.reset();
        bandQAttachment.reset();

        const auto prefix = juce::String("eqBand") + juce::String(selectedBand + 1);
        bandFrequencyAttachment = std::make_unique<SliderAttachment>(
            parameterState, prefix + "Frequency", bandFrequency.control());
        bandGainAttachment = std::make_unique<SliderAttachment>(
            parameterState, prefix + "Gain", bandGain.control());
        bandQAttachment = std::make_unique<SliderAttachment>(
            parameterState, prefix + "Q", bandQ.control());
    }

    void EqPanel::refreshBypassButtons()
    {
        const auto* hpf = parameterState.getRawParameterValue("eqHpfBypass");
        const auto* lpf = parameterState.getRawParameterValue("eqLpfBypass");
        const auto hpfBypassed = hpf != nullptr && hpf->load() >= 0.5f;
        const auto lpfBypassed = lpf != nullptr && lpf->load() >= 0.5f;

        hpfBypassButton.setText(hpfBypassed ? "HPF BYPASS" : "HPF ON");
        hpfBypassButton.setActive(hpfBypassed);

        lpfBypassButton.setText(lpfBypassed ? "LPF BYPASS" : "LPF ON");
        lpfBypassButton.setActive(lpfBypassed);

        const auto prefix = juce::String("eqBand") + juce::String(selectedBand + 1);
        const auto* band = parameterState.getRawParameterValue(prefix + "Bypass");
        const auto bandBypassed = band != nullptr && band->load() >= 0.5f;

        bandBypassButton.setText(
            "BAND " + juce::String(selectedBand + 1)
                + (bandBypassed ? " BYPASS" : " ON"));
        bandBypassButton.setActive(bandBypassed);
    }

    void EqPanel::beginBandGesture(int index)
    {
        if (!juce::isPositiveAndBelow(index, 4))
            return;

        const auto prefix = juce::String("eqBand") + juce::String(index + 1);
        if (auto* parameter = parameterState.getParameter(prefix + "Frequency"))
            parameter->beginChangeGesture();
        if (auto* parameter = parameterState.getParameter(prefix + "Gain"))
            parameter->beginChangeGesture();
    }

    void EqPanel::endBandGesture(int index)
    {
        if (!juce::isPositiveAndBelow(index, 4))
            return;

        const auto prefix = juce::String("eqBand") + juce::String(index + 1);
        if (auto* parameter = parameterState.getParameter(prefix + "Frequency"))
            parameter->endChangeGesture();
        if (auto* parameter = parameterState.getParameter(prefix + "Gain"))
            parameter->endChangeGesture();
    }

    void EqPanel::paint(juce::Graphics& g)
    {
        auto outer = getLocalBounds().toFloat().reduced(22.0f, 16.0f);

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("PARAMETRIC EQ",
                   outer.removeFromTop(22.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, outer, 14.0f);

        g.setColour(juce::Colour::fromRGB(10, 11, 12));
        g.fillRoundedRectangle(graphBounds, 10.0f);
        g.setColour(Theme::border().withAlpha(0.65f));
        g.drawRoundedRectangle(graphBounds.reduced(0.5f), 10.0f, 1.0f);

        g.setColour(Theme::grid().withAlpha(0.58f));
        for (int i = 1; i < 10; ++i)
        {
            const auto x = graphBounds.getX()
                         + graphBounds.getWidth() * static_cast<float>(i) / 10.0f;
            g.drawVerticalLine(static_cast<int>(x),
                               graphBounds.getY(), graphBounds.getBottom());
        }

        for (int i = 1; i < 8; ++i)
        {
            const auto y = graphBounds.getY()
                         + graphBounds.getHeight() * static_cast<float>(i) / 8.0f;
            g.drawHorizontalLine(static_cast<int>(y),
                                 graphBounds.getX(), graphBounds.getRight());
        }

        const auto zeroY = graphBounds.getCentreY();
        g.setColour(Theme::silverMid().withAlpha(0.38f));
        g.drawHorizontalLine(static_cast<int>(zeroY),
                             graphBounds.getX(), graphBounds.getRight());

        const auto* hpfFreq = parameterState.getRawParameterValue("eqHpfFrequency");
        const auto* hpfBypass = parameterState.getRawParameterValue("eqHpfBypass");
        const auto* lpfFreq = parameterState.getRawParameterValue("eqLpfFrequency");
        const auto* lpfBypass = parameterState.getRawParameterValue("eqLpfBypass");

        if (hpfFreq != nullptr)
        {
            const auto x = graphBounds.getX()
                         + frequencyToX(hpfFreq->load()) * graphBounds.getWidth();
            const auto bypassed = hpfBypass != nullptr && hpfBypass->load() >= 0.5f;
            g.setColour(Theme::silverLight().withAlpha(bypassed ? 0.18f : 0.48f));
            g.drawVerticalLine(static_cast<int>(x),
                               graphBounds.getY(), graphBounds.getBottom());
        }

        if (lpfFreq != nullptr)
        {
            const auto x = graphBounds.getX()
                         + frequencyToX(lpfFreq->load()) * graphBounds.getWidth();
            const auto bypassed = lpfBypass != nullptr && lpfBypass->load() >= 0.5f;
            g.setColour(Theme::silverLight().withAlpha(bypassed ? 0.18f : 0.48f));
            g.drawVerticalLine(static_cast<int>(x),
                               graphBounds.getY(), graphBounds.getBottom());
        }

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const auto point = nodeToPoint(nodes[i]);
            const auto selected = static_cast<int>(i) == selectedBand;
            const auto prefix = juce::String("eqBand") + juce::String(static_cast<int>(i) + 1);
            const auto* bypassValue = parameterState.getRawParameterValue(prefix + "Bypass");
            const auto bypassed = bypassValue != nullptr && bypassValue->load() >= 0.5f;

            g.setColour((selected ? Theme::amber() : Theme::silverMid())
                            .withAlpha(bypassed ? 0.24f : 0.42f));
            g.drawLine(point.x, zeroY, point.x, point.y, selected ? 1.5f : 1.0f);

            g.setColour(juce::Colours::black.withAlpha(0.72f));
            g.fillEllipse(point.x - 9.0f, point.y - 9.0f, 18.0f, 18.0f);

            g.setColour((selected ? Theme::amber() : Theme::silverLight())
                            .withAlpha(bypassed ? 0.38f : 1.0f));
            g.drawEllipse(point.x - 8.0f, point.y - 8.0f,
                          16.0f, 16.0f, selected ? 2.2f : 1.4f);

            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.drawText(juce::String(static_cast<int>(i) + 1),
                       juce::Rectangle<int>(
                           static_cast<int>(point.x - 7.0f),
                           static_cast<int>(point.y - 7.0f), 14, 14),
                       juce::Justification::centred, false);
        }

        const auto prefix = juce::String("eqBand") + juce::String(selectedBand + 1);
        const auto* frequency = parameterState.getRawParameterValue(prefix + "Frequency");
        const auto* gain = parameterState.getRawParameterValue(prefix + "Gain");
        const auto* q = parameterState.getRawParameterValue(prefix + "Q");

        if (frequency != nullptr && gain != nullptr && q != nullptr)
        {
            const auto hz = frequency->load();
            const juce::String frequencyText = hz >= 1000.0f
                ? juce::String(hz / 1000.0f, 2) + " kHz"
                : juce::String(hz, 0) + " Hz";

            const auto info = "BAND " + juce::String(selectedBand + 1)
                            + "   " + frequencyText
                            + "   " + juce::String(gain->load(), 1) + " dB"
                            + "   Q " + juce::String(q->load(), 2);

            g.setColour(Theme::text());
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(info,
                       graphBounds.withTrimmedTop(8.0f).withHeight(20.0f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("20 Hz",
                   juce::Rectangle<int>(
                       static_cast<int>(graphBounds.getX()),
                       static_cast<int>(graphBounds.getBottom()) + 3, 60, 16),
                   juce::Justification::centredLeft, false);
        g.drawText("22 kHz",
                   juce::Rectangle<int>(
                       static_cast<int>(graphBounds.getRight()) - 60,
                       static_cast<int>(graphBounds.getBottom()) + 3, 60, 16),
                   juce::Justification::centredRight, false);
    }

    void EqPanel::resized()
    {
        auto outer = getLocalBounds().reduced(22, 16);
        outer.removeFromTop(22);
        auto body = outer.reduced(16, 14);

        auto tools = body.removeFromTop(124);
        auto filters = tools.removeFromLeft(static_cast<int>(tools.getWidth() * 0.38f));

        auto hpfArea = filters.removeFromLeft(filters.getWidth() / 2).reduced(3);
        highPass.setBounds(hpfArea.removeFromTop(88));
        hpfBypassButton.setBounds(hpfArea.removeFromTop(30).reduced(4, 0));

        auto lpfArea = filters.reduced(3);
        lowPass.setBounds(lpfArea.removeFromTop(88));
        lpfBypassButton.setBounds(lpfArea.removeFromTop(30).reduced(4, 0));

        tools.removeFromLeft(10);
        auto bandTools = tools;
        const int buttonWidth = juce::jmin(112, bandTools.getWidth() / 4);
        bandBypassButton.setBounds(
            bandTools.removeFromRight(buttonWidth).withSizeKeepingCentre(buttonWidth, 32));
        bandTools.removeFromRight(6);

        const int knobWidth = bandTools.getWidth() / 3;
        bandFrequency.setBounds(bandTools.removeFromLeft(knobWidth).reduced(3, 0));
        bandGain.setBounds(bandTools.removeFromLeft(knobWidth).reduced(3, 0));
        bandQ.setBounds(bandTools.reduced(3, 0));

        body.removeFromTop(7);
        graphBounds = body.toFloat().reduced(6.0f, 7.0f).withTrimmedBottom(20.0f);
    }

    void EqPanel::mouseDown(const juce::MouseEvent& event)
    {
        if (!graphBounds.contains(event.position))
            return;

        float bestDistance = 24.0f;
        int best = -1;

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const auto distance = nodeToPoint(nodes[i]).getDistanceFrom(event.position);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = static_cast<int>(i);
            }
        }

        if (best < 0)
            return;

        selectBand(best);
        draggingBand = best;
        beginBandGesture(draggingBand);
        updateDraggedNode(event.position);
    }

    void EqPanel::updateDraggedNode(juce::Point<float> position)
    {
        if (draggingBand < 0
            || graphBounds.getWidth() <= 0.0f
            || graphBounds.getHeight() <= 0.0f)
            return;

        auto& node = nodes[static_cast<std::size_t>(draggingBand)];
        node.x = juce::jlimit(
            0.0f, 1.0f,
            (position.x - graphBounds.getX()) / graphBounds.getWidth());
        node.y = juce::jlimit(
            0.02f, 0.98f,
            (position.y - graphBounds.getY()) / graphBounds.getHeight());

        const auto prefix = juce::String("eqBand") + juce::String(draggingBand + 1);
        setRealParameter(prefix + "Frequency", xToFrequency(node.x));
        setRealParameter(prefix + "Gain", (0.5f - node.y) * gainSpan);

        repaint(graphBounds.toNearestInt().expanded(14));
    }

    void EqPanel::mouseDrag(const juce::MouseEvent& event)
    {
        updateDraggedNode(event.position);
    }

    void EqPanel::mouseUp(const juce::MouseEvent&)
    {
        if (draggingBand >= 0)
            endBandGesture(draggingBand);

        draggingBand = -1;
        syncNodesFromParameters();
    }
}
