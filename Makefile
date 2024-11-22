SHELL=/bin/bash

ifndef J_OVERRIDE
J=$(shell nproc --all)
else
J=$(J_OVERRIDE)
endif
$(info building with $(J) threads)

BASE.DIR=$(PWD)
SOURCE.DIR=$(BASE.DIR)/source
DOWNLOADS.DIR=$(BASE.DIR)/downloads
SCRIPTS.DIR=$(BASE.DIR)/scripts
ifndef INSTALLED_HOST_DIR
INSTALLED.HOST.DIR=$(BASE.DIR)/installed.host
else
INSTALLED.HOST.DIR=$(INSTALLED_HOST_DIR)
endif
TOOLCHAIN.DIR=$(BASE.DIR)/toolchain

CMAKE.TOOLCHAIN.FILE.RPI4=$(BASE.DIR)/aarch64-linux.cmake
CMAKE.TOOLCHAIN.FILE.WAFFLE=$(BASE.DIR)/waffle-linux.cmake

CMAKE.BIN=/usr/bin/cmake
BUILD.DIR=$(BASE.DIR)/build
GPIO.BIN=$(INSTALLED.HOST.DIR)/bin/gpio-ftdi
SPI.BIN=$(INSTALLED.HOST.DIR)/bin/iwrflasher-spi

FIRMWARE.PREBUILT.FILENAME=motion_and_presence_detection_demo.debug.appimage
FIRMWARE.PREBUILT.URL=https://buildroot-sources.s3.us-east-1.amazonaws.com/$(FIRMWARE.PREBUILT.FILENAME)
FIRMWARE.PREBUILT.PATH=$(DOWNLOADS.DIR)/$(FIRMWARE.PREBUILT.FILENAME)

GPIOD.VERSION=2.2
GPIOD.ARCHIVE=libgpiod-$(GPIOD.VERSION).tar.gz
GPIOD.URL=https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/snapshot/$(GPIOD.ARCHIVE)
GPIOD.DIR=$(DOWNLOADS.DIR)/gpiod

FIRMWARE.BIN=$(FIRMWARE.PREBUILT.PATH)
APPIMAGE.SCRIPT=$(SCRIPTS.DIR)/appimageToHex.py
HEADERS.DIR=$(BASE.DIR)/ti_headers

AMBA.TOOLCHAIN.ARCHIVE=linaro-aarch64-2020.09-gcc10.2-linux5.4-x86_64.tar.xz
AMBA.TOOLCHAIN.URL=https://buildroot-sources.s3.amazonaws.com/$(AMBA.TOOLCHAIN.ARCHIVE)

CRC32.PREFIX=crc32-11
CRC32.DIR=$(DOWNLOADS.DIR)/$(CRC32.PREFIX)
CRC32.BUILD=$(DOWNLOADS.DIR)/build.$(CRC32.PREFIX)

CRC32C.DIR=$(DOWNLOADS.DIR)/crc32c
CRC32C.BUILD=$(DOWNLOADS.DIR)/build.crc32c

toolchain.waffle: submodule
	mkdir -p $(TOOLCHAIN.DIR)
	cd $(TOOLCHAIN.DIR) && wget $(AMBA.TOOLCHAIN.URL)

ci: toolchain.waffle firmware firmware.convert.appimage.to.hex build.waffle crc32 build.x86

run: .FORCE
	$(BUILD.DIR)/xwrflasher

crc32c: .FORCE
	rm -rf $(CRC32C.DIR)
	rm -rf $(CRC32C.BUILD)
	mkdir -p $(CRC32C.BUILD)
	cd $(DOWNLOADS.DIR) && git clone --recurse-submodules -j8 git@github.com:google/crc32c.git
	cd $(CRC32C.BUILD) && $(CMAKE.BIN) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) -DCMAKE_INSTALL_PREFIX=$(INSTALLED.HOST.DIR) $(CRC32C.DIR) && make install

crc32: .FORCE
	rm -rf $(CRC32.DIR)
	rm -rf $(CRC32.BUILD)
	mkdir -p $(CRC32.BUILD)
	cd $(DOWNLOADS.DIR) && git clone git@github.com:caseykelso/crc32-11.git -b dev
	cd $(CRC32.BUILD) && $(CMAKE.BIN) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) -DCMAKE_INSTALL_PREFIX=$(INSTALLED.HOST.DIR) $(CRC32.DIR) && make install

gpiod: .FORCE
	rm -rf $(GPIOD.DIR)
	mkdir -p $(DOWNLOADS.DIR)
	cd $(DOWNLOADS.DIR) && wget $(GPIOD.URL) && tar xvf $(GPIOD.ARCHIVE)

ti.headers: .FORCE
	mkdir -p $(INSTALLED.HOST.DIR)/include/kernel/dpl && cp $(HEADERS.DIR)/DebugP.h $(INSTALLED.HOST.DIR)/include/kernel/dpl
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers && cp $(HEADERS.DIR)/gpio.h $(INSTALLED.HOST.DIR)/include/drivers
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers && cp $(HEADERS.DIR)/crc.h $(INSTALLED.HOST.DIR)/include/drivers
	mkdir -p $(INSTALLED.HOST.DIR)/include/kernel/dpl && cp $(HEADERS.DIR)/AddrTranslateP.h $(INSTALLED.HOST.DIR)/include/kernel/dpl
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers/hw_include && cp $(HEADERS.DIR)/soc_config.h $(INSTALLED.HOST.DIR)/include/drivers/hw_include

build.x86: .FORCE
	rm -rf $(BUILD.DIR) && mkdir -p $(BUILD.DIR)
	cd $(BUILD.DIR) && $(CMAKE.BIN) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) $(SOURCE.DIR) && make VERBOSE=1

build.rpi4: .FORCE
	rm -rf $(BUILD.DIR) && mkdir -p $(BUILD.DIR)
	cd $(BUILD.DIR) && $(CMAKE.BIN) -DCMAKE_TOOLCHAIN_FILE=$(CMAKE.TOOLCHAIN.FILE.RPI4) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) $(SOURCE.DIR) && VERBOSE=1 make

build.waffle: .FORCE
	rm -rf $(BUILD.DIR) && mkdir -p $(BUILD.DIR)
	cd $(BUILD.DIR) && $(CMAKE.BIN) -DCMAKE_TOOLCHAIN_FILE=$(CMAKE.TOOLCHAIN.FILE.WAFFLE) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) $(SOURCE.DIR) && VERBOSE=1 make

firmware: .FORCE
	rm -f $(FIRMWARE.PREBUILT.PATH)
	mkdir -p $(DOWNLOADS.DIR)
	cd $(DOWNLOADS.DIR) && wget $(FIRMWARE.PREBUILT.URL)
	mkdir -p $(INSTALLED.HOST.DIR)/include

firmware.convert.appimage.to.hex: .FORCE
	cd $(INSTALLED.HOST.DIR)/include && python3 $(APPIMAGE.SCRIPT) $(FIRMWARE.PREBUILT.PATH)

ctags: .FORCE
	cd $(BASE.DIR) && ctags -R --exclude=.git --exclude=downloads --exclude=installed.host --exclude=installed.target --exclude=documents  --exclude=build.*  .

clean: .FORCE
	rm -rf $(INSTALLED.HOST.DIR)
	rm -rf $(BUILD.DIR)
	rm -rf $(DOWNLOADS.DIR)

.FORCE:
