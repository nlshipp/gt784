#ifdef BCMWAPI_WPI
/*
 * sms4.c
 * SMS-4 block cipher
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sms4.c,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#include <typedefs.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <bcmcrypto/sms4.h>

#include <bcmutils.h>

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
#endif	/* __GNUC__ */
#endif	/* BCMDRIVER */

#define ROTATE(x, n)     (((x) << (n)) | ((x) >> (32 - (n))))

static void
sms4_print_bytes(char *name, const uchar *data, int len)
{
}

static const uint8 S_box[] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
};

/* Non-linear transform
 * A = (a0, a1, a2, a3)
 * B = (b0, b1, b2, b3)
 * (b0, b1, b2, b3) = tau(A) = (Sbox(a0), Sbox(a1), Sbox(a2), Sbox(a3))
 */
static INLINE uint32
tau(uint32 A)
{
	uint32 B;

	B = (S_box[A & 0xff] |
	     (S_box[(A & 0xff00) >> 8] << 8) |
	     (S_box[(A & 0xff0000) >> 16] << 16) |
	     (S_box[(A & 0xff000000) >> 24] << 24));

	return (B);
}

/* Linear transform
 * C = L(B) = B ^ (B<<<2) ^ (B<<<10) ^ (B<<<18) ^ (B<<<24)
 *   where "<<<" is a circular left shift
 */
static INLINE uint32
L(uint32 B)
{
	return (B ^ ROTATE(B, 2) ^ ROTATE(B, 10) ^ ROTATE(B, 18) ^ ROTATE(B, 24));
}

/* Compound Transform
 * T(.) = L(tau(.))
 */
static INLINE uint32
T(uint32 X)
{
	return (L(tau(X)));
}

/* Round Function
 * F(X0,X1,X2,X3,RK) = X0 ^ T(X1^X2^X3^RK)
 * static INLINE uint32
 * F(uint32 *X, uint32 RK)
 * {
 * 	return (X[0] ^ T(X[1] ^ X[2] ^ X[3] ^ RK));
 * }
 */

/* Encryption/decryption algorithm
 * Xi+4 = F(Xi, Xi+1, Xi+2, Xi+3, RKj) = Xi ^ T(Xi+1 ^ Xi+2 ^ Xi+3 ^ RKj)
 *   i=0,1,...31, j=i(enc) or j=31-i(dec)
 * (Y0, Y1, Y2, Y3) = (X35, X34, X33, X32)
 */
void
sms4_enc(uint32 *Y, uint32 *X, const uint32 *RK)
{
	uint32 z0 = X[0], z1 = X[1], z2 = X[2], z3 = X[3];
	int i;

	for (i = 0; i < SMS4_RK_WORDS; i += 4) {
		z0 ^= T(z1 ^ z2 ^ z3 ^ *RK++);
		z1 ^= T(z2 ^ z3 ^ z0 ^ *RK++);
		z2 ^= T(z3 ^ z0 ^ z1 ^ *RK++);
		z3 ^= T(z0 ^ z1 ^ z2 ^ *RK++);
	}

	Y[0] = z3;
	Y[1] = z2;
	Y[2] = z1;
	Y[3] = z0;
}

void
sms4_dec(uint32 *Y, uint32 *X, uint32 *RK)
{
	uint32 z0 = X[0], z1 = X[1], z2 = X[2], z3 = X[3];
	int i;

	RK += 32;

	for (i = 0; i < SMS4_RK_WORDS; i += 4) {
		z0 ^= T(z1 ^ z2 ^ z3 ^ *--RK);
		z1 ^= T(z2 ^ z3 ^ z0 ^ *--RK);
		z2 ^= T(z3 ^ z0 ^ z1 ^ *--RK);
		z3 ^= T(z0 ^ z1 ^ z2 ^ *--RK);
	}

	Y[0] = z3;
	Y[1] = z2;
	Y[2] = z1;
	Y[3] = z0;
}

static const uint32 CK[] = {
	0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
	0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
	0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
	0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
	0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
	0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
	0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
	0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279
};

static const uint32 FK[] = {
	0xA3B1BAC6,
	0x56AA3350,
	0x677D9197,
	0xB27022DC
};

/* Key Expansion Linear transform
 * Lprime(B) = B ^ (B<<<13) ^ (B<<<23)
 *   where "<<<" is a circular left shift
 */
