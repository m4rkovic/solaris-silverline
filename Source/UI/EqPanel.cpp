#include "EqPanel.h"
#include <cmath>

namespace solaris::ui
{
    EqPanel::EqPanel()
    {
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
    }

    juce::Point<float> EqPanel::nodeToPoint(const Node& node) const
    {
        return {
            graphBounds.getX() + node.x * graphBounds.getWidth(),
            graphBounds.getY() + node.y * graphBounds.getHeight()
        };
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

        const int verticalLines = 10;
        const int horizontalLines = 8;
        g.setColour(Theme::grid().withAlpha(0.58f));
        for (int i = 1; i < verticalLines; ++i)
        {
            const auto x = graphBounds.getX() + graphBounds.getWidth() * (float) i / verticalLines;
            g.drawVerticalLine((int) x, graphBounds.getY(), graphBounds.getBottom());
        }
        for (int i = 1; i < horizontalLines; ++i)
        {
            const auto y = graphBounds.getY() + graphBounds.getHeight() * (float) i / horizontalLines;
            g.drawHorizontalLine((int) y, graphBounds.getX(), graphBounds.getRight());
        }

        const auto zeroY = graphBounds.getCentreY();
        g.setColour(Theme::silverMid().withAlpha(0.34f));
        g.drawHorizontalLine((int) zeroY, graphBounds.getX(), graphBounds.getRight());

        if (analyserEnabled)
        {
            juce::Path spectrum;
            const int samples = 72;
            for (int i = 0; i < samples; ++i)
            {
                const auto t = (float) i / (float) (samples - 1);
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

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto p = nodeToPoint(nodes[i]);
            const bool selected = (int) i == activeNode;
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
            g.setColour(selected ? Theme::text() : Theme::amber());
            g.drawEllipse(p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f, selected ? 2.0f : 1.5f);
        }

        if (activeNode >= 0)
        {
            const auto& n = nodes[(size_t) activeNode];
            const auto frequency = 20.0 * std::pow(1000.0, (double) n.x);
            const auto gain = (0.5 - (double) n.y) * 24.0;
            const juce::String freqText = frequency >= 1000.0
                ? juce::String(frequency / 1000.0, 2) + " kHz"
                : juce::String(frequency, 0) + " Hz";
            const juce::String info = "BAND " + juce::String(activeNode + 1)
                                    + "   " + freqText
                                    + "   " + juce::String(gain, 1) + " dB"
                                    + "   Q 1.00";
            g.setColour(Theme::text());
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(info, graphBounds.withTrimmedTop(8.0f).withHeight(22.0f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        g.setColour(Theme::textMuted());
        g.setFont(juce::FontOptions(9.5f));
        g.drawText("20 Hz", juce::Rectangle<int>((int) graphBounds.getX(), (int) graphBounds.getBottom() + 3, 60, 18),
                   juce::Justification::centredLeft, false);
        g.drawText("20 kHz", juce::Rectangle<int>((int) graphBounds.getRight() - 60, (int) graphBounds.getBottom() + 3, 60, 18),
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

    void EqPanel::mouseDown(const juce::MouseEvent& e)
    {
        if (!graphBounds.contains(e.position))
            return;

        float bestDistance = 22.0f;
        int best = -1;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto distance = nodeToPoint(nodes[i]).getDistanceFrom(e.position);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = (int) i;
            }
        }

        activeNode = best;
        if (activeNode >= 0)
            repaint();
    }

    void EqPanel::updateDraggedNode(juce::Point<float> position)
    {
        if (activeNode < 0 || graphBounds.getWidth() <= 0.0f || graphBounds.getHeight() <= 0.0f)
            return;

        auto& n = nodes[(size_t) activeNode];
        n.x = juce::jlimit(0.02f, 0.98f,
                           (position.x - graphBounds.getX()) / graphBounds.getWidth());
        n.y = juce::jlimit(0.05f, 0.95f,
                           (position.y - graphBounds.getY()) / graphBounds.getHeight());
        repaint(graphBounds.toNearestInt().expanded(12));
    }

    void EqPanel::mouseDrag(const juce::MouseEvent& e)
    {
        updateDraggedNode(e.position);
    }

    void EqPanel::mouseUp(const juce::MouseEvent&)
    {
        repaint();
    }
}
