/*
 * aes.c
 * AES encrypt/decrypt wrapper functions used around Rijndael reference
 * implementation
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: aes.c,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#include <typedefs.h>
#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>	/* for size_t */
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif
#endif	/* BCMDRIVER */
#include <bcmendian.h>
#include <bcmutils.h>
#include <proto/802.11.h>
#include <bcmcrypto/aes.h>
#include <bcmcrypto/rijndael-alg-fst.h>

#ifdef BCMAES_TEST
#include <stdio.h>

#define	dbg(args)	printf args

void
pinter(const char *label, const uint8 *A, const size_t il, const uint8 *R)
{
	int k;
	printf("%s", label);
	for (k = 0; k < AES_BLOCK_SZ; k++)
		printf("%02X", A[k]);
	printf(" ");
	for (k = 0; k < il; k++) {
		printf("%02X", R[k]);
		if (!((k + 1) % AES_BLOCK_SZ))
			printf(" ");
		}
	printf("\n");
}

void
pres(const char *label, const size_t len, const uint8 *data)
{
	int k;
	printf("%s\n", label);
	for (k = 0; k < len; k++) {
		printf("%02x ", data[k]);
		if (!((k + 1) % AES_BLOCK_SZ))
			printf("\n");
	}
	printf("\n");
}
#else
#define	dbg(args)
#define pinter(label, A, il, R)
#define pres(label, len, data)
#endif /* BCMAES_TEST */

/*
* ptxt - plain text
* ctxt - cipher text
*/

/* Perform AES block encryption, including key schedule setup */
void
BCMROMFN(aes_encrypt)(const size_t kl, const uint8 *K, const uint8 *ptxt, uint8 *ctxt)
{
	uint32 rk[4 * (AES_MAXROUNDS + 1)];
	rijndaelKeySetupEnc(rk, K, (int)AES_KEY_BITLEN(kl));
	rijndaelEncrypt(rk, (int)AES_ROUNDS(kl), ptxt, ctxt);
}

/* Perform AES block decryption, including key schedule setup */
void
BCMROMFN(aes_decrypt)(const size_t kl, const uint8 *K, const uint8 *ctxt, uint8 *ptxt)
{
	uint32 rk[4 * (AES_MAXROUNDS + 1)];
	rijndaelKeySetupDec(rk, K, (int)AES_KEY_BITLEN(kl));
	rijndaelDecrypt(rk, (int)AES_ROUNDS(kl), ctxt, ptxt);
}

/* AES-CBC mode encryption algorithm
 *	- handle partial blocks with padding of type as above
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error or final length of output
 */


int
BCMROMFN(aes_cbc_encrypt_pad)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt,
                          uint8 padd_type)
{

	uint8 tmp[AES_BLOCK_SZ];
	uint32 encrypt_len = 0;
	uint32 j;

	/* First block get XORed with nonce/IV */
	const unsigned char *iv = nonce;
	unsigned char *crypt_data = ctxt;
	const unsigned char *plain_data = ptxt;
	uint32 remaining = (uint32)data_len;

	while (remaining >= AES_BLOCK_SZ)
	{
		xor_128bit_block(iv, plain_data, tmp);
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, crypt_data);
		remaining -= AES_BLOCK_SZ;
		iv = crypt_data;
		crypt_data += AES_BLOCK_SZ;
		plain_data += AES_BLOCK_SZ;
		encrypt_len += AES_BLOCK_SZ;
	}

	if (padd_type == NO_PADDING)
		return encrypt_len;

	if (remaining) {
		for (j = 0; j < remaining; j++)
		{
			tmp[j] = plain_data[j] ^ iv[j];
		}
	}
	switch (padd_type)
	{
	case PAD_LEN_PADDING:
		for (j = remaining; j < AES_BLOCK_SZ; j++) {
			tmp[j] = (AES_BLOCK_SZ - remaining) ^  iv[j];
		}
		break;
	default:
		return -1;
	}

	aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, crypt_data);
	encrypt_len += AES_BLOCK_SZ;

	return (encrypt_len);
}