static INLINE uint32
Lprime(uint32 B)
{
	return (B ^ ROTATE(B, 13) ^ ROTATE(B, 23));
}

/* Key Expansion Compound Transform
 * Tprime(.) = Lprime(tau(.))
 */
static INLINE uint32
Tprime(uint32 X)
{
	return (Lprime(tau(X)));
}

/* Key Expansion
 * Encryption key MK = (MK0, MK1, MK2, MK3)
 * (K0, K1, K2, K3) = (MK0 ^ FK0, MK1 ^ FK1, MK2 ^ FK2, MK3 ^ FK3)
 * RKi = Ki+4 = Ki ^ Tprime(Ki+1 ^ Ki+2 ^ Ki+3 ^ CKi+4)
 */
void
sms4_key_exp(uint32 *MK, uint32 *RK)
{
	int i;
	uint32 K[36];

	for (i = 0; i < 4; i++)
		K[i] = MK[i] ^ FK[i];

	for (i = 0; i < SMS4_RK_WORDS; i++) {
		K[i+4] = K[i] ^ Tprime(K[i+1] ^ K[i+2] ^ K[i+3] ^ CK[i]);
		RK[i] = K[i+4];
	}

	return;
}

/* SMS4-CBC-MAC mode for WPI
 *	- computes SMS4_WPI_CBC_MAC_LEN MAC
 *	- Integrity Check Key must be SMS4_KEY_LEN bytes
 *	- PN must be SMS4_WPI_PN_LEN bytes
 *	- AAD inludes Key Index, Reserved, and L (data len) fields
 *	- For MAC calculation purposes, the aad and data are each padded with
 *	  NULLs to a multiple of the block size
 *	- ptxt must have sufficient tailroom for storing the MAC
 *	- returns -1 on error
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_CBC_MAC_ERROR on error
 */
int
sms4_wpi_cbc_mac(const uint8 *ick,
	const uint8 *pn,
	const size_t aad_len,
	const uint8 *aad,
	uint8 *ptxt)
{
	int k, j, rem_len;
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];
	uint8 tmp[SMS4_BLOCK_SZ];
	uint16 data_len = (aad[aad_len-2] << 8) | (aad[aad_len-1]);

	if (data_len > SMS4_WPI_MAX_MPDU_LEN)
		return (SMS4_WPI_CBC_MAC_ERROR);

	sms4_print_bytes("MIC Key", (uchar *)ick, SMS4_WPI_CBC_MAC_LEN);
	sms4_print_bytes("PN ", (uchar *)pn, SMS4_WPI_PN_LEN);
	sms4_print_bytes("MIC data: PART1", (uchar *)aad, aad_len);
	sms4_print_bytes("MIC data: PART 2", (uchar *)ptxt, data_len);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)ick & 3)
			X[k] = ntoh32_ua(ick + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(ick + (SMS4_WORD_SZ * k)));
	sms4_key_exp(X, RK);

	/* First block: PN */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)pn & 3)
			X[k] = ntoh32_ua(pn + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(pn + (SMS4_WORD_SZ * k)));
	sms4_enc(Y, X, RK);

	/* Next blocks: AAD */
	for (j = 0; j < aad_len/SMS4_BLOCK_SZ; j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)aad & 3)
				X[k] = Y[k] ^ ntoh32_ua(aad + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(aad + (SMS4_WORD_SZ * k)));
		aad += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = aad_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(aad, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)tmp & 3)
				X[k] = Y[k] ^ ntoh32_ua(tmp + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(tmp + (SMS4_WORD_SZ * k)));
		sms4_enc(Y, X, RK);
	}

	/* Then the message data */
	for (j = 0; j < (data_len / SMS4_BLOCK_SZ); j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)ptxt & 3)
				X[k] = Y[k] ^ ntoh32_ua(ptxt + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(ptxt + (SMS4_WORD_SZ * k)));
		ptxt += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = data_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(ptxt, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			if ((uintptr)tmp & 3)
				X[k] = Y[k] ^ ntoh32_ua(tmp + (SMS4_WORD_SZ * k));
			else
				X[k] = Y[k] ^ ntoh32(*(uint32 *)(tmp + (SMS4_WORD_SZ * k)));
		ptxt += data_len % SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		hton32_ua_store(Y[k], ptxt);
		ptxt += SMS4_WORD_SZ;
	}

	return (SMS4_WPI_SUCCESS);
}

