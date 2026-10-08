cmake_minimum_required(VERSION 3.22)
project(BEAT_FROG VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# COMPANY_NAME "REZONANZA" is what puts the plugin in the REZONANZA folder in Ableton
juce_add_plugin(BeatFrog
    COMPANY_NAME "REZONANZA"
    BUNDLE_ID com.rezonanza.beatfrog
    PLUGIN_MANUFACTURER_CODE Rzna
    PLUGIN_CODE Bfrg
    PRODUCT_NAME "BEAT FROG"
    DESCRIPTION "Drum machine"
    FORMATS AU VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(BeatFrogData SOURCES
    Resources/Silkscreen-Regular.ttf
    Resources/Silkscreen-Bold.ttf)

target_sources(BeatFrog PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engines.cpp)

target_compile_definitions(BeatFrog PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BeatFrog
    PRIVATE
        BeatFrogData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

