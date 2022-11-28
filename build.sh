#!/usr/bin/env bash

BUILD_DIR=cmake-build

BLUE="\033[0;34m"
NC="\033[0m"

if [[ "$1" == "clean" ]]; then
  echo -e "${BLUE}==== clean ====${NC}"
  rm -rf $BUILD_DIR
  rm spectator/*.inc
  rm spectator/netflix_config.cc
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  export CC=gcc-11
  export CXX=g++-11
fi

if [[ ! -d $BUILD_DIR ]]; then
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo -e "${BLUE}==== configure default profile ====${NC}"
    conan profile new default --detect
    conan profile update settings.compiler.libcxx=libstdc++11 default
  fi

  echo -e "${BLUE}==== install required dependencies ====${NC}"
  conan install . --build=missing --install-folder $BUILD_DIR

  echo -e "${BLUE}==== install source dependencies ====${NC}"
  if [[ "$NFLX_INTERNAL" == "ON" ]]; then
    NFLX_INTERNAL=ON conan source .
  else
    conan source .
  fi
fi

pushd $BUILD_DIR || exit 1

echo -e "${BLUE}==== generate build files ====${NC}"
# Choose: Debug, Release, RelWithDebInfo and MinSizeRel
if [[ "$NFLX_INTERNAL" == "ON" ]]; then
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DNFLX_INTERNAL=ON || exit 1
else
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DNFLX_INTERNAL=OFF || exit 1
fi

echo -e "${BLUE}==== build ====${NC}"
cmake --build . || exit 1

if [[ "$1" != "skiptest" ]]; then
  echo -e "${BLUE}==== test ====${NC}"
  GTEST_COLOR=1 ctest --verbose
fi

popd || exit 1
