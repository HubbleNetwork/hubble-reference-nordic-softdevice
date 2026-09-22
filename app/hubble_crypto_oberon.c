/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <ocrypto_aes_ctr.h>
#include <ocrypto_aes_cmac.h>

#include <hubble/ble.h>
#include <hubble/port/crypto.h>

#if !defined(CONFIG_HUBBLE_NETWORK_KEY_256) || (CONFIG_HUBBLE_KEY_SIZE != 32)
#error "Oberon backend here is wired for a 256-bit Hubble key"
#endif

int hubble_crypto_init(void)
{
	/* ocrypto is a stateless library */
	return 0;
}

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_NONCE_BUFFER_SIZE],
			  const uint8_t *data, size_t len, uint8_t *output)
{
	ocrypto_aes_ctr_ctx ctx;

	if (len == 0) {
		return 0;
	}

	/* The CTR IV is the initial counter block (HUBBLE_NONCE_BUFFER_SIZE == 16). */
	ocrypto_aes_ctr_init(&ctx, key, CONFIG_HUBBLE_KEY_SIZE, nonce_counter);
	ocrypto_aes_ctr_encrypt(&ctx, output, data, len);

	hubble_crypto_zeroize(&ctx, sizeof(ctx));

	return 0;
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	ocrypto_aes_cmac_authenticate(output, HUBBLE_AES_BLOCK_SIZE, input,
				      input_len, key, CONFIG_HUBBLE_KEY_SIZE);

	return 0;
}

void hubble_crypto_zeroize(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	while (len--) {
		*p++ = 0;
	}
}
