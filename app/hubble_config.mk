# Set the key define
CFLAGS += -DHUBBLE_KEY_B64_STR=\"$(KEY)\"

# Hubble SDK root (assuming this is included in <board>/s132/armgcc)
HUBBLE_SDK_ROOT := $(PROJ_DIR)/../external/hubble-sdk
LIBB64_SDK_ROOT := $(PROJ_DIR)/../external/libb64
MBEDTLS_SDK_ROOT := $(SDK_ROOT)/external/mbedtls
CC310_SDK_ROOT := $(SDK_ROOT)/external/nrf_cc310

# Hubble Flags
CFLAGS += -DCONFIG_HUBBLE_BLE_NETWORK
# Counter source for EID rotation (v2.0.0+). DEVICE_UPTIME derives the EID
# counter purely from device uptime, so no real-time clock or time
# synchronization is required: hubble_init() takes an initial counter value
# (we start at 0) rather than a wall-clock timestamp. The counter advances one
# step per rotation period and wraps at the fixed EID pool size (128).
# CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC is currently locked to 86400 (daily).
CFLAGS += -DCONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
CFLAGS += -DCONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC=86400
# Reject a nonce (counter, sequence number) that has already been used, rather
# than emitting an advertisement that reuses the AES-CTR keystream. The SDK's
# Kconfig defaults this on; a bare-metal build has no Kconfig, so it has to be
# set here or the check is silently compiled out.
CFLAGS += -DCONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK

# Misc application configuration values
CFLAGS += -DAPP_ADV_INTERVAL=500 # units of 625us

# Sources and includes common to every crypto backend.
SRC_FILES += \
  $(HUBBLE_SDK_ROOT)/src/hubble.c \
  $(HUBBLE_SDK_ROOT)/src/hubble_ble.c \
  $(HUBBLE_SDK_ROOT)/src/hubble_crypto.c \
  $(LIBB64_SDK_ROOT)/src/cdecode.c \
  $(PROJ_DIR)/hubble_app_port.c \
  $(PROJ_DIR)/hubble_advertiser.c \

INC_FOLDERS += \
  $(LIBB64_SDK_ROOT)/include \
  $(HUBBLE_SDK_ROOT)/include \
  $(HUBBLE_SDK_ROOT)/src/utils \

# ----------------------------------------------------------------------------
# Crypto backend selection
#
# HUBBLE_CRYPTO picks the crypto implementation. Override on the command line
# with `make HUBBLE_CRYPTO=<name>`. Valid values:
#
#   cc310    - Direct CryptoCell (SaSi_Aes*) driver. AES-256. Requires pca10056
#              (nRF52840 has the CryptoCell hardware; nRF52832 does not).
#   nrf      - nRF5 SDK nrf_crypto wrapper over the CC310 backend. AES-128.
#              Requires pca10056. Same hardware path as `cc310`, but uses the
#              backend-agnostic nrf_crypto_aes_crypt() API and picks up the
#              SDK's mutex, RAM-location check, and DMA chunking.
#   mbedtls  - Raw mbedTLS software. AES-256. Any board. Only option on
#              pca10040 (no CryptoCell).
#
# Default per board: cc310 on pca10056, mbedtls elsewhere.
# ----------------------------------------------------------------------------
ifeq ($(BOARD),pca10056)
HUBBLE_CRYPTO ?= cc310
else
HUBBLE_CRYPTO ?= mbedtls
endif

# The raw-SaSi cc310 path always needs the CryptoCell hardware. (HUBBLE_CRYPTO=nrf
# only needs it when NRF_CRYPTO_BACKEND=cc310; that check lives in the nrf branch.)
ifneq ($(BOARD),pca10056)
ifeq ($(HUBBLE_CRYPTO),cc310)
$(error HUBBLE_CRYPTO=cc310 requires BOARD=pca10056 (CryptoCell hardware); use HUBBLE_CRYPTO=mbedtls or HUBBLE_CRYPTO=nrf with NRF_CRYPTO_BACKEND=mbedtls on $(BOARD))
endif
endif

ifeq ($(HUBBLE_CRYPTO),cc310)

# ---- cc310: raw SaSi_Aes* driver, AES-256 --------------------------------
CFLAGS += -DCONFIG_HUBBLE_KEY_SIZE=32
CFLAGS += -DCONFIG_HUBBLE_NETWORK_KEY_256

