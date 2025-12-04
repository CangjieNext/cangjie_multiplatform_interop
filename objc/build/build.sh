#!/bin/bash

export DYLD_LIBRARY_PATH=/opt/homebrew/opt/openssl/lib:$DYLD_LIBRARY_PATH
XCODE_DEVELOPER_DIR=/Applications/Xcode-15.4.0.app/Contents/Developer
XCODE_TOOLCHAIN_BIN_DIR=$XCODE_DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin
XCODE_PLATFORMS_DIR=$XCODE_DEVELOPER_DIR/Platforms
INSTALL_DIR=install

python3 build.py build
python3 build.py install --prefix "$INSTALL_DIR"
python3 build.py build --target darwin_aarch64
python3 build.py install --target darwin_aarch64 --prefix "$INSTALL_DIR"
python3 build.py \
  build \
  --target ios_aarch64 \
  --target-lib ios-aarch64 \
  --target-toolchain "$XCODE_TOOLCHAIN_BIN_DIR" \
  --target-sysroot "$XCODE_PLATFORMS_DIR/iPhoneOS.platform/Developer/SDKs/iPhoneOS17.5.sdk"
python3 build.py install --target ios_aarch64 --prefix "$INSTALL_DIR"
python3 build.py \
  build \
  --target ios_simulator_aarch64 \
  --target-lib ios-simulator-aarch64 \
  --target-toolchain "$XCODE_TOOLCHAIN_BIN_DIR" \
  --target-sysroot "$XCODE_PLATFORMS_DIR/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator17.5.sdk"
python3 build.py install --target ios_simulator_aarch64 --prefix "$INSTALL_DIR"
