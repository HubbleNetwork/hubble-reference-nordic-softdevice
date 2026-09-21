/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <nrf_crypto_init.h>
#include <nrf_crypto_aes.h>

#include <hubble/ble.h>
#include <hubble/port/crypto.h>

#if !defined(CONFIG_HUBBLE_NETWORK_KEY_128) || (CONFIG_HUBBLE_KEY_SIZE != 16)
#error "hubble_crypto_nrf.c requires a 128-bit Hubble key (CONFIG_HUBBLE_NETWORK_KEY_128)"
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
	nrf_crypto_aes_context_t ctx;
	size_t out_size = len;
	ret_code_t ret;

	if (len == 0) {
		return 0;
	}

	ret = nrf_crypto_aes_crypt(&ctx, &g_nrf_crypto_aes_ctr_128_info,
				   NRF_CRYPTO_ENCRYPT, (uint8_t *)key,
				   nonce_counter, (uint8_t *)data, len, output,
				   &out_size);
	if (ret != NRF_SUCCESS) {
		return -EIO;
	}

	return 0;
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	nrf_crypto_aes_context_t ctx;
	size_t mac_size = HUBBLE_AES_BLOCK_SIZE;
	ret_code_t ret;

	ret = nrf_crypto_aes_crypt(&ctx, &g_nrf_crypto_aes_cmac_128_info,
				   NRF_CRYPTO_MAC_CALCULATE, (uint8_t *)key,
				   NULL, (uint8_t *)input, input_len, output,
				   &mac_size);
	if (ret != NRF_SUCCESS) {
		return -EIO;
	}

	return 0;
}

void hubble_crypto_zeroize(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	while (len--) {
		*p++ = 0;
	}
}
