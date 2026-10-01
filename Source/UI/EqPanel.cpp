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
        highPassAttachment = std::make_unique<SliderAttachment>(
            state, "eqHpfFrequency", highPass.control());
        lowPassAttachment = std::make_unique<SliderAttachment>(
            state, "eqLpfFrequency", lowPass.control());

        analyserButton.setActive(true);
        analyserButton.onClick = [this]
        {
            analyserEnabled = !analyserEnabled;
            analyserButton.setActive(analyserEnabled);
            repaint();
        };

        addAndMakeVisible(highPass);
        addAndMakeVisible(lowPass);
        addAndMakeVisible(analyserButton);

        syncNodesFromParameters();
        startTimerHz(15);
    }

    EqPanel::~EqPanel()
    {
        stopTimer();
    }

    void EqPanel::timerCallback()
    {
        if (activeNode < 0)
            syncNodesFromParameters();
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
            node.y = juce::jlimit(0.02f, 0.98f,
                                  0.5f - gain->load(std::memory_order_relaxed) / gainSpan);
        }

        repaint(graphBounds.toNearestInt().expanded(12));
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

    void EqPanel::beginBandGesture(int index)
    {
        if (!juce::isPositiveAndBelow(index, 4))
            return;

        const auto prefix = juce::String("eqBand") + juce::String(index + 1);
        if (auto* p = parameterState.getParameter(prefix + "Frequency"))
            p->beginChangeGesture();
        if (auto* p = parameterState.getParameter(prefix + "Gain"))
            p->beginChangeGesture();
    }

    void EqPanel::endBandGesture(int index)
    {
        if (!juce::isPositiveAndBelow(index, 4))
            return;

        const auto prefix = juce::String("eqBand") + juce::String(index + 1);
        if (auto* p = parameterState.getParameter(prefix + "Frequency"))
            p->endChangeGesture();
        if (auto* p = parameterState.getParameter(prefix + "Gain"))
            p->endChangeGesture();
    }

    void EqPanel::paint(juce::Graphics& g)
    {
        auto outer = getLocalBounds().toFloat().reduced(22.0f, 18.0f);
        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("PARAMETRIC EQ", outer.removeFromTop(24.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);

        Theme::fillPanel(g, outer, 14.0f);

        g.setColour(juce::Colour::fromRGB(10, 11, 12));
        g.fillRoundedRectangle(graphBounds, 10.0f);
        g.setColour(Theme::border().withAlpha(0.65f));
        g.drawRoundedRectangle(graphBounds.reduced(0.5f), 10.0f, 1.0f);

        g.setColour(Theme::grid().withAlpha(0.58f));
        for (int i = 1; i < 10; ++i)
        {
            const auto x = graphBounds.getX() + graphBounds.getWidth() * static_cast<float>(i) / 10.0f;
            g.drawVerticalLine(static_cast<int>(x), graphBounds.getY(), graphBounds.getBottom());
        }

        for (int i = 1; i < 8; ++i)
        {
            const auto y = graphBounds.getY() + graphBounds.getHeight() * static_cast<float>(i) / 8.0f;
            g.drawHorizontalLine(static_cast<int>(y), graphBounds.getX(), graphBounds.getRight());
        }

        const auto zeroY = graphBounds.getCentreY();
        g.setColour(Theme::silverMid().withAlpha(0.34f));
        g.drawHorizontalLine(static_cast<int>(zeroY), graphBounds.getX(), graphBounds.getRight());

        if (analyserEnabled)
        {
            juce::Path spectrum;
            constexpr int samples = 72;
            for (int i = 0; i < samples; ++i)
            {
                const auto t = static_cast<float>(i) / static_cast<float>(samples - 1);
                const auto shaped = 0.60f
                                  + std::sin(t * 21.0f) * 0.045f
                                  + std::sin(t * 47.0f + 1.3f) * 0.022f
                                  - t * 0.18f;
                const auto px = graphBounds.getX() + t * graphBounds.getWidth();
                const auto py = graphBounds.getY() + shaped * graphBounds.getHeight();
                if (i == 0)
                    spectrum.startNewSubPath(px, py);
                else
                    spectrum.lineTo(px, py);
            }

            g.setColour(Theme::silverMid().withAlpha(0.17f));
            g.strokePath(spectrum, juce::PathStrokeType(1.2f));
        }

        juce::Path curve;
        curve.startNewSubPath(graphBounds.getX(), zeroY);
        for (const auto& node : nodes)
            curve.lineTo(nodeToPoint(node));
        curve.lineTo(graphBounds.getRight(), zeroY);

        g.setColour(Theme::amber().withAlpha(0.88f));
        g.strokePath(curve, juce::PathStrokeType(2.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const auto p = nodeToPoint(nodes[i]);
            const bool selected = static_cast<int>(i) == activeNode;
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
            g.setColour(selected ? Theme::text() : Theme::amber());
            g.drawEllipse(p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f, selected ? 2.0f : 1.5f);
        }

        if (activeNode >= 0)
        {
            const auto& node = nodes[static_cast<std::size_t>(activeNode)];
            const auto frequency = xToFrequency(node.x);
            const auto gain = (0.5f - node.y) * gainSpan;
            const juce::String freqText = frequency >= 1000.0f
                ? juce::String(frequency / 1000.0f, 2) + " kHz"
                : juce::String(frequency, 0) + " Hz";

            const auto info = "BAND " + juce::String(activeNode + 1)
                            + "   " + freqText
                            + "   " + juce::String(gain, 1) + " dB";

            g.setColour(Theme::text());
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(info, graphBounds.withTrimmedTop(8.0f).withHeight(22.0f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f));
        g.drawText("20 Hz", juce::Rectangle<int>(static_cast<int>(graphBounds.getX()),
                                                  static_cast<int>(graphBounds.getBottom()) + 3,
                                                  60, 18),
                   juce::Justification::centredLeft, false);
        g.drawText("22 kHz", juce::Rectangle<int>(static_cast<int>(graphBounds.getRight()) - 60,
                                                   static_cast<int>(graphBounds.getBottom()) + 3,
                                                   60, 18),
                   juce::Justification::centredRight, false);
    }

    void EqPanel::resized()
    {
        auto outer = getLocalBounds().reduced(22, 18);
        outer.removeFromTop(24);
        auto body = outer.reduced(18, 16);
        auto tools = body.removeFromTop(108);

        highPass.setBounds(tools.removeFromLeft(132));
        tools.removeFromLeft(8);
        lowPass.setBounds(tools.removeFromLeft(132));
        analyserButton.setBounds(tools.removeFromRight(110).withSizeKeepingCentre(110, 34));

        body.removeFromTop(8);
        graphBounds = body.toFloat().reduced(6.0f, 8.0f).withTrimmedBottom(22.0f);
    }

    void EqPanel::mouseDown(const juce::MouseEvent& event)
    {
        if (!graphBounds.contains(event.position))
            return;

        float bestDistance = 22.0f;
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

        activeNode = best;
        if (activeNode >= 0)
        {
            beginBandGesture(activeNode);
            updateDraggedNode(event.position);
        }
    }

    void EqPanel::updateDraggedNode(juce::Point<float> position)
    {
        if (activeNode < 0 || graphBounds.getWidth() <= 0.0f || graphBounds.getHeight() <= 0.0f)
            return;

        auto& node = nodes[static_cast<std::size_t>(activeNode)];
        node.x = juce::jlimit(0.0f, 1.0f,
                             (position.x - graphBounds.getX()) / graphBounds.getWidth());
        node.y = juce::jlimit(0.02f, 0.98f,
                             (position.y - graphBounds.getY()) / graphBounds.getHeight());

        const auto prefix = juce::String("eqBand") + juce::String(activeNode + 1);
        setRealParameter(prefix + "Frequency", xToFrequency(node.x));
        setRealParameter(prefix + "Gain", (0.5f - node.y) * gainSpan);
        repaint(graphBounds.toNearestInt().expanded(12));
    }

    void EqPanel::mouseDrag(const juce::MouseEvent& event)
    {
        updateDraggedNode(event.position);
    }

    void EqPanel::mouseUp(const juce::MouseEvent&)
    {
        if (activeNode >= 0)
            endBandGesture(activeNode);

        activeNode = -1;
        syncNodesFromParameters();
    }
}
