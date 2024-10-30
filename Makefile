SHELL=/bin/bash

ifndef J_OVERRIDE
J=$(shell nproc --all)
else
J=$(J_OVERRIDE)
endif
$(info building with $(J) threads)

BASE.DIR=$(PWD)
SOURCE.DIR=$(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
DOWNLOADS.DIR=$(BASE.DIR)/downloads
SCRIPTS.DIR=$(BASE.DIR)/scripts
ifndef INSTALLED_HOST_DIR
INSTALLED.HOST.DIR=$(BASE.DIR)/installed.host
else
INSTALLED.HOST.DIR=$(INSTALLED_HOST_DIR)
endif

CMAKE.BIN=/usr/bin/cmake

APP.BUILD=$(BASE.DIR)/build.app
GPIO.BIN=$(INSTALLED.HOST.DIR)/bin/gpio-ftdi
SPI.BIN=$(INSTALLED.HOST.DIR)/bin/iwrflasher-spi

FTDI.LIBMPSSE.VERSION=1.0.5
FTDI.LIBMPSSE.PREFIX=LibMPSSE_$(FTDI.LIBMPSSE.VERSION)
FTDI.LIBMPSSE.ARCHIVE=$(FTDI.LIBMPSSE.PREFIX).zip
FTDI.LIBMPSSE.LINUX.X86.ARCHIVE=libmpsse-x86_64-$(FTDI.LIBMPSSE.VERSION).tgz
FTDI.LIBMPSSE.URL=https://buildroot-sources.s3.amazonaws.com/$(FTDI.LIBMPSSE.ARCHIVE)
FTDI.LIBMPSSE.DIR=$(DOWNLOADS.DIR)/$(FTDI.LIBMPSSE.PREFIX)

FTDI.D2XX.VERSION=1.4.27
FTDI.D2XX.PREFIX=libftd2xx-x86_64-$(FTDI.D2XX.VERSION)
FTDI.D2XX.ARCHIVE=$(FTDI.D2XX.PREFIX).tgz
FTDI.D2XX.URL=https://buildroot-sources.s3.amazonaws.com/$(FTDI.D2XX.ARCHIVE)
FTDI.D2XX.DIR=$(DOWNLOADS.DIR)/$(FTDI.D2XX.PREFIX)

FIRMWARE.PREBUILT.FILENAME=motion_and_presence_detection_demo.debug.appimage
FIRMWARE.PREBUILT.URL=https://buildroot-sources.s3.us-east-1.amazonaws.com/$(FIRMWARE.PREBUILT.FILENAME)
FIRMWARE.PREBUILT.PATH=$(DOWNLOADS.DIR)/$(FIRMWARE.PREBUILT.FILENAME)

FIRMWARE.BIN=$(FIRMWARE.PREBUILT.PATH)
APPIMAGE.SCRIPT=$(SCRIPTS.DIR)/appimageToHex.py

ci: firmware firmware.convert.appimage.to.hex app

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
	rm -rf $(APP.BUILD)
	rm -rf $(DOWNLOADS.DIR)

.FORCE:
