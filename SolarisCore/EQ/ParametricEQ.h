#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace solaris
{
    class ParametricEQ
    {
    public:
        static constexpr int numBands = 4;
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        void process(juce::AudioBuffer<float>& buffer) noexcept;
        void setHighPass(float frequencyHz, bool bypassed) noexcept;
        void setLowPass(float frequencyHz, bool bypassed) noexcept;
        void setBand(int index, float frequencyHz, float gainDb, float q, bool bypassed) noexcept;
        juce::ValueTree createState() const;
        void restoreState(const juce::ValueTree& state);

    private:
        struct Coefficients { float b0=1, b1=0, b2=0, a1=0, a2=0; };
        class Biquad
        {
        public:
            void reset() noexcept;
            void setTarget(const Coefficients&, int rampSamples) noexcept;
            void setBypassed(bool) noexcept;
            void process(juce::AudioBuffer<float>&, int numChannels) noexcept;
        private:
            struct ChannelState { float z1=0, z2=0; };
            void advance() noexcept;
            Coefficients current, target, delta;
            std::array<ChannelState,2> states{};
            int samplesRemaining=0;
            bool bypassed=true;
        };
        struct AtomicBand
        {
            std::atomic<float> frequencyHz{1000.0f}, gainDb{0.0f}, q{0.707f};
            std::atomic<bool> bypassed{true};
        };
        static Coefficients normalise(float,float,float,float,float,float) noexcept;
        static Coefficients makeHighPass(double,float) noexcept;
        static Coefficients makeLowPass(double,float) noexcept;
        static Coefficients makePeak(double,float,float,float) noexcept;

        double sampleRate=44100.0;
        int coefficientRampSamples=64;
        std::atomic<float> highPassFrequency{70.0f};
        std::atomic<bool> highPassBypassed{true};
        std::atomic<float> lowPassFrequency{18000.0f};
        std::atomic<bool> lowPassBypassed{true};
        std::array<AtomicBand,numBands> bands;
        Biquad highPass, lowPass;
        std::array<Biquad,numBands> peakFilters;
    };
}
