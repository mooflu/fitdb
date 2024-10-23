#!/bin/bash -e
mkdir -p build
pushd build
cmake -DBUILD_SHARED_LIBS=ON ..
cmake --build . --parallel --verbose
popd

file ./build/src/fitdb