/*
 * ick		pn		ptxt	result
 * ltoh		ltoh	ltoh	fail
 * ntoh		ltoh	ltoh	fail
 * ltoh		ntoh	ltoh	fail
 * ntoh		ntoh	ltoh	fail
 *
 * ltoh		ltoh	ntoh	fail
 * ntoh		ltoh	ntoh	fail
 * ltoh		ntoh	ntoh	fail
 * ntoh		ntoh	ntoh	fail
 */

#define	s_ick(a)	ntoh32_ua(a)
#define	s_pn(a)		ntoh32_ua(a)
#define	s_ptxt(a)	ntoh32_ua(a)

int
sms4_cbc_mac(const uint8 *ick,
	const uint8 *pn,
	const size_t data_len,
	uint8 *ptxt,
	uint8 *mac)
{
	int k, j, rem_len;
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];
	uint8 tmp[SMS4_BLOCK_SZ];

	if (data_len > SMS4_WPI_MAX_MPDU_LEN)
		return (SMS4_WPI_CBC_MAC_ERROR);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		X[k] = s_ick(ick + (SMS4_WORD_SZ * k));
	sms4_key_exp(X, RK);

	/* First block: PN */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		X[k] = s_pn(pn + (SMS4_WORD_SZ * k));
	sms4_enc(Y, X, RK);

	/* Then the message data */
	for (j = 0; j < (data_len / SMS4_BLOCK_SZ); j++) {
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			X[k] = Y[k] ^ s_ptxt(ptxt + (SMS4_WORD_SZ * k));
		ptxt += SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}
	/* If the last block is partial, pad with NULLs */
	rem_len = data_len % SMS4_BLOCK_SZ;
	if (rem_len) {
		bcopy(ptxt, tmp, rem_len);
		bzero(tmp + rem_len, SMS4_BLOCK_SZ - rem_len);
		for (k = 0; k < SMS4_BLOCK_WORDS; k++)
			X[k] = Y[k] ^ s_ptxt(tmp + (SMS4_WORD_SZ * k));
		ptxt += data_len % SMS4_BLOCK_SZ;
		sms4_enc(Y, X, RK);
	}

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		hton32_ua_store(Y[k], mac);
		mac += SMS4_WORD_SZ;
	}

	return (SMS4_WPI_SUCCESS);
}

/* SMS4-OFB mode encryption/decryption algorithm
 *	- PN must be SMS4_WPI_PN_LEN bytes
 *	- assumes PN is ready to use as-is (i.e. any
 *		randomization of PN is handled by the caller)
 *	- encrypts data in place
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_OFB_ERROR on error
 */
int
sms4_ofb_crypt(const uint8 *ek,
	const uint8 *pn,
	const size_t data_len,
	uint8 *ptxt)
{
	size_t j, k;
	uint8 tmp[SMS4_BLOCK_SZ];
	uint32 RK[SMS4_RK_WORDS];
	uint32 X[SMS4_BLOCK_WORDS];

	if (data_len > SMS4_WPI_MAX_MPDU_LEN) return (SMS4_WPI_OFB_ERROR);

	sms4_print_bytes("ENC Key", (uchar *)ek, SMS4_WPI_CBC_MAC_LEN);
	sms4_print_bytes("PN ", (uint8 *)pn, SMS4_WPI_PN_LEN);
	sms4_print_bytes("data", (uchar *)ptxt, data_len);

	/* Prepare the round key */
	for (k = 0; k < SMS4_BLOCK_WORDS; k++)
		if ((uintptr)ek & 3)
			X[k] = ntoh32_ua(ek + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(ek + (SMS4_WORD_SZ * k)));
	sms4_key_exp(X, RK);

	for (k = 0; k < SMS4_BLOCK_WORDS; k++) {
		if ((uintptr)pn & 3)
			X[k] = ntoh32_ua(pn + (SMS4_WORD_SZ * k));
		else
			X[k] = ntoh32(*(uint32 *)(pn + (SMS4_WORD_SZ * k)));
	}

	for (k = 0; k < (data_len / SMS4_BLOCK_SZ); k++) {
		sms4_enc(X, X, RK);
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			hton32_ua_store(X[j], &tmp[j * SMS4_WORD_SZ]);
		}
		xor_128bit_block(ptxt, tmp, ptxt);
		ptxt += SMS4_BLOCK_SZ;
	}

	/* handle partial block */
	if (data_len % SMS4_BLOCK_SZ) {
		sms4_enc(X, X, RK);
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			hton32_ua_store(X[j], &tmp[j * SMS4_WORD_SZ]);
		}
		for (k = 0; k < (data_len % SMS4_BLOCK_SZ); k++)
			ptxt[k] ^= tmp[k];
	}

	return (SMS4_WPI_SUCCESS);
}

