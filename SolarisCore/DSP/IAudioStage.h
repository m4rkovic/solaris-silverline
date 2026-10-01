#pragma once

#include <JuceHeader.h>

namespace solaris
{
    class IAudioStage
    {
    public:
        virtual ~IAudioStage() = default;
        virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
        virtual void process(juce::AudioBuffer<float>& buffer) noexcept = 0;
        virtual void reset() noexcept = 0;
    };

    // Deliberately transparent stage used to lock down the shell signal order before
    // PRE FX, CAB, POST FX and EQ implementations arrive.
    class BypassAudioStage final : public IAudioStage
    {
    public:
        void prepare(const juce::dsp::ProcessSpec&) override {}
        void process(juce::AudioBuffer<float>&) noexcept override {}
        void reset() noexcept override {}
    };
}
