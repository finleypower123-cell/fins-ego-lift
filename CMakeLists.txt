cmake_minimum_required(VERSION 3.22)

# Universal binary on macOS (Apple Silicon + Intel). Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(FinsEgoLift VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 9.0.4 bundles Avid's AAX SDK, so no separate SDK download is needed.
# Point JUCE_DIR at a local checkout to skip the download: cmake -DJUCE_DIR=/path/to/JUCE ...
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FEL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FEL_FORMATS AU AAX)
elseif(WIN32)
    list(APPEND FEL_FORMATS AAX)
endif()

juce_add_plugin(FinsEgoLift
    COMPANY_NAME "Ego Labs"
    COMPANY_WEBSITE "https://example.com"
    PRODUCT_NAME "Fins Ego Lift 5000"
    PLUGIN_NAME "Fins Ego Lift 5000"
    BUNDLE_ID "com.egolabs.finsegolift5000"
    PLUGIN_MANUFACTURER_CODE Eglb
    PLUGIN_CODE F5k0
    FORMATS ${FEL_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Fin's Ego Lift needs your microphone so you can sing into it."
    HARDENED_RUNTIME_ENABLED FALSE
    VST3_CATEGORIES Fx Reverb Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Reverb
    DISABLE_AAX_MULTI_MONO TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FinsEgoLift PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FinsEgoLift PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FinsEgoLift
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine self-test: renders every preset and checks for silence, NaNs and runaway levels.
add_executable(FelEngineTest tests/engine_test.cpp)
target_include_directories(FelEngineTest PRIVATE Source)