# sdk_config.h #ifndef-guards each of these, so defining them here overrides
# the defaults:
#   - CC310 backend defaults to OFF in sdk_config.h; turn it on so
#     cc310_backend_init.c actually runs SaSi_LibInit().
#   - AES-only path: no RNG needed. Setting AUTO_INIT to 0 avoids pulling in
#     the RNG backend and silences the #warning in cc310_backend_init.c (which
#     would otherwise be fatal under -Werror).
CFLAGS += -DNRF_CRYPTO_ENABLED=1
CFLAGS += -DNRF_CRYPTO_BACKEND_CC310_ENABLED=1
CFLAGS += -DNRF_CRYPTO_RNG_AUTO_INIT_ENABLED=0
# We use CC310 for AES only, no RNG. nrf_crypto_rng_backend.h unconditionally
# includes every RNG backend header, so disable them all to keep those headers
# empty (no RNG .c files, no nrf_drv_rng dependency).
CFLAGS += -DNRF_CRYPTO_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_CC310_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_OPTIGA_RNG_ENABLED=0

SRC_FILES += \
  $(PROJ_DIR)/hubble_crypto_cc310.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_init.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_mutex.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_shared.c \

INC_FOLDERS += \
  $(CC310_SDK_ROOT)/include \
  $(SDK_ROOT)/components/libraries/crypto \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310 \
  $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw \
  $(SDK_ROOT)/components/libraries/crypto/backend/optiga \
  $(SDK_ROOT)/components/libraries/stack_info \

# Prebuilt CryptoCell runtime. hard-float matches this project's
# -mfloat-abi=hard; passed as an explicit object so --gc-sections keeps only
# the AES paths we reference.
LIB_FILES += $(CC310_SDK_ROOT)/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a

else ifeq ($(HUBBLE_CRYPTO),nrf)

# ---- nrf: nrf_crypto wrapper API, AES-128 --------------------------------
#
# NRF_CRYPTO_BACKEND picks which nrf_crypto backend actually implements the
# AES primitives. Override with `make HUBBLE_CRYPTO=nrf NRF_CRYPTO_BACKEND=<name>`.
# Valid values (only these two register the g_nrf_crypto_aes_{ctr,cmac}_128_info
# descriptors that hubble_crypto_nrf.c binds to):
#
#   cc310    - CryptoCell hardware. Requires pca10056.
#   mbedtls  - Software (mbedTLS) via the nrf_crypto wrapper. Any board.
#
# Default: cc310 on pca10056, mbedtls elsewhere.

ifeq ($(BOARD),pca10056)
NRF_CRYPTO_BACKEND ?= cc310
else
NRF_CRYPTO_BACKEND ?= mbedtls
endif

# hubble_crypto_nrf.c is compile-time locked to a 128-bit key because the
# nrf_crypto CC310 backend only registers 128-bit AES-CTR/CMAC descriptors and
# rejects other key sizes at runtime. Keep the 128-bit key here even when the
# mbedtls backend is selected, so switching NRF_CRYPTO_BACKEND doesn't change
# the wire format.
CFLAGS += -DCONFIG_HUBBLE_KEY_SIZE=16
CFLAGS += -DCONFIG_HUBBLE_NETWORK_KEY_128

CFLAGS += -DNRF_CRYPTO_ENABLED=1
CFLAGS += -DNRF_CRYPTO_RNG_AUTO_INIT_ENABLED=0
CFLAGS += -DNRF_CRYPTO_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_CC310_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED=0
CFLAGS += -DNRF_CRYPTO_BACKEND_OPTIGA_RNG_ENABLED=0

SRC_FILES += \
  $(PROJ_DIR)/hubble_crypto_nrf.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes_shared.c \
  $(SDK_ROOT)/components/libraries/crypto/nrf_crypto_shared.c \

# nrf_crypto_aes_backend.h pulls in both cc310_backend_aes.h and
# mbedtls_backend_aes.h unconditionally (their bodies self-guard on the
# backend's NRF_MODULE_ENABLED flag, but the header files still need to be
# findable on the include path regardless of which backend is selected).
INC_FOLDERS += \
  $(SDK_ROOT)/components/libraries/crypto \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310 \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls \
  $(SDK_ROOT)/components/libraries/stack_info \

ifeq ($(NRF_CRYPTO_BACKEND),cc310)

ifneq ($(BOARD),pca10056)
$(error NRF_CRYPTO_BACKEND=cc310 requires BOARD=pca10056; use NRF_CRYPTO_BACKEND=mbedtls on $(BOARD))
endif

CFLAGS += -DNRF_CRYPTO_BACKEND_CC310_ENABLED=1

SRC_FILES += \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_init.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_mutex.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_shared.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_aes.c \

INC_FOLDERS += \
  $(CC310_SDK_ROOT)/include \
  $(SDK_ROOT)/components/libraries/crypto/backend/nrf_hw \
  $(SDK_ROOT)/components/libraries/crypto/backend/optiga \

