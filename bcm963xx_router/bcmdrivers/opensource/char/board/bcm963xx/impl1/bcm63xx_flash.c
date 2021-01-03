/*
<:copyright-gpl
 Copyright 2002 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/
/*
 ***************************************************************************
 * File Name  : bcm63xx_flash.c
 *
 * Description: This file contains the flash device driver APIs for bcm63xx board. 
 *
 * Created on :  8/10/2002  seanl:  use cfiflash.c, cfliflash.h (AMD specific)
 *
 ***************************************************************************/


/* Includes. */
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/preempt.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/jffs2.h>
#include <linux/mount.h>
#include <linux/crc32.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#include <bcm_map_part.h>
#include <board.h>
#ifndef AEI_VDSL_CUSTOMER_QWEST_Q1000
#define BCMTAG_EXE_USE
#endif
#include <bcmTag.h>
#include "flash_api.h"
#include "boardparms.h"
#include "boardparms_voice.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#include <linux/fs_struct.h>
#endif
// #define DEBUG_FLASH

/* Only NAND flash build configures and uses CONFIG_CRC32. */
#if !defined(CONFIG_CRC32)
#undef crc32
#define crc32(a,b,c) 0
#endif

extern PFILE_TAG kerSysImageTagGet(void);
extern int jffs2_remount_fs (struct super_block *sb, int *flags, char *data);
int fsync_super(struct super_block *sb);

NVRAM_DATA bootNvramData;
static FLASH_ADDR_INFO fInfo;
static struct semaphore semflash;
static char bootCfeVersion[CFE_VERSION_MARK_SIZE+CFE_VERSION_SIZE];
static int bootFromNand = 0;

static int setScratchPad(char *buf, int len);
static char *getScratchPad(int len);

#define ALLOC_TYPE_KMALLOC   0
#define ALLOC_TYPE_VMALLOC   1

void *retriedKmalloc(size_t size)
{
    void *pBuf;
    unsigned char *bufp8 ;

    size += 4 ; /* 4 bytes are used to store the housekeeping information used for freeing */

    // Memory allocation changed from kmalloc() to vmalloc() as the latter is not susceptible to memory fragmentation under low memory conditions
    // We have modified Linux VM to search all pages by default, it is no longer necessary to retry here
    if (!in_interrupt() ) {
        pBuf = vmalloc(size);
        if (pBuf) {
            memset(pBuf, 0, size);
            bufp8 = (unsigned char *) pBuf ;
            *bufp8 = ALLOC_TYPE_VMALLOC ;
            pBuf = bufp8 + 4 ;
        }
    }
    else { // kmalloc is still needed if in interrupt
        pBuf = kmalloc(size, GFP_ATOMIC);
        if (pBuf) {
            memset(pBuf, 0, size);
            bufp8 = (unsigned char *) pBuf ;
            *bufp8 = ALLOC_TYPE_KMALLOC ;
            pBuf = bufp8 + 4 ;
        }
    }

    return pBuf;
}

void retriedKfree(void *pBuf)
{
    unsigned char *bufp8  = (unsigned char *) pBuf ;
    bufp8 -= 4 ;

    if (*bufp8 == ALLOC_TYPE_KMALLOC)
        kfree(bufp8);
    else
        vfree(bufp8);
}

// get shared blks into *** pTempBuf *** which has to be released bye the caller!
// return: if pTempBuf != NULL, poits to the data with the dataSize of the buffer
// !NULL -- ok
// NULL  -- fail
static char *getSharedBlks(int start_blk, int num_blks)
{
    int i = 0;
    int usedBlkSize = 0;
    int sect_size = 0;
    char *pTempBuf = NULL;
    char *pBuf = NULL;

    down(&semflash);

    for (i = start_blk; i < (start_blk + num_blks); i++)
        usedBlkSize += flash_get_sector_size((unsigned short) i);

    if ((pTempBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
        printk("failed to allocate memory with size: %d\n", usedBlkSize);
        up(&semflash);
        return pTempBuf;
    }
    
    pBuf = pTempBuf;
    for (i = start_blk; i < (start_blk + num_blks); i++)
    {
        sect_size = flash_get_sector_size((unsigned short) i);

#if defined(DEBUG_FLASH)
        printk("getSharedBlks: blk=%d, sect_size=%d\n", i, sect_size);
#endif
        flash_read_buf((unsigned short)i, 0, pBuf, sect_size);
        pBuf += sect_size;
    }
    up(&semflash);
    
    return pTempBuf;
}

// Set the pTempBuf to flash from start_blk for num_blks
// return:
// 0 -- ok
// -1 -- fail
static int setSharedBlks(int start_blk, int num_blks, char *pTempBuf)
{
    int i = 0;
    int sect_size = 0;
    int sts = 0;
    char *pBuf = pTempBuf;

    down(&semflash);

    for (i = start_blk; i < (start_blk + num_blks); i++)
    {
        sect_size = flash_get_sector_size((unsigned short) i);
        flash_sector_erase_int(i);

        if (flash_write_buf(i, 0, pBuf, sect_size) != sect_size)
        {
            printk("Error writing flash sector %d.", i);
            sts = -1;
            break;
        }

#if defined(DEBUG_FLASH)
        printk("setShareBlks: blk=%d, sect_size=%d\n", i, sect_size);
#endif

        pBuf += sect_size;
    }

    up(&semflash);

    return sts;
}

#if !defined(CONFIG_BRCM_IKOS)
// Initialize the flash and fill out the fInfo structure
void kerSysEarlyFlashInit( void )
{
    flash_init();
    bootFromNand = 0;

#if defined(CONFIG_BCM96368)
    if( ((GPIO->StrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) == MISC_STRAP_BUS_BOOT_NAND)
        bootFromNand = 1;
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96816)
    if( ((MISC->miscStrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) == MISC_STRAP_BUS_BOOT_NAND )
        bootFromNand = 1;
#endif

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96816)
    if( bootFromNand == 1 )
    {
        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 0x101;
        NAND->NandCsNandXor = 1;

        memcpy((unsigned char *)&bootCfeVersion, (unsigned char *)
            FLASH_BASE + CFE_VERSION_OFFSET, sizeof(bootCfeVersion));
        memcpy((unsigned char *)&bootNvramData, (unsigned char *)
            FLASH_BASE + NVRAM_DATA_OFFSET, sizeof(NVRAM_DATA));

        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 0x2;
        NAND->NandCsNandXor = 0;
    }
    else
#endif
    {
        fInfo.flash_rootfs_start_offset = flash_get_sector_size(0);
        if( fInfo.flash_rootfs_start_offset < FLASH_LENGTH_BOOT_ROM )
            fInfo.flash_rootfs_start_offset = FLASH_LENGTH_BOOT_ROM;
     
        flash_read_buf (NVRAM_SECTOR, CFE_VERSION_OFFSET,
            (unsigned char *)&bootCfeVersion, sizeof(bootCfeVersion));

        /* Read the flash contents into NVRAM buffer */
        flash_read_buf (NVRAM_SECTOR, NVRAM_DATA_OFFSET,
            (unsigned char *)&bootNvramData, sizeof (NVRAM_DATA)) ;
    }

#ifdef AEI_VDSL_CUSTOMER_NCS
    /* Enable Backup PSI by default */
    bootNvramData.backupPsi = 0x01;
#endif

#if defined(DEBUG_FLASH)
    printk("reading nvram into bootNvramData\n");
    printk("ulPsiSize 0x%x\n", (unsigned int)bootNvramData.ulPsiSize);
    printk("backupPsi 0x%x\n", (unsigned int)bootNvramData.backupPsi);
    printk("ulSyslogSize 0x%x\n", (unsigned int)bootNvramData.ulSyslogSize);
#endif

    if ((BpSetBoardId(bootNvramData.szBoardId) != BP_SUCCESS))
        printk("\n*** Board is not initialized properly ***\n\n");
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
    if ((BpSetVoiceBoardId(bootNvramData.szVoiceBoardId) != BP_SUCCESS))
        printk("\n*** Voice Board id is not initialized properly ***\n\n");
#endif
}

/***********************************************************************
 * Function Name: kerSysCfeVersionGet
 * Description  : Get CFE Version.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysCfeVersionGet(char *string, int stringLength)
{
    memcpy(string, (unsigned char *)&bootCfeVersion, stringLength);
    return(0);
}

/*******************************************************************************
 * NVRAM functions
 *******************************************************************************/
// get nvram data
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if (bootFromNand) {
        memcpy(string, (unsigned char *) &bootNvramData, strLen);
    }
    else {
        if ((pBuf = getSharedBlks(NVRAM_SECTOR, 1)) == NULL)
            return -1;

        // get string off the memory buffer
        memcpy(string, (pBuf + NVRAM_DATA_OFFSET + offset), strLen);

        retriedKfree(pBuf);
    }

    return 0;
}

