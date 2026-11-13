cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Versión mínima de macOS")

project(EffectSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(EFFECTSUITE_BUILD_PLUGIN "Compila la aplicación y los plugins" ON)
option(EFFECTSUITE_BUILD_TESTS "Compila las pruebas de medida del motor de audio" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(EFFECTSUITE_BUILD_PLUGIN)
    set(EFFECTSUITE_FORMATS Standalone VST3)
    if(APPLE)
        list(APPEND EFFECTSUITE_FORMATS AU)
    endif()

    juce_add_plugin(EffectSuite
        PRODUCT_NAME "EffectSuite"
        COMPANY_NAME "EffectSuite"
        BUNDLE_ID "com.effectsuite.EffectSuite"
        PLUGIN_MANUFACTURER_CODE Efsu
        PLUGIN_CODE Efs1
        FORMATS ${EFFECTSUITE_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx Distortion
        AU_MAIN_TYPE kAudioUnitType_Effect
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "EffectSuite necesita la entrada de audio para procesar la guitarra."
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(EffectSuite PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp)

    target_include_directories(EffectSuite PRIVATE src)

    target_compile_definitions(EffectSuite PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_ASIO=1)

    target_link_libraries(EffectSuite
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endif()

if(EFFECTSUITE_BUILD_TESTS)
    juce_add_console_app(EffectSuiteMeasure PRODUCT_NAME "EffectSuiteMeasure")
    target_sources(EffectSuiteMeasure PRIVATE tests/Measure.cpp)
    target_include_directories(EffectSuiteMeasure PRIVATE src)
    target_compile_definitions(EffectSuiteMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EffectSuiteMeasure
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME measure COMMAND EffectSuiteMeasure)
endif()
