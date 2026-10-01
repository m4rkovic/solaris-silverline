#pragma once

#include "EffectDSP.h"
#include "IEffect.h"
#include "OversampledEffect.h"
#include <array>

namespace solaris
{
    class VintageCompressor final : public EffectBase
    {
    public:
        const char* id() const noexcept override { return "leveler"; }
        void setSustain(float value) noexcept { sustain.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setAttack(float value) noexcept { attack.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setLevel(float value) noexcept { level.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareEffect(const EffectPrepareSpec& spec) override;
        void resetEffect() noexcept override;
        void processEffect(juce::AudioBuffer<float>& buffer) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sustain, attack, level;
        effectdsp::OnePoleLowpass detectorTone;
        effectdsp::OnePoleHighpass outputHighpass;
        effectdsp::OnePoleLowpass outputLowpass;
        std::array<float, effectdsp::maxChannels> envelope {};
        std::array<float, effectdsp::maxChannels> gainState {};
    };

    class AsymmetricOverdrive final : public OversampledEffectBase
    {
    public:
        const char* id() const noexcept override { return "turbo-drive"; }
        void setDrive(float value) noexcept { drive.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setTone(float value) noexcept { tone.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setLevel(float value) noexcept { level.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareOversampledEffect(double sampleRate, std::size_t channels) override;
        void resetOversampledEffect() noexcept override;
        void processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> drive, tone, level;
        effectdsp::OnePoleLowpass clippingSplit, darkTone, brightTone;
        effectdsp::DcBlocker dcBlocker;
    };

    class FlexibleDistortion final : public OversampledEffectBase
    {
    public:
        const char* id() const noexcept override { return "badland-dist"; }
        void setGain(float value) noexcept { gain.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setContour(float value) noexcept { contour.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setLevel(float value) noexcept { level.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareOversampledEffect(double sampleRate, std::size_t channels) override;
        void resetOversampledEffect() noexcept override;
        void processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain, contour, level;
        effectdsp::OnePoleHighpass inputHighpass;
        effectdsp::OnePoleLowpass lowBand, midBand, antiFizz;
        effectdsp::DcBlocker dcBlocker;
    };

    class HardClipDistortion final : public OversampledEffectBase
    {
    public:
        const char* id() const noexcept override { return "vermin-drive"; }
        void setDistortion(float value) noexcept { distortion.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setFilter(float value) noexcept { filter.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setLevel(float value) noexcept { level.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareOversampledEffect(double sampleRate, std::size_t channels) override;
        void resetOversampledEffect() noexcept override;
        void processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> distortion, filter, level;
        effectdsp::OnePoleHighpass lowCut, upperDrive;
        effectdsp::OnePoleLowpass darkFilter, brightFilter;
        effectdsp::DcBlocker dcBlocker;
    };

    class SustainingFuzz final : public OversampledEffectBase
    {
    public:
        const char* id() const noexcept override { return "void-fuzz"; }
        void setSustain(float value) noexcept { sustain.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setTone(float value) noexcept { tone.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
        void setLevel(float value) noexcept { level.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }

    private:
        void prepareOversampledEffect(double sampleRate, std::size_t channels) override;
        void resetOversampledEffect() noexcept override;
        void processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept override;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sustain, tone, level;
        effectdsp::OnePoleHighpass inputHighpass;
        effectdsp::OnePoleLowpass stageOneBandwidth, stageTwoBandwidth, toneLow;
        effectdsp::OnePoleHighpass toneHigh;
        effectdsp::DcBlocker dcBlocker;
    };
}
