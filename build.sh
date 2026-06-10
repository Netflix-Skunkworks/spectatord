#!/usr/bin/env bash

set -e

# usage: ./build.sh [clean|clean --confirm|skipsource|skiptest]

if [[ -z "$BUILD_DIR" ]]; then
  BUILD_DIR="cmake-build"
fi

if [[ -z "$BUILD_TYPE" ]]; then
  # Choose: Debug, Release, RelWithDebInfo and MinSizeRel. Use Debug for asan checking locally.
  BUILD_TYPE="Debug"
fi

BLUE="\033[0;34m"
RED="\033[0;31m"
NC="\033[0m"

if [[ "$1" == "clean" ]]; then
  echo -e "${BLUE}==== clean ====${NC}"
  rm -rf "$BUILD_DIR"
  # extracted and generated files
  rm -f metatron/auth_context.pb.cc
  rm -f metatron/auth_context.pb.h
  rm -f metatron/auth_context.proto
  rm -f metatron/metatron_config.cc
  rm -rf ska
  rm -f spectator/*.inc
  rm -f spectator/registry/netflix_config.cc
  if [[ "$2" == "--confirm" ]]; then
    # remove all packages from the conan cache, to allow swapping between Release/Debug builds
    conan remove "*" --confirm
  fi
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  if ! command -v gcc-15 &> /dev/null; then
    echo -e "${RED}ERROR: gcc-15 is required but not found${NC}"
    exit 1
  fi
  if ! command -v g++-15 &> /dev/null; then
    echo -e "${RED}ERROR: g++-15 is required but not found${NC}"
    exit 1
  fi
  export CC=gcc-15
  export CXX=g++-15
fi

if [[ ! -f "$HOME/.conan2/profiles/default" ]]; then
  echo -e "${BLUE}==== create default profile ====${NC}"
  conan profile detect
fi

## Modify the default profile to set the compiler version and C++ standard
DEFAULT_PROFILE="$HOME/.conan2/profiles/default"
sed -i.bak -E \
  -e 's/^compiler\.version=.*/compiler.version=15.2/' \
  -e 's/^compiler\.cppstd=.*/compiler.cppstd=26/' \
  "$DEFAULT_PROFILE"
rm -f "$DEFAULT_PROFILE.bak"


if [[ ! -d $BUILD_DIR ]]; then
  echo -e "${BLUE}==== install required dependencies ====${NC}"
  # build all library dependencies from source; pre-built binaries from Conan
  # Center may be linked against a newer glibc than the build environment
  # provides. ninja is excluded: it is a host-only build tool that never links
  # into spectatord, and building it from source triggers an LTO internal
  # compiler error under the RHEL/Rocky gcc-toolset, so we use a prebuilt ninja.
  # use the system cmake to avoid the same glibc issue with Conan's cmake.
  if [[ "$BUILD_TYPE" == "Debug" ]]; then
    conan install . --output-folder="$BUILD_DIR" --build="*" --build="!ninja/*" --settings=build_type="$BUILD_TYPE" --profile=./sanitized \
      -c tools.cmake:cmake_program="$(which cmake)"
  else
    conan install . --output-folder="$BUILD_DIR" --build="*" --build="!ninja/*" \
      -c tools.cmake:cmake_program="$(which cmake)"
  fi

  # this switch is necessary for internal centos builds
  if [[ "$1" != "skipsource" ]]; then
    echo -e "${BLUE}==== install source dependencies ====${NC}"
    conan source .
  fi
fi

pushd "$BUILD_DIR"

echo -e "${BLUE}==== configure conan environment to access tools ====${NC}"
source conanbuild.sh

if [[ $OSTYPE == "darwin"* ]]; then
  export MallocNanoZone=0
fi

echo -e "${BLUE}==== generate build files ====${NC}"
if [[ "$NFLX_INTERNAL" != "ON" ]]; then
  NFLX_INTERNAL=OFF
fi
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DNFLX_INTERNAL="$NFLX_INTERNAL"

echo -e "${BLUE}==== build ====${NC}"
cmake --build . --parallel

if [[ "$1" != "skiptest" ]]; then
  echo -e "${BLUE}==== test ====${NC}"
  GTEST_COLOR=1 ctest --verbose
fi

popd
