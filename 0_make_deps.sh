#!/bin/bash -e

rm -rf deps
mkdir deps

# git clone https://github.com/emcrisostomo/fswatch --depth 1 deps/fswatch/s
git clone https://github.com/tamaskenez/fswatch deps/fswatch/s # using fork until my PR gets merged

git clone https://github.com/boostorg/nowide.git deps/nowide/s --branch standalone --depth 1
git clone https://github.com/cameron314/readerwriterqueue.git deps/readerwriterqueue/s --depth 1

for build_type in Debug Release; do
    cmake -S deps/fswatch/s -B deps/fswatch/b -DCMAKE_INSTALL_PREFIX=$PWD/i -DCMAKE_DEBUG_POSTFIX=_d -DCMAKE_BUILD_TYPE=$build_type
    cmake --build deps/fswatch/b --target install --config $build_type -j

    cmake -S deps/nowide/s -B deps/nowide/b -DCMAKE_INSTALL_PREFIX=$PWD/i -DCMAKE_DEBUG_POSTFIX=_d -DCMAKE_BUILD_TYPE=$build_type -DBUILD_TESTING=0
    cmake --build deps/nowide/b --target install --config $build_type -j
done

cmake -S deps/readerwriterqueue/s -B deps/readerwriterqueue/b -DCMAKE_INSTALL_PREFIX=$PWD/i
cmake --build deps/readerwriterqueue/b --target install

conan install -b missing -g cmake_find_package_multi -if $PWD/i/cmake -s build_type=Debug .
conan install -b missing -g cmake_find_package_multi -if $PWD/i/cmake -s build_type=Release .
