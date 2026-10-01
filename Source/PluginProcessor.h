#pragma once

#include <JuceHeader.h>
#include "Parameters.h"
#include "../SolarisCore/Amp/AmpRegistry.h"
#include "../SolarisCore/DSP/IAudioStage.h"
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    solaris::AmpParameters readAmpParameters() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    solaris::AmpRegistry ampRegistry;

    juce::dsp::Gain<float> inputGainStage;
    solaris::BypassAudioStage preFxStage;
    solaris::BypassAudioStage cabStage;
    solaris::BypassAudioStage postFxStage;
    solaris::BypassAudioStage eqStage;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessor)
};
