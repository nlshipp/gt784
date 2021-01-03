/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/****************************************************************************
 *
 * AdslFile.c -- Adsl driver file I/O functions
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.4 $
 *
 * $Id: AdslFile.c,v 1.4 2011/06/24 01:52:39 dkhoo Exp $
 *
 * $Log: AdslFile.c,v $
 * Revision 1.4  2011/06/24 01:52:39  dkhoo
 * [All]Per PLM, update DSL driver to 23j and bonding datapump to Av4bC035d
 *
 * Revision 1.1  2004/04/14 21:11:59  ilyas
 * Inial CVS checkin
 *
 ****************************************************************************/

#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#else
#define __KERNEL_SYSCALLS__
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#include <linux/autoconf.h>
#else
#include <linux/config.h>
#endif
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>

#include "AdslFile.h"

extern void * AdslCoreSetSdramImageAddr(ulong lmem2, ulong sdramSize);
extern void	AdslCoreSetXfaceOffset(unsigned long lmemAddr, unsigned long lmemSize);


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9))

int AdslFileLoadImage(char * fname, void *pAdslLMem, void *pAdslSDRAM)
{
	adslPhyImageHdr		phyHdr;
	struct file  			* fp;
	mm_segment_t		fs;
#if 1 && defined(LMEM_ACCESS_WORKAROUND)
	uint	lmemhdr[3];
#endif
	fp = filp_open(fname, O_RDONLY, 0);
	if (!fp || !fp->f_op || !fp->f_op->read) {
		printk("Unable to load '%s'.\n", fname);
		return 0;
	}

	fs= get_fs();
	set_fs(get_ds());

	fp->f_pos = 0;
	if (fp->f_op->read(fp, (void *)&phyHdr, sizeof(phyHdr), &fp->f_pos) != sizeof(phyHdr)) {
		printk("Failed to read image header from '%s'.\n", fname);
		filp_close(fp, NULL);
		return 0;
	}
#if 1 && defined(LMEM_ACCESS_WORKAROUND)
	fp->f_pos = phyHdr.lmemOffset;
	if (fp->f_op->read(fp, (void *)lmemhdr, 12, &fp->f_pos) != 12) {
		printk("Failed to read lmemhdr from '%s'.\n", fname);
		filp_close(fp, NULL);
		return 0;
	}
#endif
	fp->f_pos = phyHdr.lmemOffset;
	if (fp->f_op->read(fp, pAdslLMem, phyHdr.lmemSize, &fp->f_pos) != phyHdr.lmemSize) {
		printk("Failed to read LMEM image from '%s'.\n", fname);
		filp_close(fp, NULL);
		return 0;
	}
#if 1 && defined(LMEM_ACCESS_WORKAROUND)
	printk("lmemhdr[2]=0x%08X, pAdslLMem[2]=0x%08X\n", lmemhdr[2], ((uint *)pAdslLMem)[2]);
	pAdslSDRAM = AdslCoreSetSdramImageAddr(lmemhdr[2], phyHdr.sdramSize);
#else
	pAdslSDRAM = AdslCoreSetSdramImageAddr(((ulong*)pAdslLMem)[2], phyHdr.sdramSize);
#endif
	AdslCoreSetXfaceOffset((ulong)pAdslLMem, phyHdr.lmemSize);

	fp->f_pos = phyHdr.sdramOffset;
	if (fp->f_op->read(fp, pAdslSDRAM, phyHdr.sdramSize, &fp->f_pos) != phyHdr.sdramSize) {
		printk("Failed to read SDRAM image from '%s'.\n", fname);
		filp_close(fp, NULL);
		return 0;
	}
	
	filp_close(fp, NULL);
	set_fs(fs);
	return phyHdr.lmemSize + phyHdr.sdramSize;
}


#else


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define	adslf_open		sys_open
#define	adslf_read		sys_read
#define	adslf_close		sys_close
#define	adslf_lseek		sys_lseek
#else
#define	adslf_open		open
#define	adslf_read		read
#define	adslf_close		close
#define	adslf_lseek		lseek

static int errno;
#endif

int AdslFileLoadImage(char * fname, void *pAdslLMem, void *pAdslSDRAM)
{
	adslPhyImageHdr		phyHdr;
	int			fd;
	mm_segment_t		fs = get_fs();
	set_fs(get_ds());

	fd = adslf_open(fname, 0, 0);
	if (fd == -1) {
		printk("Unable to load '%s'.\n", fname);
		return 0;
	}

	if (adslf_read(fd, (void *)&phyHdr, sizeof(phyHdr)) != sizeof(phyHdr)) {
		printk("Failed to read image header from '%s'.\n", fname);
		adslf_close(fd);
		return 0;
	}

	adslf_lseek(fd, phyHdr.lmemOffset, 0);
	if (adslf_read(fd, pAdslLMem, phyHdr.lmemSize) != phyHdr.lmemSize) {
		printk("Failed to read LMEM image from '%s'.\n", fname);
		adslf_close(fd);
		return 0;
	}

	pAdslSDRAM = AdslCoreSetSdramImageAddr(((ulong*)pAdslLMem)[2], phyHdr.sdramSize);
	AdslCoreSetXfaceOffset((ulong)pAdslLMem, phyHdr.lmemSize);
	
	adslf_lseek(fd, phyHdr.sdramOffset, 0);
	if (adslf_read(fd, pAdslSDRAM, phyHdr.sdramSize) != phyHdr.sdramSize) {
		printk("Failed to read SDRAM image from '%s'.\n", fname);
		adslf_close(fd);
		return 0;
	}

#if 0
	printk ("Image Load: LMEM=(0x%lX, %ld,%ld) SDRAM=(0x%lX, %ld,%ld)\n", 
		pAdslLMem, phyHdr.lmemOffset, phyHdr.lmemSize, 
		pAdslSDRAM, phyHdr.sdramOffset, phyHdr.sdramSize);
#endif
	adslf_close(fd);
	set_fs(fs);
	return phyHdr.lmemSize + phyHdr.sdramSize;
}


#endif

