# TI IWRL6432 Tooling to Boot from SPI without a QSPI Flash

## Setup Ubuntu 24.04 Build Environment
```bash
sudo apt install git build-essential cmake libdocopt-dev libuv1-dev flex bison libgtest-dev spi-tools gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf arm-linux-gnueabihf-g++
```
## Initial Build
```bash
make ci
```

## Incremental Build
```bash
make build
```

