cmake_minimum_required(VERSION 3.22)

project(DRIFT VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ---- Fetch JUCE ------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---- DRIFT plugin / standalone --------------------------------------------
set(DRIFT_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND DRIFT_FORMATS AU)
endif()

juce_add_plugin(DRIFT
    COMPANY_NAME                  "itselliott"
    COMPANY_WEBSITE               "https://github.com/itselliott/drift"
    BUNDLE_ID                     com.itselliott.drift
    PLUGIN_MANUFACTURER_CODE      ItsE
    PLUGIN_CODE                   Drft
    FORMATS                       ${DRIFT_FORMATS}
    PRODUCT_NAME                  "DRIFT"
    DESCRIPTION                   "Hybrid analog/digital echo — '80s rack-delay character"
    VST3_CATEGORIES               Fx Delay
    AU_MAIN_TYPE                  kAudioUnitType_Effect
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              TRUE    # MIDI CC 14..19 mapped to the 6 faders, plus PC + clock
    NEEDS_MIDI_OUTPUT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   TRUE
    COPY_PLUGIN_AFTER_BUILD       FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "DRIFT needs microphone access in standalone mode."
)

target_sources(DRIFT PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/DriftStandaloneApp.cpp
)

target_compile_definitions(DRIFT PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

target_link_libraries(DRIFT PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
