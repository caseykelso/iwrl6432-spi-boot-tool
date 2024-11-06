# TI IWRL6432 Tooling to Boot from SPI without a QSPI Flash

## Setup Ubuntu 24.04 Build Environment - Build for RPI4 Ubuntu 22.04
```bash
sudo apt install git build-essential cmake libdocopt-dev libuv1-dev flex bison libgtest-dev spi-tools gcc-11-aarch64-linux-gnu g++-11-aarch64-linux-gnu

```
## Initial Build
```bash
make ci
```

## Incremental Build
```bash
make build
```

