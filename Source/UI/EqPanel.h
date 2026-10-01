#pragma once

#include <JuceHeader.h>
#include <array>
#include "Controls.h"

namespace solaris::ui
{
    class EqPanel final : public juce::Component
    {
    public:
        EqPanel();

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        struct Node
        {
            float x = 0.5f;
            float y = 0.5f;
        };

        juce::Point<float> nodeToPoint(const Node&) const;
        void updateDraggedNode(juce::Point<float> position);

        std::array<Node, 4> nodes {{
            { 0.18f, 0.58f },
            { 0.38f, 0.43f },
            { 0.62f, 0.52f },
            { 0.82f, 0.36f }
        }};

        SolarisKnob highPass { "HPF", " Hz", 20.0, 400.0, 1.0, 70.0, true };
        SolarisKnob lowPass { "LPF", " kHz", 6.0, 22.0, 0.1, 18.0, true };
        SolarisButton analyserButton { "SPECTRUM", true };
        juce::Rectangle<float> graphBounds;
        int activeNode = -1;
        bool analyserEnabled = true;
    };
}
