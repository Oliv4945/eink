#!/bin/bash
# Build and configure ESP8266 toolchain to compile Sprite_tm espeink project
# Initial project http://git.spritesserver.nl/espeink.git/
# Associated documentation https://spritesmods.com/?art=einkdisplay

# Get current directory
current_dir=`pwd`

# *********	Install ESP SDK	& tools		**********
# ESP-OPEN-SDK
mkdir tools
cd tools
git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
make STANDALONE=n VENDOR_SDK=1.0.1
# Revert open sdk patch
cp esp_iot_sdk_v1.0.1/include/c_types.h.orig esp_iot_sdk_v1.0.1/include/c_types.h
cd ..

# esptool-ck
git clone https://github.com/igrr/esptool-ck.git
cd esptool-ck
make
cd ../..

# Build espeink
# Init submodule if not done by git clone --recursive
git submodule init
git submodule update

# Update Makefile
# Paths
sed -i "s!^XTENSA_TOOLS_ROOT.*!XTENSA_TOOLS_ROOT ?= $current_dir/tools/esp-open-sdk/xtensa-lx106-elf/bin!" Makefile
sed -i "s!^SDK_EXTRA_INCLUDES.*!SDK_EXTRA_INCLUDES ?= $current_dir/tools/esp-open-sdk/sdk/include!" Makefile
sed -i "s!^SDK_EXTRA_LIBS.*!SDK_EXTRA_LIBS ?= $current_dir/tools/esp-open-sdk/sdk/lib!" Makefile
sed -i "s!^SDK_BASE.*!SDK_BASE ?= $current_dir/tools/esp-open-sdk/sdk!" Makefile
sed -i "s!^ESPTOOL.*!ESPTOOL ?= $current_dir/tools/esptool-ck/esptool!" Makefile

# Use open SDK symbol, to avoid undeclared types errors
sed -i '/-Wno-address/s/$/ -DUSE_OPENSDK/' Makefile

# Make project
make
make webpages.espfs
