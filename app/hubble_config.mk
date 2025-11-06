# Set the time and key defines
CFLAGS += -DHUBBLE_KEY_B64_STR=\"$(KEY)\"
CFLAGS += -DHUBBLE_UTC_TIME_MS=$(TIME)

# Hubble SDK root (assuming this is included in <board>/s132/armgcc)
HUBBLE_SDK_ROOT := $(PROJ_DIR)/../external/hubble-sdk
LIBB64_SDK_ROOT := $(PROJ_DIR)/../external/libb64
MBEDTLS_SDK_ROOT := $(SDK_ROOT)/external/mbedtls

# Hubble Flags
CFLAGS += -DCONFIG_HUBBLE_KEY_SIZE=32
CFLAGS += -DCONFIG_HUBBLE_NETWORK_KEY_256
CFLAGS += -DCONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY

# Misc application configuration values
CFLAGS += -DAPP_ADV_INTERVAL=500 # units of 625us

# MBEDTLS Flags
CFLAGS += -DMMBEDTLS
CFLAGS += -DMBEDTLS_CIPHER
CFLAGS += -DMBEDTLS_CMAC
CFLAGS += -DMBEDTLS_CMAC_C
CFLAGS += -DMBEDTLS_CIPHER_AES

SRC_FILES += \
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
  $(MBEDTLS_SDK_ROOT)/library/platform_util.c \
  $(MBEDTLS_SDK_ROOT)/library/poly1305.c \
  $(HUBBLE_SDK_ROOT)/src/hubble_ble.c \
  $(HUBBLE_SDK_ROOT)/src/crypto/mbedtls.c \
  $(LIBB64_SDK_ROOT)/src/cdecode.c \
  $(PROJ_DIR)/hubble_app_port.c \
  $(PROJ_DIR)/hubble_advertiser.c \

INC_FOLDERS += \
  $(MBEDTLS_SDK_ROOT)/include \
  $(LIBB64_SDK_ROOT)/include \
  $(HUBBLE_SDK_ROOT)/include \
  $(HUBBLE_SDK_ROOT)/src/utils \