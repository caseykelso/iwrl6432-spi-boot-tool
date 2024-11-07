# TI IWRL6432 Tooling to Boot from SPI without a QSPI Flash

## Setup Ubuntu 24.04 Build Environment - Build for RPI4 Ubuntu 22.04
Update the contents of your /etc/apt/sources.list to match the sources.list in this repo.

```bash
sudo apt update -y
sudo apt install git build-essential cmake libdocopt-dev libuv1-dev flex bison libgtest-dev spi-tools gcc-11-aarch64-linux-gnu g++-11-aarch64-linux-gnu libgpiod-dev libgpiod-dev:arm64
```

## Update the Ubuntu 22.04 environemnt on your RPI4 target
```bash
sudo apt update -y && sudo apt install libgpiod2
```

## Initial Build
```bash
make ci
```

## Incremental Build
```bash
make build
```

