#pragma once

#include "NeuralAmpModel.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>

namespace solaris
{
    enum class NeuralLoadState
    {
        idle,
        loading,
        loaded,
        error,
        missing
    };

    struct NeuralLoadStatus
    {
        NeuralLoadState state = NeuralLoadState::idle;
        juce::String requestedPath;
        juce::String error;
        NeuralModelMetadata metadata;
        std::uint64_t generation = 0;
    };

    class NeuralModelLoadService final : private juce::Thread
    {
    public:
        using Completion = std::function<void(std::unique_ptr<NeuralAmpModel>, const NeuralLoadStatus&)>;

        NeuralModelLoadService();
        ~NeuralModelLoadService() override;

        void request(const juce::File& modelFile,
                     const AmpPrepareSpec& prepareSpec,
                     const AmpParameters& parameters,
                     Completion completion);
        void cancelPending();
        void shutdown();
        NeuralLoadStatus getStatus() const;

    private:
        struct Request
        {
            juce::File file;
            AmpPrepareSpec prepareSpec;
            AmpParameters parameters;
            Completion completion;
            std::uint64_t generation = 0;
        };

        void run() override;
        void setStatus(NeuralLoadStatus next);

        mutable juce::CriticalSection lock;
        Request pending;
        bool hasPending = false;
        NeuralLoadStatus status;
        juce::WaitableEvent wakeup;
        std::atomic<std::uint64_t> latestGeneration { 0 };
    };
}
