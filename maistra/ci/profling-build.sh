#!/bin/bash

set -e
set -o pipefail
set +x

# shellcheck disable=SC1091
source /opt/rh/gcc-toolset-9/enable

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1090
source "${DIR}/common.sh"

# Override the flags:
# - do not constrain to two jobs.
# - flags specific to profiling.
COMMON_FLAGS="\
    --incompatible_linkopts_to_linklibs \
    --config=release-symbol \
    --config=${ARCH} \
     --compilation_mode=opt --cxxopt=-g --cxxopt=-ggdb3 \
    --disk_cache=/bazel-cache \
"

# Build WASM extensions first
CC=clang CXX=clang++ bazel_build \
  //extensions:stats.wasm \
  //extensions:metadata_exchange.wasm \
  //extensions:attributegen.wasm
CC=cc CXX=g++ bazel_build @envoy//test/tools/wee8_compile:wee8_compile_tool

CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/stats.wasm bazel-bin/extensions/stats.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/metadata_exchange.wasm bazel-bin/extensions/metadata_exchange.compiled.wasm
CC=clang CXX=clang++ bazel-bin/external/envoy/test/tools/wee8_compile/wee8_compile_tool bazel-bin/extensions/attributegen.wasm bazel-bin/extensions/attributegen.compiled.wasm

echo "WASM extensions built succesfully. Now building envoy binary."

# Build Envoy
CC=cc CXX=g++ bazel_build //src/envoy:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/src/envoy/envoy --version

#bazel-bin/src/envoy/envoy 
