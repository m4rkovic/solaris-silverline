#pragma once

#include "IAmpModel.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace solaris
{
    class AmpRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<IAmpModel>()>;

        struct Entry
        {
            AmpMetadata metadata;
            Factory factory;
        };

        void registerFactory(AmpMetadata metadata, Factory factory);

        template <typename Model>
        void registerModelType()
        {
            registerFactory(Model::staticMetadata(), [] { return std::make_unique<Model>(); });
        }

        // Selection constructs a model and must be called away from the audio callback.
        bool select(const std::string& id);

        void prepare(const AmpPrepareSpec& spec);
        void setParameters(const AmpParameters& parameters) noexcept;
        void process(juce::AudioBuffer<float>& buffer) noexcept;
        void reset() noexcept;

        // The replacement must already be prepared. The caller is responsible for
        // synchronising this tiny pointer swap with the host callback.
        void swapSelected(std::unique_ptr<IAmpModel>& replacement) noexcept;

        IAmpModel* current() noexcept { return selected.get(); }
        const IAmpModel* current() const noexcept { return selected.get(); }
        int latencySamples() const noexcept;
        double tailLengthSeconds() const noexcept;

        const std::vector<Entry>& entries() const noexcept { return factories; }
        const AmpPrepareSpec& lastPrepareSpec() const noexcept { return lastSpec; }
        const AmpParameters& lastParametersValue() const noexcept { return lastParameters; }
        bool isPrepared() const noexcept { return hasBeenPrepared; }

    private:
        std::vector<Entry> factories;
        std::unique_ptr<IAmpModel> selected;
        AmpPrepareSpec lastSpec {};
        AmpParameters lastParameters {};
        bool hasBeenPrepared = false;
    };
}
