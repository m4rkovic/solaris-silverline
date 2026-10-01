#pragma once

#include "IAmpModel.h"
#include <memory>
#include <vector>

namespace solaris
{
    class AmpRegistry
    {
    public:
        void registerModel(std::unique_ptr<IAmpModel> model);
        bool select(const std::string& id);

        void prepare(const AmpPrepareSpec& spec);
        void process(juce::AudioBuffer<float>& buffer) noexcept;
        void reset() noexcept;

        IAmpModel* current() noexcept { return selected; }

    private:
        std::vector<std::unique_ptr<IAmpModel>> models;
        IAmpModel* selected = nullptr;
        AmpPrepareSpec lastSpec {};
    };
}