// set nvram 
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if (bootFromNand == 0)
    {
        if ((pBuf = getSharedBlks(NVRAM_SECTOR, 1)) == NULL)
            return -1;

        // set string to the memory buffer
        memcpy((pBuf + NVRAM_DATA_OFFSET + offset), string, strLen);

        if (setSharedBlks(NVRAM_SECTOR, 1, pBuf) != 0)
            sts = -1;
    
        retriedKfree(pBuf);
    }
    else
        sts = -1;

    return sts;
}

/***********************************************************************
 * Function Name: kerSysEraseNvRam
 * Description  : Erase the NVRAM storage section of flash memory.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysEraseNvRam(void)
{
    int sts = 1;

    char *tempStorage = retriedKmalloc(NVRAM_LENGTH);
    
    if (bootFromNand == 0)
    {
        // just write the whole buf with '0xff' to the flash
        if (!tempStorage)
            sts = 0;
        else
        {
            memset(tempStorage, 0xff, NVRAM_LENGTH);
            if (kerSysNvRamSet(tempStorage, NVRAM_LENGTH, 0) != 0)
                sts = 0;
            retriedKfree(tempStorage);
        }
    }
    else
        sts = 0;

    return sts;
}

#if !defined(AEI_CONFIG_AUXFS_JFFS2)
unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    int sect = flash_get_blk((int) fromaddr);
    unsigned char *start = flash_get_memptr(sect);
    flash_read_buf( sect, (int) fromaddr - (int) start, toaddr, len );

    return( len );
}
#endif

#else // CONFIG_BRCM_IKOS
static NVRAM_DATA ikos_nvram_data =
    {
    NVRAM_VERSION_NUMBER,
    "",
    "ikos",
    0,
    DEFAULT_PSI_SIZE,
    11,
    {0x02, 0x10, 0x18, 0x01, 0x00, 0x01},
    0x00, 0x00,
    0x720c9f60
    };

void kerSysEarlyFlashInit( void )
{
    memcpy((unsigned char *)&bootNvramData, (unsigned char *)&ikos_nvram_data,
        sizeof (NVRAM_DATA));
    fInfo.flash_scratch_pad_length = 0;
    fInfo.flash_persistent_start_blk = 0;
}

int kerSysCfeVersionGet(char *string, int stringLength)
{
    *string = '\0';
    return(0);
}

int kerSysNvRamGet(char *string, int strLen, int offset)
{
    memcpy(string, (unsigned char *) &ikos_nvram_data, sizeof(NVRAM_DATA));
    return(0);
}

int kerSysNvRamSet(char *string, int strLen, int offset)
{
    return(0);
}

int kerSysEraseNvRam(void)
{
    return(0);
}

unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    return(memcpy((unsigned char *) toaddr, (unsigned char *) fromaddr, len));
}
#endif  // CONFIG_BRCM_IKOS
#ifndef AEI_VDSL_CUSTOMER_QWEST_Q1000
static UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc)
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}
static void kerSysUpdateNvramData()
{
    NVRAM_DATA  nvramData;

    kerSysNvRamGet((char *)&nvramData, sizeof(NVRAM_DATA), 0);
    if (5 == nvramData.ulVersion) {
        NVRAM_DATA_OLD  nvramDataOld;
        UINT32 crc = CRC32_INIT_VALUE;

        printk("\n\n ###################################\n");
        printk(" Update NVRAM from version = %ld to version = %ld\n", nvramData.ulVersion, NVRAM_VERSION_NUMBER);
        printk("####################################\n\n");
        kerSysNvRamGet((char *)&nvramDataOld, sizeof(NVRAM_DATA_OLD), 0);
        memcpy(nvramData.ulSerialNumber, nvramDataOld.ulSerialNumber, 32);
        memcpy(nvramData.chFactoryFWVersion, nvramDataOld.chFactoryFWVersion, 48);
        memcpy(nvramData.wpsPin, nvramDataOld.wpsPin, 32);
        memcpy(nvramData.wpaKey, nvramDataOld.wpaKey, 32);
        nvramData.ulVersion = NVRAM_VERSION_NUMBER;
        nvramData.ulCheckSum = 0;
        crc = getCrc32((char *)&nvramData, sizeof(NVRAM_DATA), crc);
        nvramData.ulCheckSum = crc;
        kerSysNvRamSet((char *)&nvramData, sizeof(NVRAM_DATA), 0);
    }
}
#endif
void kerSysFlashInit( void )
{
    sema_init(&semflash, 1);
    flash_init_info(&bootNvramData, &fInfo);
#ifndef AEI_VDSL_CUSTOMER_QWEST_Q1000    
    kerSysUpdateNvramData();
#endif
}

/***********************************************************************
 * Function Name: kerSysFlashAddrInfoGet
 * Description  : Fills in a structure with information about the NVRAM
 *                and persistent storage sections of flash memory.  
 *                Fro physmap.c to mount the fs vol.
 * Returns      : None.
 ***********************************************************************/
void kerSysFlashAddrInfoGet(PFLASH_ADDR_INFO pflash_addr_info)
{
    memcpy(pflash_addr_info, &fInfo, sizeof(FLASH_ADDR_INFO));
}

/*******************************************************************************
 * PSI functions
 *******************************************************************************/
#define PSI_FILE_NAME           "/data/psi"
#define PSI_BACKUP_FILE_NAME    "/data/psibackup"
#define SCRATCH_PAD_FILE_NAME   "/data/scratchpad"


// get psi data
// return:
//  0 - ok
//  -1 - fail
int kerSysPersistentGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;
    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read PSI from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;
        int len;

        memset(string, 0x00, strLen);
        fp = filp_open(PSI_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((len = (int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos)) <= 0)
                printk("Failed to read psi from '%s'\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        fInfo.flash_persistent_number_blk)) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + fInfo.flash_persistent_blk_offset + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}

int kerSysBackupPsiGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read backup PSI from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;
        int len;

        memset(string, 0x00, strLen);
        fp = filp_open(PSI_BACKUP_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((len = (int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos)) <= 0)
                printk("Failed to read psi from '%s'\n", PSI_BACKUP_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if (fInfo.flash_backup_psi_number_blk <= 0)
    {
        printk("No backup psi blks allocated, change it in CFE\n");
        return -1;
    }

    if (fInfo.flash_persistent_start_blk == 0)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_backup_psi_start_blk,
        (fInfo.flash_backup_psi_start_blk + fInfo.flash_backup_psi_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}


// set psi 
// return:
//  0 - ok
//  -1 - fail
int kerSysPersistentSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_FILE_NAME);

        return 0;
    }
    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        fInfo.flash_persistent_number_blk)) == NULL)
        return -1;

    // set string to the memory buffer
    memcpy((pBuf + fInfo.flash_persistent_blk_offset + offset), string, strLen);

		//	printk(KERN_EMERG"pt_blk=%d,pnblk=%d,poff=%d,len=%d\n",fInfo.flash_persistent_start_blk, fInfo.flash_persistent_number_blk,fInfo.flash_persistent_blk_offset,fInfo.flash_persistent_length);
		//	printk(KERN_EMERG"st_blk=%d,snblk=%d,soff=%d,len=%d\n",fInfo.flash_scratch_pad_start_blk, fInfo.flash_scratch_pad_number_blk,fInfo.flash_scratch_pad_blk_offset,fInfo.flash_scratch_pad_length);

    if (setSharedBlks(fInfo.flash_persistent_start_blk, 
        fInfo.flash_persistent_number_blk, pBuf) != 0)
        sts = -1;
    
    retriedKfree(pBuf);


    return sts;
}