/* SMS4-WPI mode encryption of 802.11 packet
 * 	- constructs aad and pn from provided frame
 * 	- calls sms4_wpi_cbc_mac() to compute MAC
 * 	- calls sms4_ofb_crypt() to encrypt frame
 * 	- encrypts data in place
 * 	- supplied packet must have sufficient tailroom for CBC-MAC MAC
 * 	- data_len includes 802.11 header and CBC-MAC MAC
 *	- returns SMS4_WPI_SUCCESS on success, SMS4_WPI_ENCRYPT_ERROR on error
 */
int
sms4_wpi_pkt_encrypt(const uint8 *ek,
                          const uint8 *ick,
                          const size_t data_len,
                          uint8 *p)
{
	uint8 aad[SMS4_WPI_MAX_AAD_LEN];
	uint8 *paad = aad;
	struct dot11_header *h = (struct dot11_header *)p;
	struct wpi_iv *iv_data;
	uint16 fc, seq;
	uint header_len, aad_len, qos_len = 0, hdr_add_len = 0;
	bool wds = FALSE;
	uint8 tmp[SMS4_BLOCK_SZ];
	int k;

	bzero(aad, SMS4_WPI_MAX_AAD_LEN);

	fc = ltoh16_ua(&(h->fc));

	/* WPI only supports protection of DATA frames */
	if (FC_TYPE(fc) != FC_TYPE_DATA)
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* frame must have Protected flag set */
	if (!(fc & FC_WEP))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	if (FC_SUBTYPE(fc) & FC_SUBTYPE_QOS_DATA)
		qos_len += 2;

	/* length of A4, if using wds frames */
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	if (wds)
		hdr_add_len += ETHER_ADDR_LEN;

	/* length of MPDU header, including PN */
	header_len = DOT11_A3_HDR_LEN + SMS4_WPI_IV_LEN + hdr_add_len + qos_len;

	/* pointer to IV */
	iv_data = (struct wpi_iv *)(p + DOT11_A3_HDR_LEN + qos_len + hdr_add_len);

	/* Payload must be > 0 bytes */
	if (data_len <= (header_len + SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* aad: maskedFC || A1 || A2 || maskedSC || A3 || A4 || KeyIdx || Reserved || L */

	fc &= SMS4_WPI_FC_MASK;
	*paad++ = (uint8)(fc & 0xff);
	*paad++ = (uint8)((fc >> 8) & 0xff);

	bcopy((uchar *)&h->a1, paad, 2*ETHER_ADDR_LEN);
	paad += 2*ETHER_ADDR_LEN;

	seq = ltoh16_ua(&(h->seq));
	seq &= FRAGNUM_MASK;

	*paad++ = (uint8)(seq & 0xff);
	*paad++ = (uint8)((seq >> 8) & 0xff);

	bcopy((uchar *)&h->a3, paad, ETHER_ADDR_LEN);
	paad += ETHER_ADDR_LEN;

	if (wds) {
		bcopy((uchar *)&h->a4, paad, ETHER_ADDR_LEN);
	}
	/* A4 for the MIC, even when there is no A4 in the packet */
	paad += ETHER_ADDR_LEN;

	if (qos_len) {
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len];
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len + 1];
	}

	*paad++ = iv_data->key_idx;
	*paad++ = iv_data->reserved;

	*paad++ = ((data_len - header_len - SMS4_WPI_CBC_MAC_LEN) >> 8) & 0xff;
	*paad++ = (data_len - header_len - SMS4_WPI_CBC_MAC_LEN) & 0xff;

	/* length of AAD */
	aad_len = SMS4_WPI_MIN_AAD_LEN + qos_len;

	for (k = 0; k < SMS4_BLOCK_SZ; k++)
		tmp[SMS4_BLOCK_SZ-(k+1)] = (iv_data->PN)[k];

	/* calculate MAC */
	if (sms4_wpi_cbc_mac(ick, tmp, aad_len, aad, p + header_len))
		return (SMS4_WPI_ENCRYPT_ERROR);

	/* encrypt data */
	if (sms4_ofb_crypt(ek, tmp, data_len - header_len, p + header_len))
		return (SMS4_WPI_ENCRYPT_ERROR);

	return (SMS4_WPI_SUCCESS);
}

