#pragma once

#include <JuceHeader.h>
#include "Parameters.h"
#include "../SolarisCore/Amp/AmpRegistry.h"
#include "../SolarisCore/Amp/NeuralModelLoadService.h"
#include "../SolarisCore/Cab/CabinetEngine.h"
#include "../SolarisCore/DSP/IAudioStage.h"
#include "../SolarisCore/EQ/ParametricEQ.h"
#include "../SolarisCore/Presets/PresetManager.h"
#include "../SolarisCore/Effects/EffectChain.h"
#include "../SolarisCore/Effects/PreEffects.h"
#include "../SolarisCore/Effects/PostEffects.h"
#include "../SolarisCore/Tuner/TunerEngine.h"
#include <array>
#include <atomic>

class SolarisSilverlineAudioProcessor final : public juce::AudioProcessor
{
public:
    SolarisSilverlineAudioProcessor();
    ~SolarisSilverlineAudioProcessor() override;

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
    solaris::PresetManager& getPresetManager() noexcept { return presetManager; }
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

    // Non-blocking NAM API. Filesystem access/model construction happen on a worker thread.
    bool requestNeuralAmpModelLoad(const juce::File& modelFile, bool rememberRecent = true);
    bool loadNeuralAmpModel(const juce::File& modelFile) { return requestNeuralAmpModelLoad(modelFile); }
    bool useAnalogueAmp();
    juce::String getActiveAmpModelId() const;
    solaris::NeuralLoadStatus getNeuralModelStatus() const { return neuralModelLoader.getStatus(); }
    juce::StringArray getRecentNeuralModels() const;

    juce::ValueTree capturePresetState();
    void applyPresetState(const juce::ValueTree& state);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    solaris::AmpParameters readAmpParameters() const noexcept;
    void syncCabParameters() noexcept;
    void syncEffectParameters() noexcept;
    void syncPostEqParameters() noexcept;
    void routeGuitarInput(juce::AudioBuffer<float>& buffer) noexcept;
    void cacheParameterPointers();
    void scheduleDesiredNeuralModel();
    void completeNeuralModelLoad(std::unique_ptr<solaris::NeuralAmpModel> candidate,
                                 const solaris::NeuralLoadStatus& status);
    juce::ValueTree createAmpState() const;
    void restoreAmpStateMetadata(const juce::ValueTree& ampState);

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
    solaris::PresetManager presetManager;

    juce::dsp::Gain<float> inputGainStage;

    solaris::VintageCompressor preCompressor;
    solaris::AsymmetricOverdrive preOverdrive;
    solaris::FlexibleDistortion preDistortion;
    solaris::HardClipDistortion preHardClip;
    solaris::SustainingFuzz preFuzz;
    solaris::EffectChain preFxChain;

    solaris::MultiStagePhaser postPhaser;
    solaris::AnalogChorus postChorus;
    solaris::BiasTremolo postTremolo;
    solaris::AnalogDelay postDelay;
    solaris::SpringSpaceReverb postReverb;
    solaris::EffectChain postFxChain;

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

    struct PedalPointers
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* first = nullptr;
        std::atomic<float>* second = nullptr;
        std::atomic<float>* third = nullptr;
    };

    std::array<PedalPointers, 5> prePedalParameters {};
    std::array<PedalPointers, 5> postPedalParameters {};

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
    int selectedInputChannel = 0;
    bool prepared = false;

    mutable juce::CriticalSection ampStateLock;
    juce::String desiredAmpBackend { "silverline68" };
    juce::String desiredNeuralPath;
    juce::StringArray recentNeuralModels;

    // Keep this last so shutdown happens before state used by completion callbacks is destroyed.
    solaris::NeuralModelLoadService neuralModelLoader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolarisSilverlineAudioProcessor)
};