int kerSysBackupPsiSet(char *string, int strLen, int offset)
{
    int i;
    int sts = 0;
    int usedBlkSize = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write backup PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_BACKUP_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_BACKUP_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_BACKUP_FILE_NAME);


        return 0;
    }

    if (fInfo.flash_backup_psi_number_blk <= 0)
    {
        printk("No backup psi blks allocated, change it in CFE\n");
        return -1;
    }

    if (fInfo.flash_persistent_start_blk == 0)
        return -1;

    /*
     * The backup PSI does not share its blocks with anybody else, so I don't have
     * to read the flash first.  But now I have to make sure I allocate a buffer
     * big enough to cover all blocks that the backup PSI spans.
     */
    for (i=fInfo.flash_backup_psi_start_blk;
         i < (fInfo.flash_backup_psi_start_blk + fInfo.flash_backup_psi_number_blk); i++)
    {
       usedBlkSize += flash_get_sector_size((unsigned short) i);
    }

#ifdef AEI_VDSL_CUSTOMER_NCS
    if (strLen + offset > usedBlkSize)
        return -1;
#endif

    if ((pBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
       printk("failed to allocate memory with size: %d\n", usedBlkSize);
       return -1;
    }

    memset(pBuf, 0, usedBlkSize);

    // set string to the memory buffer
    memcpy((pBuf + offset), string, strLen);
    if (setSharedBlks(fInfo.flash_backup_psi_start_blk, 
       ( fInfo.flash_backup_psi_number_blk), pBuf) != 0)
       sts = -1;
    
    retriedKfree(pBuf);

    return sts;
}

#define SYSLOG_FILE_NAME        "/etc/syslog"

int kerSysSyslogGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read syslog from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        memset(string, 0x00, strLen);
        fp = filp_open(SYSLOG_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos) <= 0)
                printk("Failed to read psi from '%s'\n", SYSLOG_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if (fInfo.flash_syslog_number_blk <= 0)
    {
        printk("No syslog blks allocated, change it in CFE\n");
        return -1;
    }
    
    if (strLen > fInfo.flash_syslog_length)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_syslog_start_blk,
        (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}

int kerSysSyslogSet(char *string, int strLen, int offset)
{
    int i;
    int sts = 0;
    int usedBlkSize = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_FILE_NAME);

        return 0;
    }

    if (fInfo.flash_syslog_number_blk <= 0)
    {
        printk("No syslog blks allocated, change it in CFE\n");
        return -1;
    }
    
    if (strLen > fInfo.flash_syslog_length)
        return -1;

    /*
     * The syslog does not share its blocks with anybody else, so I don't have
     * to read the flash first.  But now I have to make sure I allocate a buffer
     * big enough to cover all blocks that the syslog spans.
     */
    for (i=fInfo.flash_syslog_start_blk;
         i < (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk); i++)
    {
        usedBlkSize += flash_get_sector_size((unsigned short) i);
    }

    if ((pBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
       printk("failed to allocate memory with size: %d\n", usedBlkSize);
       return -1;
    }

    memset(pBuf, 0, usedBlkSize);

    // set string to the memory buffer
    memcpy((pBuf + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_syslog_start_blk, 
        (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk), pBuf) != 0)
        sts = -1;

    retriedKfree(pBuf);

    return sts;
}


#define je16_to_cpu(x) ((x).v16)
#define je32_to_cpu(x) ((x).v32)

/*
 * nandUpdateSeqNum
 * 
 * Read the sequence number from each rootfs partition.  The sequence number is
 * the extension on the cferam file.  Add one to the highest sequence number
 * and change the extenstion of the cferam in the image to be flashed to that
 * number.
 */
static int nandUpdateSeqNum(unsigned char *imagePtr, int imageSize, int blkLen)
{
    char fname[] = NAND_CFE_RAM_NAME;
    int fname_actual_len = strlen(fname);
    int fname_cmp_len = strlen(fname) - 3; /* last three are digits */
    char cferam_base[32], cferam_buf[32], cferam_fmt[32]; 
    int i;
    struct file *fp;
    int seq = -1;
    int ret = 1;


    strcpy(cferam_base, fname);
    cferam_base[fname_cmp_len] = '\0';
    strcpy(cferam_fmt, cferam_base);
    strcat(cferam_fmt, "%3.3d");

    /* Find the sequence number of the partion that is booted from. */
    for( i = 0; i < 999; i++ )
    {
        sprintf(cferam_buf, cferam_fmt, i);
        fp = filp_open(cferam_buf, O_RDONLY, 0);
        if (!IS_ERR(fp) )
        {
            filp_close(fp, NULL);

            /* Seqence number found. */
            seq = i;
            break;
        }
    }

    /* Find the sequence number of the partion that is not booted from. */
    if( do_mount("mtd:rootfs_update", "/mnt", "jffs2", MS_RDONLY, NULL) == 0 )
    {
        strcpy(cferam_fmt, "/mnt/");
        strcat(cferam_fmt, cferam_base);
        strcat(cferam_fmt, "%3.3d");

        for( i = 0; i < 999; i++ )
        {
            sprintf(cferam_buf, cferam_fmt, i);
            fp = filp_open(cferam_buf, O_RDONLY, 0);
            if (!IS_ERR(fp) )
            {
                filp_close(fp, NULL);
                /*Seq number found. Take the greater of the two seq numbers.*/
                if( seq < i )
                    seq = i;
                break;
            }
        }
    }

    if( seq != -1 )
    {
        unsigned char *buf, *p;
        int len = blkLen;
        struct jffs2_raw_dirent *pdir;
        unsigned long version = 0;
        int done = 0;

        if( *(unsigned short *) imagePtr != JFFS2_MAGIC_BITMASK )
        {
            imagePtr += len;
            imageSize -= len;
        }

        /* Increment the new highest sequence number. Add it to the CFE RAM
         * file name.
         */
        seq++;

        /* Search the image and replace the last three characters of file
         * cferam.000 with the new sequence number.
         */
        for(buf = imagePtr; buf < imagePtr+imageSize && done == 0; buf += len)
        {
            p = buf;
            while( p < buf + len )
            {
                pdir = (struct jffs2_raw_dirent *) p;
                if( je16_to_cpu(pdir->magic) == JFFS2_MAGIC_BITMASK )
                {
                    if( je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT &&
                        fname_actual_len == pdir->nsize &&
                        !memcmp(fname, pdir->name, fname_cmp_len) &&
                        je32_to_cpu(pdir->version) > version &&
                        je32_to_cpu(pdir->ino) != 0 )
                     {
                        /* File cferam.000 found. Change the extension to the
                         * new sequence number and recalculate file name CRC.
                         */
                        p = pdir->name + fname_cmp_len;
                        p[0] = (seq / 100) + '0';
                        p[1] = ((seq % 100) / 10) + '0';
                        p[2] = ((seq % 100) % 10) + '0';
                        p[3] = '\0';

                        je32_to_cpu(pdir->name_crc) =
                            crc32(0, pdir->name, (unsigned int)
                            fname_actual_len);

                        version = je32_to_cpu(pdir->version);

                        /* Setting 'done = 1' assumes there is only one version
                         * of the directory entry.
                         */
                        done = 1;
                        ret = (buf - imagePtr) / len; /* block number */
                        break;
                    }

                    p += (je32_to_cpu(pdir->totlen) + 0x03) & ~0x03;
                }
                else
                {
                    done = 1;
                    break;
                }
            }
        }
    }

    return(ret);
}

