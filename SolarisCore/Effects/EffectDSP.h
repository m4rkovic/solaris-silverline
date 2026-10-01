#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace solaris::effectdsp
{
    constexpr std::size_t maxChannels = 2;
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    inline float onePoleCoefficient(double sampleRate, float cutoffHz) noexcept
    {
        const auto sr = static_cast<float>(juce::jmax(1.0, sampleRate));
        const auto fc = juce::jlimit(1.0f, sr * 0.45f, cutoffHz);
        return 1.0f - std::exp(-2.0f * pi * fc / sr);
    }

    struct OnePoleLowpass
    {
        void prepare(double sampleRateToUse, float cutoffHz) noexcept
        {
            sampleRate = juce::jmax(1.0, sampleRateToUse);
            setCutoff(cutoffHz);
            reset();
        }

        void setCutoff(float cutoffHz) noexcept
        {
            coefficient = onePoleCoefficient(sampleRate, cutoffHz);
        }

        float process(std::size_t channel, float input) noexcept
        {
            auto& z = state[channel];
            z += coefficient * (input - z);
            return z;
        }

        void reset() noexcept { state.fill(0.0f); }

        double sampleRate = 44100.0;
        float coefficient = 0.0f;
        std::array<float, maxChannels> state {};
    };

    struct OnePoleHighpass
    {
        void prepare(double sampleRateToUse, float cutoffHz) noexcept
        {
            lowpass.prepare(sampleRateToUse, cutoffHz);
        }

        void setCutoff(float cutoffHz) noexcept { lowpass.setCutoff(cutoffHz); }

        float process(std::size_t channel, float input) noexcept
        {
            return input - lowpass.process(channel, input);
        }

        void reset() noexcept { lowpass.reset(); }

        OnePoleLowpass lowpass;
    };

    struct DcBlocker
    {
        void prepare(double sampleRate, float cutoffHz = 8.0f) noexcept
        {
            pole = std::exp(-2.0f * pi * cutoffHz
                            / static_cast<float>(juce::jmax(1.0, sampleRate)));
            reset();
        }

        float process(std::size_t channel, float input) noexcept
        {
            const auto output = input - x1[channel] + pole * y1[channel];
            x1[channel] = input;
            y1[channel] = output;
            return output;
        }

        void reset() noexcept
        {
            x1.fill(0.0f);
            y1.fill(0.0f);
        }

        float pole = 0.995f;
        std::array<float, maxChannels> x1 {};
        std::array<float, maxChannels> y1 {};
    };

    inline float smoothDiode(float input,
                             float positiveThreshold,
                             float negativeThreshold,
                             float softness) noexcept
    {
        if (input >= 0.0f)
        {
            if (input <= positiveThreshold)
                return input;
            const auto excess = input - positiveThreshold;
            return positiveThreshold + softness * (1.0f - std::exp(-excess / softness));
        }

        const auto magnitude = -input;
        if (magnitude <= negativeThreshold)
            return input;

        const auto excess = magnitude - negativeThreshold;
        return -(negativeThreshold + softness * (1.0f - std::exp(-excess / softness)));
    }

    inline float sharpDiodeClamp(float input, float threshold, float knee = 0.035f) noexcept
    {
        const auto magnitude = std::abs(input);
        if (magnitude <= threshold)
            return input;

        const auto clipped = threshold + knee * (1.0f - std::exp(-(magnitude - threshold) / knee));
        return std::copysign(clipped, input);
    }

    inline float rationalSaturator(float input) noexcept
    {
        return input / (1.0f + std::abs(input));
    }

    inline float cubicSoftClip(float input) noexcept
    {
        const auto x = juce::jlimit(-1.5f, 1.5f, input);
        if (x <= -1.0f)
            return -2.0f / 3.0f;
        if (x >= 1.0f)
            return 2.0f / 3.0f;
        return x - x * x * x / 3.0f;
    }

    class CubicDelayLine
    {
    public:
        void prepare(double sampleRateToUse, float maximumDelaySeconds, std::size_t channels)
        {
            sampleRate = juce::jmax(1.0, sampleRateToUse);
            activeChannels = juce::jlimit<std::size_t>(1u, maxChannels, channels);
            size = juce::jmax<std::size_t>(
                8u,
                static_cast<std::size_t>(std::ceil(sampleRate * maximumDelaySeconds)) + 8u);

            for (auto& channel : buffer)
                channel.assign(size, 0.0f);

            writeIndex = 0;
        }

        void reset() noexcept
        {
            for (auto& channel : buffer)
                std::fill(channel.begin(), channel.end(), 0.0f);
            writeIndex = 0;
        }

        void write(std::size_t channel, float value) noexcept
        {
            buffer[channel][writeIndex] = value;
        }

        float read(std::size_t channel, float delaySamples) const noexcept
        {
            if (size < 8)
                return 0.0f;

            const auto safeDelay = juce::jlimit(2.0f,
                static_cast<float>(size - 4u), delaySamples);

            auto readPosition = static_cast<float>(writeIndex) - safeDelay;
            while (readPosition < 0.0f)
                readPosition += static_cast<float>(size);

            const auto index1 = static_cast<std::size_t>(std::floor(readPosition)) % size;
            const auto frac = readPosition - std::floor(readPosition);
            const auto index0 = (index1 + size - 1u) % size;
            const auto index2 = (index1 + 1u) % size;
            const auto index3 = (index1 + 2u) % size;

            const auto a = buffer[channel][index0];
            const auto b = buffer[channel][index1];
            const auto c = buffer[channel][index2];
            const auto d = buffer[channel][index3];

            const auto cb = c - b;
            const auto k1 = 0.5f * (c - a);
            const auto k3 = k1 + 0.5f * (d - b) - 2.0f * cb;
            const auto k2 = cb - k1 - k3;
            return b + frac * (k1 + frac * (k2 + frac * k3));
        }

        void advance() noexcept
        {
            writeIndex = (writeIndex + 1u) % size;
        }

        double getSampleRate() const noexcept { return sampleRate; }

    private:
        std::array<std::vector<float>, maxChannels> buffer;
        std::size_t writeIndex = 0;
        std::size_t size = 8;
        std::size_t activeChannels = 2;
        double sampleRate = 44100.0;
    };
}
