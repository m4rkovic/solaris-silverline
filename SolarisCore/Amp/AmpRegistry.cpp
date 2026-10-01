#include "AmpRegistry.h"
#include <utility>

namespace solaris
{
    void AmpRegistry::registerFactory(AmpMetadata metadata, Factory factory)
    {
        if (!factory)
            return;

        factories.push_back({ std::move(metadata), std::move(factory) });
    }

    bool AmpRegistry::select(const std::string& id)
    {
        for (const auto& entry : factories)
        {
            if (entry.metadata.id != id)
                continue;

            auto next = entry.factory();
            if (!next)
                return false;

            if (hasBeenPrepared)
                next->prepare(lastSpec);

            next->setParameters(lastParameters);
            selected = std::move(next);
            return true;
        }

        return false;
    }

    void AmpRegistry::prepare(const AmpPrepareSpec& spec)
    {
        lastSpec = spec;
        hasBeenPrepared = true;

        if (selected != nullptr)
            selected->prepare(spec);
    }

    void AmpRegistry::setParameters(const AmpParameters& parameters) noexcept
    {
        lastParameters = parameters;

        if (selected != nullptr)
            selected->setParameters(parameters);
    }

    void AmpRegistry::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (selected != nullptr)
            selected->process(buffer);
    }

    void AmpRegistry::reset() noexcept
    {
        if (selected != nullptr)
            selected->reset();
    }

    double AmpRegistry::tailLengthSeconds() const noexcept
    {
        return selected != nullptr ? selected->tailLengthSeconds() : 0.0;
    }
}
