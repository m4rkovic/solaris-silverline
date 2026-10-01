#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cstddef>

namespace solaris
{
    enum class CabinetIRSlot { micA, micB };

    struct CabinetModel
    {
        juce::String id;
        juce::String displayName;
    };

    class CabinetEngine
    {
    public:
        CabinetEngine();
        ~CabinetEngine() = default;

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void process(juce::AudioBuffer<float>& buffer) noexcept;

        void setCabinetModelId(juce::String modelId);
        juce::String getCabinetModelId() const;
        void setWetMix(float wet) noexcept;
        void setMicBlend(float micBAmount) noexcept;
        void setPhaseInverted(CabinetIRSlot slot, bool shouldInvert) noexcept;
        void setSlotActive(CabinetIRSlot slot, bool active) noexcept;

        void requestImpulseResponseFromFile(CabinetIRSlot slot,
                                            const juce::File& file,
                                            bool stereo,
                                            bool trim = true,
                                            bool normalise = true,
                                            std::size_t expectedSize = 0);

        void requestImpulseResponseBuffer(CabinetIRSlot slot,
                                          juce::AudioBuffer<float>&& buffer,
                                          double sourceSampleRate,
                                          bool stereo,
                                          bool trim = true,
                                          bool normalise = true);

        int getLatencySamples() const noexcept;
        juce::ValueTree createState() const;
        void restoreState(const juce::ValueTree& state);

    private:
        enum class PendingLoadType { none, file, buffer };

        struct PendingLoad
        {
            juce::SpinLock lock;
            PendingLoadType type = PendingLoadType::none;
            juce::File file;
            juce::AudioBuffer<float> buffer;
            double sourceSampleRate = 44100.0;
            bool stereo = false;
            bool trim = true;
            bool normalise = true;
            std::size_t expectedSize = 0;
        };

        juce::dsp::Convolution& convolverFor(CabinetIRSlot slot) noexcept;
        PendingLoad& pendingFor(CabinetIRSlot slot) noexcept;
        void applyPendingLoad(CabinetIRSlot slot) noexcept;

        juce::dsp::ConvolutionMessageQueue messageQueue { 8 };
        juce::dsp::Convolution convolutionA { messageQueue };
        juce::dsp::Convolution convolutionB { messageQueue };
        PendingLoad pendingA;
        PendingLoad pendingB;

        juce::AudioBuffer<float> workA;
        juce::AudioBuffer<float> workB;
        int maximumBlockSize = 0;
        int preparedChannels = 0;

        std::atomic<float> wetMix { 1.0f };
        std::atomic<float> micBlend { 0.0f };
        std::atomic<bool> phaseA { false };
        std::atomic<bool> phaseB { false };
        std::atomic<bool> slotAActive { false };
        std::atomic<bool> slotBActive { false };
        std::atomic<bool> slotAStereo { false };
        std::atomic<bool> slotBStereo { false };

        mutable juce::CriticalSection metadataLock;
        juce::String cabinetModelId;
        juce::String sourcePathA;
        juce::String sourcePathB;
    };
}
