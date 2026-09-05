cmake_minimum_required(VERSION 3.22)
project(Outflank VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- downstream projects pin an exact commit SHA
# on `main` instead. This is the same commit Hex/Pugilist pin.
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# PATCH_COMMAND (two patches, copied verbatim from Hex -- see each file for
# the full explanation; neither reported upstream yet):
#
# 1. dpf-clap-state-chunked-read.patch -- PluginCLAP::stateLoad() can't tell
#    a genuine embedded NUL terminator from simply running out of bytes at a
#    chunk boundary, so a host that returns short reads from
#    clap_istream_t::read() can lose every parameter on state load.
# 2. dpf-clap-activate-latency.patch -- PluginCLAP::activate() never reports
#    the plugin's initial latency. Outflank itself reports 0 latency always
#    (DISTRHO_PLUGIN_WANT_LATENCY 0), but this patch fixes a real bug in the
#    shared DPF checkout regardless, so it's applied unconditionally like
#    every other DPF-based plugin in this workspace.
#
# CAVEAT: the patch step only runs on a *fresh* population of the source
# dir -- delete build/_deps/dpf-* when you need to be sure the patches took
# effect. cmake/apply_patch.cmake is idempotent (skips an already-applied
# patch), so a re-run is harmless but not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(OUTFLANK_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND OUTFLANK_DPF_TARGETS au)
endif()

dpf_add_plugin(Outflank
    TARGETS ${OUTFLANK_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/OutflankPluginAdapter.cpp
        Source/DSP/CrossoverMS.cpp
    FILES_UI
        Source/OutflankUI.cpp
)

target_include_directories(Outflank PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# DPF's dpf_add_plugin() unconditionally compiles dgl/src/pugl.cpp's inlined
# WebViewImpl.cpp (a DPF bug -- see Hex's CMakeLists.txt for the full
# explanation), which uses the GNU `typeof` extension and fails under strict
# ISO C++ (our CMAKE_CXX_EXTENSIONS OFF default). Allow GNU extensions on
# DPF's own dgl-opengl target rather than patching vendored DPF code.
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Shared HUI/DSP library ───────────────────────────────────────────────────
# github.com/TriYop/spellbound-common is private -- local dev machines clone
# it over the developer's own cached git/gh credentials, while CI
# authenticates via a short-lived `git config url.insteadOf` rewrite driven
# by the COMMON_REPO_TOKEN secret (see .github/workflows/ci.yml's "Configure
# Common repo access" step, added below, which must run before Configure).
#
# ORDERING (load-bearing): this must come *after* dpf_add_plugin(Outflank
# ...), not merely after FetchContent_MakeAvailable(dpf) -- DPF creates its
# backend-specific DGL target (dgl-opengl) lazily from inside
# dpf_add_plugin(), so at FetchContent_MakeAvailable(dpf) time no dgl-*
# target exists yet. Common guards AudioPluginsCommon::hui_dgl on one of
# those targets existing, so pulling Common in any earlier silently drops
# hui_dgl and the link below fails.
#
# Common's own CMakeLists.txt builds its STATIC libraries without setting
# POSITION_INDEPENDENT_CODE -- every one of Outflank's plugin formats
# (VST3/CLAP/LV2) is a shared object, and DPF only sets that property TRUE
# on the targets *it* creates. Setting CMAKE_POSITION_INDEPENDENT_CODE here,
# before Common's add_subdirectory runs, makes it the default for every
# target in Common (and transitively pugixml) -- without this, linking
# AudioPluginsCommon::hui_dgl into Outflank-ui/Outflank.vst3/Outflank.clap/
# Outflank.lv2 fails at link time.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# Linked into `Outflank-ui` rather than `Outflank`: dpf_add_plugin() splits
# the plugin into a common `Outflank` static lib plus `Outflank-dsp` /
# `Outflank-ui` static libs, and OutflankUI.cpp (the only consumer of the
# widget) lives in `Outflank-ui`. Linking it into `Outflank` PRIVATE would
# keep hui_dgl's include directories away from Outflank-ui's own compile,
# and linking it PUBLIC there would drag a GL/UI library into the DSP-only
# LV2 binary as well.
target_link_libraries(Outflank-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)

# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
# Unchanged from the pre-migration CMakeLists.txt -- CrossoverMS/AllpassFilter/
# QuadraturePair/StateVariableFilter never depended on JUCE, so these test
# executables don't need any porting.
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

add_executable(test_allpassfilter
    Tests/test_allpassfilter.cpp
)
target_include_directories(test_allpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_allpassfilter PRIVATE cxx_std_20)
add_test(NAME AllpassFilter COMMAND test_allpassfilter)

add_executable(test_quadraturepair
    Tests/test_quadraturepair.cpp
)
target_include_directories(test_quadraturepair PRIVATE Source/ Tests/)
target_compile_features(test_quadraturepair PRIVATE cxx_std_20)
add_test(NAME QuadraturePair COMMAND test_quadraturepair)

add_executable(test_outflank_parameters
    Tests/test_outflank_parameters.cpp
)
target_include_directories(test_outflank_parameters PRIVATE Source/ Tests/)
target_compile_features(test_outflank_parameters PRIVATE cxx_std_20)
add_test(NAME OutflankParameters COMMAND test_outflank_parameters)

add_executable(test_outflank_factorypresets
    Tests/test_outflank_factorypresets.cpp
)
target_include_directories(test_outflank_factorypresets PRIVATE Source/ Tests/)
target_link_libraries(test_outflank_factorypresets PRIVATE AudioPluginsCommon::presets)
target_compile_features(test_outflank_factorypresets PRIVATE cxx_std_20)
add_test(NAME OutflankFactoryPresets COMMAND test_outflank_factorypresets)

# ── Install rules ─────────────────────────────────────────────────────────────
# DPF's dpf_add_plugin() writes all format outputs under a flat bin/ dir --
# no Standalone binary: Outflank is an effect, and this workspace only
# requires Standalone for instruments (see Hex's CLAUDE.md, same rule).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Outflank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

# CLAP is a flat shared-library file on Linux/Windows but a bundle
# *directory* on macOS (like VST3/AU) -- see Hex's CMakeLists.txt, which hit
# this as a real CI failure.
if(APPLE)
    install(DIRECTORY  "${_BIN}/Outflank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Outflank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Outflank.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

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
