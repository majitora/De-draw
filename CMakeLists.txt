cmake_minimum_required(VERSION 3.22)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(DeDraw VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(DeDrawAssets SOURCES assets/ref.jpeg)

juce_add_plugin(DeDraw
    COMPANY_NAME "maji"
    PLUGIN_MANUFACTURER_CODE Maji
    PLUGIN_CODE Ddrw
    FORMATS VST3
    PRODUCT_NAME "de_draw"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay)

target_sources(DeDraw PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h)

target_compile_definitions(DeDraw PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DeDraw
    PRIVATE
        DeDrawAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)
