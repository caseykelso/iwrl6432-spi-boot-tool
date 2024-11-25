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

toolchain.waffle: submodule
	mkdir -p $(TOOLCHAIN.DIR)
	cd $(TOOLCHAIN.DIR) && wget $(AMBA.TOOLCHAIN.URL)

ci: firmware firmware.convert.appimage.to.hex build.waffle build.x86

run: .FORCE
	$(BUILD.DIR)/xwrflasher

gdb: .FORCE
	gdb $(BUILD.DIR)/xwrflasher

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