/* AES-CBC mode encryption algorithm
 *	- does not handle partial blocks
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_cbc_encrypt)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt)
{
	if (data_len % AES_BLOCK_SZ) return (-1);
	if (data_len < AES_BLOCK_SZ) return (-1);

	return aes_cbc_encrypt_pad(rk, key_len, nonce, data_len, ptxt, ctxt, NO_PADDING);
}


/* AES-CBC mode decryption algorithm
 *	- handle partial plaintext blocks with padding
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */
int
BCMROMFN(aes_cbc_decrypt_pad)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt,
                          uint8 padd_type)
{
	uint8 tmp[AES_BLOCK_SZ];
	uint32 remaining = (uint32)data_len;
	/* First block get XORed with nonce/IV */
	const unsigned char *iv = nonce;
	const unsigned char *crypt_data = ctxt;
	uint32 plaintext_len = 0;
	unsigned char *plain_data = ptxt;

	if (data_len % AES_BLOCK_SZ) return (-1);
	if (data_len < AES_BLOCK_SZ) return (-1);

	while (remaining >= AES_BLOCK_SZ)
	{
		aes_block_decrypt((int)AES_ROUNDS(key_len), rk, crypt_data, tmp);
		xor_128bit_block(tmp, iv, plain_data);
		remaining -= AES_BLOCK_SZ;
		iv = crypt_data;
		crypt_data += AES_BLOCK_SZ;
		plain_data += AES_BLOCK_SZ;
		plaintext_len += AES_BLOCK_SZ;
	}
	if (padd_type == PAD_LEN_PADDING)
		plaintext_len -= ptxt[plaintext_len -1];
	return (plaintext_len);
}

int
BCMROMFN(aes_cbc_decrypt)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt)
{

	return aes_cbc_decrypt_pad(rk, key_len, nonce, data_len, ctxt, ptxt, NO_PADDING);

}


/* AES-CTR mode encryption/decryption algorithm
 *	- max data_len is (AES_BLOCK_SZ * 2^16)
 *	- nonce must be AES_BLOCK_SZ bytes
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */
int
BCMROMFN(aes_ctr_crypt)(unsigned int *rk,
                        const size_t key_len,
                        const uint8 *nonce,
                        const size_t data_len,
                        const uint8 *ptxt,
                        uint8 *ctxt)
{
	size_t k;
	uint8 tmp[AES_BLOCK_SZ], ctr[AES_BLOCK_SZ];

	if (data_len > (AES_BLOCK_SZ * AES_CTR_MAXBLOCKS)) return (-1);

	bcopy(nonce, ctr, AES_BLOCK_SZ);

	for (k = 0; k < (data_len / AES_BLOCK_SZ); k++) {
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, ctr, tmp);
		xor_128bit_block(ptxt, tmp, ctxt);
		ctr[AES_BLOCK_SZ-1]++;
		if (!ctr[AES_BLOCK_SZ - 1]) ctr[AES_BLOCK_SZ - 2]++;
		ptxt += AES_BLOCK_SZ;
		ctxt += AES_BLOCK_SZ;
	}
	/* handle partial block */
	if (data_len%AES_BLOCK_SZ) {
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, ctr, tmp);
		for (k = 0; k < (data_len % AES_BLOCK_SZ); k++)
			ctxt[k] = ptxt[k] ^ tmp[k];
	}

	return (0);
}


