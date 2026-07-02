cmake_minimum_required(VERSION 3.22)
project(Outflank VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Must come AFTER FetchContent_MakeAvailable(JUCE)
set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

# Embed factory presets as binary data
file(GLOB FACTORY_PRESET_FILES "Source/Presets/Factory/*.xml")
juce_add_binary_data(Outflank_BinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE   Outflank_BinaryData
    SOURCES     ${FACTORY_PRESET_FILES}
)

set(OUTFLANK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OUTFLANK_FORMATS AU)
endif()

juce_add_plugin(Outflank
    COMPANY_NAME                "Spellbound"
    PLUGIN_MANUFACTURER_CODE    Spbd
    PLUGIN_CODE                 Otfk
    FORMATS                     ${OUTFLANK_FORMATS}
    PRODUCT_NAME                "Outflank"
    BUNDLE_ID                   "com.spellbound.outflank"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DESCRIPTION                 "Mono-bass / wide-highs stereo crossover"
)

target_sources(Outflank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/CrossoverMS.cpp
    Source/PresetManager.cpp
)

target_compile_definitions(Outflank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Outflank PRIVATE Source/)

target_compile_features(Outflank PUBLIC cxx_std_20)

target_link_libraries(Outflank
    PRIVATE
        Outflank_BinaryData
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Must come AFTER target_link_libraries
clap_juce_extensions_plugin(
    TARGET      Outflank
    CLAP_ID     "com.spellbound.outflank"
    CLAP_FEATURES audio-effect utility stereo
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
)

# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
enable_testing()

add_executable(test_statevariablefilter
    Tests/test_statevariablefilter.cpp
)
target_include_directories(test_statevariablefilter PRIVATE Source/ Tests/)
target_compile_features(test_statevariablefilter PRIVATE cxx_std_20)
add_test(NAME StateVariableFilter COMMAND test_statevariablefilter)

add_executable(test_crossoverms
    Tests/test_crossoverms.cpp
    Source/DSP/CrossoverMS.cpp
)
target_include_directories(test_crossoverms PRIVATE Source/ Tests/)
target_compile_features(test_crossoverms PRIVATE cxx_std_20)
add_test(NAME CrossoverMS COMMAND test_crossoverms)

# ── Install rules ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_ARTS "${CMAKE_BINARY_DIR}/Outflank_artefacts/${CMAKE_BUILD_TYPE}")

install(DIRECTORY  "${_ARTS}/VST3/Outflank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(FILES      "${_ARTS}/CLAP/Outflank.clap"
        DESTINATION CLAP
        COMPONENT   Runtime
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(PROGRAMS   "${_ARTS}/Standalone/Outflank"
        DESTINATION bin
        COMPONENT   Runtime)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ───────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Outflank)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mono-bass / wide-highs stereo crossover")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Outflank-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
