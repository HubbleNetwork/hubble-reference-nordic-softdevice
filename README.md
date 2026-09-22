# Hubble Reference Application — Nordic SoftDevice

Minimal reference firmware for Nordic nRF52 devices that demonstrates the Hubble protocol running on an nRF52840.
This repo contains a tiny example application that constructs and emits Hubble BLE frames and shows the integration points required to onboard a device to Hubble.


## Who this is for

- Developers who want the **simplest working example** of broadcasting Hubble frames from Nordic nRF52 devices.
- Engineers who need a reference illustrating how to integrate Hubble code into an **nRF5 SDK + SoftDevice** workflow.

> NOTE! If you plan to start a new product today, note that Nordic recommends the nRF Connect SDK / SoftDevice Controller for new designs. This repo is intended as a concise SoftDevice (nRF5 SDK) reference.


## Quick start (copy / paste)

Before starting, you must download ```gcc-arm-none-eabi-10.3-2021.10``` and locate the instalation path to pass to ```GNU_INSTALL_ROOT```. Other revisions may work but have not been tested.

```bash
# 1) Clone repo (with submodules)
git clone --recurse-submodules git@github.com:HubbleNetwork/hubble-reference-nordic-softdevice.git
cd hubble-reference-nordic-softdevice/app

# 2) Build (example: set toolchain root/version and your base64 KEY)
make GNU_INSTALL_ROOT=/opt/gcc-arm-none-eabi-10.3-2021.10/bin/ GNU_VERSION=10.3 KEY=<BASE64_KEY>

# 3) Flash SoftDevice (only required once per device / device family)
make GNU_INSTALL_ROOT=/opt/gcc-arm-none-eabi-10.3-2021.10/bin/ GNU_VERSION=10.3 flash_softdevice

# 4) Flash application
make GNU_INSTALL_ROOT=/opt/gcc-arm-none-eabi-10.3-2021.10/bin/ GNU_VERSION=10.3 KEY=<BASE64_KEY> flash

# 5) Verify with Python (optional)
pipx install pyhubblenetwork    # or: pip3 install --user pyhubblenetwork
hubblenetwork ble scan -k <BASE64_KEY>
```

## Prerequisites
* **GCC ARM toolchain** (e.g., gcc-arm-none-eabi-10.3-2021.10)
    If your toolchain is installed at the nRF SDK default path you may omit GNU_INSTALL_ROOT/GNU_VERSION. The project was built/tested with 10.3 — your mileage may vary.
README
* **Segger / Nordic Command Line Tools** (```nrfjprog```) for flashing the device.
    nRF5 SDK (compatible version used during development). If you vendor the SDK locally, configure your paths accordingly.
* **A SoftDevice HEX** compatible with your SoC (e.g., S132 for nRF52832, S140 for nRF52840).
* **Python 3** and ```pyhubblenetwork``` (for quick verification).
* **A Hubble device key** (base64) obtained from the Hubble console — this is passed as KEY to the Makefile. The existing README expects a base64 key; preserve that format.


## Build
From the app directory:

```bash
cd app
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> KEY=<BASE64_KEY> [BOARD=<BOARD>]
```

* ```GNU_INSTALL_ROOT```: path to the gcc-arm-none-eabi binaries (optional if installed in SDK default).
* ```GNU_VERSION```: the GCC toolchain version string (e.g., 10.3).
* ```KEY```: your base64-encoded Hubble device key obtained when registering your device with Hubble.
* ```BOARD```: target board (optional, defaults to ```pca10056```).
* ```HUBBLE_CRYPTO```: crypto backend (optional). One of ```cc310```, ```nrf```, ```oberon```, ```mbedtls```. Defaults to ```cc310``` on ```pca10056``` and ```mbedtls``` elsewhere. See **Crypto backend** below.
* ```NRF_CRYPTO_BACKEND```: backend used when ```HUBBLE_CRYPTO=nrf``` (optional). One of ```cc310```, ```mbedtls```. Defaults to ```cc310``` on ```pca10056``` and ```mbedtls``` elsewhere.