/* AES-CCM mode MAC calculation
 *	- computes AES_CCM_AUTH_LEN MAC
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_mac)(unsigned int *rk,
                      const size_t key_len,
                      const uint8 *nonce,
                      const size_t aad_len,
                      const uint8 *aad,
                      const size_t data_len,
                      const uint8 *ptxt,
                      uint8 *mac)
{
	uint8 B_0[AES_BLOCK_SZ], X[AES_BLOCK_SZ];
	size_t j, k;

	if (aad_len > AES_CCM_AAD_MAX_LEN) return (-1);

	pres("aes_ccm_mac: nonce:", AES_CCM_NONCE_LEN, nonce);
	pres("aes_ccm_mac: aad:", aad_len, aad);
	pres("aes_ccm_mac: input:", data_len, ptxt);

	/* B_0 = Flags || Nonce || l(m) */
	B_0[0] = AES_CCM_AUTH_FLAGS;
	if (aad_len)
		B_0[0] |= AES_CCM_AUTH_AAD_FLAG;
	bcopy(nonce, &B_0[1], AES_CCM_NONCE_LEN);
	B_0[AES_BLOCK_SZ - 2] = (uint8)(data_len >> 8) & 0xff;
	B_0[AES_BLOCK_SZ - 1] = (uint8)(data_len & 0xff);

	/* X_1 := E( K, B_0 ) */
	pres("aes_ccm_mac: CBC IV in:", AES_BLOCK_SZ, B_0);
	aes_block_encrypt((int)AES_ROUNDS(key_len), rk, B_0, X);
	pres("aes_ccm_mac: CBC IV out:", AES_BLOCK_SZ, X);

	/* X_i + 1 := E( K, X_i XOR B_i )  for i = 1, ..., n */

	/* first the AAD */
	if (aad_len) {
		pres("aes_ccm_mac: aad:", aad_len, aad);
		X[0] ^= (aad_len >> 8) & 0xff;
		X[1] ^= aad_len & 0xff;
		k = 2;
		j = aad_len;
		while (j--) {
			X[k] ^= *aad++;
			k++;
			if (k == AES_BLOCK_SZ) {
				pres("aes_ccm_mac: After xor (full block aad):", AES_BLOCK_SZ, X);
				aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
				pres("aes_ccm_mac: After AES (full block aad):", AES_BLOCK_SZ, X);
				k = 0;
			}
		}
		/* handle partial last block */
		if (k % AES_BLOCK_SZ) {
			pres("aes_ccm_mac: After xor (partial block aad):", AES_BLOCK_SZ, X);
			aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
			pres("aes_ccm_mac: After AES (partial block aad):", AES_BLOCK_SZ, X);
		}
	}

	/* then the message data */
	for (k = 0; k < (data_len / AES_BLOCK_SZ); k++) {
		xor_128bit_block(X, ptxt, X);
		pres("aes_ccm_mac: After xor (full block data):", AES_BLOCK_SZ, X);
		ptxt += AES_BLOCK_SZ;
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
		pres("aes_ccm_mac: After AES (full block data):", AES_BLOCK_SZ, X);
	}
	/* last block may be partial, padding is implicit in this xor */
	for (k = 0; k < (data_len % AES_BLOCK_SZ); k++)
		X[k] ^= *ptxt++;
	if (data_len % AES_BLOCK_SZ) {
		pres("aes_ccm_mac: After xor (final block data):", AES_BLOCK_SZ, X);
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
		pres("aes_ccm_mac: After AES (final block data):", AES_BLOCK_SZ, X);
	}

	/* T := first-M-bytes( X_n+1 ) */
	bcopy(X, mac, AES_CCM_AUTH_LEN);
	pres("aes_ccm_mac: MAC:", AES_CCM_AUTH_LEN, mac);

	return (0);
}

/* AES-CCM mode encryption
 *	- computes AES_CCM_AUTH_LEN MAC and then encrypts ptxt and MAC
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- ctxt must have sufficient tailroom for CCM MAC
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_encrypt)(unsigned int *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t aad_len,
                          const uint8 *aad,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt,
                          uint8 *mac)
{
	uint8 A[AES_BLOCK_SZ], X[AES_BLOCK_SZ];

	/* initialize counter */
	A[0] = AES_CCM_CRYPT_FLAGS;
	bcopy(nonce, &A[1], AES_CCM_NONCE_LEN);
	A[AES_BLOCK_SZ-2] = 0;
	A[AES_BLOCK_SZ-1] = 0;
	pres("aes_ccm_encrypt: initial counter:", AES_BLOCK_SZ, A);

	/* calculate and encrypt MAC */
	if (aes_ccm_mac(rk, key_len, nonce, aad_len, aad, data_len, ptxt, X))
		return (-1);
	pres("aes_ccm_encrypt: MAC:", AES_CCM_AUTH_LEN, X);
	if (aes_ctr_crypt(rk, key_len, A, AES_CCM_AUTH_LEN, X, X))
		return (-1);
	pres("aes_ccm_encrypt: encrypted MAC:", AES_CCM_AUTH_LEN, X);
	bcopy(X, mac, AES_CCM_AUTH_LEN);

	/* encrypt data */
	A[AES_BLOCK_SZ - 1] = 1;
	if (aes_ctr_crypt(rk, key_len, A, data_len, ptxt, ctxt))
		return (-1);
	pres("aes_ccm_encrypt: encrypted data:", data_len, ctxt);

	return (0);
}

