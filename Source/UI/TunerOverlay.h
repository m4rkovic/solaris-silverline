#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Controls.h"
#include "../../SolarisCore/Tuner/TunerEngine.h"

namespace solaris::ui
{
    class TunerOverlay final : public juce::Component
    {
    public:
        TunerOverlay();

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;

        void setSnapshot(const solaris::TunerSnapshot& snapshot);
        void setMuted(bool shouldBeMuted);

        std::function<void()> onClose;
        std::function<void(bool)> onMuteChanged;

    private:
        juce::String noteNameForMidi(int midiNote) const;
        juce::String confidenceLabel(float confidence) const;

        juce::Rectangle<float> cardBounds;
        SolarisButton closeButton { "CLOSE", false };
        SolarisButton muteButton { "MUTE", true };

        solaris::TunerSnapshot displayedSnapshot {};
        bool muted = false;

        int candidateNote = -1;
        int candidateFrames = 0;
        int invalidFrames = 0;
    };
}