### Supported boards

| ```BOARD``` | Kit | SoC | SoftDevice | Build output |
|---|---|---|---|---|
| ```pca10056``` (default) | nRF52840-DK | nRF52840 | S140 7.2.0 | ```_build/nrf52840_xxaa.hex``` |
| ```pca10040``` | nRF52-DK | nRF52832 | S132 7.2.0 | ```_build/nrf52832_xxaa.hex``` |

```BOARD``` selects the SoC and board defines, the MDK startup/system files, the
SoftDevice headers and hex, and the linker script. It must be passed to every
```make``` invocation for a given board, including ```flash``` and
```flash_softdevice``` — otherwise you will flash the wrong SoftDevice. Example
for the nRF52-DK:

```bash
make BOARD=pca10040 GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=10.3 KEY=<BASE64_KEY>
make BOARD=pca10040 GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=10.3 flash_softdevice
make BOARD=pca10040 GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=10.3 KEY=<BASE64_KEY> flash
```

**Example**:

```bash
make GNU_INSTALL_ROOT=/Applications/ARM/bin/ GNU_VERSION=10.3 KEY=7HLguuPSA5Y2thEfqfCwnKVELdeR+g7sGWGssI3WO0w=
```

### Port layer

```app/hubble_app_port.c``` implements the system abstraction the SDK expects from
the application (```hubble/port/sys.h```):

* ```hubble_uptime_get()``` — monotonic milliseconds, built on ```app_timer``` with
  RTC rollover tracking.
* ```hubble_log()``` — forwards to ```NRF_LOG```.
* ```hubble_lock_init()``` / ```hubble_lock()``` / ```hubble_unlock()``` — required
  since SDK v3.0.0. ```hubble_ble_advertise_get()``` allocates the sequence number
  and validates the nonce under this lock, and both the main thread and the
  ```app_timer``` handler reach that path in this app.

  The implementation uses a SoftDevice critical region
  (```app_util_critical_region_enter/exit```) rather than a mutex: it cannot
  deadlock when taken from the timer interrupt, and the only state it guards — the
  SDK's default RAM sequence counter — never blocks. If you replace
  ```hubble_sequence_counter_get()``` with one that can block (persisting the
  counter to flash, say), switch to a blocking-capable recursive mutex as
  ```sys.h``` requires. Note that ```CRITICAL_REGION_ENTER/EXIT``` cannot be used
  directly here: under ```SOFTDEVICE_PRESENT``` they are brace-scoped macros that
  keep the nesting flag in a local, so they cannot be split across two functions.

```hubble_sequence_counter_get()``` is left to the SDK default (a RAM counter), so
the sequence restarts at 0 on every boot.

### Device configuration vector

On boot the SDK logs its configuration vector, which is a quick way to confirm the
build matches what the Cloud recorded for the device:

```
Hubble Device SDK initialized (HDCV:1.0/E:256/CS:DU/EC:128/RP:S86400/N:T/TV:0)
```

### Counter source (device uptime)

This reference is configured for the Hubble SDK's **device-uptime counter source**
(```CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME```). The Ephemeral Identifier (EID)
counter is derived purely from device uptime, so **no real-time clock or time
synchronization is required** — that is why there is no longer a ```TIME```/epoch
build parameter.

* ```hubble_init()``` is called with an initial counter value of ```0```, so the
  device starts at EID counter 0 on every boot.
* The EID rotates once per ```CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC``` (locked at
  86400s — daily) and wraps at the fixed EID pool size of 128.
* Because the counter restarts at 0 after a power cycle, the receiver recovers the
  EID by scanning the 128-entry pool; no clock alignment is needed. To instead keep
  the counter continuous across reboots, persist ```hubble_counter_get()``` to flash
  and pass the saved value to ```hubble_init()```.

### Nonce reuse checking

```CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK``` is enabled in
```app/hubble_config.mk```. The SDK's Kconfig turns this on by default, but a
bare-metal build has no Kconfig, so it must be set explicitly or the check is
silently compiled out.

With it enabled, ```hubble_ble_advertise_get()``` returns ```-EPERM``` rather than
emitting an advertisement that reuses an ```(EID counter, sequence number)``` nonce
— which would reuse the AES-CTR keystream. ```hubble_advertiser_update_data()```
logs the error and leaves the previous advertisement on air.

The default 5-minute refresh means ~288 advertisements per EID rotation period,
well under the 1024-entry sequence space, so the check should not fire in normal
operation.

### Crypto backend

```HUBBLE_CRYPTO``` selects which crypto implementation is compiled in. Override
it on any ```make``` invocation. The default is ```cc310``` on ```pca10056``` and
```mbedtls``` on every other board.

| ```HUBBLE_CRYPTO``` | Implementation | AES key size | Boards |
|---|---|---|---|
| ```cc310``` | Direct CryptoCell (```SaSi_Aes*```) driver | AES-256 | ```pca10056``` only |
| ```nrf``` | nRF5 SDK ```nrf_crypto``` wrapper API | AES-128 | any (backend-dependent) |
| ```oberon``` | Prebuilt Oberon ```ocrypto``` software library | AES-256 | any |
| ```mbedtls``` | Raw mbedTLS software | AES-256 | any |

* ```cc310``` and ```nrf``` (with its CryptoCell backend) both run on the nRF52840's
  CryptoCell hardware, so they require ```BOARD=pca10056``` — the nRF52832 has no
  CryptoCell. The build errors out if you request them on another board.
* ```nrf``` uses the backend-agnostic ```nrf_crypto_aes_crypt()``` API and picks up
  the SDK's mutex, RAM-location check, and DMA chunking. It is locked to a 128-bit
  key because the ```nrf_crypto``` backends only register 128-bit AES-CTR/CMAC
  descriptors.
* ```oberon``` calls the prebuilt Oberon ```ocrypto_aes_*``` functions directly (no
  ```nrf_crypto``` wrapper, no runtime init). It is pure software, works on any
  board, and pulls in far less code than the full mbedTLS cipher family.
* ```mbedtls``` is pure software and works on any board. It is the default on
  ```pca10040```.

When ```HUBBLE_CRYPTO=nrf```, ```NRF_CRYPTO_BACKEND``` chooses which backend
actually implements the AES primitives:

| ```NRF_CRYPTO_BACKEND``` | Implementation | Boards |
|---|---|---|
| ```cc310``` | CryptoCell hardware | ```pca10056``` only |
| ```mbedtls``` | Software (mbedTLS) via the ```nrf_crypto``` wrapper | any |

It defaults to ```cc310``` on ```pca10056``` and ```mbedtls``` elsewhere. The wire
format stays 128-bit regardless of which backend you pick.

Examples:

```bash
# nRF52840 with the nrf_crypto wrapper over the software mbedTLS backend
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=10.3 KEY=<BASE64_KEY> \
  HUBBLE_CRYPTO=nrf NRF_CRYPTO_BACKEND=mbedtls

# nRF52-DK, software mbedTLS (the default there)
make BOARD=pca10040 GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=10.3 KEY=<BASE64_KEY> \
  HUBBLE_CRYPTO=mbedtls
```

> **The key size differs by backend.** ```cc310```, ```oberon```, and ```mbedtls```
> use AES-256; ```nrf``` uses AES-128. The base64 ```KEY``` you pass is the same, but
> make sure the backend you build matches how your device is registered with Hubble.

## Flashing
Flash the SoftDevice (once per board/SoftDevice version):

```bash
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> flash_softdevice
`````````

Flash the application:

```bash
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> KEY=<BASE64_KEY> flash
```