#if defined(AEI_CONFIG_AUXFS_JFFS2)
/* Erase the specified NAND flash block but preserve the spare area. */
static int nandEraseBlkNotSpare( struct mtd_info *mtd, int blk )
{
    int sts = -1;

    /* block_is bad returns 0 if block is not bad */
    if( mtd->block_isbad(mtd, blk) == 0 )
    {
        unsigned char oobbuf[64]; /* expected to be a max size */
        struct mtd_oob_ops ops;

        memset(&ops, 0x00, sizeof(ops));
        ops.ooblen = mtd->oobsize;
        ops.ooboffs = 0;
        ops.datbuf = NULL;
        ops.oobbuf = oobbuf;
        ops.len = 0;
        ops.mode = MTD_OOB_PLACE;

        /* Read and save the spare area. */
        sts = mtd->read_oob(mtd, blk, &ops);
        if( sts == 0 )
        {
            struct erase_info erase;

            /* Erase the flash block. */
            memset(&erase, 0x00, sizeof(erase));
            erase.addr = blk;
            erase.len = mtd->erasesize;
            erase.mtd = mtd;

            sts = mtd->erase(mtd, &erase);
            if( sts == 0 )
            {
                int i;

                /* Function local_bh_disable has been called and this
                 * is the only operation that should be occurring.
                 * Therefore, spin waiting for erase to complete.
                 */
                for(i = 0; i < 10000 && erase.state != MTD_ERASE_DONE &&
                    erase.state != MTD_ERASE_FAILED; i++ )
                {
                    udelay(100);
                }

                if( erase.state != MTD_ERASE_DONE )
                    sts = -1;
            }

            if( sts == 0 )
            {
                memset(&ops, 0x00, sizeof(ops));
                ops.ooblen = mtd->oobsize;
                ops.ooboffs = 0;
                ops.datbuf = NULL;
                ops.oobbuf = oobbuf;
                ops.len = 0;
                ops.mode = MTD_OOB_PLACE;

                /* Restore the spare area. */
                if( (sts = mtd->write_oob(mtd, blk, &ops)) != 0 )
                    printk("nandImageSet - Block 0x%8.8x. Error writing spare "
                        "area.\n", blk);
            }
            else
                printk("nandImageSet - Block 0x%8.8x. Error erasing block.\n",blk);
        }
        else
            printk("nandImageSet - Block 0x%8.8x. Error read spare area.\n", blk);
    }

    return( sts );
}
#endif

// NAND flash bcm image 
// return: 
// 0 - ok
// !0 - the sector number fail to be flashed (should not be 0)
static int nandImageSet( int flash_start_addr, char *string, int img_size )
{
    int sts = -1;
    int blk = 0;
    int i; 
    struct mtd_info *mtd = NULL, *mtd0 = NULL;
    struct mtd_info *mtd1 = get_mtd_device_nm("nvram");
    char *end_string = string + img_size;
#if defined(AEI_CONFIG_AUXFS_JFFS2)
    int         cferam_blk, fs_start_blk, ofs, retlen = 0;
    char        *cferam_string;
    /* Allow room to flash cferam sequence number at start of file system. */
    const int   fs_start_blk_num = 8;
#endif

    if( mtd1 )
    {
        int blksize = mtd1->erasesize / 1024;
        WFI_TAG wt;

        memcpy(&wt, end_string, sizeof(wt));
        if( (wt.wfiVersion & WFI_ANY_VERS_MASK) == WFI_ANY_VERS &&
            ((blksize == 16 && wt.wfiFlashType != WFI_NAND16_FLASH) ||
             (blksize == 128 && wt.wfiFlashType != WFI_NAND128_FLASH)) )
        {
            printk("\nERROR: NAND flash block size does not match image "
                "block size\n\n");
        }
        else
        {
            mtd0 = get_mtd_device_nm("rootfs_update");

            if( mtd0 == NULL || mtd0->size == 0LL )
            {
                /* Flash device is configured to use only one file system. */
                if( mtd0 )
                    put_mtd_device(mtd0);
                mtd0 = get_mtd_device_nm("rootfs");
            }
        }
    }

    if( mtd0 && mtd1 )
    {
        // Disable other tasks from this point on
#if defined(CONFIG_SMP)
        smp_send_stop();
        udelay(20);
#endif
   
#if !defined(AEI_CONFIG_AUXFS_JFFS2)
        nandUpdateSeqNum((unsigned char *) string, img_size, mtd0->erasesize);
        local_bh_disable();
#endif 
        if( *(unsigned short *) string == JFFS2_MAGIC_BITMASK ) {
           /* Image only contains file system. */
            mtd = mtd0; /* only flash file system */
#if defined(AEI_CONFIG_AUXFS_JFFS2)
            ofs = 0; /* use entire string image to find sequence number */
#endif
        }
        else
        {
            PNVRAM_DATA pnd = (PNVRAM_DATA) (string + NVRAM_DATA_OFFSET);
            char *p;
#if defined(AEI_CONFIG_AUXFS_JFFS2)
            ofs = mtd0->erasesize; /* skip block 0 to find sequence number */
#endif
            mtd = mtd1; /* flash first boot block and then file system */

            /* Copy NVRAM data to block to be flashed so it is preserved. */
            memcpy((unsigned char *) pnd, (unsigned char *) &bootNvramData,
                sizeof(NVRAM_DATA));

            for( p = pnd->szBootline; p[2] != '\0'; p++ )
            {
                if( p[0] == 'p' && p[1] == '=' && p[2] != BOOT_LATEST_IMAGE )
                {
                    // Change boot partition to boot from new image.
                    p[2] = BOOT_LATEST_IMAGE;
                    break;
                }
            }

            /* Recalculate the nvramData CRC. */
            pnd->ulCheckSum = 0;
            pnd->ulCheckSum = crc32(CRC32_INIT_VALUE, pnd, sizeof(NVRAM_DATA));
        }
#if defined(AEI_CONFIG_AUXFS_JFFS2)
        /* Update the sequence number that replaces that extension in file
         * cferam.000
         */
        cferam_blk = nandUpdateSeqNum((unsigned char *) string + ofs,
            img_size - ofs, mtd0->erasesize) * mtd0->erasesize;
        cferam_string = string + ofs + cferam_blk;

        fs_start_blk = fs_start_blk_num * mtd0->erasesize;

        local_bh_disable();

        if( *(unsigned short *) string != JFFS2_MAGIC_BITMASK )
        {
            /* Flash the CFE ROM boot loader. */
            nandEraseBlkNotSpare( mtd1, 0 );
            mtd1->write(mtd1, 0, mtd1->erasesize, &retlen, string);
            string += mtd1->erasesize;
        }

        /* Erase block with sequence number before flashing the image. */
        nandEraseBlkNotSpare( mtd0, cferam_blk );

        /* Flash the image except for the part with the sequence number. */
        for( blk = fs_start_blk; blk < mtd0->size; blk += mtd0->erasesize )
        {
            if( (sts = nandEraseBlkNotSpare( mtd0, blk )) == 0 )
            {
                /* Write a block of the image to flash. */
                if( string < end_string && string != cferam_string )
                {
                    mtd0->write(mtd0, blk, mtd0->erasesize,
                        &retlen, string);

                    if( retlen == mtd0->erasesize )
                    {
                        printk(".");
                        string += mtd0->erasesize;
                    }
                }
                else
                    string += mtd0->erasesize;
            }
        }

        /* Flash the image part with the sequence number. */
        for( blk = 0; blk < fs_start_blk; blk += mtd0->erasesize )
        {
            if( (sts = nandEraseBlkNotSpare( mtd0, blk )) == 0 )
            {
                /* Write a block of the image to flash. */
                if( cferam_string )
                {
                    mtd0->write(mtd0, blk, mtd0->erasesize,
                        &retlen, cferam_string);

                    if( retlen == mtd0->erasesize )
                    {
                        printk(".");
                        cferam_string = NULL;
                    }
                }
            }
        }
#else 
        sts = blk = 0;
        while( sts == 0 )
        {
            /* block_is bad returns 0 if block is not bad */
            if( mtd->block_isbad(mtd, blk) == 0 )
            {
                unsigned char oobbuf[64]; /* expected to be a max size */
                struct mtd_oob_ops ops;

                memset(&ops, 0x00, sizeof(ops));
                ops.ooblen = mtd->oobsize;
                ops.ooboffs = 0;
                ops.datbuf = NULL;
                ops.oobbuf = oobbuf;
                ops.len = 0;
                ops.mode = MTD_OOB_PLACE;

                /* Read and save the spare area. */
                sts = mtd->read_oob(mtd, blk, &ops);
                if( sts == 0 )
                {
                    struct erase_info erase;

                    /* Erase the flash block. */
                    memset(&erase, 0x00, sizeof(erase));
                    erase.addr = blk;
                    erase.len = mtd->erasesize;
                    erase.mtd = mtd;

                    sts = mtd->erase(mtd, &erase);
                    if( sts == 0 )
                    {
                        /* Function local_bh_disable has been called and this
                         * is the only operation that should be occurring.
                         * Therefore, spin waiting for erase to complete.
                         */
                        for(i = 0; i < 10000 && erase.state != MTD_ERASE_DONE &&
                            erase.state != MTD_ERASE_FAILED; i++ )
                        {
                            udelay(100);
                        }

                        if( erase.state != MTD_ERASE_DONE )
                            sts = -1;
                    }

                    if( sts == 0 )
                    {
                        memset(&ops, 0x00, sizeof(ops));
                        ops.ooblen = mtd->oobsize;
                        ops.ooboffs = 0;
                        ops.datbuf = NULL;
                        ops.oobbuf = oobbuf;
                        ops.len = 0;
                        ops.mode = MTD_OOB_PLACE;

                        /* Restore the spare area. */
                        sts = mtd->write_oob(mtd, blk, &ops);

                        /* Write out a block of the image if there is still
                         * image left to flash.
                         */
                        if( sts == 0 && blk < img_size )
                        {
                            int retlen = 0;

                            /* Write a block of the image to flash. */
                            sts = mtd->write(mtd, blk, mtd->erasesize, &retlen,
                                string);

                            string += mtd->erasesize;

                            if( retlen == mtd->erasesize )
                                printk(".");
                            else
                                sts = -1;

                            if( sts )
                                printk("nandImageSet - Block 0x%8.8x. Error writing"
                                    " block.\n", blk);
                        }
                        else
                            if( sts )
                                printk("nandImageSet - Block 0x%8.8x. Error writing"
                                    " spare area.\n", blk);
                    }
                    else
                        printk("nandImageSet - Block 0x%8.8x. Error erasing "
                            "block.\n", blk);
                }
                else
                    printk("nandImageSet - Block 0x%8.8x. Error read spare area.\n",
                        blk);
            }

            if( mtd == mtd1 )
            {
                /* The first raw, boot block was flashed.  Change to rootfs. */
                img_size -= mtd->erasesize;
                mtd = mtd0;
                blk = 0;
            }
            else
            {
                /* Update to next block, stop when all blocks flashed. */
                blk += mtd->erasesize;
                if( blk >= mtd->size )
                    break;
            }
        }

#endif 
        local_bh_enable();

        if( sts )
            sts = (blk > mtd->erasesize) ? blk / mtd->erasesize : 1;
    }

    if( mtd0 )
        put_mtd_device(mtd0);

    if( mtd1 )
        put_mtd_device(mtd1);

    return sts;
}

