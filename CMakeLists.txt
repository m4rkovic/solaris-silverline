cmake_minimum_required(VERSION 3.24)

project(SolarisSilverline
    VERSION 0.3.0
    DESCRIPTION "Solaris Silverline - boutique guitar amplifier suite"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOLARIS_COPY_PLUGIN_AFTER_BUILD "Copy plugin to the system plugin directory after build" OFF)
option(SOLARIS_ENABLE_AU "Build Audio Unit format on macOS" OFF)
option(SOLARIS_ENABLE_NEURAL_AUDIO "Build NeuralAudio/NAM backend" ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(SOLARIS_ENABLE_NEURAL_AUDIO)
    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
    set(BUILD_RTNEURAL OFF CACHE BOOL "" FORCE)
    set(BUILD_NAMCORE ON CACHE BOOL "" FORCE)
    set(BUILD_INTERNAL_STATIC_WAVENET ON CACHE BOOL "" FORCE)
    set(BUILD_STATIC_INTERNAL_NAMA2 ON CACHE BOOL "" FORCE)
    set(BUILD_INTERNAL_STATIC_LSTM ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        NeuralAudio
        GIT_REPOSITORY https://github.com/mikeoliphant/NeuralAudio.git
        GIT_TAG 048d195c5113116e07d5ac25844b21380323db6f
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(NeuralAudio)
endif()

set(SOLARIS_PLUGIN_FORMATS VST3 Standalone)
if(APPLE AND SOLARIS_ENABLE_AU)
    list(APPEND SOLARIS_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(SolarisSilverline
    COMPANY_NAME "Solaris Audio"
    BUNDLE_ID "audio.solaris.silverline"
    PLUGIN_MANUFACTURER_CODE Sola
    PLUGIN_CODE Slvr
    FORMATS ${SOLARIS_PLUGIN_FORMATS}
    PRODUCT_NAME "Solaris Silverline"
    COMPANY_COPYRIGHT "Copyright (c) Solaris Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SOLARIS_COPY_PLUGIN_AFTER_BUILD}
)

juce_generate_juce_header(SolarisSilverline)

set(SOLARIS_SOURCES
    Source/Parameters.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UI/Theme.h
    Source/UI/Controls.cpp
    Source/UI/Controls.h
    Source/UI/NavigationBar.cpp
    Source/UI/NavigationBar.h
    Source/UI/AmpPanel.cpp
    Source/UI/AmpPanel.h
    Source/UI/CabPanel.cpp
    Source/UI/CabPanel.h
    Source/UI/EqPanel.cpp
    Source/UI/EqPanel.h
    Source/UI/PedalBoardPanel.cpp
    Source/UI/PedalBoardPanel.h
    Source/UI/TunerOverlay.cpp
    Source/UI/TunerOverlay.h
    SolarisCore/DSP/IAudioStage.h
    SolarisCore/Amp/AmpParameters.h
    SolarisCore/Amp/INonlinearAmpStage.h
    SolarisCore/Amp/IAmpModel.h
    SolarisCore/Amp/AmpRegistry.h
    SolarisCore/Amp/AmpRegistry.cpp
    SolarisCore/Amp/NeuralAmpModel.h
    SolarisCore/Amp/NeuralAmpModel.cpp
    SolarisCore/Cab/CabinetEngine.h
    SolarisCore/Cab/CabinetEngine.cpp
    SolarisCore/EQ/ParametricEQ.h
    SolarisCore/EQ/ParametricEQ.cpp
    SolarisCore/Tuner/TunerEngine.h
    SolarisCore/Tuner/TunerEngine.cpp
    SolarisCore/Presets/PresetState.h
    SolarisCore/Presets/PresetState.cpp
    Silverline/AmpModels/Silverline68AnalogueStage.h
    Silverline/AmpModels/Silverline68AnalogueStage.cpp
    Silverline/AmpModels/Silverline68Amp.h
    Silverline/AmpModels/Silverline68Amp.cpp
)

target_sources(SolarisSilverline PRIVATE ${SOLARIS_SOURCES})
target_include_directories(SolarisSilverline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SolarisSilverline
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(SOLARIS_ENABLE_NEURAL_AUDIO)
    target_compile_definitions(SolarisSilverline PRIVATE SOLARIS_ENABLE_NEURAL_AUDIO=1)
endif()

if(MSVC)
    target_compile_options(SolarisSilverline PRIVATE /permissive- /Zc:__cplusplus /utf-8)
endif()

target_link_libraries(SolarisSilverline
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(SOLARIS_ENABLE_NEURAL_AUDIO)
    target_link_libraries(SolarisSilverline PRIVATE NeuralAudio)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOLARIS_SOURCES})

if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SolarisSilverline_Standalone)
endif()

message(STATUS "Solaris formats: ${SOLARIS_PLUGIN_FORMATS}")
message(STATUS "Solaris NeuralAudio backend: ${SOLARIS_ENABLE_NEURAL_AUDIO}")
