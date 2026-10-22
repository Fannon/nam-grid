cmake_minimum_required(VERSION 3.24)
project(nam_ir_comparison LANGUAGES C CXX)
# Standalone experiment: never included by the plugin build or CTest.
get_filename_component(NAM_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(NAM_DEPS_ROOT "${NAM_REPO_ROOT}" CACHE PATH "Checkout containing prepared pinned NAM dependencies")
include(FetchContent)
FetchContent_Declare(fftconvolver
  GIT_REPOSITORY https://github.com/HiFi-LoFi/FFTConvolver.git
  GIT_TAG f2cdeb04c42141d2caec19ca4f137398b2a76b85
  GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(fftconvolver)
option(IR_FFT_PRESERVE_TAIL "Experiment: trim only exact zeros, preserving quiet IR coefficients" OFF)
set(fft_uniform "${fftconvolver_SOURCE_DIR}/FFTConvolver.cpp")
set(fft_two_stage "${fftconvolver_SOURCE_DIR}/TwoStageFFTConvolver.cpp")
if(IR_FFT_PRESERVE_TAIL)
  # Generate visibly marked experimental copies. Keep the pinned dependency unchanged.
  foreach(source FFTConvolver TwoStageFFTConvolver)
    file(READ "${fftconvolver_SOURCE_DIR}/${source}.cpp" original)
    set(needle "::fabs(ir[irLen-1]) < 0.000001f")
    string(FIND "${original}" "${needle}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Pinned FFTConvolver tail-trim code changed")
    endif()
    string(REPLACE "${needle}" "ir[irLen-1] == 0.0f" modified "${original}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${source}-preserve-tail.cpp"
      "// Experimental modification by nam-grid: preserve nonzero quiet coefficients.\n${modified}")
  endforeach()
  set(fft_uniform "${CMAKE_CURRENT_BINARY_DIR}/FFTConvolver-preserve-tail.cpp")
  set(fft_two_stage "${CMAKE_CURRENT_BINARY_DIR}/TwoStageFFTConvolver-preserve-tail.cpp")
endif()
add_executable(nam-ir-comparison main.cpp wdl.cpp
  "${fft_uniform}" "${fft_two_stage}"
  "${fftconvolver_SOURCE_DIR}/AudioFFT.cpp"
  "${fftconvolver_SOURCE_DIR}/Utilities.cpp"
  "${NAM_DEPS_ROOT}/iPlug2/WDL/fft.c"
  "${NAM_DEPS_ROOT}/AudioDSPTools/dsp/dsp.cpp"
  "${NAM_DEPS_ROOT}/AudioDSPTools/dsp/ImpulseResponse.cpp"
  "${NAM_DEPS_ROOT}/AudioDSPTools/dsp/wav.cpp")
target_compile_features(nam-ir-comparison PRIVATE cxx_std_20)
target_compile_definitions(nam-ir-comparison PRIVATE NOMINMAX IR_FFT_PRESERVE_TAIL=$<BOOL:${IR_FFT_PRESERVE_TAIL}>)
target_include_directories(nam-ir-comparison PRIVATE "${fftconvolver_SOURCE_DIR}"
  "${NAM_DEPS_ROOT}/AudioDSPTools/dsp"
  "${NAM_DEPS_ROOT}/iPlug2/WDL" "${NAM_DEPS_ROOT}/eigen"
  "${NAM_DEPS_ROOT}/NeuralAmpModelerCore/Dependencies/nlohmann")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|amd64|x86_64|x64")
  target_compile_definitions(nam-ir-comparison PRIVATE FFTCONVOLVER_USE_SSE)
endif()
