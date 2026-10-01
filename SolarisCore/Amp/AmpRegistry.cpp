#include "AmpRegistry.h"

namespace solaris
{
    void AmpRegistry::registerModel(std::unique_ptr<IAmpModel> model)
    {
        if (!model)
            return;

        models.push_back(std::move(model));
    }

    bool AmpRegistry::select(const std::string& id)
    {
        for (auto& model : models)
        {
            if (model->metadata().id == id)
            {
                selected = model.get();
                selected->prepare(lastSpec);
                return true;
            }
        }

        return false;
    }

    void AmpRegistry::prepare(const AmpPrepareSpec& spec)
    {
        lastSpec = spec;

        for (auto& model : models)
            model->prepare(spec);
    }

    void AmpRegistry::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (selected != nullptr)
            selected->process(buffer);
    }

    void AmpRegistry::reset() noexcept
    {
        for (auto& model : models)
            model->reset();
    }
}
