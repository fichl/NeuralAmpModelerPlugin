cmake_minimum_required(VERSION 3.25)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(PLUGIN_ROOT "${PROJECT_ROOT}/NeuralAmpModeler")
set(IPLUG2_ROOT "${PROJECT_ROOT}/iPlug2")
set(VST3_SDK_ROOT "${IPLUG2_ROOT}/Dependencies/IPlug/VST3_SDK")
set(NAM_CORE_ROOT "${PROJECT_ROOT}/NeuralAmpModelerCore")
set(AUDIO_DSP_TOOLS_ROOT "${PROJECT_ROOT}/AudioDSPTools")

file(STRINGS "${PLUGIN_ROOT}/config.h" PLUG_VERSION_LINE REGEX "^#define PLUG_VERSION_STR ")
string(REGEX REPLACE "^#define PLUG_VERSION_STR \"([0-9]+\\.[0-9]+\\.[0-9]+)\".*$" "\\1" PLUG_VERSION "${PLUG_VERSION_LINE}")

project(NAMOnSteroids-Linux VERSION ${PLUG_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "" FORCE)
set(SMTG_CXX_STANDARD 20 CACHE STRING "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
include(SMTG_VST3_SDK)

smtg_setup_platform_toolset()
smtg_setup_symbol_visibility()

set(ROOT "${VST3_SDK_ROOT}")
set(SDK_ROOT "${VST3_SDK_ROOT}")
set(public_sdk_SOURCE_DIR "${SDK_ROOT}/public.sdk")
set(pluginterfaces_SOURCE_DIR "${SDK_ROOT}/pluginterfaces")
set(SDK_IDE_LIBS_FOLDER FOLDER "Libraries")

include_directories("${ROOT}" "${SDK_ROOT}")

smtg_create_lib_base_target()
smtg_create_pluginterfaces_target()
smtg_create_public_sdk_common_target()
smtg_create_public_sdk_target()

find_package(Threads REQUIRED)

file(GLOB NAM_CORE_SOURCES
  "${NAM_CORE_ROOT}/NAM/*.cpp"
  "${NAM_CORE_ROOT}/NAM/*.c"
  "${NAM_CORE_ROOT}/NAM/*/*.cpp"
)

set(PLUGIN_SOURCES
  "${PLUGIN_ROOT}/NeuralAmpModeler.cpp"
  "${PLUGIN_ROOT}/ToneStack.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugAPIBase.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugParameter.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugPaths.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugPluginBase.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugProcessor.cpp"
  "${IPLUG2_ROOT}/IPlug/IPlugTimer.cpp"
  "${IPLUG2_ROOT}/IPlug/VST3/IPlugVST3.cpp"
  "${IPLUG2_ROOT}/IPlug/VST3/IPlugVST3_Controller.cpp"
  "${IPLUG2_ROOT}/IPlug/VST3/IPlugVST3_ProcessorBase.cpp"
  # The SDK's `sdk` target does not include this implementation, although
  # IPlugVST3_ProcessorBase derives from SingleComponentEffect.
  "${VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
  # Entry point functions (ModuleEntry/ModuleExit) for Linux VST3
  "${VST3_SDK_ROOT}/public.sdk/source/main/linuxmain.cpp"
  "${AUDIO_DSP_TOOLS_ROOT}/dsp/dsp.cpp"
  "${AUDIO_DSP_TOOLS_ROOT}/dsp/ImpulseResponse.cpp"
  "${AUDIO_DSP_TOOLS_ROOT}/dsp/NoiseGate.cpp"
  "${AUDIO_DSP_TOOLS_ROOT}/dsp/RecursiveLinearFilter.cpp"
  "${AUDIO_DSP_TOOLS_ROOT}/dsp/wav.cpp"
  ${NAM_CORE_SOURCES}
)

smtg_add_vst3plugin(NAMOnSteroids PACKAGE_NAME "NAM On Steroids" SOURCES_LIST ${PLUGIN_SOURCES})

target_compile_definitions(NAMOnSteroids
  PRIVATE
    VST3_API
    NO_IGRAPHICS
    NAM_HEADLESS_LINUX
    NAM_ENABLE_A2_FAST
)

target_include_directories(NAMOnSteroids
  PRIVATE
    "${PLUGIN_ROOT}"
    "${IPLUG2_ROOT}"
    "${IPLUG2_ROOT}/IPlug"
    "${IPLUG2_ROOT}/IPlug/Extras"
    "${IPLUG2_ROOT}/IPlug/VST3"
    "${IPLUG2_ROOT}/WDL"
    "${VST3_SDK_ROOT}"
    "${VST3_SDK_ROOT}/base/source"
    "${VST3_SDK_ROOT}/pluginterfaces"
    "${VST3_SDK_ROOT}/public.sdk/source/vst"
    "${NAM_CORE_ROOT}"
    "${NAM_CORE_ROOT}/NAM"
    "${NAM_CORE_ROOT}/Dependencies/eigen"
    "${NAM_CORE_ROOT}/Dependencies/nlohmann"
    "${AUDIO_DSP_TOOLS_ROOT}"
    "${AUDIO_DSP_TOOLS_ROOT}/dsp"
    "${AUDIO_DSP_TOOLS_ROOT}/Dependencies/eigen"
)

target_link_libraries(NAMOnSteroids PRIVATE sdk Threads::Threads)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(NAMOnSteroids PRIVATE stdc++fs)
  endif()
  # Shared libraries otherwise allow unresolved symbols, which only surface
  # when a host attempts to dlopen the VST3 module.
  target_link_options(NAMOnSteroids PRIVATE "-Wl,-z,defs")
endif()

target_compile_options(NAMOnSteroids
  PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-deprecated-declarations
    -include cstdlib
    $<$<CONFIG:Release>:-O3>
)

set_target_properties(NAMOnSteroids
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)