#ifdef AEI_VDSL_CUSTOMER_QWEST
static void reportUpgradePercent(int percent)
{
        struct file     *f;

        long int        rv;
        mm_segment_t    old_fs;
        loff_t          offset=0;
        char buf[128] = "";

        sprintf(buf, "%d\n", percent);
        f = filp_open("/var/UpgradePercent", O_RDWR|O_CREAT|O_TRUNC, 0600);
        if(f == NULL)
                return;

        old_fs = get_fs();
        set_fs( get_ds() );
        rv = f->f_op->write(f, buf, strlen(buf), &offset);
        set_fs(old_fs);

        if (rv < strlen(buf)) {
                printk("write /var/UpgradePercent  error\n");
        }
        filp_close(f , 0);
        return;
}
#endif

// flash bcm image 
// return: 
// 0 - ok
// !0 - the sector number fail to be flashed (should not be 0)
int kerSysBcmImageSet( int flash_start_addr, char *string, int size)
{
    int sts;
    int sect_size;
    int blk_start;
    int savedSize = size;
    int whole_image = 0;
    WFI_TAG wt = {0};
#ifdef AEI_VDSL_CUSTOMER_QWEST
    int whole_size = size;
#endif

    if (flash_start_addr == FLASH_BASE)
    {
        unsigned long chip_id = (PERF->RevID & 0xFFFE0000) >> 16;

        /* Force BCM681x variants to be be BCM6816) */
        if( (chip_id & 0xfff0) == 0x6810 )
            chip_id = 0x6816;

        whole_image = 1;
        memcpy(&wt, string + size, sizeof(wt));
        if( (wt.wfiVersion & WFI_ANY_VERS_MASK) == WFI_ANY_VERS &&
            wt.wfiChipId != chip_id )
        {
            printk("Chip Id error.  Image Chip Id = %04lx, Board Chip Id = "
                "%04lx.\n", wt.wfiChipId, chip_id);
            return -1;
        }
    }

    if( bootFromNand )
    {
        if( whole_image == 1 )
            return( nandImageSet( flash_start_addr, string, size ) );

        printk("Illegal NAND flash image\n");
        return -1;
    }

    if( whole_image && (wt.wfiVersion & WFI_ANY_VERS_MASK) == WFI_ANY_VERS &&
        wt.wfiFlashType != WFI_NOR_FLASH )
    {
        printk("ERROR: Image does not support a NOR flash device.\n");
        return -1;
    }


#if defined(DEBUG_FLASH)
    printk("kerSysBcmImageSet: flash_start_addr=0x%x string=%p len=%d whole_image=%d\n",
           flash_start_addr, string, size, whole_image);
#endif

    blk_start = flash_get_blk(flash_start_addr);
    if( blk_start < 0 )
        return( -1 );

    // Disable other tasks from this point on
#if defined(CONFIG_SMP)
    smp_send_stop();
    udelay(20);
#endif

#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS)
    down_interruptible(&semflash);
#else
    local_bh_disable();
#endif

    /* write image to flash memory */
    do 
    {
        sect_size = flash_get_sector_size(blk_start);

        flash_sector_erase_int(blk_start);     // erase blk before flash

        if (sect_size > size) 
        {
            if (size & 1) 
                size++;
            sect_size = size;
        }
        
        if (flash_write_buf(blk_start, 0, string, sect_size) != sect_size) {
            break;
        }

#ifdef AEI_VDSL_CUSTOMER_QWEST 
        reportUpgradePercent(100-size*100/whole_size);
#endif
        printk(".");
        blk_start++;
        string += sect_size;
        size -= sect_size; 
    } while (size > 0);

    if (whole_image && savedSize > fInfo.flash_rootfs_start_offset)
    {
        // If flashing a whole image, erase to end of flash.
        int total_blks = flash_get_numsectors();
        while( blk_start < total_blks )
        {
            flash_sector_erase_int(blk_start);
            printk(".");
            blk_start++;
        }
    }

    printk("\n\n");

    if( size == 0 ) 
        sts = 0;  // ok
    else  
        sts = blk_start;    // failed to flash this sector

#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS)
    up(&semflash);
#else
    local_bh_enable();
#endif

    return sts;
}

/*******************************************************************************
 * SP functions
 *******************************************************************************/

