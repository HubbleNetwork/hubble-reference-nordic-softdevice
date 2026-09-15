/*
 * Minimal mbedtls config for the Hubble SDK mbedtls backend (AES-CTR and
 * AES-CMAC). Must match the mbedtls sources listed in hubble_config.mk.
 */
#ifndef MBEDTLS_CONFIG_HUBBLE_H
#define MBEDTLS_CONFIG_HUBBLE_H

#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CIPHER_MODE_CTR

/* AES tables in flash instead of ~8.7 KB of RAM; FEWER_TABLES keeps the flash
 * cost down. */
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_HUBBLE_H */
