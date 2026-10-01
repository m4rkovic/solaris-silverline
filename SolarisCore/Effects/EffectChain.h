#pragma once

#include "IEffect.h"
#include <array>

namespace solaris
{
    class EffectChain
    {
    public:
        static constexpr std::size_t maxEffects = 16;

        bool addEffect(IEffect& effect) noexcept;
        bool setOrder(const juce::StringArray& effectIds);
        void prepare(const EffectPrepareSpec& spec);
        void reset() noexcept;
        void process(juce::AudioBuffer<float>& buffer) noexcept;

        juce::ValueTree createState(juce::Identifier type) const;
        void restoreState(const juce::ValueTree& state);

        std::size_t size() const noexcept { return orderCount; }
        IEffect* at(std::size_t index) noexcept
        {
            return index < orderCount ? order[index] : nullptr;
        }

        double tailLengthSeconds() const noexcept;

    private:
        IEffect* findById(const juce::String& effectId) const noexcept;

        std::array<IEffect*, maxEffects> registered {};
        std::array<IEffect*, maxEffects> order {};
        std::size_t registeredCount = 0;
        std::size_t orderCount = 0;
        EffectPrepareSpec lastSpec {};
        bool prepared = false;
    };
}