/* SMS4-WPI mode decryption of 802.11 packet
 * 	- constructs aad and pn from provided frame
 * 	- calls sms4_ofb_crypt() to decrypt frame
 * 	- calls sms4_wpi_cbc_mac() to compute MAC
 * 	- decrypts in place
 * 	- data_len includes 802.11 header and CBC-MAC MAC
 *	- returns SMS4_WPI_DECRYPT_ERROR on general error
 */
int
sms4_wpi_pkt_decrypt(const uint8 *ek,
                          const uint8 *ick,
                          const size_t data_len,
                          uint8 *p)
{
	uint8 aad[SMS4_WPI_MAX_AAD_LEN];
	uint8 MAC[SMS4_WPI_CBC_MAC_LEN];
	uint8 *paad = aad;
	struct dot11_header *h = (struct dot11_header *)p;
	struct wpi_iv *iv_data;
	uint16 fc, seq;
	uint header_len, aad_len, qos_len = 0, hdr_add_len = 0;
	bool wds = FALSE;
	uint8 tmp[SMS4_BLOCK_SZ];
	int k;

	bzero(aad, SMS4_WPI_MAX_AAD_LEN);

	fc = ltoh16_ua(&(h->fc));

	/* WPI only supports protection of DATA frames */
	if (FC_TYPE(fc) != FC_TYPE_DATA)
		return (SMS4_WPI_DECRYPT_ERROR);

	/* frame must have Protected flag set */
	if (!(fc & FC_WEP))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	if ((FC_SUBTYPE(fc) & FC_SUBTYPE_QOS_DATA))
		qos_len += 2;

	/* length of A4, if using wds frames */
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	if (wds)
		hdr_add_len += ETHER_ADDR_LEN;

	/* length of MPDU header, including PN */
	header_len = DOT11_A3_HDR_LEN + SMS4_WPI_IV_LEN + hdr_add_len + qos_len;

	/* pointer to IV */
	iv_data = (struct wpi_iv *)(p + DOT11_A3_HDR_LEN + qos_len + hdr_add_len);

	/* Payload must be > 0 bytes plus MAC */
	if (data_len <= (header_len + SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* aad: maskedFC || A1 || A2 || maskedSC || A3 || A4 || KeyIdx || Reserved || L */

	fc &= SMS4_WPI_FC_MASK;
	*paad++ = (uint8)(fc & 0xff);
	*paad++ = (uint8)((fc >> 8) & 0xff);

	bcopy((uchar *)&h->a1, paad, 2*ETHER_ADDR_LEN);
	paad += 2*ETHER_ADDR_LEN;

	seq = ltoh16_ua(&(h->seq));
	seq &= FRAGNUM_MASK;

	*paad++ = (uint8)(seq & 0xff);
	*paad++ = (uint8)((seq >> 8) & 0xff);

	bcopy((uchar *)&h->a3, paad, ETHER_ADDR_LEN);
	paad += ETHER_ADDR_LEN;

	if (wds) {
		bcopy((uchar *)&h->a4, paad, ETHER_ADDR_LEN);
	}
	/* A4 for the MIC, even when there is no A4 in the packet */
	paad += ETHER_ADDR_LEN;

	if (qos_len) {
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len];
		*paad++ = p[DOT11_A3_HDR_LEN + hdr_add_len + 1];
	}

	*paad++ = iv_data->key_idx;
	*paad++ = iv_data->reserved;

	*paad++ = ((data_len - header_len - SMS4_WPI_CBC_MAC_LEN) >> 8) & 0xff;
	*paad++ = (data_len - header_len - SMS4_WPI_CBC_MAC_LEN) & 0xff;

	/* length of AAD */
	aad_len = SMS4_WPI_MIN_AAD_LEN + qos_len;

	for (k = 0; k < SMS4_BLOCK_SZ; k++)
		tmp[SMS4_BLOCK_SZ-(k+1)] = (iv_data->PN)[k];

	/* decrypt data */
	if (sms4_ofb_crypt(ek, tmp, data_len - header_len,
		p + header_len))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* store MAC */
	bcopy(p + data_len - SMS4_WPI_CBC_MAC_LEN, MAC, SMS4_WPI_CBC_MAC_LEN);

	/* calculate MAC */
	if (sms4_wpi_cbc_mac(ick, tmp, aad_len, aad, p + header_len))
		return (SMS4_WPI_DECRYPT_ERROR);

	/* compare MAC */
	if (bcmp(p + data_len - SMS4_WPI_CBC_MAC_LEN, MAC, SMS4_WPI_CBC_MAC_LEN))
		return (SMS4_WPI_CBC_MAC_ERROR);

	return (SMS4_WPI_SUCCESS);
}

#ifdef BCMSMS4_TEST
#include <stdlib.h>
#include <bcmcrypto/sms4_vectors.h>

#ifdef BCMSMS4_TEST_EMBED

/* Number of iterations is sensitive to the state of the test vectors since
 * encrypt and decrypt are done in place
 */
#define SMS4_TIMING_ITER	1001
#define	dbg(args)
#define pres(label, len, data)

/* returns current time in msec */
static void
get_time(uint *t)
{
	*t = hndrte_time();
}

#else
#ifdef BCMDRIVER

/* Number of iterations is sensitive to the state of the test vectors since
 * encrypt and decrypt are done in place
 */
#define SMS4_TIMING_ITER	1001
#define	dbg(args)		printk args
#define pres(label, len, data)

/* returns current time in msec */
static void
get_time(uint *t)
{
	*t = jiffies_to_msecs(jiffies);
}

#else

#define SMS4_TIMING_ITER	100001
#include <stdio.h>
#define	dbg(args)		printf args

#include <sys/time.h>

/* returns current time in msec */
static void
get_time(uint *t)
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	*t = ts.tv_sec * 1000 + ts.tv_usec / 1000;
}

