#!/bin/bash -e

conan install . -b missing -if i/cmake -of of -pr:b=default -s build_type=Debug
conan install . -b missing -if i/cmake -of of -pr:b=default -s build_type=Release

rm -rf deps
mkdir deps

# git clone https://github.com/emcrisostomo/fswatch --depth 1 deps/fswatch/s
git clone https://github.com/tamaskenez/fswatch deps/fswatch/s # using fork until my PR gets merged
git clone https://github.com/boostorg/nowide.git deps/nowide/s --branch standalone --depth 1
git clone https://github.com/ocornut/imgui deps/imgui/s --branch v1.89.3 --depth 1

for build_type in Debug Release; do
    cmake -S deps/fswatch/s -B deps/fswatch/b -DCMAKE_INSTALL_PREFIX=$PWD/i -DCMAKE_DEBUG_POSTFIX=_d -DCMAKE_BUILD_TYPE=$build_type
    cmake --build deps/fswatch/b --target install --config $build_type -j

    cmake -S deps/nowide/s -B deps/nowide/b -DCMAKE_INSTALL_PREFIX=$PWD/i -DCMAKE_DEBUG_POSTFIX=_d -DCMAKE_BUILD_TYPE=$build_type -DBUILD_TESTING=0
    cmake --build deps/nowide/b --target install --config $build_type -j

    cmake -S cmake/imgui -B deps/imgui/b -DCMAKE_INSTALL_PREFIX=$PWD/i -DIMGUI_DIR=${PWD}/deps/imgui/s -DCMAKE_BUILD_TYPE=$build_type 
    cmake --build deps/imgui/b --target install --config $build_type -j
done

