#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace solaris
{
    struct TunerSnapshot
    {
        bool valid=false;
        float frequencyHz=0.0f;
        float cents=0.0f;
        float confidence=0.0f;
        int midiNote=-1;
    };

    class TunerEngine final : private juce::Thread
    {
    public:
        TunerEngine();
        ~TunerEngine() override;
        void prepare(double hostSampleRate);
        void stop();
        void pushSamples(const juce::AudioBuffer<float>& buffer) noexcept;
        void setReferenceA4(float frequencyHz) noexcept;
        float getReferenceA4() const noexcept;
        TunerSnapshot getSnapshot() const noexcept;

    private:
        static constexpr int fifoCapacity=32768;
        static constexpr int analysisSize=4096;
        static constexpr int hopSize=1024;
        static constexpr int maxLagStorage=analysisSize/2;
        void run() override;
        void drainFifo();
        void consumeHostSamples(const float* samples,int count);
        void analyseFrame();
        void publish(const TunerSnapshot&) noexcept;
        void publishInvalid() noexcept;

        juce::AbstractFifo fifo{fifoCapacity};
        std::array<float,fifoCapacity> fifoBuffer{};
        std::array<float,analysisSize> history{};
        std::array<float,analysisSize> frame{};
        std::array<float,maxLagStorage+2> yin{};
        std::array<float,5> recentFrequencies{};
        int historyWrite=0, historyValid=0, freshAnalysisSamples=0;
        int frequencyHistoryCount=0, frequencyHistoryWrite=0, invalidFrames=0;
        int decimationFactor=1, decimationCount=0;
        float decimationAccumulator=0.0f;
        double analysisSampleRate=44100.0;
        std::atomic<float> referenceA4{440.0f};
        mutable std::atomic<std::uint32_t> resultSequence{0};
        std::atomic<bool> resultValid{false};
        std::atomic<float> resultFrequency{0.0f}, resultCents{0.0f}, resultConfidence{0.0f};
        std::atomic<int> resultMidiNote{-1};
    };
}
