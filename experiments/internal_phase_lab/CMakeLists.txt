cmake_minimum_required(VERSION 3.20)
project(NAMInternalPhaseLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(NAM_CORE "${REPO_ROOT}/NeuralAmpModelerCore")

file(GLOB NAM_SOURCES
  "${NAM_CORE}/NAM/*.cpp"
  "${NAM_CORE}/NAM/*/*.cpp"
)

add_executable(internal_phase_lab
  internal_phase_lab.cpp
  ${NAM_SOURCES}
  "${REPO_ROOT}/AudioDSPTools/dsp/wav.cpp"
)

target_include_directories(internal_phase_lab PRIVATE
  "${REPO_ROOT}"
  "${NAM_CORE}"
  "${NAM_CORE}/Dependencies/eigen"
  "${NAM_CORE}/Dependencies/nlohmann"
  "${REPO_ROOT}/AudioDSPTools"
)

target_compile_definitions(internal_phase_lab PRIVATE
  NAM_ENABLE_A2_FAST
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

if(MSVC)
  target_compile_options(internal_phase_lab PRIVATE
    /O2
    /arch:AVX
    /W3
  )
endif()
