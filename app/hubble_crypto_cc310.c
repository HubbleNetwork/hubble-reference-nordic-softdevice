/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <sns_silib.h>
#include <ssi_aes.h>

#include <nrf_crypto_init.h>

#include <hubble/ble.h>
#include <hubble/port/crypto.h>

#if !defined(CONFIG_HUBBLE_NETWORK_KEY_256) || (CONFIG_HUBBLE_KEY_SIZE != 32)
#error "CC310 backend here is wired for a 256-bit Hubble key"
#endif

int hubble_crypto_init(void)
{
	ret_code_t ret = nrf_crypto_init();

	if (ret != NRF_SUCCESS && ret != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
		return -EIO;
	}

	return 0;
}

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_NONCE_BUFFER_SIZE],
			  const uint8_t *data, size_t len, uint8_t *output)
{
	SaSiError_t rc;
	int ret = -EIO;
	SaSiAesUserContext_t ctx;
	SaSiAesUserKeyData_t key_data;
	size_t out_size = len;

	if (len == 0) {
		return 0;
	}

	rc = SaSi_AesInit(&ctx, SASI_AES_ENCRYPT, SASI_AES_MODE_CTR,
			  SASI_AES_PADDING_NONE);
	if (rc != SASI_OK) {
		return -EIO;
	}

	key_data.pKey = (uint8_t *)key;
	key_data.keySize = CONFIG_HUBBLE_KEY_SIZE; /* 32 bytes -> AES-256 */

	rc = SaSi_AesSetKey(&ctx, SASI_AES_USER_KEY, &key_data,
			    sizeof(key_data));
	if (rc != SASI_OK) {
		goto out;
	}

	/* For CTR the IV is the initial counter block. */
	rc = SaSi_AesSetIv(&ctx, nonce_counter);
	if (rc != SASI_OK) {
		goto out;
	}

	rc = SaSi_AesFinish(&ctx, len, (uint8_t *)data, len, output, &out_size);
	if (rc != SASI_OK) {
		goto out;
	}

	ret = 0;
out:
	SaSi_AesFree(&ctx);
	return ret;
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	SaSiError_t rc;
	int ret = -EIO;
	SaSiAesUserContext_t ctx;
	SaSiAesUserKeyData_t key_data;
	size_t mac_size = HUBBLE_AES_BLOCK_SIZE;

	rc = SaSi_AesInit(&ctx, SASI_AES_ENCRYPT, SASI_AES_MODE_CMAC,
			  SASI_AES_PADDING_NONE);
	if (rc != SASI_OK) {
		return -EIO;
	}

	key_data.pKey = (uint8_t *)key;
	key_data.keySize = CONFIG_HUBBLE_KEY_SIZE; /* 32 bytes -> AES-256 */

	rc = SaSi_AesSetKey(&ctx, SASI_AES_USER_KEY, &key_data,
			    sizeof(key_data));
	if (rc != SASI_OK) {
		goto out;
	}

	rc = SaSi_AesFinish(&ctx, input_len, (uint8_t *)input, input_len,
			    output, &mac_size);
	if (rc != SASI_OK) {
		goto out;
	}

	ret = 0;
out:
	SaSi_AesFree(&ctx);
	return ret;
}

void hubble_crypto_zeroize(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	while (len--) {
		*p++ = 0;
	}
}
