cmake_minimum_required(VERSION 3.22)
project(ChordDice VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

# Use a local JUCE checkout if one is given (-DJUCE_PATH=...), otherwise download JUCE 8.0.12.
set(JUCE_PATH "" CACHE PATH "Path to a JUCE checkout (optional)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ChordDice
    COMPANY_NAME "Florian Azemi"
    PRODUCT_NAME "Chord Dice"
    PLUGIN_MANUFACTURER_CODE Flaz
    PLUGIN_CODE Chdc
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Generator
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(ChordDiceWeb SOURCES web/index.html)

target_sources(ChordDice PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ChordDice PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChordDice
    PRIVATE
        ChordDiceWeb
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Linux only (not needed for FL Studio): the web view uses WebKitGTK.
if(UNIX AND NOT APPLE)
    target_link_libraries(ChordDice PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
endif()
