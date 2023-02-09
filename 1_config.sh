#!/bin/bash -e

cmake -S . -B b -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$PWD/i -DCMAKE_INSTALL_PREFIX=$PWD/i "$@"
