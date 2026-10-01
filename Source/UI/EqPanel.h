#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include "Controls.h"

namespace solaris::ui
{
    class EqPanel final : public juce::Component,
                          private juce::Timer
    {
    public:
        explicit EqPanel(juce::AudioProcessorValueTreeState& state);
        ~EqPanel() override;

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        struct Node
        {
            float x = 0.5f;
            float y = 0.5f;
        };

        void timerCallback() override;
        juce::Point<float> nodeToPoint(const Node&) const;
        void updateDraggedNode(juce::Point<float> position);
        void syncNodesFromParameters();
        void beginBandGesture(int index);
        void endBandGesture(int index);
        void setRealParameter(const juce::String& id, float realValue);

        juce::AudioProcessorValueTreeState& parameterState;
        std::array<Node, 4> nodes {};

        SolarisKnob highPass { "HPF", " Hz", 20.0, 1000.0, 1.0, 70.0, true };
        SolarisKnob lowPass { "LPF", " Hz", 1000.0, 22000.0, 10.0, 18000.0, true };
        SolarisButton analyserButton { "SPECTRUM", true };

        std::unique_ptr<SliderAttachment> highPassAttachment;
        std::unique_ptr<SliderAttachment> lowPassAttachment;

        juce::Rectangle<float> graphBounds;
        int activeNode = -1;
        bool analyserEnabled = true;
    };
}
