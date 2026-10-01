#pragma once

namespace solaris::ParameterIDs
{
    inline constexpr auto inputGain = "inputGain";
    inline constexpr auto outputGain = "outputGain";

    inline constexpr auto ampEnabled = "ampEnabled";
    inline constexpr auto ampChannel = "ampChannel";
    inline constexpr auto ampVolume = "ampVolume";
    inline constexpr auto ampBass = "ampBass";
    inline constexpr auto ampTreble = "ampTreble";
    inline constexpr auto ampReverb = "ampReverb";
    inline constexpr auto ampTremoloSpeed = "ampTremoloSpeed";
    inline constexpr auto ampTremoloIntensity = "ampTremoloIntensity";

    inline constexpr auto preCompEnabled = "preCompEnabled";
    inline constexpr auto preCompSustain = "preCompSustain";
    inline constexpr auto preCompAttack = "preCompAttack";
    inline constexpr auto preCompLevel = "preCompLevel";

    inline constexpr auto preDriveEnabled = "preDriveEnabled";
    inline constexpr auto preDriveDrive = "preDriveDrive";
    inline constexpr auto preDriveTone = "preDriveTone";
    inline constexpr auto preDriveLevel = "preDriveLevel";

    inline constexpr auto preDistEnabled = "preDistEnabled";
    inline constexpr auto preDistGain = "preDistGain";
    inline constexpr auto preDistContour = "preDistContour";
    inline constexpr auto preDistLevel = "preDistLevel";

    inline constexpr auto preHardEnabled = "preHardEnabled";
    inline constexpr auto preHardDistortion = "preHardDistortion";
    inline constexpr auto preHardFilter = "preHardFilter";
    inline constexpr auto preHardLevel = "preHardLevel";

    inline constexpr auto preFuzzEnabled = "preFuzzEnabled";
    inline constexpr auto preFuzzSustain = "preFuzzSustain";
    inline constexpr auto preFuzzTone = "preFuzzTone";
    inline constexpr auto preFuzzLevel = "preFuzzLevel";

    inline constexpr auto postPhaserEnabled = "postPhaserEnabled";
    inline constexpr auto postPhaserRate = "postPhaserRate";
    inline constexpr auto postPhaserDepth = "postPhaserDepth";
    inline constexpr auto postPhaserMix = "postPhaserMix";

    inline constexpr auto postChorusEnabled = "postChorusEnabled";
    inline constexpr auto postChorusRate = "postChorusRate";
    inline constexpr auto postChorusDepth = "postChorusDepth";
    inline constexpr auto postChorusMix = "postChorusMix";

    inline constexpr auto postTremoloEnabled = "postTremoloEnabled";
    inline constexpr auto postTremoloRate = "postTremoloRate";
    inline constexpr auto postTremoloDepth = "postTremoloDepth";
    inline constexpr auto postTremoloShape = "postTremoloShape";

    inline constexpr auto postDelayEnabled = "postDelayEnabled";
    inline constexpr auto postDelayTime = "postDelayTime";
    inline constexpr auto postDelayFeedback = "postDelayFeedback";
    inline constexpr auto postDelayMix = "postDelayMix";

    inline constexpr auto postReverbEnabled = "postReverbEnabled";
    inline constexpr auto postReverbDecay = "postReverbDecay";
    inline constexpr auto postReverbTone = "postReverbTone";
    inline constexpr auto postReverbMix = "postReverbMix";
}
