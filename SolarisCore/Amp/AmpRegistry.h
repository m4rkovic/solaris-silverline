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

        // Selection may construct a model and must be called from a non-audio thread.
        bool select(const std::string& id);

        void prepare(const AmpPrepareSpec& spec);
        void setParameters(const AmpParameters& parameters) noexcept;
        void process(juce::AudioBuffer<float>& buffer) noexcept;
        void reset() noexcept;

        IAmpModel* current() noexcept { return selected.get(); }
        const IAmpModel* current() const noexcept { return selected.get(); }
        double tailLengthSeconds() const noexcept;

        const std::vector<Entry>& entries() const noexcept { return factories; }

    private:
        std::vector<Entry> factories;
        std::unique_ptr<IAmpModel> selected;
        AmpPrepareSpec lastSpec {};
        AmpParameters lastParameters {};
        bool hasBeenPrepared = false;
    };
}
