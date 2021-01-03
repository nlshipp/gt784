/* crypto/hmac/hmac.c 
 * Code copied from openssl distribution and
 * Modified just enough so that compiles and runs standalone
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: hmac_sha256.c,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcmcrypto/sha256.h"

#if defined(UNDER_CE)
/* include wl driver config file if this file is compiled for driver */
#ifdef BCMDRIVER
#include <osl.h>
#else
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, int len);
extern int bcmp(const void *b1, const void *b2, int len);
extern void bzero(void *b, int len);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif /* defined(__GNUC__) */
#endif /* BCMDRIVER */
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif /* UNDER_CE */
void hmac_sha256(const void *key, int key_len,
                 const unsigned char *text, size_t text_len, unsigned char *digest,
                 unsigned int *digest_len)

{
	SHA256_CTX ctx;

	int i;
	unsigned char sha_key[SHA256_CBLOCK];
	unsigned char k_ipad[SHA256_CBLOCK];    /* inner padding -
						 * key XORd with ipad
						 */
	unsigned char k_opad[SHA256_CBLOCK];    /* outer padding -
						 * key XORd with opad
							 */
		/* set the key  */
		/* block size smaller than key size : hash down */
		if (SHA256_CBLOCK < key_len)
		{
			SHA256_Init(&ctx);
			SHA256_Update(&ctx, key, key_len);
			SHA256_Final(sha_key, &ctx);
			key = sha_key;
			key_len = SHA256_DIGEST_LENGTH;
		}

		/*
		 * the HMAC_SHA256 transform looks like:
		 *
		 * SHA256(K XOR opad, SHA256(K XOR ipad, text))
		 *
		 * where K is an n byte key
		 * ipad is the byte 0x36 repeated 64 times
		 * opad is the byte 0x5c repeated 64 times
		 * and text is the data being protected
		 */
		/* compute inner and outer pads from key */
		bzero(k_ipad, sizeof(k_ipad));
		bzero(k_opad, sizeof(k_opad));
		bcopy(key, k_ipad, key_len);
		bcopy(key, k_opad, key_len);

		/* XOR key with ipad and opad values */
		for (i = 0; i < 64; i++) {
			k_ipad[i] ^= 0x36;
			k_opad[i] ^= 0x5c;
		}


		/*
		 * perform inner SHA256
		 */
		SHA256_Init(&ctx);                   /* init context for 1st pass */
		SHA256_Update(&ctx, k_ipad, SHA256_CBLOCK);     /* start with inner pad */
		SHA256_Update(&ctx, text, text_len); /* then text of datagram */
		SHA256_Final(digest, &ctx);          /* finish up 1st pass */
		/*
		 * perform outer SHA256
		 */
		SHA256_Init(&ctx);                   /* init context for 2nd pass */
		SHA256_Update(&ctx, k_opad, SHA256_CBLOCK); /* start with outer pad */
		SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH); /* then results of 1st hash */
		SHA256_Final(digest, &ctx);          /* finish up 2nd pass */

		if (digest_len)
			*digest_len = SHA256_DIGEST_LENGTH;
}