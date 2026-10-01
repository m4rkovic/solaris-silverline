#include "EffectChain.h"

namespace solaris
{
    bool EffectChain::addEffect(IEffect& effect) noexcept
    {
        if (registeredCount >= maxEffects)
            return false;

        for (std::size_t i = 0; i < registeredCount; ++i)
            if (juce::String(registered[i]->id()) == effect.id())
                return false;

        registered[registeredCount++] = &effect;
        order[orderCount++] = &effect;

        if (prepared)
            effect.prepare(lastSpec);

        return true;
    }

    IEffect* EffectChain::findById(const juce::String& effectId) const noexcept
    {
        for (std::size_t i = 0; i < registeredCount; ++i)
            if (effectId == registered[i]->id())
                return registered[i];

        return nullptr;
    }

    bool EffectChain::setOrder(const juce::StringArray& effectIds)
    {
        if (effectIds.size() != static_cast<int>(registeredCount))
            return false;

        std::array<IEffect*, maxEffects> candidate {};
        for (int i = 0; i < effectIds.size(); ++i)
        {
            auto* effect = findById(effectIds[i]);
            if (effect == nullptr)
                return false;

            for (int previous = 0; previous < i; ++previous)
                if (candidate[static_cast<std::size_t>(previous)] == effect)
                    return false;

            candidate[static_cast<std::size_t>(i)] = effect;
        }

        order = candidate;
        orderCount = registeredCount;
        return true;
    }

    void EffectChain::prepare(const EffectPrepareSpec& spec)
    {
        lastSpec = spec;
        prepared = true;

        for (std::size_t i = 0; i < registeredCount; ++i)
            registered[i]->prepare(spec);
    }

    void EffectChain::reset() noexcept
    {
        for (std::size_t i = 0; i < registeredCount; ++i)
            registered[i]->reset();
    }

    void EffectChain::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        for (std::size_t i = 0; i < orderCount; ++i)
            order[i]->process(buffer);
    }

    juce::ValueTree EffectChain::createState(juce::Identifier type) const
    {
        juce::ValueTree state(type);
        for (std::size_t i = 0; i < orderCount; ++i)
            state.setProperty("slot" + juce::String(static_cast<int>(i)),
                              order[i]->id(), nullptr);
        return state;
    }

    void EffectChain::restoreState(const juce::ValueTree& state)
    {
        if (!state.isValid() || registeredCount == 0)
            return;

        juce::StringArray ids;
        for (std::size_t i = 0; i < registeredCount; ++i)
        {
            const auto key = "slot" + juce::String(static_cast<int>(i));
            const auto id = state.getProperty(key).toString();
            if (id.isEmpty())
                return;
            ids.add(id);
        }

        setOrder(ids);
    }

    double EffectChain::tailLengthSeconds() const noexcept
    {
        double longest = 0.0;
        for (std::size_t i = 0; i < registeredCount; ++i)
            longest = juce::jmax(longest, registered[i]->tailLengthSeconds());
        return longest;
    }
}