// get scratch pad data into *** pTempBuf *** which has to be released by the
//      caller!
// return: if pTempBuf != NULL, points to the data with the dataSize of the
//      buffer
// !NULL -- ok
// NULL  -- fail
static char *getScratchPad(int len)
{
    /* Root file system is on a writable NAND flash.  Read scratch pad from
     * a file on the NAND flash.
     */
    char *ret = NULL;

    if( (ret = retriedKmalloc(len)) != NULL )
    {
        struct file *fp;
        mm_segment_t fs;

        memset(ret, 0x00, len);
        fp = filp_open(SCRATCH_PAD_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->read(fp, (void *) ret, len, &fp->f_pos) <= 0)
                printk("Failed to read scratch pad from '%s'\n",
                    SCRATCH_PAD_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
    }
    else
        printk("Could not allocate scratch pad memory.\n");

    return( ret );
}

// set scratch pad - write the scratch pad file
// return:
// 0 -- ok
// -1 -- fail
static int setScratchPad(char *buf, int len)
{
    /* Root file system is on a writable NAND flash.  Write PSI to
     * a file on the NAND flash.
     */
    int ret = -1;
    struct file *fp;
    mm_segment_t fs;

    fp = filp_open(SCRATCH_PAD_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
        S_IRUSR | S_IWUSR);

    if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
    {
        fs = get_fs();
        set_fs(get_ds());

        fp->f_pos = 0;

        if((int) fp->f_op->write(fp, (void *) buf, len, &fp->f_pos) == len)
            ret = 0;
        else
            printk("Failed to write scratch pad to '%s'.\n", 
                SCRATCH_PAD_FILE_NAME);

        filp_close(fp, NULL);
        set_fs(fs);
    }
    else
        printk("Unable to open '%s'.\n", SCRATCH_PAD_FILE_NAME);

    return( ret );
}

/*
 * get list of all keys/tokenID's in the scratch pad.
 * NOTE: memcpy work here -- not using copy_from/to_user
 *
 * return:
 *         greater than 0 means number of bytes copied to tokBuf,
 *         0 means fail,
 *         negative number means provided buffer is not big enough and the
 *         absolute value of the negative number is the number of bytes needed.
 */
int kerSysScratchPadList(char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pBuf = NULL;
    char *pShareBuf = NULL;
    char *startPtr = NULL;
    int usedLen;
    int tokenNameLen=0;
    int copiedLen=0;
    int needLen=0;
    int sts = 0;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks(fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            printk("could not getSharedBlks.\n");
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;  
    }

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0) 
    {
        printk("Scratch pad is not initialized.\n");
        retriedKfree(pShareBuf);
        return sts;
    }

    // Walk through all the tokens
    usedLen = sizeof(SP_HEADER);
    startPtr = pBuf + sizeof(SP_HEADER);
    pToken = (PSP_TOKEN) startPtr;

    while( pToken->tokenName[0] != '\0' && pToken->tokenLen > 0 &&
           ((usedLen + pToken->tokenLen) <= fInfo.flash_scratch_pad_length))
    {
        tokenNameLen = strlen(pToken->tokenName);
        needLen += tokenNameLen + 1;
        if (needLen <= bufLen)
        {
            strcpy(&tokBuf[copiedLen], pToken->tokenName);
            copiedLen += tokenNameLen + 1;
        }

        usedLen += ((pToken->tokenLen + 0x03) & ~0x03);
        startPtr += sizeof(SP_TOKEN) + ((pToken->tokenLen + 0x03) & ~0x03);
        pToken = (PSP_TOKEN) startPtr;
    }

    if ( needLen > bufLen )
    {
        // User may purposely pass in a 0 length buffer just to get
        // the size, so don't log this as an error.
        sts = needLen * (-1);
    }
    else
    {
        sts = copiedLen;
    }

    retriedKfree(pShareBuf);

    return sts;
}

/*
 * get sp data.  NOTE: memcpy work here -- not using copy_from/to_user
 * return:
 *         greater than 0 means number of bytes copied to tokBuf,
 *         0 means fail,
 *         negative number means provided buffer is not big enough and the
 *         absolute value of the negative number is the number of bytes needed.
 */
int kerSysScratchPadGet(char *tokenId, char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pBuf = NULL;
    char *pShareBuf = NULL;
    char *startPtr = NULL;
    int usedLen;
    int sts = 0;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks(fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            printk("could not getSharedBlks.\n");
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;
    }

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0) 
    {
        printk("Scratch pad is not initialized.\n");
        retriedKfree(pShareBuf);
        return sts;
    }

    // search for the token
    usedLen = sizeof(SP_HEADER);
    startPtr = pBuf + sizeof(SP_HEADER);
    pToken = (PSP_TOKEN) startPtr;
    while( pToken->tokenName[0] != '\0' && pToken->tokenLen > 0 &&
        pToken->tokenLen < fInfo.flash_scratch_pad_length &&
        usedLen < fInfo.flash_scratch_pad_length )
    {

        if (strncmp(pToken->tokenName, tokenId, TOKEN_NAME_LEN) == 0)
        {
            if ( pToken->tokenLen > bufLen )
            {
               // User may purposely pass in a 0 length buffer just to get
               // the size, so don't log this as an error.
               // printk("The length %d of token %s is greater than buffer len %d.\n", pToken->tokenLen, pToken->tokenName, bufLen);
                sts = pToken->tokenLen * (-1);
            }
            else
            {
                memcpy(tokBuf, startPtr + sizeof(SP_TOKEN), pToken->tokenLen);
                sts = pToken->tokenLen;
            }
            break;
        }

        usedLen += ((pToken->tokenLen + 0x03) & ~0x03);
        startPtr += sizeof(SP_TOKEN) + ((pToken->tokenLen + 0x03) & ~0x03);
        pToken = (PSP_TOKEN) startPtr;
    }

    retriedKfree(pShareBuf);

    return sts;
}

// set sp.  NOTE: memcpy work here -- not using copy_from/to_user
// return:
//  0 - ok
//  -1 - fail
int kerSysScratchPadSet(char *tokenId, char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pShareBuf = NULL;
    char *pBuf = NULL;
    SP_HEADER SPHead;
    SP_TOKEN SPToken;
    char *curPtr;
    int sts = -1;

    if( bufLen >= fInfo.flash_scratch_pad_length - sizeof(SP_HEADER) -
        sizeof(SP_TOKEN) )
    {
        printk("Scratch pad overflow by %d bytes.  Information not saved.\n",
            bufLen  - fInfo.flash_scratch_pad_length - sizeof(SP_HEADER) -
            sizeof(SP_TOKEN));
        return sts;
    }

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks( fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;  
    }

    // form header info.
    memset((char *)&SPHead, 0, sizeof(SP_HEADER));
    memcpy(SPHead.SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN);
    SPHead.SPVersion = SP_VERSION;

    // form token info.
    memset((char*)&SPToken, 0, sizeof(SP_TOKEN));
    strncpy(SPToken.tokenName, tokenId, TOKEN_NAME_LEN - 1);
    SPToken.tokenLen = bufLen;

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0)
    {
        // new sp, so just flash the token
        printk("No scratch pad found.  Initialize scratch pad...\n");
        memcpy(pBuf, (char *)&SPHead, sizeof(SP_HEADER));
        curPtr = pBuf + sizeof(SP_HEADER);
        memcpy(curPtr, (char *)&SPToken, sizeof(SP_TOKEN));
        curPtr += sizeof(SP_TOKEN);
        if( tokBuf )
            memcpy(curPtr, tokBuf, bufLen);
    }
    else  
    {
        int putAtEnd = 1;
        int curLen;
        int usedLen;
        int skipLen;

        /* Calculate the used length. */
        usedLen = sizeof(SP_HEADER);
        curPtr = pBuf + sizeof(SP_HEADER);
        pToken = (PSP_TOKEN) curPtr;
        skipLen = (pToken->tokenLen + 0x03) & ~0x03;
        while( pToken->tokenName[0] >= 'A' && pToken->tokenName[0] <= 'z' &&
            strlen(pToken->tokenName) < TOKEN_NAME_LEN &&
            pToken->tokenLen > 0 &&
            pToken->tokenLen < fInfo.flash_scratch_pad_length &&
            usedLen < fInfo.flash_scratch_pad_length )
        {
            usedLen += sizeof(SP_TOKEN) + skipLen;
            curPtr += sizeof(SP_TOKEN) + skipLen;
            pToken = (PSP_TOKEN) curPtr;
            skipLen = (pToken->tokenLen + 0x03) & ~0x03;
        }

        if( usedLen + SPToken.tokenLen + sizeof(SP_TOKEN) >
            fInfo.flash_scratch_pad_length )
        {
            printk("Scratch pad overflow by %d bytes.  Information not saved.\n",
                (usedLen + SPToken.tokenLen + sizeof(SP_TOKEN)) -
                fInfo.flash_scratch_pad_length);
            return sts;
        }

        curPtr = pBuf + sizeof(SP_HEADER);
        curLen = sizeof(SP_HEADER);
        while( curLen < usedLen )
        {
            pToken = (PSP_TOKEN) curPtr;
            skipLen = (pToken->tokenLen + 0x03) & ~0x03;
            if (strncmp(pToken->tokenName, tokenId, TOKEN_NAME_LEN) == 0)
            {
                // The token id already exists.
                if( tokBuf && pToken->tokenLen == bufLen )
                {
                    // The length of the new data and the existing data is the
                    // same.  Overwrite the existing data.
                    memcpy((curPtr+sizeof(SP_TOKEN)), tokBuf, bufLen);
                    putAtEnd = 0;
                }
                else
                {
                    // The length of the new data and the existing data is
                    // different.  Shift the rest of the scratch pad to this
                    // token's location and put this token's data at the end.
                    char *nextPtr = curPtr + sizeof(SP_TOKEN) + skipLen;
                    int copyLen = usedLen - (curLen+sizeof(SP_TOKEN) + skipLen);
                    memcpy( curPtr, nextPtr, copyLen );
                    memset( curPtr + copyLen, 0x00, 
                        fInfo.flash_scratch_pad_length - (curLen + copyLen) );
                    usedLen -= sizeof(SP_TOKEN) + skipLen;
                }
                break;
            }

            // get next token
            curPtr += sizeof(SP_TOKEN) + skipLen;
            curLen += sizeof(SP_TOKEN) + skipLen;
        } // end while

        if( putAtEnd )
        {
            if( tokBuf )
            {
                memcpy( pBuf + usedLen, &SPToken, sizeof(SP_TOKEN) );
                memcpy( pBuf + usedLen + sizeof(SP_TOKEN), tokBuf, bufLen );
            }
            memcpy( pBuf, &SPHead, sizeof(SP_HEADER) );
        }

    } // else if not new sp

    if( bootFromNand )
        sts = setScratchPad(pShareBuf, fInfo.flash_scratch_pad_length);
    else
        sts = setSharedBlks(fInfo.flash_scratch_pad_start_blk, 
            fInfo.flash_scratch_pad_number_blk, pShareBuf);
    
    retriedKfree(pShareBuf);

    return sts;

    
}
#if defined(AEI_VDSL_CUSTOMER_NCS)

