# Hubble Reference Application for Nordic Softdevices

This application shows a reference implementation of the Hubble protcol running on an nRF52840.

This project

## Building

To build the application, enter the ```app``` directory and run:

```bash
cd app
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> TIME=<EPOCH_TIME_SM> KEY=<KEY>
```

Where:
- ```GNU_INSTALL_ROOT``` is where your ```gcc-arm-none-eabi``` install is located
- ```GNU_VERSION``` is the version
- ```TIME``` is the current epoch time in milliseconds
- ```KEY``` is your base64 encoded key acquired from Hubble when registering your device

**Note!** Both ```GNU_INSTALL_ROOT``` and ```GNU_VERSION``` are optional if your gcc toolchain is installed where the nRF SDK expects it (e.g. ```/opt/gcc-arm-none-eabi-10.3-2021.10/bin/``` on posix systems). The SDK was built and tested with 10.3 so that is recommended.

Example usage:

```bash
make GNU_INSTALL_ROOT=/Applications/ARM/bin/ GNU_VERSION=10.3 TIME=1762629542000 KEY=7HLguuPSA5Y2thEfqfCwnKVELdeR+g7sGWGssI3WO0w=
```

## Flashing

To flash your device, you must first flash the softdevice, then flash your application:

Flash softdevice:
```bash
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> flash_softdevice
```

The softdevice needs only be programmed once.

Flash your application:
```bash
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> TIME=<EPOCH_TIME_SM> KEY=<KEY> flash
```

## Application Information

This application uses the following libraries/SDKs:
- [Hubble SDK](https://github.com/HubbleNetwork/sdk) - core SDK for Hubble functionality
- [libb64](https://github.com/libb64/libb64) - used for decoding the Hubble key (this is just for convenience and not a requirement for Hubble functionality)
- [nRF5-SDK](https://github.com/greenlsi/nrf5-sdk) - for Nordic SDK functionality (this is a misc mirror - for any product usage you should pull the code directly from Nordic)
- ```MBEDTLS``` - backend for cryptography (from within the nRF5-SDK)

Most Hubble-specific configuration can be done in ```app/hubble_config.mk``` makefile which includes configuration values as well as necessary sources/includes for the Hubble protocol.

## Cloning

To clone the repo and the submodules included, clone the repo recursively:

```bash
git clone --recurse-submodules git@github.com:HubbleNetwork/hubble-reference-nordic-softdevice.git
```

## Testing

Once programmed, pyhubblenetwork can be used to ensure packets are being constructed correctly:

```bash
pipx install pyhubblenetwork
hubblenetwork ble scan -k <YOUR_BAS64_KEY>
```
