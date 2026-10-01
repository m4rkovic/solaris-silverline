#include "PreEffects.h"
#include <cmath>

namespace solaris
{
    namespace
    {
        void prepareSmoother(juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& smoother,
                             double sampleRate,
                             float initialValue,
                             double seconds = 0.025)
        {
            smoother.reset(sampleRate, seconds);
            smoother.setCurrentAndTargetValue(initialValue);
        }

        float levelGain(float normalized) noexcept
        {
            return juce::Decibels::decibelsToGain(-12.0f + 24.0f * normalized);
        }
    }

    void VintageCompressor::prepareEffect(const EffectPrepareSpec& spec)
    {
        prepareSmoother(sustain, spec.sampleRate, 0.45f, 0.030);
        prepareSmoother(attack, spec.sampleRate, 0.35f, 0.030);
        prepareSmoother(level, spec.sampleRate, 0.5f, 0.020);

        detectorTone.prepare(spec.sampleRate, 3600.0f);
        outputHighpass.prepare(spec.sampleRate, 55.0f);
        outputLowpass.prepare(spec.sampleRate, 14500.0f);
        resetEffect();
    }

    void VintageCompressor::resetEffect() noexcept
    {
        detectorTone.reset();
        outputHighpass.reset();
        outputLowpass.reset();
        envelope.fill(0.0f);
        gainState.fill(1.0f);
    }

