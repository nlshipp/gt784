/*
 * Shell-like utility functions
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wapi_utils.h,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */

#ifndef _WAPI_UTILS_H_
#define _WAPI_UTILS_H_

/* WAPI ramfs directories */
#define RAMFS_WAPI_DIR			__CONFIG_WAPI_CONF__
#define CONFIG_DIR			"config"
#define WAPI_WAI_DIR			RAMFS_WAPI_DIR"/"CONFIG_DIR
#define WAPI_AS_DIR			RAMFS_WAPI_DIR"/"CONFIG_DIR"/as1000"

#define WAPI_TGZ_TMP_FILE		RAMFS_WAPI_DIR"/config.tgz"

#define WAPI_AS_CER_FILE		WAPI_AS_DIR"/as.cer"

/* WAPI partition magic number: "wapi" */
#define WAPI_MTD_MAGIC			"\077\061\070\069"

typedef struct {
	unsigned int magic;
	unsigned int len;
	unsigned short checksum;
} wapi_mtd_hdr_t;

/*
 * Write a file to an MTD device
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int wapi_mtd_backup();

/*
 * Read MTD device to file
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int wapi_mtd_restore();

#endif /* _WAPI_UTILS_H_ */
