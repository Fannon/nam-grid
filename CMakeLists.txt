cmake_minimum_required(VERSION 3.20)
# macOS releases run on macOS 11, the first Apple Silicon release. This must precede project():
# afterwards the cache entry exists, iPlug2's default no longer applies, and the build
# machine's SDK version becomes the minimum.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(nam-grid LANGUAGES C CXX)
set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/iPlug2")
set(IPLUG2_CXX_STANDARD 20)
set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "Deploy plugins after building")
enable_testing()
add_subdirectory(NeuralAmpModeler)
