cmake_minimum_required(VERSION 3.24)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Versión mínima de macOS")

project(EffectSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
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

# Neural Amp Modeler: lector y motor de capturas .nam (licencia MIT), con Eigen para el álgebra.
# Solo se descargan las fuentes; la biblioteca se compila aquí para no arrastrar sus herramientas.
FetchContent_Declare(nam_core
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG v0.6.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    SOURCE_SUBDIR no-cmake)
FetchContent_Declare(nam_eigen
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://gitlab.com/libeigen/eigen/-/archive/bc3b39870ecb690a623a3f49149a358b95c5781d/eigen-bc3b39870ecb690a623a3f49149a358b95c5781d.tar.gz
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(nam_core nam_eigen)

file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
    "${nam_core_SOURCE_DIR}/NAM/*.cpp"
    "${nam_core_SOURCE_DIR}/NAM/wavenet/*.cpp")

add_library(effectsuite_neural STATIC ${NAM_SOURCES} src/dsp/NeuralAmp.cpp)
target_include_directories(effectsuite_neural SYSTEM PRIVATE
    "${nam_core_SOURCE_DIR}"
    "${nam_core_SOURCE_DIR}/Dependencies/nlohmann"
    "${nam_eigen_SOURCE_DIR}")
target_include_directories(effectsuite_neural PUBLIC src)
target_compile_definitions(effectsuite_neural PRIVATE NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST NOMINMAX)
set_target_properties(effectsuite_neural PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(effectsuite_neural PRIVATE /bigobj /w)
else()
    target_compile_options(effectsuite_neural PRIVATE -w)
endif()

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
            # Entera: las arquitecturas de red se registran con inicializadores estáticos que el
            # enlazador descartaría si solo tomara de la biblioteca lo que se referencia.
            $<LINK_LIBRARY:WHOLE_ARCHIVE,effectsuite_neural>
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endif()

if(EFFECTSUITE_BUILD_TESTS)
    juce_add_console_app(EffectSuiteMeasure PRODUCT_NAME "EffectSuiteMeasure")
    target_sources(EffectSuiteMeasure PRIVATE tests/Measure.cpp)
    target_include_directories(EffectSuiteMeasure PRIVATE src)
    target_compile_definitions(EffectSuiteMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        EFFECTSUITE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    target_link_libraries(EffectSuiteMeasure
        PRIVATE
            juce::juce_dsp
            juce::juce_audio_formats
            # Entera: las arquitecturas de red se registran con inicializadores estáticos que el
            # enlazador descartaría si solo tomara de la biblioteca lo que se referencia.
            $<LINK_LIBRARY:WHOLE_ARCHIVE,effectsuite_neural>
        PUBLIC
            juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME measure COMMAND EffectSuiteMeasure)
endif()