int kerClearScratchPad(int blk_size)
{
    char buf[256];
    memset(buf,0, 256);
	if(bootFromNand){

	}else{
	
	    kerSysPersistentSet( buf, 256, 0 );
	    kerSysBackupPsiSet( buf, 256, 0 );	
    	    kerSysScratchPadClearAll();
#if !defined (CONFIG_BCM96328)
#if defined(AEI_VDSL_CUSTOMER_Q2000H) || defined(AEI_VDSL_DEFAULT_NO_BONDING)			
	   restoreDatapump(2);		
#else
	   restoreDatapump(0);		
#endif
#endif
	}
}

#endif


// wipe out the scratchPad
// return:
//  0 - ok
//  -1 - fail
int kerSysScratchPadClearAll(void)
{ 
    int sts = -1;
    char *pShareBuf = NULL;
   int j ;
   int usedBlkSize = 0;

   // printk ("kerSysScratchPadClearAll.... \n") ;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        memset(pShareBuf, 0x00, fInfo.flash_scratch_pad_length);

        setScratchPad(pShareBuf, fInfo.flash_scratch_pad_length);
    }
    else
    {
  #if defined(AEI_VDSL_UPGRADE_HISTORY_SPAD)
	/* get upgradeHistroy */
	char uph[1024] = {0};
	kerSysScratchPadGet("upgradeHistory", uph, 1024);
#endif  
        if( (pShareBuf = getSharedBlks( fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
            return sts;

        if (fInfo.flash_scratch_pad_number_blk == 1)
            memset(pShareBuf + fInfo.flash_scratch_pad_blk_offset, 0x00, fInfo.flash_scratch_pad_length) ;
        else
        {
            for (j = fInfo.flash_scratch_pad_start_blk;
                j < (fInfo.flash_scratch_pad_start_blk + fInfo.flash_scratch_pad_number_blk);
                j++)
            {
                usedBlkSize += flash_get_sector_size((unsigned short) j);
            }

            memset(pShareBuf, 0x00, usedBlkSize) ;
        }

        sts = setSharedBlks(fInfo.flash_scratch_pad_start_blk,    
            fInfo.flash_scratch_pad_number_blk,  pShareBuf);
#if defined(AEI_VDSL_UPGRADE_HISTORY_SPAD)
	/* save upgradeHistory back to scratch */
	kerSysScratchPadSet("upgradeHistory", uph, 1024);
#endif		
    }

   retriedKfree(pShareBuf);

   //printk ("kerSysScratchPadClearAll Done.... \n") ;
   return sts;
}

int kerSysFlashSizeGet(void)
{
    int ret = 0;

    if( bootFromNand )
    {
        struct mtd_info *mtd;

        if( (mtd = get_mtd_device_nm("rootfs")) != NULL )
        {
            ret = mtd->size;
            put_mtd_device(mtd);
        }
    }
    else
        ret = flash_get_total_size();

    return ret;
}

#if defined(AEI_CONFIG_AUXFS_JFFS2)
#if !defined(CONFIG_BRCM_IKOS) 

int kerSysEraseFlash(unsigned long eraseaddr, unsigned long len)
{
    int blk;
    int bgnBlk = flash_get_blk(eraseaddr);
    int endBlk = flash_get_blk(eraseaddr + len);
    unsigned long bgnAddr = (unsigned long) flash_get_memptr(bgnBlk);
    unsigned long endAddr = (unsigned long) flash_get_memptr(endBlk);

#ifdef DEBUG_FLASH
    printk("kerSysEraseFlash blk[%d] eraseaddr[0x%08x] len[%lu]\n",
    bgnBlk, (int)eraseaddr, len);
#endif

    if ( bgnAddr != eraseaddr)
    {
       printk("ERROR: kerSysEraseFlash eraseaddr[0x%08x]"
              " != first block start[0x%08x]\n",
              (int)eraseaddr, (int)bgnAddr);
        return (len);
    }

    if ( (endAddr - bgnAddr) != len)
    {
        printk("ERROR: kerSysEraseFlash eraseaddr[0x%08x] + len[%lu]"
               " != last+1 block start[0x%08x]\n",
               (int)eraseaddr, len, (int) endAddr);
        return (len);
    }

    for (blk=bgnBlk; blk<endBlk; blk++)
        flash_sector_erase_int(blk);

    return 0;
}



unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    int blk, offset, bytesRead;
    unsigned long blk_start;
    char * trailbyte = (char*) NULL;
    char val[2];

    blk = flash_get_blk((int)fromaddr);	/* sector in which fromaddr falls */
    blk_start = (unsigned long)flash_get_memptr(blk); /* sector start address */
    offset = (int)(fromaddr - blk_start); /* offset into sector */

#ifdef DEBUG_FLASH
    printk("kerSysReadFromFlash blk[%d] fromaddr[0x%08x]\n",
           blk, (int)fromaddr);
#endif

    bytesRead = 0;

        /* cfiflash : hardcoded for bankwidths of 2 bytes. */
    if ( offset & 1 )   /* toaddr is not 2 byte aligned */
    {
        flash_read_buf(blk, offset-1, val, 2);
        *((char*)toaddr) = val[1];

        toaddr = (void*)((char*)toaddr+1);
        fromaddr += 1;
        len -= 1;
        bytesRead = 1;

        /* if len is 0 we could return here, avoid this if */

        /* recompute blk and offset, using new fromaddr */
        blk = flash_get_blk(fromaddr);
        blk_start = (unsigned long)flash_get_memptr(blk);
        offset = (int)(fromaddr - blk_start);
    }

        /* cfiflash : hardcoded for len of bankwidths multiples. */
    if ( len & 1 )
    {
        len -= 1;
        trailbyte = (char *)toaddr + len;
    }

        /* Both len and toaddr will be 2byte aligned */
    if ( len )
    {
       flash_read_buf(blk, offset, toaddr, len);
       bytesRead += len;
    }

        /* write trailing byte */
    if ( trailbyte != (char*) NULL )
    {
        fromaddr += len;
        blk = flash_get_blk(fromaddr);
        blk_start = (unsigned long)flash_get_memptr(blk);
        offset = (int)(fromaddr - blk_start);
        flash_read_buf(blk, offset, val, 2 );
        *trailbyte = val[0];
        bytesRead += 1;
    }

    return( bytesRead );
}

/*
 * Function: kerSysWriteToFlash
 *
 * Description:
 * This function assumes that the area of flash to be written was
 * previously erased. An explicit erase is therfore NOT needed 
 * prior to a write. This function ensures that the offset and len are
 * two byte multiple. [cfiflash hardcoded for bankwidth of 2 byte].
 *
 * Parameters:
 *      toaddr : destination flash memory address
 *      fromaddr: RAM memory address containing data to be written
 *      len : non zero bytes to be written
 * Return:
 *      FAILURE: number of bytes remaining to be written
 *      SUCCESS: 0 (all requested bytes were written)
 */
int kerSysWriteToFlash( unsigned long toaddr,
                        void * fromaddr, unsigned long len)
{
    int blk, offset, size, blk_size, bytesWritten;
    unsigned long blk_start;
    char * trailbyte = (char*) NULL;
    unsigned char val[2];

#ifdef DEBUG_FLASH
    printk("kerSysWriteToFlash flashAddr[0x%08x] fromaddr[0x%08x] len[%lu]\n",
    (int)toaddr, (int)fromaddr, len);
#endif

    blk = flash_get_blk(toaddr);	/* sector in which toaddr falls */
    blk_start = (unsigned long)flash_get_memptr(blk); /* sector start address */
    offset = (int)(toaddr - blk_start);	/* offset into sector */

	/* cfiflash : hardcoded for bankwidths of 2 bytes. */
    if ( offset & 1 )	/* toaddr is not 2 byte aligned */
    {
        val[0] = 0xFF; // ignored
        val[1] = *((char *)fromaddr); /* write the first byte */
        bytesWritten = flash_write_buf(blk, offset-1, val, 2);
        if ( bytesWritten != 2 )
        {
#ifdef DEBUG_FLASH
           printk("ERROR kerSysWriteToFlash ... remaining<%d>\n", len); 
#endif
           return len;
        }

	toaddr += 1;
        fromaddr = (void*)((char*)fromaddr+1);
        len -= 1;

	/* if len is 0 we could return bytesWritten, avoid this if */

	/* recompute blk and offset, using new toaddr */
        blk = flash_get_blk(toaddr);
        blk_start = (unsigned long)flash_get_memptr(blk);
        offset = (int)(toaddr - blk_start);
    }

	/* cfiflash : hardcoded for len of bankwidths multiples. */
    if ( len & 1 )
    {
	/* need to handle trailing byte seperately */
        len -= 1;
        trailbyte = (char *)fromaddr + len;
        toaddr += len;
    }

	/* Both len and toaddr will be 2byte aligned */
    while ( len > 0 )
    {
        blk_size = flash_get_sector_size(blk);
        size = blk_size - offset; /* space available in sector from offset */
        if ( size > len )
            size = len;

        bytesWritten = flash_write_buf(blk, offset, fromaddr, size); 
        if ( bytesWritten !=  size )
        {
#ifdef DEBUG_FLASH
           printk("ERROR kerSysWriteToFlash ... remaining<%d>\n", 
               (len - bytesWritten + ((trailbyte == (char*)NULL)? 0 : 1)));
#endif
           return (len - bytesWritten + ((trailbyte == (char*)NULL)? 0 : 1));
        }

        fromaddr += size;
        len -= size;

        blk++;	/* Move to the next block */
        offset = 0; /* All further blocks will be written at offset 0 */
    }

	/* write trailing byte */
    if ( trailbyte != (char*) NULL )
    {
        blk = flash_get_blk(toaddr);
        blk_start = (unsigned long)flash_get_memptr(blk);
        offset = (int)(toaddr - blk_start);
        val[0] = *trailbyte; /* trailing byte */
        val[1] = 0xFF; // ignored
        bytesWritten = flash_write_buf(blk, offset, val, 2 );
        if ( bytesWritten != 2 )
        {
#ifdef DEBUG_FLASH
           printk("ERROR kerSysWriteToFlash ... remaining<%d>\n",1);
#endif
           return 1;
        }
    } 

    return len;
}
/*
 * Function: kerSysWriteToFlashREW
 * 
 * Description:
 * This function does not assume that the area of flash to be written was erased.
 * An explicit erase is therfore needed prior to a write.  
 * kerSysWriteToFlashREW uses a sector copy  algorithm. The first and last sectors
 * may need to be first read if they are not fully written. This is needed to
 * avoid the situation that there may be some valid data in the sector that does
 * not get overwritten, and would be erased.
 *
 * Due to run time costs for flash read, optimizations to read only that data
 * that will not be overwritten is introduced.
 *
 * Parameters:
 *	toaddr : destination flash memory address
 *	fromaddr: RAM memory address containing data to be written
 *	len : non zero bytes to be written
 * Return:
 *	FAILURE: number of bytes remaining to be written 
 *	SUCCESS: 0 (all requested bytes were written)
 *
 */
int kerSysWriteToFlashREW( unsigned long toaddr,
                        void * fromaddr, unsigned long len)
{
    int blk, offset, size, blk_size, bytesWritten;
    unsigned long sect_start;
    int mem_sz = 0;
    char * mem_p = (char*)NULL;

#ifdef DEBUG_FLASH
    printk("kerSysWriteToFlashREW flashAddr[0x%08x] fromaddr[0x%08x] len[%lu]\n",
    (int)toaddr, (int)fromaddr, len);
#endif

    blk = flash_get_blk( toaddr );
    sect_start = (unsigned long) flash_get_memptr(blk);
    offset = toaddr - sect_start;

    while ( len > 0 )
    {
        blk_size = flash_get_sector_size(blk);
        size = blk_size - offset; /* space available in sector from offset */

		/* bound size to remaining len in final block */
        if ( size > len )
            size = len;

		/* Entire blk written, no dirty data to read */
        if ( size == blk_size )
        {
            flash_sector_erase_int(blk);

            bytesWritten = flash_write_buf(blk, 0, fromaddr, blk_size);

            if ( bytesWritten != blk_size )
            {
                if ( mem_p != NULL )
                    retriedKfree(mem_p);
                return (len - bytesWritten);	/* FAILURE */
            }
        }
        else
        {
                /* Support for variable sized blocks, paranoia */
            if ( (mem_p != NULL) && (mem_sz < blk_size) )
            {
                retriedKfree(mem_p);	/* free previous temp buffer */
                mem_p = (char*)NULL;
            }

            if ( (mem_p == (char*)NULL)
              && ((mem_p = (char*)retriedKmalloc(blk_size)) == (char*)NULL) )
            {
                printk("\tERROR kerSysWriteToFlashREW fail to allocate memory\n");
                return len;
            }
            else
                mem_sz = blk_size;

            if ( offset ) /* First block */
            {
                if ( (offset + size) == blk_size)
                {
                   flash_read_buf(blk, 0, mem_p, offset);
                }
                else
                {  /*
		    *	 Potential for future optimization:
		    * Should have read the begining and trailing portions
		    * of the block. If the len written is smaller than some
		    * break even point.
		    * For now read the entire block ... move on ...
		    */
                   flash_read_buf(blk, 0, mem_p, blk_size);
                }
            }
            else
            {
                /* Read the tail of the block which may contain dirty data*/
                flash_read_buf(blk, len, mem_p+len, blk_size-len );
            }

            flash_sector_erase_int(blk);

            memcpy(mem_p+offset, fromaddr, size); /* Rebuild block contents */

            bytesWritten = flash_write_buf(blk, 0, mem_p, blk_size);

            if ( bytesWritten != blk_size )
            {
                if ( mem_p != (char*)NULL )
                    retriedKfree(mem_p);
                return (len + (blk_size - size) - bytesWritten );
            }
        }

		/* take into consideration that size bytes were copied */
        fromaddr += size;
        toaddr += size;
        len -= size;

        blk++;		/* Move to the next block */
        offset = 0;     /* All further blocks will be written at offset 0 */

    }

    if ( mem_p != (char*)NULL )
        retriedKfree(mem_p);

    return ( len );
}

#endif
#endif
