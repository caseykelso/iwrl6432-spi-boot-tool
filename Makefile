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

CMAKE.BIN=/usr/bin/cmake
BUILD.DIR=$(BASE.DIR)/build
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
HEADERS.DIR=$(BASE.DIR)/ti_headers

ci: firmware firmware.convert.appimage.to.hex ti.headers libmpsse build

ti.headers: .FORCE
	mkdir -p $(INSTALLED.HOST.DIR)/include/kernel/dpl && cp $(HEADERS.DIR)/DebugP.h $(INSTALLED.HOST.DIR)/include/kernel/dpl
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers && cp $(HEADERS.DIR)/gpio.h $(INSTALLED.HOST.DIR)/include/drivers
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers && cp $(HEADERS.DIR)/crc.h $(INSTALLED.HOST.DIR)/include/drivers
	mkdir -p $(INSTALLED.HOST.DIR)/include/kernel/dpl && cp $(HEADERS.DIR)/AddrTranslateP.h $(INSTALLED.HOST.DIR)/include/kernel/dpl
	mkdir -p $(INSTALLED.HOST.DIR)/include/drivers/hw_include && cp $(HEADERS.DIR)/soc_config.h $(INSTALLED.HOST.DIR)/include/drivers/hw_include

build: .FORCE
	rm -rf $(BUILD.DIR) && mkdir -p $(BUILD.DIR)
	cd $(BUILD.DIR) && $(CMAKE.BIN) -DCMAKE_PREFIX_PATH=$(INSTALLED.HOST.DIR) $(SOURCE.DIR) && make

ftdi.d2xx: .FORCE
	rm -rf $(FTDI.D2XX.DIR)
	rm -f $(DOWNLOADS.DIR)/$(FTDI.D2XX.ARCHIVE)
	mkdir -p $(INSTALLED.HOST.DIR)/lib
	mkdir -p $(INSTALLED.HOST.DIR)/linclude
	mkdir -p $(DOWNLOADS.DIR)
	mkdir -p $(FTDI.D2XX.DIR)
	cd $(DOWNLOADS.DIR) && wget $(FTDI.D2XX.URL) && cd $(FTDI.D2XX.DIR) && tar xvf ../$(FTDI.D2XX.ARCHIVE)
	cd $(FTDI.D2XX.DIR)/release/build && cp libftd2xx.so.1.4.27 $(INSTALLED.HOST.DIR)/lib && ln -sf libftd2xx.so.$(FTDI.D2XX.VERSION) $(INSTALLED.HOST.DIR)/lib/libftd2xx.so


ftdi.libmpsse: .FORCE
	rm -rf $(FTDI.LIBMPSSE.DIR)
	rm -f $(DOWNLOADS.DIR)/$(FTDI.LIBMPSSE.ARCHIVE)
	mkdir -p $(INSTALLED.HOST.DIR)/lib
	mkdir -p $(INSTALLED.HOST.DIR)/linclude
	mkdir -p $(DOWNLOADS.DIR)
	cd $(DOWNLOADS.DIR) && wget $(FTDI.LIBMPSSE.URL) && unzip $(FTDI.LIBMPSSE.ARCHIVE)
	cd $(FTDI.LIBMPSSE.DIR)/Linux && tar xvf $(FTDI.LIBMPSSE.LINUX.X86.ARCHIVE) && cp release/build/libmpsse.so.$(FTDI.LIBMPSSE.VERSION) $(INSTALLED.HOST.DIR)/lib && ln -sf libmpsse.so.$(FTDI.LIBMPSSE.VERSION) $(INSTALLED.HOST.DIR)/lib/libmpsse.so && cp release/build/libmpsse_spi.h release/libftd2xx/*.h $(INSTALLED.HOST.DIR)/include



libmpsse: .FORCE
	mkdir -p $(DOWNLOADS.DIR)
	rm -rf $(DOWNLOADS.DIR)/libmpsse
	cd $(DOWNLOADS.DIR) && git clone git@github.com:caseykelso/libmpsse.git -b master
	cd $(DOWNLOADS.DIR)/libmpsse/src && CFLAGS=-I/usr/include/libftdi1 ./configure --disable-python --prefix=$(INSTALLED.HOST.DIR) && make -j8 && make install

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