/* AES-CCM mode decryption
 *	- decrypts ctxt, then computes AES_CCM_AUTH_LEN MAC and checks it against
 *	  then decrypted MAC
 *	- the decrypted MAC is included in ptxt
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_decrypt)(unsigned int *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t aad_len,
                          const uint8 *aad,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt)
{
	uint8 A[AES_BLOCK_SZ], X[AES_BLOCK_SZ];

	/* initialize counter */
	A[0] = AES_CCM_CRYPT_FLAGS;
	bcopy(nonce, &A[1], AES_CCM_NONCE_LEN);
	A[AES_BLOCK_SZ - 2] = 0;
	A[AES_BLOCK_SZ - 1] = 1;
	pres("aes_ccm_decrypt: initial counter:", AES_BLOCK_SZ, A);

	/* decrypt data */
	if (aes_ctr_crypt(rk, key_len, A, data_len-AES_CCM_AUTH_LEN, ctxt, ptxt))
		return (-1);
	pres("aes_ccm_decrypt: decrypted data:", data_len-AES_CCM_AUTH_LEN, ptxt);

	/* decrypt MAC */
	A[AES_BLOCK_SZ - 2] = 0;
	A[AES_BLOCK_SZ - 1] = 0;
	if (aes_ctr_crypt(rk, key_len, A, AES_CCM_AUTH_LEN,
	                  ctxt+data_len-AES_CCM_AUTH_LEN, ptxt + data_len - AES_CCM_AUTH_LEN))
		return (-1);
	pres("aes_ccm_decrypt: decrypted MAC:", AES_CCM_AUTH_LEN,
	     ptxt + data_len - AES_CCM_AUTH_LEN);

	/* calculate MAC */
	if (aes_ccm_mac(rk, key_len, nonce, aad_len, aad,
	                data_len - AES_CCM_AUTH_LEN, ptxt, X))
		return (-1);
	pres("aes_ccm_decrypt: MAC:", AES_CCM_AUTH_LEN, X);
	if (bcmp(X, ptxt + data_len - AES_CCM_AUTH_LEN, AES_CCM_AUTH_LEN) != 0)
		return (-1);

	return (0);
}

/* AES-CCMP mode encryption algorithm
 *	- packet buffer should be an 802.11 MPDU, starting with Frame Control,
 *	  and including the CCMP extended IV
 *	- encrypts in-place
 *	- packet buffer must have sufficient tailroom for CCMP MAC
 *	- returns -1 on error
 */


int
BCMROMFN(aes_ccmp_encrypt)(unsigned int *rk,
                           const size_t key_len,
                           const size_t data_len,
                           uint8 *p,
                           bool legacy,
                           uint8 nonce_1st_byte)
{
	uint8 nonce[AES_CCMP_NONCE_LEN], aad[AES_CCMP_AAD_MAX_LEN];
	struct dot11_header *h = (struct dot11_header*) p;
	uint la, lh;
	int status = 0;

	aes_ccmp_cal_params(h, legacy, nonce_1st_byte, nonce, aad, &la, &lh);

	pres("aes_ccmp_encrypt: aad:", la, aad);

	/*
	 *	MData:
	 *	B3..Bn, n = floor((l(m)+(AES_BLOCK_SZ-1))/AES_BLOCK_SZ) + 2
	 *	m || pad(m)
	 */

	status = aes_ccm_encrypt(rk, key_len, nonce, la, aad,
	                         data_len - lh, p + lh, p + lh, p + data_len);

	pres("aes_ccmp_encrypt: Encrypted packet with MAC:",
	     data_len+AES_CCMP_AUTH_LEN, p);

	if (status) return (AES_CCMP_ENCRYPT_ERROR);
	else return (AES_CCMP_ENCRYPT_SUCCESS);
}

int
BCMROMFN(aes_ccmp_decrypt)(unsigned int *rk,
                           const size_t key_len,
                           const size_t data_len,
                           uint8 *p,
                           bool legacy,
                           uint8 nonce_1st_byte)
{
	uint8 nonce[AES_CCMP_NONCE_LEN], aad[AES_CCMP_AAD_MAX_LEN];
	struct dot11_header *h = (struct dot11_header *)p;
	uint la, lh;
	int status = 0;

	aes_ccmp_cal_params(h, legacy, nonce_1st_byte, nonce, aad, &la, &lh);

	pres("aes_ccmp_decrypt: aad:", la, aad);

	/*
	 *	MData:
	 *	B3..Bn, n = floor((l(m)+(AES_BLOCK_SZ-1))/AES_BLOCK_SZ) + 2
	 *	m || pad(m)
	 */

	status = aes_ccm_decrypt(rk, key_len, nonce, la, aad,
	                         data_len - lh, p + lh, p + lh);

	pres("aes_ccmp_decrypt: Decrypted packet with MAC:", data_len, p);

	if (status) return (AES_CCMP_DECRYPT_MIC_FAIL);
	else return (AES_CCMP_DECRYPT_SUCCESS);
}

