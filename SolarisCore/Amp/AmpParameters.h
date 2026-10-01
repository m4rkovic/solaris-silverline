#pragma once

#include <algorithm>

namespace solaris
{
    enum class AmpChannel
    {
        custom = 0,
        vintage = 1
    };

    // Product-agnostic amp controls. Continuous values are normalized to [0, 1]
    // so the plugin shell does not need to know a model's internal electrical ranges.
    // Reserved controls keep the IAmpModel interface stable as richer models arrive.
    struct AmpParameters
    {
        bool enabled = true;
        AmpChannel channel = AmpChannel::custom;

        float volume = 0.45f;
        float bass = 0.5f;
        float treble = 0.55f;
        float reverb = 0.2f;
        float tremoloSpeedHz = 4.0f;
        float tremoloIntensity = 0.0f;

        float mid = 0.5f;
        float presence = 0.5f;
        float master = 1.0f;

        void clampToValidRange() noexcept
        {
            volume = std::clamp(volume, 0.0f, 1.0f);
            bass = std::clamp(bass, 0.0f, 1.0f);
            treble = std::clamp(treble, 0.0f, 1.0f);
            reverb = std::clamp(reverb, 0.0f, 1.0f);
            tremoloSpeedHz = std::clamp(tremoloSpeedHz, 0.1f, 12.0f);
            tremoloIntensity = std::clamp(tremoloIntensity, 0.0f, 1.0f);
            mid = std::clamp(mid, 0.0f, 1.0f);
            presence = std::clamp(presence, 0.0f, 1.0f);
            master = std::clamp(master, 0.0f, 1.0f);
        }
    };
}