LIB_FILES += $(CC310_SDK_ROOT)/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a

else ifeq ($(NRF_CRYPTO_BACKEND),mbedtls)

# Software mbedTLS behind the nrf_crypto wrapper.
#
# mbedtls_backend_aes.c wires the 128-bit CTR/CMAC info descriptors to the
# mbedTLS AES/cipher/cmac library sources below.
#
#   - MBEDTLS_CMAC_C: not on in the SDK's default mbedtls/config.h.
#   - MBEDTLS_PLATFORM_MEMORY: needed because mbedtls_backend_init.c
#     unconditionally calls mbedtls_platform_set_calloc_free(), which only
#     exists when this is on. (MBEDTLS_PLATFORM_C, its prerequisite, is
#     already defined in the SDK's config.h.) Adds one source file (platform.c)
#     and no runtime allocation on the path we exercise (nrf_crypto_aes_crypt
#     with a stack context).
#   - NRF_CRYPTO_ALLOCATOR=3 (MALLOC) pins the nrf_crypto memory hook to
#     stdlib malloc/free. The default would resolve to NRF_MALLOC and drag in
#     the SDK's mem_manager. We don't need dynamic allocation on the crypt
#     path, but the wrapper still has to build.
CFLAGS += -DNRF_CRYPTO_BACKEND_MBEDTLS_ENABLED=1
CFLAGS += -DMBEDTLS_CMAC_C
CFLAGS += -DMBEDTLS_PLATFORM_MEMORY
CFLAGS += -DNRF_CRYPTO_ALLOCATOR=3

# cipher.c and cipher_wrap.c reference symbols from every cipher the shipped
# mbedtls/config.h enables (GCM, CCM, ChaCha20/Poly1305, ARC4, Blowfish,
# Camellia, DES). Rather than #undef each unused feature, pull in the full
# cipher family — --gc-sections drops what we don't reach from
# hubble_crypto_nrf.c.
SRC_FILES += \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_init.c \
  $(SDK_ROOT)/components/libraries/crypto/backend/mbedtls/mbedtls_backend_aes.c \
  $(MBEDTLS_SDK_ROOT)/library/aes.c \
  $(MBEDTLS_SDK_ROOT)/library/arc4.c \
  $(MBEDTLS_SDK_ROOT)/library/blowfish.c \
  $(MBEDTLS_SDK_ROOT)/library/camellia.c \
  $(MBEDTLS_SDK_ROOT)/library/ccm.c \
  $(MBEDTLS_SDK_ROOT)/library/chacha20.c \
  $(MBEDTLS_SDK_ROOT)/library/chachapoly.c \
  $(MBEDTLS_SDK_ROOT)/library/cipher.c \
  $(MBEDTLS_SDK_ROOT)/library/cipher_wrap.c \
  $(MBEDTLS_SDK_ROOT)/library/cmac.c \
  $(MBEDTLS_SDK_ROOT)/library/des.c \
  $(MBEDTLS_SDK_ROOT)/library/gcm.c \
  $(MBEDTLS_SDK_ROOT)/library/platform.c \
  $(MBEDTLS_SDK_ROOT)/library/platform_util.c \
  $(MBEDTLS_SDK_ROOT)/library/poly1305.c \

INC_FOLDERS += \
  $(MBEDTLS_SDK_ROOT)/include \

else
$(error Unknown NRF_CRYPTO_BACKEND='$(NRF_CRYPTO_BACKEND)'; valid values: cc310, mbedtls)
endif

else ifeq ($(HUBBLE_CRYPTO),mbedtls)

# ---- mbedtls: pure software, AES-256 -------------------------------------
CFLAGS += -DCONFIG_HUBBLE_KEY_SIZE=32
CFLAGS += -DCONFIG_HUBBLE_NETWORK_KEY_256

CFLAGS += -DMBEDTLS_CONFIG_FILE=\"mbedtls_config_hubble.h\"

SRC_FILES += \
  $(MBEDTLS_SDK_ROOT)/library/aes.c \
  $(MBEDTLS_SDK_ROOT)/library/cipher.c \
  $(MBEDTLS_SDK_ROOT)/library/cipher_wrap.c \
  $(MBEDTLS_SDK_ROOT)/library/cmac.c \
  $(MBEDTLS_SDK_ROOT)/library/platform_util.c \
  $(HUBBLE_SDK_ROOT)/src/crypto/mbedtls.c \

INC_FOLDERS += \
  $(MBEDTLS_SDK_ROOT)/include \

else
$(error Unknown HUBBLE_CRYPTO='$(HUBBLE_CRYPTO)'; valid values: cc310, nrf, mbedtls)
endif