void
BCMROMFN(aes_ccmp_cal_params)(struct dot11_header *h, bool legacy,
	uint8 nonce_1st_byte, uint8 *nonce, uint8 *aad, uint *la, uint *lh)
{
	uint8 *iv_data;
	uint16 fc, subtype;
	uint16 seq, qc = 0;
	uint addlen = 0;
	bool wds, qos;

	bzero(nonce, AES_CCMP_NONCE_LEN);
	bzero(aad, AES_CCMP_AAD_MAX_LEN);

	fc = ltoh16(h->fc);
	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	qos = (FC_TYPE(fc) == FC_TYPE_DATA) && (subtype & FC_SUBTYPE_QOS_DATA);

	if (qos) {
		qc = ltoh16(*((uint16 *)((uchar *)h +
		                         (wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN))));
	}

	if (wds) {
		dbg(("aes_ccmp_cal_params: A4 present\n"));
		addlen += ETHER_ADDR_LEN;
	}
	if (qos) {
		dbg(("aes_ccmp_cal_params: QC present\n"));
		addlen += DOT11_QOS_LEN;
	}

	/* length of MPDU header, including IV */
	*lh = DOT11_A3_HDR_LEN + DOT11_IV_AES_CCM_LEN + addlen;
	/* length of AAD */
	*la = AES_CCMP_AAD_MIN_LEN + addlen;
	/* pointer to IV */
	iv_data = (uint8 *)h + DOT11_A3_HDR_LEN + addlen;

	*nonce++ = nonce_1st_byte;

	bcopy((uchar *)&h->a2, nonce, ETHER_ADDR_LEN);
	nonce += ETHER_ADDR_LEN;

	/* PN[5] */
	*nonce++ = iv_data[7];
	/* PN[4] */
	*nonce++ = iv_data[6];
	/* PN[3] */
	*nonce++ = iv_data[5];
	/* PN[2] */
	*nonce++ = iv_data[4];
	/* PN[1] */
	*nonce++ = iv_data[1];
	/* PN[0] */
	*nonce++ = iv_data[0];

	pres("aes_ccmp_cal_params: nonce:", AES_CCM_NONCE_LEN, nonce);

	/* B1..B2 =  l(aad) || aad || pad(aad) */
	/* aad: maskedFC || A1 || A2 || A3 || maskedSC || A4 || maskedQC */

	if (!legacy) {
		if (nonce_1st_byte == 0xff)
			fc &= (FC_SUBTYPE_MASK | AES_CCMP_FC_MASK);
		else
			fc &= AES_CCMP_FC_MASK;
	} else {
		/* 802.11i Draft 3 inconsistencies:
		 * Clause 8.3.4.4.3: "FC ­ MPDU Frame Control field, with Retry bit masked
		 * to zero."  (8.3.4.4.3).
		 * Figure 29: "FC ­ MPDU Frame Control field, with Retry, MoreData, CF-ACK,
		 * CF-POLL bits masked to zero."
		 * F.10.4.1: "FC ­ MPDU Frame Control field, with Retry, MoreData,
		 * PwrMgmt bits masked to zero."
		 */

		/* Our implementation: match 8.3.4.4.3 */
		fc &= AES_CCMP_LEGACY_FC_MASK;
	}
	*aad++ = (uint8)(fc & 0xff);
	*aad++ = (uint8)((fc >> 8) & 0xff);

	bcopy((uchar *)&h->a1, aad, 3*ETHER_ADDR_LEN);
	aad += 3*ETHER_ADDR_LEN;

	seq = ltoh16(h->seq);
	if (!legacy) {
		seq &= AES_CCMP_SEQ_MASK;
	} else {
		seq &= AES_CCMP_LEGACY_SEQ_MASK;
	}

	*aad++ = (uint8)(seq & 0xff);
	*aad++ = (uint8)((seq >> 8) & 0xff);

	if (wds) {
		bcopy((uchar *)&h->a4, aad, ETHER_ADDR_LEN);
		aad += ETHER_ADDR_LEN;
	}
	if (qos) {
		if (!legacy) {
			/* 802.11i Draft 7.0 inconsistencies:
			 * Clause 8.3.3.3.2: "QC ­ The Quality of Service Control, a
			 * two-octet field that includes the MSDU priority, reserved
			 * for future use."
			 * I.7.4: TID portion of QoS
			 */

			/* Our implementation: match the test vectors */
			qc &= AES_CCMP_QOS_MASK;
			*aad++ = (uint8)(qc & 0xff);
			*aad++ = (uint8)((qc >> 8) & 0xff);
		} else {
			/* 802.11i Draft 3.0 inconsistencies: */
			/* Clause 8.3.4.4.3: "QC ­ The QoS Control, if present." */
			/* Figure 30: "QC ­ The QoS TC from QoS Control, if present." */
			/* F.10.4.1: "QC ­ The QoS TC from QoS Control, if present." */

			/* Our implementation: Match clause 8.3.4.4.3 */
			qc &= AES_CCMP_LEGACY_QOS_MASK;
			*aad++ = (uint8)(qc & 0xff);
			*aad++ = (uint8)((qc >> 8) & 0xff);
		}
	}
}