void
pres(const char *label, const size_t len, const uint8 *data)
{
	int k;
	printf("%s\n", label);
	for (k = 0; k < len; k++) {
		printf("0x%02x, ", data[k]);
		if (!((k + 1) % (SMS4_BLOCK_SZ/2)))
			printf("\n");
	}
	printf("\n");
}
#endif /* BCMDRIVER */
#endif /* BCMSMS4_TEST_EMBED */

int
sms4_test_enc_dec()
{
	uint tstart, tend;
	int i, j, k, fail = 0;
	uint32 RK[32];
	uint32 X[SMS4_BLOCK_WORDS], Y[SMS4_BLOCK_WORDS];

	for (k = 0; k < NUM_SMS4_VECTORS; k++) {
		/* Note that the algorithm spec example output lists X[0] -
		 * X[32], but those should really be labelled X[4] - X[35]
		 * (they're the round output, not the input)
		 */

		dbg(("sms4_test_enc_dec: Instance %d:\n", k + 1));
		dbg(("sms4_test_enc_dec:   Plain Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     PlainText[%02d] = 0x%08x\n",
				i, sms4_vec[k].input[i]));

		dbg(("sms4_test_enc_dec:   Encryption Master Key:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     MK[%02d] = 0x%08x\n",
				i, sms4_vec[k].key[i]));

		sms4_key_exp(sms4_vec[k].key, RK);
		dbg(("sms4_test_enc_dec:   Round Key:\n"));
		for (i = 0; i < SMS4_RK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     rk[%02d] = 0x%08x\n",
				i, RK[i]));

		for (j = 0; j < SMS4_BLOCK_WORDS; j++)
			Y[j] = sms4_vec[k].input[j];
		get_time(&tstart);
		for (i = 0; i < *sms4_vec[k].niter; i++) {
			for (j = 0; j < SMS4_BLOCK_WORDS; j++)
				X[j] = Y[j];
			sms4_enc(Y, X, RK);
		}
		get_time(&tend);
		dbg(("sms4_test_enc_dec:   Cipher Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:     CipherText[%02d] = 0x%08x\n",
				i, Y[i]));
		dbg(("sms4_test_enc_dec:   Time for Instance %d Encrypt: %d msec\n",
			k + 1, tend - tstart));
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			if (Y[j] != sms4_vec[k].ref[j]) {
				dbg(("sms4_test_enc_dec: sms4_enc failed\n"));
				fail++;
			}
		}

		for (j = 0; j < SMS4_BLOCK_WORDS; j++)
			X[j] = sms4_vec[k].ref[j];
		get_time(&tstart);
		for (i = 0; i < *sms4_vec[k].niter; i++) {
			for (j = 0; j < SMS4_BLOCK_WORDS; j++)
				X[j] = Y[j];
			sms4_dec(Y, X, RK);
		}
		get_time(&tend);
		dbg(("sms4_test_enc_dec:  Decrypted Plain Text:\n"));
		for (i = 0; i < SMS4_BLOCK_WORDS; i++)
			dbg(("sms4_test_enc_dec:    PlainText[%02d] = 0x%08x\n", i, Y[i]));
		dbg(("sms4_test_enc_dec:  Time for Instance %d Decrypt: %d msec\n",
			k + 1, tend - tstart));
		for (j = 0; j < SMS4_BLOCK_WORDS; j++) {
			if (Y[j] != sms4_vec[k].input[j]) {
				dbg(("sms4_test_enc_dec: sms4_dec failed\n"));
				fail++;
			}
		}

		dbg(("\n"));
	}

	return (fail);
}

