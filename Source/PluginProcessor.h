#pragma once

#include <JuceHeader.h>
#include "Parameters.h"
#include "../SolarisCore/Amp/AmpRegistry.h"
#include "../SolarisCore/Cab/CabinetEngine.h"
#include "../SolarisCore/DSP/IAudioStage.h"
#include "../SolarisCore/EQ/ParametricEQ.h"
#include "../SolarisCore/Tuner/TunerEngine.h"
#include <array>
#include <atomic>

class SolarisSilverlineAudioProcessor final : public juce::AudioProcessor
{
public:
    SolarisSilverlineAudioProcessor();
    ~SolarisSilverlineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    solaris::CabinetEngine& getCabinetEngine() noexcept { return cabinetEngine; }
    solaris::TunerSnapshot getTunerSnapshot() const noexcept { return tunerEngine.getSnapshot(); }

    void setTunerMuted(bool shouldMute) noexcept { tunerMuted.store(shouldMute, std::memory_order_relaxed); }
    bool isTunerMuted() const noexcept { return tunerMuted.load(std::memory_order_relaxed); }

    float getInputPeakDb() const noexcept { return inputPeakDb.load(std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outputPeakDb.load(std::memory_order_relaxed); }
    bool consumeInputClip() noexcept { return inputClip.exchange(false, std::memory_order_relaxed); }
    bool consumeOutputClip() noexcept { return outputClip.exchange(false, std::memory_order_relaxed); }

    bool isNeuralAudioAvailable() const noexcept
    {
       #if SOLARIS_ENABLE_NEURAL_AUDIO
        return true;
       #else
        return false;
       #endif
    }

    // Explicit model changes run off the audio callback. Model construction/loading
    // happens first; only the final unique_ptr swap is protected by JUCE's callback lock.
    bool loadNeuralAmpModel(const juce::File& modelFile);
    bool useAnalogueAmp();
    juce::String getActiveAmpModelId() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    solaris::AmpParameters readAmpParameters() const noexcept;
    void syncCabParameters() noexcept;
    void syncPostEqParameters() noexcept;
    void cacheParameterPointers();

    struct EqBandPointers
    {
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
    };

    juce::AudioProcessorValueTreeState parameters;
    solaris::AmpRegistry ampRegistry;
    solaris::CabinetEngine cabinetEngine;
    solaris::ParametricEQ postEq;
    solaris::TunerEngine tunerEngine;

    juce::dsp::Gain<float> inputGainStage;
    solaris::BypassAudioStage preFxStage;
    solaris::BypassAudioStage postFxStage;
    juce::dsp::Gain<float> outputGainStage;

    std::atomic<float>* inputGainParameter = nullptr;
    std::atomic<float>* outputGainParameter = nullptr;
    std::atomic<float>* ampEnabledParameter = nullptr;
    std::atomic<float>* ampChannelParameter = nullptr;
    std::atomic<float>* ampVolumeParameter = nullptr;
    std::atomic<float>* ampBassParameter = nullptr;
    std::atomic<float>* ampTrebleParameter = nullptr;
    std::atomic<float>* ampReverbParameter = nullptr;
    std::atomic<float>* ampTremoloSpeedParameter = nullptr;
    std::atomic<float>* ampTremoloIntensityParameter = nullptr;

    std::atomic<float>* cabWetParameter = nullptr;
    std::atomic<float>* cabMicBlendParameter = nullptr;
    std::atomic<float>* cabPhaseBParameter = nullptr;

    std::atomic<float>* eqHpfBypassParameter = nullptr;
    std::atomic<float>* eqHpfFrequencyParameter = nullptr;
    std::atomic<float>* eqLpfBypassParameter = nullptr;
    std::atomic<float>* eqLpfFrequencyParameter = nullptr;
    std::array<EqBandPointers, solaris::ParametricEQ::numBands> eqBandParameters {};

    solaris::AmpPrepareSpec lastAmpSpec {};
    std::atomic<bool> tunerMuted { false };
    std::atomic<float> inputPeakDb { -72.0f };
    std::atomic<float> outputPeakDb { -72.0f };
    std::atomic<bool> inputClip { false };
    std::atomic<bool> outputClip { false };
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessor)
};
