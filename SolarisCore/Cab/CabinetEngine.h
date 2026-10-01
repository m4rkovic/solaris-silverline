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

    struct CabinetIRStatus
    {
        juce::String sourcePath;
        juce::String error;
        double sourceSampleRate = 0.0;
        juce::int64 lengthInSamples = 0;
        int channels = 0;
        bool active = false;
        bool missing = false;
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

        juce::Result loadImpulseResponseFromFile(CabinetIRSlot slot,
                                                 const juce::File& file,
                                                 bool trim = true,
                                                 bool normalise = true,
                                                 std::size_t expectedSize = 0);

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

        CabinetIRStatus getSlotStatus(CabinetIRSlot slot) const;
        int getLatencySamples() const noexcept;
        juce::ValueTree createState() const;
        void restoreState(const juce::ValueTree& state);

    private:
        struct Validation
        {
            juce::Result result = juce::Result::ok();
            double sampleRate = 0.0;
            juce::int64 lengthInSamples = 0;
            int channels = 0;
        };

        static Validation validateWavIR(const juce::File& file);
        juce::dsp::Convolution& convolverFor(CabinetIRSlot slot) noexcept;
        std::atomic<bool>& activeFor(CabinetIRSlot slot) noexcept;
        std::atomic<bool>& stereoFor(CabinetIRSlot slot) noexcept;
        std::atomic<bool>& phaseFor(CabinetIRSlot slot) noexcept;
        CabinetIRStatus& statusFor(CabinetIRSlot slot) noexcept;
        const CabinetIRStatus& statusFor(CabinetIRSlot slot) const noexcept;

        juce::dsp::ConvolutionMessageQueue messageQueue { 8 };
        juce::dsp::Convolution convolutionA { messageQueue };
        juce::dsp::Convolution convolutionB { messageQueue };

        juce::AudioBuffer<float> workA;
        juce::AudioBuffer<float> workB;
        int maximumBlockSize = 0;
        int preparedChannels = 0;

        std::atomic<float> wetMix { 1.0f };
        std::atomic<float> micBlend { 0.5f };
        std::atomic<bool> phaseA { false };
        std::atomic<bool> phaseB { false };
        std::atomic<bool> slotAActive { false };
        std::atomic<bool> slotBActive { false };
        std::atomic<bool> slotAStereo { false };
        std::atomic<bool> slotBStereo { false };

        mutable juce::CriticalSection metadataLock;
        juce::String cabinetModelId;
        CabinetIRStatus statusA;
        CabinetIRStatus statusB;
    };
}
