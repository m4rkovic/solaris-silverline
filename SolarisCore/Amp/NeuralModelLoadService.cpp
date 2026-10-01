#include "NeuralModelLoadService.h"

namespace solaris
{
    NeuralModelLoadService::NeuralModelLoadService()
        : juce::Thread("Solaris NAM Loader")
    {
        startThread(juce::Thread::Priority::low);
    }

    NeuralModelLoadService::~NeuralModelLoadService()
    {
        shutdown();
    }

    void NeuralModelLoadService::shutdown()
    {
        latestGeneration.fetch_add(1, std::memory_order_acq_rel);
        signalThreadShouldExit();
        wakeup.signal();
        if (isThreadRunning())
            stopThread(5000);
    }

    void NeuralModelLoadService::request(const juce::File& modelFile,
                                         const AmpPrepareSpec& prepareSpec,
                                         const AmpParameters& parameters,
                                         Completion completion)
    {
        Request request;
        request.file = modelFile;
        request.prepareSpec = prepareSpec;
        request.parameters = parameters;
        request.completion = std::move(completion);
        request.generation = latestGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;

        NeuralLoadStatus next;
        next.state = NeuralLoadState::loading;
        next.requestedPath = modelFile.getFullPathName();
        next.generation = request.generation;

        {
            const juce::ScopedLock guard(lock);
            pending = std::move(request);
            hasPending = true;
            status = next;
        }

        wakeup.signal();
    }

    void NeuralModelLoadService::cancelPending()
    {
        latestGeneration.fetch_add(1, std::memory_order_acq_rel);
        const juce::ScopedLock guard(lock);
        hasPending = false;
        status.state = NeuralLoadState::idle;
        status.error.clear();
    }

    NeuralLoadStatus NeuralModelLoadService::getStatus() const
    {
        const juce::ScopedLock guard(lock);
        return status;
    }

    void NeuralModelLoadService::setStatus(NeuralLoadStatus next)
    {
        const juce::ScopedLock guard(lock);
        if (next.generation == latestGeneration.load(std::memory_order_acquire))
            status = std::move(next);
    }

    void NeuralModelLoadService::run()
    {
        while (!threadShouldExit())
        {
            wakeup.wait(250);
            if (threadShouldExit())
                break;

            Request request;
            {
                const juce::ScopedLock guard(lock);
                if (!hasPending)
                    continue;
                request = std::move(pending);
                hasPending = false;
            }

            NeuralLoadStatus result;
            result.requestedPath = request.file.getFullPathName();
            result.generation = request.generation;

            if (!request.file.existsAsFile())
            {
                result.state = NeuralLoadState::missing;
                result.error = "NAM file is missing: " + result.requestedPath;
                setStatus(result);
                if (request.generation == latestGeneration.load(std::memory_order_acquire) && request.completion)
                    request.completion(nullptr, result);
                continue;
            }

            auto candidate = std::make_unique<NeuralAmpModel>();
            candidate->prepare(request.prepareSpec);
            candidate->setParameters(request.parameters);

            if (!candidate->loadFromFile(request.file))
            {
                result.state = NeuralLoadState::error;
                result.error = candidate->lastLoadError();
                setStatus(result);
                if (request.generation == latestGeneration.load(std::memory_order_acquire) && request.completion)
                    request.completion(nullptr, result);
                continue;
            }

            result.state = NeuralLoadState::loaded;
            result.metadata = candidate->modelMetadata();
            setStatus(result);

            if (request.generation == latestGeneration.load(std::memory_order_acquire) && request.completion)
                request.completion(std::move(candidate), result);
        }
    }
}
