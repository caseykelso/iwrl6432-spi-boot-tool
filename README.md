# TI IWRL6432 Tooling to Boot from SPI without a QSPI Flash by transferring the firmware over SPI to RAM and booting
This example uses a Raspberry PI 4 (RPI4) as the SPI Controller.

## Setup Ubuntu 24.04 Build Environment - Build for RPI4 Raspbian
Update the contents of your /etc/apt/sources.list to match the sources.list in this repo.

```bash
sudo dpkg --add-architecture arm64
sudo dpkg --print-foreign-architectures
sudo apt update -y
sudo apt install git build-essential cmake libdocopt-dev libuv1-dev flex bison libgtest-dev spi-tools gcc-aarch64-linux-gnu g++-aarch64-linux-gnu libgpiod-dev libgpiod-dev:arm64 
```

## Update the Raspbian environemnt on your RPI4 target
```bash
sudo apt update -y && sudo apt install libgpiod2
```

## Configure SPI on your RPI4
https://pimylifeup.com/raspberry-pi-spi/


## Initial Build
```bash
make ci
```
## Incremental Build for RPI4 Target
```bash
make build.rpi4
```

## Incremental Build for PC
```bash
make build.x86
```

## Waveforms

### Full Waveform
<img src="waveform/full-waveform.png" />