Notes:
* Pass the same ```BOARD=``` value you built with (see **Supported boards** above). The ```flash_softdevice``` target programs the SoftDevice that matches ```BOARD```, so omitting it on an nRF52-DK would flash S140 onto an nRF52832.
* The SoftDevice only needs programming once for a given part or device family. See ```flash_softdevice``` target for exact behavior.
README
* If flashing fails or nrfjprog cannot find the device, confirm Segger drivers are installed and try ```nrfjprog --recover`` or reconnecting the device.

## Verification / Testing
After flashing, you can verify that Hubble packets are being constructed and broadcast with the pyhubblenetwork helper:

```bash
pipx install pyhubblenetwork
hubblenetwork ble scan -k <YOUR_BASE64_KEY> --counter-mode device_uptime
```

This command scans for and decodes Hubble advertisements using your device key.

> **```--counter-mode device_uptime``` is required.** This firmware derives its EID
> counter from device uptime (see **Counter source** above), but ```ble scan```
> defaults to ```--counter-mode unix_time```. Without the flag every packet fails
> to decrypt even though the key is correct — verified: in ```unix_time``` mode the
> same key failed on 100% of packets from this device.

Expected output for a working device (```DECRYPT``` column via
```--show-failed-decryption```):

```
# freshly reset device (uptime < 5 min)
| DECRYPT | RSSI | VERSION | EID      | TAG      | COUNTER | SALT/SEQ | PAYLOAD |
| OK      | -43  | 0       | e96073a9 | b22658a4 | 0       | 0        |         |

# after the first refresh timer fire (uptime >= 5 min)
| DECRYPT | RSSI | VERSION | EID      | TAG      | COUNTER | SALT/SEQ | PAYLOAD |
| OK      | -43  | 0       | e96073a9 | b22658a4 | 0       | 1        | 300     |
```

* ```COUNTER``` is ```0``` until the device has been up for a full EID rotation
  period (86400s), then increments and wraps at 128.
* ```PAYLOAD``` is **empty for the first 5 minutes** after boot. The application
  seeds the advertisement with no payload in ```hubble_advertiser_init()``` and only
  writes the uptime string when the ```ADV_UPDATE_INTERVAL_MS``` timer first fires.
  An empty payload on a freshly reset device is expected, not a failure.
* ```SALT/SEQ``` increments each time the advertisement is regenerated, so it is
  the quickest way to confirm the refresh timer is running.


## Application / Dependencies
This application depends on:

- [Hubble Device SDK](https://github.com/HubbleNetwork/hubble-device-sdk) - core SDK for Hubble functionality (pinned to **v3.0.0**)
- [libb64](https://github.com/libb64/libb64) - used for decoding the Hubble key (this is just for convenience and not a requirement for Hubble functionality)
- [nRF5-SDK](https://github.com/greenlsi/nrf5-sdk) - for Nordic SDK functionality (this is a misc mirror - for any product usage you should pull the code directly from Nordic)
- Crypto backend (selected with ```HUBBLE_CRYPTO``` — see **Crypto backend** above). Options come from within the nRF5-SDK:
    - ```mbedtls``` - software mbedTLS
    - ```cc310``` - the nRF52840 CryptoCell hardware driver (```nrf_cc310``` prebuilt library)
    - ```nrf``` - the SDK's ```nrf_crypto``` wrapper over either CryptoCell or mbedTLS
    - ```oberon``` - the prebuilt Oberon ```ocrypto``` software library (```nrf_oberon```)

## Troubleshooting
* **Build errors referencing toolchain**: confirm ```GNU_INSTALL_ROOT``` and ```GNU_VERSION``` are correct and GCC is installed.
* **Device not found by** ```nrfjprog```: check Segger drivers, USB cables/ports, and run ```nrfjprog --recover``` if necessary.
* **App crashes after flashing SoftDevice**: verify the app linker script and memory offsets match the SoftDevice variant programmed to the device (SoftDevice reserves a flash/RAM region).
* **KEY or payload decode failures**: confirm your ```KEY``` is the base64 key issued by Hubble and is passed verbatim to the build.


## License & Security
* **License**: Apache-2.0
* **Security**: If you find a vulnerability, please contact support@hubble.com.
