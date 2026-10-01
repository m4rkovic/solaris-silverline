#pragma once

#include <JuceHeader.h>
#include "../SolarisCore/Amp/AmpRegistry.h"
#include "../SolarisCore/Cab/CabinetEngine.h"
#include "../SolarisCore/EQ/ParametricEQ.h"
#include "../SolarisCore/Tuner/TunerEngine.h"

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
    double getTailLengthSeconds() const override { return 0.0; }
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncPostEqParameters() noexcept;

    juce::AudioProcessorValueTreeState parameters;
    solaris::AmpRegistry ampRegistry;
    solaris::CabinetEngine cabinetEngine;
    solaris::ParametricEQ postEq;
    solaris::TunerEngine tunerEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessor)
};