int
sms4_test_cbc_mac()
{
	int retv, k, fail = 0;

	uint8 mac[SMS4_WPI_CBC_MAC_LEN];

	for (k = 0; k < NUM_SMS4_CBC_MAC_VECTORS; k++) {
		dbg(("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC vector %d\n", k));
		retv = sms4_cbc_mac(sms4_cbc_mac_vec[k].ick,
			sms4_cbc_mac_vec[k].pn,
			sms4_cbc_mac_vec[k].al,
			sms4_cbc_mac_vec[k].input,
			mac);
		if (retv) {
			dbg(("sms4_test_cbc_mac: sms4_wpi_cbc_mac of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}

		pres("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC computed: ", SMS4_WPI_CBC_MAC_LEN,
			mac);
		pres("sms4_test_cbc_mac: SMS4-WPI-CBC-MAC reference: ", SMS4_WPI_CBC_MAC_LEN,
			sms4_cbc_mac_vec[k].ref);

		if (bcmp(mac, sms4_cbc_mac_vec[k].ref, SMS4_WPI_CBC_MAC_LEN) != 0) {
			dbg(("sms4_test_cbc_mac: sms4_wpi_cbc_mac of vector %d"
				" reference mismatch\n", k));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_ofb_crypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_OFB_VECTORS; k++) {
		dbg(("sms4_test_ofb_crypt: SMS4-OFB vector %d\n", k));
		retv = sms4_ofb_crypt(sms4_ofb_vec[k].ek,
			sms4_ofb_vec[k].pn,
			sms4_ofb_vec[k].il,
			sms4_ofb_vec[k].input);
		if (retv) {
			dbg(("sms4_test_ofb_crypt: encrypt of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}

		pres("sms4_test_ofb_crypt: SMS4-OFB ctxt: ",
			sms4_ofb_vec[k].il, sms4_ofb_vec[k].input);

		pres("sms4_test_ofb_crypt: SMS4-OFB ref: ",
			sms4_ofb_vec[k].il, sms4_ofb_vec[k].ref);

		if (bcmp(sms4_ofb_vec[k].input, sms4_ofb_vec[k].ref,
			sms4_ofb_vec[k].il) != 0) {
			dbg(("sms4_test_ofb_crypt: sms4_ofb_crypt of vector %d"
				" reference mismatch\n", k));
			fail++;
		}

		/* Run again to decrypt and restore vector */
		retv = sms4_ofb_crypt(sms4_ofb_vec[k].ek,
			sms4_ofb_vec[k].pn,
			sms4_ofb_vec[k].il,
			sms4_ofb_vec[k].input);
		if (retv) {
			dbg(("sms4_test_ofb_crypt: decrypt of vector %d returned error %d\n",
			     k, retv));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}



int
sms4_test_wpi_pkt_encrypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		dbg(("sms4_test_wpi_pkt_encrypt: SMS4-WPI packet vector %d\n", k));
		retv = sms4_wpi_pkt_encrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
		if (retv) {
			dbg(("sms4_test_wpi_pkt_encrypt: sms4_wpi_pkt_encrypt of vector %d"
				" returned error %d\n", k, retv));
			fail++;
		}

		pres("sms4_test_wpi_pkt_encrypt: SMS4-WPI ctxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);

		pres("sms4_test_wpi_pkt_encrypt: SMS4-WPI ref: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].ref);

		if (bcmp(sms4_wpi_pkt_vec[k].input, sms4_wpi_pkt_vec[k].ref,
			sms4_wpi_pkt_vec[k].il) != 0) {
			dbg(("sms4_test_wpi_pkt_encrypt: sms4_wpi_pkt_encrypt of vector %d"
				" reference mismatch\n", k));
			fail++;
		}
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_wpi_pkt_decrypt()
{
	int retv, k, fail = 0;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		dbg(("sms4_test_wpi_pkt_decrypt: SMS4-WPI packet vector %d\n", k));
		retv = sms4_wpi_pkt_decrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
		if (retv) {
			dbg(("sms4_test_wpi_pkt_decrypt: sms4_wpi_pkt_decrypt of vector %d"
				" returned error %d\n", k, retv));
			fail++;
		}

		pres("sms4_test_wpi_pkt_decrypt: SMS4-WPI ptxt: ",
			sms4_wpi_pkt_vec[k].il,
			sms4_wpi_pkt_vec[k].input);
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test_wpi_pkt_micfail()
{
	int retv, k, fail = 0;
	uint8 *pkt;

	for (k = 0; k < NUM_SMS4_WPI_PKT_VECTORS; k++) {
		/* copy the reference data, with an error in the last byte */
		pkt = malloc(sms4_wpi_pkt_vec[k].il);
		if (pkt == NULL) {
			dbg(("%s: out of memory\n", __FUNCTION__));
			fail++;
			return (fail);
		}

		bcopy(sms4_wpi_pkt_vec[k].ref, pkt, sms4_wpi_pkt_vec[k].il);

		/* create an error in the last byte of the MIC */
		pkt[sms4_wpi_pkt_vec[k].il - 1]++;

		/* decrypt */
		dbg(("sms4_test_wpi_pkt_decrypt: SMS4-WPI packet vector %d\n", k));
		retv = sms4_wpi_pkt_decrypt(sms4_wpi_pkt_vec[k].ek,
			sms4_wpi_pkt_vec[k].ick,
			sms4_wpi_pkt_vec[k].il,
			pkt);
		if (!retv) {
			dbg(("sms4_test_wpi_pkt_decrypt: sms4_wpi_pkt_decrypt of vector %d"
				" did not return expected error %d\n", k, retv));
			fail++;
		}

		free(pkt);
	}

	dbg(("\n"));
	return (fail);
}

int
sms4_test(int *t)
{
	int fail = 0;

	*t = 0;

	fail += sms4_test_enc_dec();
	fail += sms4_test_cbc_mac();
	fail += sms4_test_ofb_crypt();

	/* since encrypt and decrypt are done in place, and these
	 * functions use the same vectors, the tests must be run in order
	 */

	/* since encrypt and decrypt are done in place, and these
	 * functions use the same vectors, the tests must be run in order
	 */
	fail += sms4_test_wpi_pkt_encrypt();
	fail += sms4_test_wpi_pkt_decrypt();
	fail += sms4_test_wpi_pkt_micfail();

	return (fail);
}

#ifdef BCMSMS4_TEST_STANDALONE
int
main(int argc, char **argv)
{
	int fail = 0, t;

	fail += sms4_test(&t);

	dbg(("%s: timing result: %d msec\n", __FUNCTION__, t));
	fprintf(stderr, "%s: %s\n", *argv, fail ? "FAILED" : "PASSED");
	return (fail);

}
#endif /* BCMSMS4_TEST_STANDALONE */

#endif /* BCMSMS4_TEST */

#endif /* BCMWAPI_WPI */
