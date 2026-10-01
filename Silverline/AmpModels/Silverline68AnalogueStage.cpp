#include "Silverline68AnalogueStage.h"
#include <algorithm>
#include <cmath>

namespace solaris
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;

        float lerp(float a, float b, float amount) noexcept
        {
            return a + (b - a) * amount;
        }
    }

    void Silverline68AnalogueStage::OnePoleLowpass::prepare(double sampleRate,
                                                             float cutoffHz) noexcept
    {
        const auto safeRate = std::max(1.0, sampleRate);
        const auto safeCutoff = std::clamp(cutoffHz, 1.0f,
                                           static_cast<float>(safeRate * 0.45));
        coefficient = 1.0f - std::exp(-2.0f * pi * safeCutoff
                                      / static_cast<float>(safeRate));
    }

    float Silverline68AnalogueStage::OnePoleLowpass::process(std::size_t channel,
                                                              float input) noexcept
    {
        auto& z = state[channel];
        z += coefficient * (input - z);
        return z;
    }

    void Silverline68AnalogueStage::OnePoleLowpass::reset() noexcept
    {
        state.fill(0.0f);
    }

    void Silverline68AnalogueStage::prepare(const juce::dsp::ProcessSpec& spec)
    {
        baseSampleRate = std::max(1.0, spec.sampleRate);
        activeChannels = std::clamp<std::size_t>(static_cast<std::size_t>(spec.numChannels),
                                                 1u, maxChannels);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            activeChannels,
            oversamplingStages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            false);
        oversampling->initProcessing(static_cast<std::size_t>(spec.maximumBlockSize));

        const auto factor = static_cast<double>(oversampling->getOversamplingFactor());
        oversampledRate = baseSampleRate * factor;

        bassBand.prepare(oversampledRate, 220.0f);
        trebleBand.prepare(oversampledRate, 2600.0f);
        lowTightener.prepare(oversampledRate, 118.0f);
        antiFizz.prepare(oversampledRate, 13800.0f);
        antiFizz2.prepare(oversampledRate, 18500.0f);
        dcTracker.prepare(oversampledRate, 7.0f);

        constexpr double smoothingSeconds = 0.025;
        volume.reset(oversampledRate, smoothingSeconds);
        bass.reset(oversampledRate, smoothingSeconds);
        treble.reset(oversampledRate, smoothingSeconds);
        channelBlend.reset(oversampledRate, smoothingSeconds);
        mid.reset(oversampledRate, smoothingSeconds);
        presence.reset(oversampledRate, smoothingSeconds);
        master.reset(oversampledRate, smoothingSeconds);

        volume.setCurrentAndTargetValue(0.45f);
        bass.setCurrentAndTargetValue(0.5f);
        treble.setCurrentAndTargetValue(0.55f);
        channelBlend.setCurrentAndTargetValue(0.0f);
        mid.setCurrentAndTargetValue(0.5f);
        presence.setCurrentAndTargetValue(0.5f);
        master.setCurrentAndTargetValue(1.0f);

        reset();
    }

    void Silverline68AnalogueStage::setParameters(const AmpParameters& parameters) noexcept
    {
        volume.setTargetValue(parameters.volume);
        bass.setTargetValue(parameters.bass);
        treble.setTargetValue(parameters.treble);
        channelBlend.setTargetValue(parameters.channel == AmpChannel::vintage ? 1.0f : 0.0f);
        mid.setTargetValue(parameters.mid);
        presence.setTargetValue(parameters.presence);
        master.setTargetValue(parameters.master);
    }

    void Silverline68AnalogueStage::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (oversampling == nullptr || buffer.getNumSamples() == 0)
            return;

        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);
        const auto channels = std::min<std::size_t>(oversampledBlock.getNumChannels(), activeChannels);
        const auto samples = oversampledBlock.getNumSamples();

        for (std::size_t sample = 0; sample < samples; ++sample)
        {
            const auto volumeValue = volume.getNextValue();
            const auto bassValue = bass.getNextValue();
            const auto trebleValue = treble.getNextValue();
            const auto vintage = channelBlend.getNextValue();
            const auto midValue = mid.getNextValue();
            const auto presenceValue = presence.getNextValue();
            const auto masterValue = master.getNextValue();

            // Custom is a little tighter and more eager to break up; Vintage retains
            // more headroom and a softer top. These are musical voicings, not a circuit clone.
            const auto driveDb = lerp(-2.0f, 22.5f, volumeValue)
                               + lerp(1.6f, -0.8f, vintage);
            const auto drive = juce::Decibels::decibelsToGain(driveDb);
            const auto bias = lerp(0.085f, 0.045f, vintage);
            const auto sagAmount = lerp(0.20f, 0.13f, vintage) * (0.35f + 0.65f * volumeValue);

            for (std::size_t channel = 0; channel < channels; ++channel)
            {
                auto x = oversampledBlock.getSample(channel, sample);

                // A small amount of bass is withheld before the nonlinear stage, increasingly
                // at higher Volume settings. This keeps palm-muted lows firm without making the
                // final tone stack thin.
                const auto lowForDrive = lowTightener.process(channel, x);
                const auto tightening = lerp(0.15f, 0.07f, vintage)
                                      * (0.30f + 0.70f * volumeValue);
                const auto driveInput = x - lowForDrive * tightening;

                // First triode-like stage: asymmetric soft clipping. A modest linear contribution
                // keeps pick attack and guitar-volume cleanup from collapsing into a flat waveform.
                const auto driven = driveInput * drive;
                const auto clipped = std::tanh(driven + bias) - std::tanh(bias);
                const auto cleanBlend = lerp(0.15f, 0.045f, volumeValue);
                auto preamp = clipped * (1.0f - cleanBlend)
                            + driveInput * std::sqrt(drive) * cleanBlend;

                // Remove the tiny DC component introduced by asymmetric clipping.
                const auto dc = dcTracker.process(channel, preamp);
                preamp -= dc;

                // Broad passive-stack approximation. It preserves pick transients while
                // allowing bass and treble to interact before the secondary saturation.
                const auto low = bassBand.process(channel, preamp);
                const auto trebleLow = trebleBand.process(channel, preamp);
                const auto high = preamp - trebleLow;
                const auto middle = preamp - low - high;

                const auto bassGain = 1.0f + (bassValue - 0.5f) * 1.35f;
                const auto trebleGain = 1.0f + (trebleValue - 0.5f) * 1.55f;
                const auto midGain = 1.0f + (midValue - 0.5f) * 0.65f;
                const auto presenceGain = 1.0f + (presenceValue - 0.5f) * 0.35f;

                auto shaped = low * bassGain
                            + middle * midGain
                            + high * trebleGain * presenceGain;

                // Simple supply-sag envelope before a gentler power-stage-like limiter.
                auto& state = channelState[channel];
                const auto magnitude = std::abs(shaped);
                const auto envelopeCoeff = magnitude > state.sagEnvelope ? 0.018f : 0.0011f;
                state.sagEnvelope += envelopeCoeff * (magnitude - state.sagEnvelope);
                const auto sagGain = 1.0f / (1.0f + sagAmount * state.sagEnvelope);

                const auto powerDrive = lerp(0.95f, 1.58f, volumeValue);
                const auto powerInput = shaped * sagGain * powerDrive;
                const auto normaliser = std::atan(1.65f * powerDrive);
                auto powered = std::atan(powerInput * 1.65f)
                             / juce::jmax(0.25f, normaliser);

                // Two gentle poles tame generated ultrasonics without shaving off the glassy
                // upper guitar harmonics. The oversampler then handles the final decimation.
                powered = antiFizz.process(channel, powered);
                powered = antiFizz2.process(channel, powered);

                const auto levelComp = lerp(0.92f, 0.53f, volumeValue);
                oversampledBlock.setSample(channel, sample,
                                           powered * levelComp * lerp(0.75f, 1.0f, masterValue));
            }
        }

        oversampling->processSamplesDown(block);
    }

    void Silverline68AnalogueStage::reset() noexcept
    {
        if (oversampling != nullptr)
            oversampling->reset();

        bassBand.reset();
        trebleBand.reset();
        lowTightener.reset();
        antiFizz.reset();
        antiFizz2.reset();
        dcTracker.reset();
        channelState.fill(ChannelState{});
    }
}
