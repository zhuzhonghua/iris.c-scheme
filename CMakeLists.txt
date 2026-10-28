cmake_minimum_required(VERSION 3.20)

# ============================================================================
# iris-c -- workspace gluing iris.c (https://github.com/antirez/iris.c) to
# dummyscheme (https://github.com/zhuzhonghua/dummyscheme).
#
# Rules:
#   1. Never modify or copy upstream sources; reference them via SCHEME_DIR /
#      IRIS_DIR.
#   2. All new code, build output, scripts and docs live in this directory.
#   3. dummyscheme is compiled as C++98 (upstream constraint); iris.c as C99.
#   4. The VM must never be built with -ffast-math / -march=native.
#
# Usage:
#   cmake -S . -B build \
#     -DIRIS_DIR=/path/to/iris.c \
#     -DSCHEME_DIR=/path/to/dummyscheme \
#     -DIRIS_BACKEND=mps
#   cmake --build build -j
#   ./build/iris-scm /path/to/iris.c/flux-klein-4b test.scm
#
# Options (either -D or an environment variable of the same name):
#   IRIS_DIR=/path/to/iris.c            required
#   SCHEME_DIR=/path/to/dummyscheme     required
#   IRIS_BACKEND=auto|mps|blas|generic  default: auto
#   IRIS_NATIVE=ON|OFF                  default: ON
#
# The iris core is compiled here (same sources, same flags as upstream) instead
# of calling "make lib" inside IRIS_DIR: that target only builds the portable C
# core, while the Metal objects exist only in the "mps-build" target, and make
# would write .o files into the upstream tree.
# ============================================================================

