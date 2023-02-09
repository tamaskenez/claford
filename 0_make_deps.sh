#!/bin/bash -e

rm -rf deps
mkdir deps
# git clone https://github.com/emcrisostomo/fswatch --depth 1 deps/fswatch
git clone https://github.com/tamaskenez/fswatch deps/fswatch/s
for build_type in Debug Release; do
    cmake -S deps/fswatch/s -B deps/fswatch/b -DCMAKE_DEBUG_POSTFIX=_d -DCMAKE_BUILD_TYPE=$build_type
    cmake --build deps/fswatch/b --target install --config $build_type -j
    conan install -b missing -g cmake_find_package_multi -if $PWD/i/cmake -s build_type=$build_type .
done


