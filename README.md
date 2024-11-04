# TI IWRL6432 Tooling to Boot from SPI without a QSPI Flash

## Setup Ubuntu 24.04 Build Environment
```bash
sudo apt install git build-essential cmake libdocopt-dev libuv1-dev flex bison libgtest-dev libftdi1-dev libftdipp1-dev
```
## Initial Build
```bash
make ci
```

## Incremental Build
```bash
make build
```

