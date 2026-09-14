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
make GNU_INSTALL_ROOT=<GNU_INSTALL_ROOT> GNU_VERSION=<GNU_VERSION> KEY=<BASE64_KEY>
```

* ```GNU_INSTALL_ROOT```: path to the gcc-arm-none-eabi binaries (optional if installed in SDK default).
* ```GNU_VERSION```: the GCC toolchain version string (e.g., 10.3).
* ```KEY```: your base64-encoded Hubble device key obtained when registering your device with Hubble.

**Example**:

```bash
make GNU_INSTALL_ROOT=/Applications/ARM/bin/ GNU_VERSION=10.3 KEY=7HLguuPSA5Y2thEfqfCwnKVELdeR+g7sGWGssI3WO0w=
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

- [Hubble SDK](https://github.com/HubbleNetwork/hubble-device-sdk) - core SDK for Hubble functionality
- [libb64](https://github.com/libb64/libb64) - used for decoding the Hubble key (this is just for convenience and not a requirement for Hubble functionality)
- [nRF5-SDK](https://github.com/greenlsi/nrf5-sdk) - for Nordic SDK functionality (this is a misc mirror - for any product usage you should pull the code directly from Nordic)
- ```MBEDTLS``` - backend for cryptography (from within the nRF5-SDK)


## Troubleshooting
* **Build errors referencing toolchain**: confirm ```GNU_INSTALL_ROOT``` and ```GNU_VERSION``` are correct and GCC is installed.
* **Device not found by** ```nrfjprog```: check Segger drivers, USB cables/ports, and run ```nrfjprog --recover``` if necessary.
* **App crashes after flashing SoftDevice**: verify the app linker script and memory offsets match the SoftDevice variant programmed to the device (SoftDevice reserves a flash/RAM region).
* **KEY or payload decode failures**: confirm your ```KEY``` is the base64 key issued by Hubble and is passed verbatim to the build.


## License & Security
* **License**: Apache-2.0
* **Security**: If you find a vulnerability, please contact support@hubble.com.
