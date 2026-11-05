cmake_minimum_required(VERSION 3.22)
project(DerekReverb VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DerekReverb
    COMPANY_NAME "nobullnobullnobull"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    FORMATS VST3
    VST3_CATEGORIES Fx Reverb
    PRODUCT_NAME "Derek Reverb"
    PLUGIN_MANUFACTURER_CODE Nbnb
    PLUGIN_CODE Drkr
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(DerekReverb)

target_sources(DerekReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

# Optional: drop your artwork at Assets/background.png and it is embedded automatically
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.png)
    juce_add_binary_data(DerekAssets SOURCES Assets/background.png)
    target_link_libraries(DerekReverb PRIVATE DerekAssets)
    target_compile_definitions(DerekReverb PRIVATE HAS_BG=1)
endif()

target_compile_definitions(DerekReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DerekReverb PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
