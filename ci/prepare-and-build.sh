#!/bin/bash

echo "### Prepare environemnt"

mkdir -p sdk
mkdir -p dist

pushd sdk
wget https://github.com/kwrx/aplus-toolchain/releases/latest/download/x86_64-aplus-toolchain-x86_64-linux-gnu.tar.xz
tar xJf x86_64-aplus-toolchain-x86_64-linux-gnu.tar.xz
popd

git clone https://www.github.com/kwrx/aplus --depth=1

export PATH=$(pwd)/sdk/bin:$PATH


echo "### Build"

make CC=x86_64-aplus-gcc DESTDIR=$(pwd)/dist -j$(nproc) install || exit 1


echo "### Package"

tar -cJf nesemu.tar.xz -C dist .