project(iris.c-scheme LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Upstream roots: -D cache variable or environment variable, no local default.
set(SCHEME_DIR "$ENV{SCHEME_DIR}"
  CACHE PATH "dummyscheme root (env SCHEME_DIR)")
set(IRIS_DIR "$ENV{IRIS_DIR}"
  CACHE PATH "iris.c root (env IRIS_DIR)")
set(IRIS_BACKEND "auto"
  CACHE STRING "iris backend: auto | mps | blas | generic")
set(IRIS_NATIVE ON
  CACHE BOOL "Add -march=native to the iris core (never to the VM)")

if(NOT SCHEME_DIR)
  message(FATAL_ERROR
    "SCHEME_DIR is not set.\n"
    "Pass -DSCHEME_DIR=/path/to/dummyscheme (or export SCHEME_DIR).")
endif()
if(NOT IRIS_DIR)
  message(FATAL_ERROR
    "IRIS_DIR is not set.\n"
    "Pass -DIRIS_DIR=/path/to/iris.c (or export IRIS_DIR).")
endif()
if(NOT EXISTS "${SCHEME_DIR}/vm.cpp")
  message(FATAL_ERROR
    "Invalid SCHEME_DIR: ${SCHEME_DIR}\n"
    "It must be the dummyscheme root (containing vm.cpp).")
endif()
if(NOT EXISTS "${IRIS_DIR}/iris.c")
  message(FATAL_ERROR
    "Invalid IRIS_DIR: ${IRIS_DIR}\n"
    "It must be the iris.c root (containing iris.c).")
endif()

# ---------------------------------------------------------------------------
# Backend selection
# ---------------------------------------------------------------------------
if(IRIS_BACKEND STREQUAL "auto")
  if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(IRIS_BACKEND mps)
  elseif(APPLE)
    set(IRIS_BACKEND blas)
  else()
    set(IRIS_BACKEND generic)
  endif()
endif()
message(STATUS "iris backend: ${IRIS_BACKEND}")
message(STATUS "iris dir:     ${IRIS_DIR}")
message(STATUS "scheme dir:   ${SCHEME_DIR}")

# ---------------------------------------------------------------------------
# dummyscheme core (C++98)
# ---------------------------------------------------------------------------
set(SCHEME_CORE_FILES
  vm.cpp scmapi.cpp scmbasic.cpp scmmath.cpp scmstring.cpp
  scmcompiler.cpp scmport.cpp scmvector.cpp scmtable.cpp
  scminit.cpp scmdebug.cpp)

set(SCHEME_CORE_SRCS)
foreach(f IN LISTS SCHEME_CORE_FILES)
  list(APPEND SCHEME_CORE_SRCS "${SCHEME_DIR}/${f}")
endforeach()

add_library(scheme_core STATIC ${SCHEME_CORE_SRCS})
target_include_directories(scheme_core PUBLIC "${SCHEME_DIR}")
set_target_properties(scheme_core PROPERTIES
  CXX_STANDARD 98
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
target_compile_options(scheme_core PRIVATE -O2 -g -fexceptions -frtti)

# ---------------------------------------------------------------------------
# iris core (C99)
# ---------------------------------------------------------------------------
set(IRIS_CORE_FILES
  iris.c iris_kernels.c iris_tokenizer.c iris_vae.c
  iris_transformer_flux.c iris_transformer_zimage.c iris_transformer_flux3.c
  iris_sample.c iris_image.c jpeg.c iris_safetensors.c
  iris_qwen3.c iris_qwen3_tokenizer.c terminals.c)

set(IRIS_CORE_SRCS)
foreach(f IN LISTS IRIS_CORE_FILES)
  list(APPEND IRIS_CORE_SRCS "${IRIS_DIR}/${f}")
endforeach()

add_library(iris_core STATIC ${IRIS_CORE_SRCS})
target_include_directories(iris_core PUBLIC
  "${IRIS_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/gen")
target_compile_options(iris_core PRIVATE -O3 -g -Wall -Wextra)
if(IRIS_NATIVE)
  target_compile_options(iris_core PRIVATE -march=native)
endif()

# Same as upstream: iris may use -ffast-math, the VM may not.
target_compile_options(iris_core PRIVATE -ffast-math)

if(IRIS_BACKEND STREQUAL "mps")
  if(NOT APPLE)
    message(FATAL_ERROR "The mps backend is macOS only")
  endif()
  enable_language(OBJC)

  # Compile the Metal shaders at runtime from an embedded source blob,
  # matching the upstream Makefile.
  set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
  set(SHADER_HDR "${GEN_DIR}/iris_shaders_source.h")
  add_custom_command(
    OUTPUT "${SHADER_HDR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GEN_DIR}"
    COMMAND sh -c "xxd -i < '${IRIS_DIR}/iris_shaders.metal' | sed 's/stdin/iris_shaders_metal/g' > '${SHADER_HDR}'"
    DEPENDS "${IRIS_DIR}/iris_shaders.metal"
    VERBATIM)
  add_custom_target(iris_shaders_gen DEPENDS "${SHADER_HDR}")

  target_sources(iris_core PRIVATE
    "${IRIS_DIR}/iris_metal.m"
    "${IRIS_DIR}/iris_dev_metal.m")
  add_dependencies(iris_core iris_shaders_gen)

  set_source_files_properties(
    "${IRIS_DIR}/iris_metal.m"
    "${IRIS_DIR}/iris_dev_metal.m"
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

  target_compile_definitions(iris_core PUBLIC
    USE_BLAS USE_METAL IRIS_DEV ACCELERATE_NEW_LAPACK)
  target_link_libraries(iris_core PUBLIC
    "-framework Accelerate"
    "-framework Metal"
    "-framework MetalPerformanceShaders"
    "-framework MetalPerformanceShadersGraph"
    "-framework Foundation")

elseif(IRIS_BACKEND STREQUAL "blas")
  if(APPLE)
    target_compile_definitions(iris_core PUBLIC
      USE_BLAS ACCELERATE_NEW_LAPACK)
    target_link_libraries(iris_core PUBLIC "-framework Accelerate")
  else()
    target_compile_definitions(iris_core PUBLIC
      USE_BLAS USE_OPENBLAS)
    target_link_libraries(iris_core PUBLIC openblas)
  endif()

elseif(IRIS_BACKEND STREQUAL "generic")
  target_compile_definitions(iris_core PUBLIC GENERIC_BUILD)

else()
  message(FATAL_ERROR "Unknown IRIS_BACKEND: ${IRIS_BACKEND}")
endif()

# ---------------------------------------------------------------------------
# Bridge layer (this directory)
# ---------------------------------------------------------------------------
add_executable(iris-scm
  main.cpp
  iris_scm.cpp)

target_include_directories(iris-scm PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(iris-scm PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)

target_link_libraries(iris-scm PRIVATE scheme_core iris_core)

if(NOT APPLE)
  target_link_libraries(iris-scm PRIVATE m)
endif()