    void VintageCompressor::processEffect(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(
            static_cast<std::size_t>(buffer.getNumChannels()), effectdsp::maxChannels);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto sustainValue = sustain.getNextValue();
            const auto attackValue = attack.getNextValue();
            const auto levelValue = level.getNextValue();

            const auto thresholdDb = -18.0f - sustainValue * 18.0f;
            const auto ratio = 2.2f + sustainValue * 4.8f;
            const auto attackMs = 1.5f + attackValue * 26.0f;
            const auto releaseMs = 90.0f + sustainValue * 260.0f;
            const auto attackCoeff = std::exp(-1.0f / (0.001f * attackMs
                                              * static_cast<float>(preparedSampleRate)));
            const auto releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs
                                               * static_cast<float>(preparedSampleRate)));
            const auto makeupDb = 2.5f + sustainValue * 8.5f;
            const auto outputGain = levelGain(levelValue);

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto x = buffer.getSample(static_cast<int>(channel), sample);
                const auto dark = detectorTone.process(channel, x);
                const auto detectorInput = std::abs(x + 0.14f * (x - dark));

                auto& env = envelope[channel];
                const auto coeff = detectorInput > env ? attackCoeff : releaseCoeff;
                env = detectorInput + coeff * (env - detectorInput);

                const auto inputDb = juce::Decibels::gainToDecibels(
                    juce::jmax(env, 1.0e-6f), -120.0f);
                const auto overDb = juce::jmax(0.0f, inputDb - thresholdDb);
                const auto reductionDb = overDb * (1.0f - 1.0f / ratio);
                const auto targetGain = juce::Decibels::decibelsToGain(makeupDb - reductionDb);

                auto& smoothedGain = gainState[channel];
                const auto gainCoeff = targetGain < smoothedGain ? attackCoeff : releaseCoeff;
                smoothedGain = targetGain + gainCoeff * (smoothedGain - targetGain);

                auto y = x * smoothedGain;
                y = effectdsp::cubicSoftClip(y * 0.70f) * 1.43f;
                y = outputHighpass.process(channel, y);
                y = outputLowpass.process(channel, y);
                buffer.setSample(static_cast<int>(channel), sample, y * outputGain);
            }
        }
    }

    void AsymmetricOverdrive::prepareOversampledEffect(double sampleRate, std::size_t)
    {
        prepareSmoother(drive, sampleRate, 0.35f);
        prepareSmoother(tone, sampleRate, 0.5f);
        prepareSmoother(level, sampleRate, 0.5f, 0.020);
        clippingSplit.prepare(sampleRate, 720.0f);
        darkTone.prepare(sampleRate, 2100.0f);
        brightTone.prepare(sampleRate, 9800.0f);
        dcBlocker.prepare(sampleRate, 8.0f);
        resetOversampledEffect();
    }

    void AsymmetricOverdrive::resetOversampledEffect() noexcept
    {
        clippingSplit.reset();
        darkTone.reset();
        brightTone.reset();
        dcBlocker.reset();
    }

    void AsymmetricOverdrive::processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(block.getNumChannels(), effectdsp::maxChannels);

        for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto driveValue = drive.getNextValue();
            const auto toneValue = tone.getNextValue();
            const auto outputGain = levelGain(level.getNextValue());
            const auto gainValue = juce::Decibels::decibelsToGain(8.0f + driveValue * 31.0f);

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto x = block.getSample(channel, sample);
                const auto low = clippingSplit.process(channel, x);
                const auto high = x - low;
                const auto frequencySelective = x + high * (0.55f + 1.35f * driveValue);
                const auto clipped = effectdsp::smoothDiode(
                    frequencySelective * gainValue, 0.46f, 0.72f, 0.16f);

                auto y = 0.16f * x + 0.84f * clipped;
                const auto dark = darkTone.process(channel, y);
                const auto bright = brightTone.process(channel, y);
                y = dark + (bright - dark) * toneValue;
                y = dcBlocker.process(channel, y);
                block.setSample(channel, sample, y * outputGain * 1.18f);
            }
        }
    }

    void FlexibleDistortion::prepareOversampledEffect(double sampleRate, std::size_t)
    {
        prepareSmoother(gain, sampleRate, 0.45f);
        prepareSmoother(contour, sampleRate, 0.5f);
        prepareSmoother(level, sampleRate, 0.5f, 0.020);
        inputHighpass.prepare(sampleRate, 72.0f);
        lowBand.prepare(sampleRate, 240.0f);
        midBand.prepare(sampleRate, 2200.0f);
        antiFizz.prepare(sampleRate, 12500.0f);
        dcBlocker.prepare(sampleRate);
        resetOversampledEffect();
    }

    void FlexibleDistortion::resetOversampledEffect() noexcept
    {
        inputHighpass.reset();
        lowBand.reset();
        midBand.reset();
        antiFizz.reset();
        dcBlocker.reset();
    }

    void FlexibleDistortion::processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(block.getNumChannels(), effectdsp::maxChannels);

        for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto gainValue = gain.getNextValue();
            const auto contourValue = contour.getNextValue();
            const auto outputGain = levelGain(level.getNextValue());
            const auto preGain = juce::Decibels::decibelsToGain(10.0f + gainValue * 30.0f);
            const auto secondDrive = 1.4f + gainValue * 4.1f;
            const auto normaliser = juce::jmax(0.001f, std::atan(secondDrive));
            const auto bias = 0.045f + 0.055f * gainValue;

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto x = inputHighpass.process(channel, block.getSample(channel, sample));
                const auto stageOne = effectdsp::rationalSaturator(x * preGain) * 1.95f;
                auto stageTwo = std::atan((stageOne + bias) * secondDrive) / normaliser;
                stageTwo -= std::atan(bias * secondDrive) / normaliser;

                const auto low = lowBand.process(channel, stageTwo);
                const auto lowAndMid = midBand.process(channel, stageTwo);
                const auto mid = lowAndMid - low;
                const auto high = stageTwo - lowAndMid;

                auto y = low * (1.03f + 0.08f * (1.0f - contourValue))
                       + mid * (1.42f - contourValue * 0.88f)
                       + high * (0.72f + contourValue * 0.56f);
                y = antiFizz.process(channel, y);
                y = dcBlocker.process(channel, y);
                block.setSample(channel, sample, y * outputGain * 0.88f);
            }
        }
    }

    void HardClipDistortion::prepareOversampledEffect(double sampleRate, std::size_t)
    {
        prepareSmoother(distortion, sampleRate, 0.45f);
        prepareSmoother(filter, sampleRate, 0.45f);
        prepareSmoother(level, sampleRate, 0.5f, 0.020);
        lowCut.prepare(sampleRate, 62.0f);
        upperDrive.prepare(sampleRate, 1450.0f);
        darkFilter.prepare(sampleRate, 760.0f);
        brightFilter.prepare(sampleRate, 14500.0f);
        dcBlocker.prepare(sampleRate);
        resetOversampledEffect();
    }

    void HardClipDistortion::resetOversampledEffect() noexcept
    {
        lowCut.reset();
        upperDrive.reset();
        darkFilter.reset();
        brightFilter.reset();
        dcBlocker.reset();
    }

    void HardClipDistortion::processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(block.getNumChannels(), effectdsp::maxChannels);

        for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto distValue = distortion.getNextValue();
            const auto filterValue = filter.getNextValue();
            const auto outputGain = levelGain(level.getNextValue());
            const auto gainValue = juce::Decibels::decibelsToGain(13.0f + distValue * 38.0f);

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                const auto x = lowCut.process(channel, block.getSample(channel, sample));
                const auto upper = upperDrive.process(channel, x);
                auto y = effectdsp::sharpDiodeClamp((x + 0.31f * upper) * gainValue,
                                                    0.50f, 0.030f);
                const auto dark = darkFilter.process(channel, y);
                const auto bright = brightFilter.process(channel, y);
                y = bright + (dark - bright) * filterValue;
                y = dcBlocker.process(channel, y);
                block.setSample(channel, sample, y * outputGain * 1.34f);
            }
        }
    }

    void SustainingFuzz::prepareOversampledEffect(double sampleRate, std::size_t)
    {
        prepareSmoother(sustain, sampleRate, 0.55f);
        prepareSmoother(tone, sampleRate, 0.5f);
        prepareSmoother(level, sampleRate, 0.5f, 0.020);
        inputHighpass.prepare(sampleRate, 58.0f);
        stageOneBandwidth.prepare(sampleRate, 1850.0f);
        stageTwoBandwidth.prepare(sampleRate, 1280.0f);
        toneLow.prepare(sampleRate, 720.0f);
        toneHigh.prepare(sampleRate, 1180.0f);
        dcBlocker.prepare(sampleRate);
        resetOversampledEffect();
    }

    void SustainingFuzz::resetOversampledEffect() noexcept
    {
        inputHighpass.reset();
        stageOneBandwidth.reset();
        stageTwoBandwidth.reset();
        toneLow.reset();
        toneHigh.reset();
        dcBlocker.reset();
    }

    void SustainingFuzz::processOversampledEffect(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const auto channels = juce::jmin<std::size_t>(block.getNumChannels(), effectdsp::maxChannels);

        for (std::size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto sustainValue = sustain.getNextValue();
            const auto toneValue = tone.getNextValue();
            const auto outputGain = levelGain(level.getNextValue());
            const auto stageOneGain = juce::Decibels::decibelsToGain(16.0f + sustainValue * 22.0f);
            const auto stageTwoGain = juce::Decibels::decibelsToGain(18.0f + sustainValue * 24.0f);

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                auto x = inputHighpass.process(channel, block.getSample(channel, sample));
                auto first = effectdsp::smoothDiode(x * stageOneGain, 0.55f, 0.55f, 0.11f);
                first = stageOneBandwidth.process(channel, first);
                auto second = effectdsp::smoothDiode(first * stageTwoGain, 0.52f, 0.52f, 0.085f);
                second = stageTwoBandwidth.process(channel, second);

                const auto low = toneLow.process(channel, second);
                const auto high = toneHigh.process(channel, second);
                auto y = low * (1.0f - toneValue) + high * toneValue;
                y = dcBlocker.process(channel, y);
                block.setSample(channel, sample, y * outputGain * 1.72f);
            }
        }
    }
}