#ifdef BCMAES_TEST
#include "aes_vectors.h"

static uint8
get_nonce_1st_byte(struct dot11_header *h)
{
	uint16 fc, subtype;
	uint16 qc;
	bool wds, qos;

	qc = 0;
	fc = ltoh16(h->fc);
	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	qos = (FC_TYPE(fc) == FC_TYPE_DATA) && (subtype & FC_SUBTYPE_QOS_DATA);

	if (qos) {
		qc = ltoh16(*((uint16 *)((uchar *)h +
			(wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN))));
	}

	/* nonce = priority octet || A2 || PN, !legacy */
	return (uint8)(QOS_TID(qc) & 0x0f);
}

int main(int argc, char **argv)
{
	uint8 output[BUFSIZ], input2[BUFSIZ];
	int retv, k, fail = 0;
	uint32 rk[4 * (AES_MAXROUNDS + 1)];

	/* AES-CBC */
	dbg(("%s: AES-CBC\n", *argv));
	for (k = 0; k < NUM_CBC_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_cbc_vec[k].key,
		                    AES_KEY_BITLEN(aes_cbc_vec[k].kl));
		retv = aes_cbc_encrypt(rk, aes_cbc_vec[k].kl,
		                       aes_cbc_vec[k].nonce, aes_cbc_vec[k].il,
		                       aes_cbc_vec[k].input, output);
		pres("AES CBC Encrypt: ", aes_cbc_vec[k].il, output);

		if (retv == -1) {
			dbg(("%s: aes_cbc_encrypt failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_cbc_vec[k].ref, aes_cbc_vec[k].il) != 0) {
			dbg(("%s: aes_cbc_encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupDec(rk, aes_cbc_vec[k].key,
		                    AES_KEY_BITLEN(aes_cbc_vec[k].kl));
		retv = aes_cbc_decrypt(rk, aes_cbc_vec[k].kl,
		                       aes_cbc_vec[k].nonce, aes_cbc_vec[k].il,
		                       aes_cbc_vec[k].ref, input2);
		pres("AES CBC Decrypt: ", aes_cbc_vec[k].il, input2);

		if (retv == -1) {
			dbg(("%s: aes_cbc_decrypt failed\n", *argv));
			fail++;
		}
		if (bcmp(aes_cbc_vec[k].input, input2, aes_cbc_vec[k].il) != 0) {
			dbg(("%s: aes_cbc_decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, full blocks */
	dbg(("%s: AES-CTR, full blocks\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, aes_ctr_vec[k].il,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Encrypt: ", aes_ctr_vec[k].il, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_ctr_vec[k].ref, aes_ctr_vec[k].il) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, aes_ctr_vec[k].il,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Decrypt: ", aes_ctr_vec[k].il, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(aes_ctr_vec[k].input, input2, aes_ctr_vec[k].il) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, one partial block */
	dbg(("%s: AES-CTR, one partial block\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, k+1,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Partial Block Encrypt: ", k+1, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_ctr_vec[k].ref, k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, k + 1,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Partial Block Decrypt: ", k + 1, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(aes_ctr_vec[k].input, input2, k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, multi-block partial */
	dbg(("%s: AES-CTR, multi-block partial\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, AES_BLOCK_SZ + NUM_CTR_VECTORS+k+1,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Partial Multi-Block Encrypt: ",
		     AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_ctr_vec[k].ref, AES_BLOCK_SZ+NUM_CTR_VECTORS + k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Partial Multi-Block Decrypt: ",
		     AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (bcmp(aes_ctr_vec[k].input, input2,
		         AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CCM */
	dbg(("%s: AES-CCM\n", *argv));
	for (k = 0; k < NUM_CCM_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_mac(rk, aes_ccm_vec[k].kl,
		                   aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                   aes_ccm_vec[k].aad, aes_ccm_vec[k].il,
		                   aes_ccm_vec[k].input, output);

		if (retv) {
			dbg(("%s: aes_ccm_mac failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_ccm_vec[k].mac, AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccm_mac failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_encrypt(rk, aes_ccm_vec[k].kl,
		                       aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                       aes_ccm_vec[k].aad, aes_ccm_vec[k].il,
		                       aes_ccm_vec[k].input, output, &output[aes_ccm_vec[k].il]);
		pres("AES CCM Encrypt: ", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN, output);

		if (retv) {
			dbg(("%s: aes_ccm_encrypt failed\n", *argv));
			fail++;
		}
		if (bcmp(output, aes_ccm_vec[k].ref, aes_ccm_vec[k].il + AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccm_encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_decrypt(rk, aes_ccm_vec[k].kl,
		                       aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                       aes_ccm_vec[k].aad, aes_ccm_vec[k].il + AES_CCM_AUTH_LEN,
		                       aes_ccm_vec[k].ref, input2);
		pres("AES CCM Decrypt: ", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN, input2);

		if (retv) {
			dbg(("%s: aes_ccm_decrypt failed\n", *argv));
			fail++;
		}
		if (bcmp(aes_ccm_vec[k].input, input2, aes_ccm_vec[k].il) != 0) {
			dbg(("%s: aes_ccm_decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CCMP */
	dbg(("%s: AES-CCMP\n", *argv));
	for (k = 0; k < NUM_CCMP_VECTORS; k++) {
		uint8 nonce_1st_byte;
		dbg(("%s: AES-CCMP vector %d\n", *argv, k));
		rijndaelKeySetupEnc(rk, aes_ccmp_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccmp_vec[k].kl));
		bcopy(aes_ccmp_vec[k].input, output, aes_ccmp_vec[k].il);
		nonce_1st_byte = get_nonce_1st_byte((struct dot11_header *)output);
		retv = aes_ccmp_encrypt(rk, aes_ccmp_vec[k].kl,
		                        aes_ccmp_vec[k].il, output,
		                        aes_ccmp_vec[k].flags[2],
		                        nonce_1st_byte);

		if (retv) {
			dbg(("%s: aes_ccmp_encrypt of vector %d returned error\n", *argv, k));
			fail++;
		}
		if (bcmp(output, aes_ccmp_vec[k].ref,
			aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccmp_encrypt of vector %d reference mismatch\n", *argv, k));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccmp_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccmp_vec[k].kl));
		bcopy(aes_ccmp_vec[k].ref, output, aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN);
		nonce_1st_byte = get_nonce_1st_byte((struct dot11_header *)output);
		retv = aes_ccmp_decrypt(rk, aes_ccmp_vec[k].kl,
			aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN, output,
			aes_ccmp_vec[k].flags[2],
			nonce_1st_byte);

		if (retv) {
			dbg(("%s: aes_ccmp_decrypt of vector %d returned error %d\n",
			     *argv, k, retv));
			fail++;
		}
		if (bcmp(output, aes_ccmp_vec[k].input, aes_ccmp_vec[k].il) != 0) {
			dbg(("%s: aes_ccmp_decrypt of vector %d reference mismatch\n", *argv, k));
			fail++;
		}
	}


	fprintf(stderr, "%s: %s\n", *argv, fail ? "FAILED" : "PASSED");
	return (fail);
}

#endif /* BCMAES_TEST */
