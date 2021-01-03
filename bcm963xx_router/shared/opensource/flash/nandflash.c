/*
    Copyright 2000-2010 Broadcom Corporation

    Unless you and Broadcom execute a separate written software license
    agreement governing use of this software, this software is licensed
    to you under the terms of the GNU General Public License version 2
    (the “GPL”), available at http://www.broadcom.com/licenses/GPLv2.php,
    with the following added to such license:

        As a special exception, the copyright holders of this software give
        you permission to link this software with independent modules, and to
        copy and distribute the resulting executable under terms of your
        choice, provided that you also meet, for each linked independent
        module, the terms and conditions of the license of that module. 
        An independent module is a module which is not derived from this
        software.  The special exception does not apply to any modifications
        of the software.

    Notwithstanding the above, under no circumstances may you combine this
    software in any way with any other Broadcom software provided under a
    license other than the GPL, without Broadcom's express prior written
    consent.
*/                       

/***************************************************************************
 * File Name  : nandflash.c
 *
 * Description: This file implements the Broadcom DSL defined flash api for
 *              for NAND flash parts.
 ***************************************************************************/

/** Includes. **/

#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "bcm_map.h"  
#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "jffs2.h"

/* for debugging in jtag */
#if !defined(CFG_RAMAPP)
#define static 
#endif


/** Defines. **/

#define NR_OOB_SCAN_PAGES       4
#define SPARE_MAX_SIZE          64
#define PAGE_MAX_SIZE           2048
#define CTRLR_SPARE_SIZE        16
#define CTRLR_CACHE_SIZE        512

/* Flash manufacturers. */
#define FLASHTYPE_SAMSUNG       0xec
#define FLASHTYPE_ST            0x20
#define FLASHTYPE_MICRON        0x2c

/* Samsung flash parts. */
#define SAMSUNG_K9F5608U0A      0x55

/* ST flash parts. */
#define ST_NAND512W3A2CN6       0x76
#define ST_NAND01GW3B2CN6       0xf1

/* Micron flash parts. */
#define MICRON_MT29F1G08AAC     0xf1

/* Flash id to name mapping. */
#define NAND_MAKE_ID(A,B)    \
    (((unsigned short) (A) << 8) | ((unsigned short) B & 0xff))

#define NAND_FLASH_DEVICES                                                    \
  {{NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F5608U0A),"Samsung K9F5608U0"},  \
   {NAND_MAKE_ID(FLASHTYPE_ST,ST_NAND512W3A2CN6),"ST NAND512W3A2CN6"},        \
   {NAND_MAKE_ID(FLASHTYPE_ST,ST_NAND01GW3B2CN6),"ST NAND01GW3B2CN6"},        \
   {NAND_MAKE_ID(FLASHTYPE_MICRON,MICRON_MT29F1G08AAC),"Micron MT29F1G08AAC"},\
   {0,""}                                                                     \
  }

/* One byte for small page NAND flash parts. */
#define SPARE_SP_BI_INDEX_1     5
#define SPARE_SP_BI_INDEX_2     5

/* Two bytes for small page NAND flash parts. */
#define SPARE_LP_BI_INDEX_1     0
#define SPARE_LP_BI_INDEX_2     1

#define SPARE_BI_MARKER         0
#define SPARE_BI_ECC_MASK                            \
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, \
     0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, \
     0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, \
     0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}

#define JFFS2_CLEANMARKER      {JFFS2_MAGIC_BITMASK, \
    JFFS2_NODETYPE_CLEANMARKER, 0x0000, 0x0008}

#undef DEBUG_NAND
#if defined(DEBUG_NAND) && defined(CFG_RAMAPP)
#define DBG_PRINTF printf
#else
#define DBG_PRINTF(...)
#endif


/** Externs. **/

extern void board_setleds(unsigned long);


/** Structs. **/

typedef struct CfeNandChip
{
    char *chip_name;
    unsigned long chip_device_id;
    unsigned long chip_base;
    unsigned long chip_total_size;
    unsigned long chip_block_size;
    unsigned long chip_page_size;
    unsigned long chip_spare_size;
    unsigned char *chip_spare_mask;
    unsigned long chip_bi_index_1;
    unsigned long chip_bi_index_2;
} CFE_NAND_CHIP, *PCFE_NAND_CHIP;

struct flash_name_from_id
{
    unsigned short fnfi_id;
    char fnfi_name[30];
};


#if defined(CFG_RAMAPP)
/** Prototypes for CFE RAM. **/
int nand_flash_init(flash_device_info_t **flash_info);
int mpinand_flash_init(flash_device_info_t **flash_info);
static void nand_init_cleanmarker(PCFE_NAND_CHIP pchip);
static void nand_read_cfg(PCFE_NAND_CHIP pchip);
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not);
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip);
static void nand_mark_bad_blk(PCFE_NAND_CHIP pchip, unsigned long page_addr);
static int nand_flash_sector_erase_int(unsigned short blk);
static int nand_flash_read_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len);
static int nand_flash_write_buf(unsigned short blk, int offset,
    unsigned char *buffer, int numbytes);
static int nand_flash_get_numsectors(void);
static int nand_flash_get_sector_size(unsigned short sector);
static unsigned char *nand_flash_get_memptr(unsigned short sector);
static int nand_flash_get_blk(int addr);
static int nand_flash_get_total_size(void);
static int nandflash_wait_status(unsigned long status_mask);
static int nandflash_read_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len);
static int nandflash_write_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len);
static int nandflash_read_page(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, unsigned char *buffer, int len);
static int nandflash_write_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len);
static int nandflash_block_erase(PCFE_NAND_CHIP pchip, unsigned long blk_addr);
#else
/** Prototypes for CFE ROM. **/
void rom_nand_flash_init(void);
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not);
static void nand_read_cfg(PCFE_NAND_CHIP pchip);
int nand_flash_get_sector_size(unsigned short sector);
int nand_flash_get_numsectors(void);
static int nandflash_wait_status(unsigned long status_mask);
static int nandflash_read_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len);
static int nandflash_read_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len);
int nand_flash_read_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len);
static inline void nandflash_copy_from_cache(unsigned char *buffer,
    int offset, int numbytes);
static inline void nandflash_copy_from_spare(unsigned char *buffer,
    int numbytes);
static int nandflash_wait_status(unsigned long status_mask);
static inline int nandflash_wait_cmd(void);
static inline int nandflash_wait_device(void);
static inline int nandflash_wait_cache(void);
static inline int nandflash_wait_spare(void);
static int nandflash_check_ecc(void);
#endif


#if defined(CFG_RAMAPP)
/** Variables for CFE RAM. **/
CFE_NAND_CHIP g_chip = {NULL,0,0,0,0,0,0};
static unsigned char g_spare_mask[] = SPARE_BI_ECC_MASK;
static unsigned char g_spare_cleanmarker[SPARE_MAX_SIZE];

static flash_device_info_t flash_nand_dev =
    {
        0xffff,
        FLASH_IFC_NAND,
        "",
        nand_flash_sector_erase_int,
        nand_flash_read_buf,
        nand_flash_write_buf,
        nand_flash_get_numsectors,
        nand_flash_get_sector_size,
        nand_flash_get_memptr,
        nand_flash_get_blk,
        nand_flash_get_total_size
    };

#else
/** Variables for CFE ROM. **/
CFE_NAND_CHIP g_chip;
static unsigned char g_spare_mask[] = SPARE_BI_ECC_MASK;
#endif


#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nand_flash_init
 * Description  : Initialize flash part.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
int nand_flash_init(flash_device_info_t **flash_info)
{
    static int initialized = 0;
    int ret = FLASH_API_OK;

    if( initialized == 0 )
    {
        PCFE_NAND_CHIP pchip = &g_chip;
        static struct flash_name_from_id fnfi[] = NAND_FLASH_DEVICES;
        struct flash_name_from_id *fnfi_ptr;

        DBG_PRINTF(">> nand_flash_init - entry\n");

        /* Enable NAND data on MII ports. */
#if !defined(_BCM96328_)
        PERF->blkEnables |= NAND_CLK_EN;
#endif
        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 2;

        /* Read the NAND flash chip id. Only use the most signficant 16 bits.*/
        pchip->chip_device_id = NAND->NandFlashDeviceId >> 16;
        flash_nand_dev.flash_device_id = pchip->chip_device_id;

        for( fnfi_ptr = fnfi; fnfi_ptr->fnfi_id != 0; fnfi_ptr++ )
        {
            if( fnfi_ptr->fnfi_id == pchip->chip_device_id )
            {
                strcpy(flash_nand_dev.flash_device_name, fnfi_ptr->fnfi_name);
                break;
            }
        }

        /* If NAND chip is not in the list of NAND chips, the correct
         * configuration maybe still have been set by the NAND controller.
         */
        if( flash_nand_dev.flash_device_name[0] == '\0' )
            strcpy(flash_nand_dev.flash_device_name, "<not identified>");

        *flash_info = &flash_nand_dev;

        NAND->NandCsNandXor = 0;
        pchip->chip_base = PHYS_FLASH_BASE + BOOT_OFFSET;
        nand_read_cfg(pchip);
        nand_init_cleanmarker(pchip);

        /* If the first block's spare area is not a JFFS2 cleanmarker,
         * initialize all block's spare area to a cleanmarker.
         */
        if( !nand_is_blk_cleanmarker(pchip, 0, 0) )
            ret = nand_initialize_spare_area(pchip);

        DBG_PRINTF(">> nand_flash_init - return %d\n", ret);

        initialized = 1;
    }
    else
        *flash_info = &flash_nand_dev;

    return( ret );
} /* nand_flash_init */

/***************************************************************************
 * Function Name: nand_init_cleanmarker
 * Description  : Initializes the JFFS2 clean marker buffer.
 * Returns      : None.
 ***************************************************************************/
static void nand_init_cleanmarker(PCFE_NAND_CHIP pchip)
{
    unsigned short cleanmarker[] = JFFS2_CLEANMARKER;
    unsigned char *pcm = (unsigned char *) cleanmarker;
    int i, j;

    /* Skip spare area offsets reserved for ECC bytes. */
    for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
    {
        if( pchip->chip_spare_mask[i] == 0 && j < sizeof(cleanmarker))
            g_spare_cleanmarker[i] = pcm[j++];
        else
            g_spare_cleanmarker[i] = 0xff;
    }
} /* nand_init_cleanmarker */

#else
/***************************************************************************
 * Function Name: rom_nand_flash_init
 * Description  : Initialize flash part just enough to read blocks.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
void rom_nand_flash_init(void)
{
    PCFE_NAND_CHIP pchip = &g_chip;

    /* Enable NAND data on MII ports. */
#if !defined(_BCM96328_)
    PERF->blkEnables |= NAND_CLK_EN;
#endif
    NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 2;

    pchip->chip_base = PHYS_FLASH_BASE + BOOT_OFFSET;

    /* Read the chip id. Only use the most signficant 16 bits. */
    pchip->chip_device_id = NAND->NandFlashDeviceId >> 16;

    if( pchip->chip_device_id > 0 && pchip->chip_device_id < 0xffff )
        nand_read_cfg(pchip);
    else
        board_setleds(0x4d494532);
} /* nand_flash_init */
#endif

/***************************************************************************
 * Function Name: nand_read_cfg
 * Description  : Reads and stores the chip configuration.
 * Returns      : None.
 ***************************************************************************/
static void nand_read_cfg(PCFE_NAND_CHIP pchip)
{
    /* Read chip configuration. */
    unsigned long cfg = NAND->NandConfig;

    pchip->chip_total_size =
        (4 * (1 << ((cfg & NC_DEV_SIZE_MASK) >> NC_DEV_SIZE_SHIFT))) << 20;

    switch( (cfg & NC_BLK_SIZE_MASK) )
    {
    case NC_BLK_SIZE_512K:
        pchip->chip_block_size = 512 * 1024;
        break;

    case NC_BLK_SIZE_128K:
        pchip->chip_block_size = 128 * 1024;
        break;

    case NC_BLK_SIZE_16K:
        pchip->chip_block_size = 16 * 1024;
        break;

    case NC_BLK_SIZE_8K:
        pchip->chip_block_size = 8 * 1024;
        break;
    }

    if( (cfg & NC_PG_SIZE_MASK) == NC_PG_SIZE_512B )
    {
        pchip->chip_page_size = 512;
        pchip->chip_bi_index_1 = SPARE_SP_BI_INDEX_1;
        pchip->chip_bi_index_2 = SPARE_SP_BI_INDEX_2;
    }
    else
    {
        pchip->chip_page_size = 2048;
        pchip->chip_bi_index_1 = SPARE_LP_BI_INDEX_1;
        pchip->chip_bi_index_2 = SPARE_LP_BI_INDEX_2;
    }

    pchip->chip_spare_mask = g_spare_mask;
    pchip->chip_spare_mask[pchip->chip_bi_index_1] = 1;
    pchip->chip_spare_mask[pchip->chip_bi_index_2] = 1;

    pchip->chip_spare_size = pchip->chip_page_size >> 5;

    DBG_PRINTF(">> nand_read_cfg - size=%luMB, block=%luKB, page=%luB, "
        "spare=%lu\n", pchip->chip_total_size / (1024 * 1024),
        pchip->chip_block_size / 1024, pchip->chip_page_size,
        pchip->chip_spare_size);
} /* nand_read_cfg */

/***************************************************************************
 * Function Name: nand_is_blk_cleanmarker
 * Description  : Compares a buffer to see if it a JFFS2 cleanmarker.
 * Returns      : 1 - is cleanmarker, 0 - is not cleanmarker
 ***************************************************************************/
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not)
{
    unsigned short cleanmarker[] = JFFS2_CLEANMARKER;
    unsigned char *pcm = (unsigned char *) cleanmarker;
    unsigned char spare[SPARE_MAX_SIZE], comparebuf[SPARE_MAX_SIZE];
    unsigned long i, j;
    int ret = 0;

    if( nandflash_read_spare_area( pchip, start_addr, spare,
        pchip->chip_spare_size) == FLASH_API_OK )
    {
        /* Skip spare offsets that are reserved for the ECC.  Make spare data
         * bytes contiguous in the spare buffer.
         */
        for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
            if( pchip->chip_spare_mask[i] == 0 )
                comparebuf[j++] = spare[i];

        /* Compare spare area data to the JFFS2 cleanmarker. */
        for( i = 0, ret = 1; i < sizeof(cleanmarker) && ret == 1; i++ )
            if( comparebuf[i] != pcm[i])
                ret = 0;
    }

#if defined(CFG_RAMAPP)
    if( ret == 0 && spare[pchip->chip_bi_index_1] != SPARE_BI_MARKER &&
        spare[pchip->chip_bi_index_2] != SPARE_BI_MARKER && write_if_not )
    {
        /* The spare area is not a clean marker but the block is not bad.
         * Write a clean marker to this block. (Assumes the block is erased.)
         */
        if( nandflash_write_spare_area(pchip, start_addr, (unsigned char *)
            g_spare_cleanmarker, pchip->chip_spare_size) == FLASH_API_OK )
        {
            ret = nand_is_blk_cleanmarker(pchip, start_addr, 0);
        }
    }
#endif

    return( ret );
} /* nand_is_blk_cleanmarker */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nand_initialize_spare_area
 * Description  : Initializes the spare area of the first page of each block 
 *                to a cleanmarker.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip)
{
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long i;
    int ret;

    DBG_PRINTF(">> nand_initialize_spare_area - entry\n");

    for( i = 0; i < pchip->chip_total_size; i += pchip->chip_block_size )
    {
        /* Read the current spare area. */
        ret = nandflash_read_spare_area(pchip,0,spare,pchip->chip_spare_size);
        if(ret == FLASH_API_OK
          /*&& spare[pchip->chip_bi_index_1] != SPARE_BI_MARKER*/
          /*&& spare[pchip->chip_bi_index_2] != SPARE_BI_MARKER*/)
        {
            if( nandflash_block_erase(pchip, i) == FLASH_API_OK )
            {
                nandflash_write_spare_area(pchip, i, (unsigned char *)
                    g_spare_cleanmarker, pchip->chip_spare_size);
            }
        }
    }

    return( FLASH_API_OK );
} /* nand_initialize_spare_area */


/***************************************************************************
 * Function Name: nand_mark_bad_blk
 * Description  : Marks the specified block as bad by writing 0xFFs to the
 *                spare area and updating the in memory bad block table.
 * Returns      : None.
 ***************************************************************************/
static void nand_mark_bad_blk(PCFE_NAND_CHIP pchip, unsigned long page_addr)
{
    unsigned char spare[SPARE_MAX_SIZE];

    DBG_PRINTF(">> nand_mark_bad_blk - addr=0x%8.8lx, block=0x%8.8lx\n",
        page_addr, page_addr / pchip->chip_block_size);

    nandflash_block_erase(pchip, page_addr);
    memset(spare, 0xff, pchip->chip_spare_size);
    spare[pchip->chip_bi_index_1] = SPARE_BI_MARKER;
    spare[pchip->chip_bi_index_2] = SPARE_BI_MARKER;
    nandflash_write_spare_area(pchip,page_addr,spare,pchip->chip_spare_size);
} /* nand_mark_bad_blk */


/***************************************************************************
 * Function Name: nand_flash_sector_erase_int
 * Description  : Erase the specfied flash sector.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nand_flash_sector_erase_int(unsigned short blk)
{
    int ret = FLASH_API_OK;
    PCFE_NAND_CHIP pchip = &g_chip;

    if( blk == NAND_REINIT_FLASH )
        nand_initialize_spare_area(pchip);
    else
    {
        unsigned long page_addr = blk * pchip->chip_block_size;

        /* Only erase the block if the spare area is a JFFS2 cleanmarker.
         * Assume that the only the CFE boot loader only touches JFFS2 blocks.
         * This check prevents the Linux NAND MTD driver bad block block table
         * from being erased.  The NAND_REINIT_FLASH option unconditionally
         * erases all NAND flash blocks.
         */
        if( nand_is_blk_cleanmarker(pchip, page_addr, 0) )
        {
            ret = nandflash_block_erase(pchip, page_addr);
            nandflash_write_spare_area(pchip, page_addr, g_spare_cleanmarker,
                pchip->chip_spare_size);
        }

        DBG_PRINTF(">> nand_flash_sector_erase_int - blk=0x%8.8lx, ret=%d\n",
            blk, ret);
    }

    return( ret );
} /* nand_flash_sector_erase_int */
#endif

/***************************************************************************
 * Function Name: nand_flash_read_buf
 * Description  : Reads from flash memory.
 * Returns      : number of bytes read or FLASH_API_ERROR
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_read_buf(unsigned short blk, int offset, unsigned char *buffer,
    int len)
{
    int ret = len;
    PCFE_NAND_CHIP pchip = &g_chip;
    UINT32 start_addr;
    UINT32 blk_addr;
    UINT32 blk_offset;
    UINT32 size;

    DBG_PRINTF(">> nand_flash_read_buf - 1 blk=0x%8.8lx, offset=%d, len=%lu\n",
        blk, offset, len);

    start_addr = (blk * pchip->chip_block_size) + offset;
    blk_addr = start_addr & ~(pchip->chip_block_size - 1);
    blk_offset = start_addr - blk_addr;
    size = pchip->chip_block_size - blk_offset;

    if(size > len)
        size = len;

    do
    {
        if(nandflash_read_page(pchip,start_addr,buffer,size) != FLASH_API_OK)
        {
            ret = FLASH_API_ERROR;
            break;
        }

        len -= size;
        if( len )
        {
            blk++;

            DBG_PRINTF(">> nand_flash_read_buf - 2 blk=0x%8.8lx, len=%lu\n",
                blk, len);

            start_addr = blk * pchip->chip_block_size;
            buffer += size;
            if(len > pchip->chip_block_size)
                size = pchip->chip_block_size;
            else
                size = len;
        }
    } while(len);

    DBG_PRINTF(">> nand_flash_read_buf - ret=%d\n", ret);

    return( ret ) ;
} /* nand_flash_read_buf */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nand_flash_write_buf
 * Description  : Writes to flash memory.
 * Returns      : number of bytes written or FLASH_API_ERROR
 ***************************************************************************/
static int nand_flash_write_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len)
{
    int ret = len;
    PCFE_NAND_CHIP pchip = &g_chip;
    UINT32 start_addr;
    UINT32 blk_addr;
    UINT32 blk_offset;
    UINT32 size;

    DBG_PRINTF(">> nand_flash_write_buf - 1 blk=0x%8.8lx, offset=%d, len=%d\n",
        blk, offset, len);

    start_addr = (blk * pchip->chip_block_size) + offset;
    blk_addr = start_addr & ~(pchip->chip_block_size - 1);
    blk_offset = start_addr - blk_addr;
    size = pchip->chip_block_size - blk_offset;

    if(size > len)
        size = len;

    do
    {
        if(nandflash_write_page(pchip,start_addr,buffer,size) != FLASH_API_OK)
        {
            ret = ret - len;
            break;
        }
        else
        {
            len -= size;
            if( len )
            {
                blk++;

                DBG_PRINTF(">> nand_flash_write_buf- 2 blk=0x%8.8lx, len=%d\n",
                    blk, len);

                offset = 0;
                start_addr = blk * pchip->chip_block_size;
                buffer += size;
                if(len > pchip->chip_block_size)
                    size = pchip->chip_block_size;
                else
                    size = len;
            }
        }
    } while(len);

    DBG_PRINTF(">> nand_flash_write_buf - ret=%d\n", ret);

    return( ret ) ;
} /* nand_flash_write_buf */

/***************************************************************************
 * Function Name: nand_flash_get_memptr
 * Description  : Returns the base MIPS memory address for the specfied flash
 *                sector.
 * Returns      : Base MIPS memory address for the specfied flash sector.
 ***************************************************************************/
static unsigned char *nand_flash_get_memptr(unsigned short sector)
{
    /* Bad things will happen if this pointer is referenced.  But it can
     * be used for pointer arithmetic to deterine sizes.
     */
    return((unsigned char *) (FLASH_BASE + (sector * g_chip.chip_block_size)));
} /* nand_flash_get_memptr */

/***************************************************************************
 * Function Name: nand_flash_get_blk
 * Description  : Returns the flash sector for the specfied MIPS address.
 * Returns      : Flash sector for the specfied MIPS address.
 ***************************************************************************/
static int nand_flash_get_blk(int addr)
{
    return( (addr - FLASH_BASE) / g_chip.chip_block_size );
} /* nand_flash_get_blk */

/***************************************************************************
 * Function Name: nand_flash_get_total_size
 * Description  : Returns the number of bytes in the "CFE Linux code"
 *                partition.
 * Returns      : Number of bytes
 ***************************************************************************/
static int nand_flash_get_total_size(void)
{
    return(g_chip.chip_total_size);
} /* nand_flash_get_total_size */
#endif

/***************************************************************************
 * Function Name: nand_flash_get_sector_size
 * Description  : Returns the number of bytes in the specfied flash sector.
 * Returns      : Number of bytes in the specfied flash sector.
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_get_sector_size(unsigned short sector)
{
    return(g_chip.chip_block_size);
} /* nand_flash_get_sector_size */

/***************************************************************************
 * Function Name: nand_flash_get_numsectors
 * Description  : Returns the number of blocks in the "CFE Linux code"
 *                partition.
 * Returns      : Number of blocks
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_get_numsectors(void)
{
    return(g_chip.chip_total_size / g_chip.chip_block_size);
} /* nand_flash_get_numsectors */


/***************************************************************************
 * NAND Flash Implementation Functions
 ***************************************************************************/

/***************************************************************************
 * Function Name: nandflash_copy_from_cache
 * Description  : Copies data from the chip NAND cache to a local memory
 *                buffer.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_from_cache(unsigned char *buffer,
    int offset, int numbytes)
{
    unsigned char *cache = (unsigned char *) NAND_CACHE;

    /* XXX memcpy will only work for 32-bit aligned data */
    memcpy(buffer, &cache[offset], numbytes);
} /* nandflash_copy_from_cache */

/***************************************************************************
 * Function Name: nandflash_copy_from_spare
 * Description  : Copies data from the chip NAND spare registers to a local
 *                memory buffer.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_from_spare(unsigned char *buffer,
    int numbytes)
{
    unsigned long *spare_area = (unsigned long *) &NAND->NandSpareAreaReadOfs0;

    /* XXX memcpy will only work for 32-bit aligned data */
    memcpy(buffer, spare_area, numbytes);
} /* nandflash_copy_from_spare */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_copy_to_cache
 * Description  : Copies data from a local memory buffer to the the chip NAND
 *                cache.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_to_cache(unsigned char *buffer, int offset,
    int numbytes)
{
    unsigned char *cache = (unsigned char *) NAND_CACHE;
    unsigned long i;

    for( i = 0; i < numbytes; i += sizeof(long) )
        *(unsigned long *) &cache[i] =
            ((unsigned long) buffer[i + 0] << 24) |
            ((unsigned long) buffer[i + 1] << 16) |
            ((unsigned long) buffer[i + 2] <<  8) |
            ((unsigned long) buffer[i + 3] <<  0);
} /* nandflash_copy_to_cache */

/***************************************************************************
 * Function Name: nandflash_copy_to_spare
 * Description  : Copies data from a local memory buffer to the the chip NAND
 *                spare registers.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_to_spare(unsigned char *buffer,int numbytes)
{
    unsigned long *spare_area = (unsigned long *) &NAND->NandSpareAreaWriteOfs0;
    unsigned long *pbuff = (unsigned long *)buffer;
    int i;

    for(i=0; i< numbytes / sizeof(unsigned long); ++i)
        spare_area[i] = pbuff[i];
} /* nandflash_copy_to_spare */
#endif

/***************************************************************************
 * Function Name: nandflash_wait_status
 * Description  : Polls the NAND status register waiting for a condition.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_wait_status(unsigned long status_mask)
{

    const unsigned long nand_poll_max = 1000000;
    unsigned long data;
    unsigned long poll_count = 0;
    int ret = FLASH_API_OK;

    do
    {
        data = NAND->NandIntfcStatus;
    } while(!(status_mask & data) && (++poll_count < nand_poll_max));

    if(poll_count >= nand_poll_max)
    {
        printf("Status wait timeout: nandsts=0x%8.8lx mask=0x%8.8lx, count="
            "%lu\n", NAND->NandIntfcStatus, status_mask, poll_count);
        ret = FLASH_API_ERROR;
    }

    return( ret );
} /* nandflash_wait_status */

static inline int nandflash_wait_cmd(void)
{
    return nandflash_wait_status(NIS_CTLR_READY);
} /* nandflash_wait_cmd */

static inline int nandflash_wait_device(void)
{
    return nandflash_wait_status(NIS_FLASH_READY);
} /* nandflash_wait_device */

static inline int nandflash_wait_cache(void)
{
    return nandflash_wait_status(NIS_CACHE_VALID);
} /* nandflash_wait_cache */

static inline int nandflash_wait_spare(void)
{
    return nandflash_wait_status(NIS_SPARE_VALID);
} /* nandflash_wait_spare */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_check_ecc
 * Description  : Reads ECC status.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_check_ecc(void)
{
    int ret = FLASH_API_OK;
    UINT32 intrCtrl;
    UINT32 accessCtrl;

    /* read interrupt status */
    intrCtrl = NAND_INTR->NandInterrupt;


    if( (intrCtrl & NINT_ECC_ERROR_UNC) != 0 )
    {
        accessCtrl = NAND->NandAccControl;
        printf("Uncorrectable ECC Error detected: intrCtrl=0x%08X, "
            "accessCtrl=0x%08X\n", (UINT)intrCtrl, (UINT)accessCtrl);
        ret = FLASH_API_ERROR;
    }

    if( (intrCtrl & NINT_ECC_ERROR_CORR) != 0 )
    {
        accessCtrl = NAND->NandAccControl;
        printf("Correctable ECC Error detected: intrCtrl=0x%08X, "
            "accessCtrl=0x%08X\n", (UINT)intrCtrl, (UINT)accessCtrl);
    }

    return( ret );
}
#else
static int nandflash_check_ecc(void)
{
    return( FLASH_API_OK );
}
#endif

/***************************************************************************
 * Function Name: nandflash_read_spare_area
 * Description  : Reads the spare area for the specified page.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_read_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len >= pchip->chip_spare_size )
    {
        UINT32 steps = pchip->chip_spare_size / CTRLR_SPARE_SIZE;
        UINT32 i;

        for( i = 0; i < steps; i++ )
        {
            NAND->NandCmdAddr = pchip->chip_base + page_addr +
                (i * CTRLR_CACHE_SIZE);
            NAND->NandCmdExtAddr = 0;
            NAND->NandCmdStart = NCMD_SPARE_READ;

            if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
            {
                /* wait until data is available in the spare area registers */
                if( (ret = nandflash_wait_spare()) == FLASH_API_OK )
                    nandflash_copy_from_spare(buffer + (i * CTRLR_SPARE_SIZE),
                        CTRLR_SPARE_SIZE);
                else
                    break;
            }
            else
                break;
        }
    }

    return ret;
} /* nandflash_read_spare_area */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_write_spare_area
 * Description  : Reads the spare area for the specified page.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_write_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len)
{
    int ret = FLASH_API_OK;
    unsigned char spare[SPARE_MAX_SIZE];

    if( len <= pchip->chip_spare_size )
    {
        UINT32 steps = pchip->chip_spare_size / CTRLR_SPARE_SIZE;
        UINT32 i;

        memset(spare, 0xff, pchip->chip_spare_size);
        memcpy(spare, buffer, len);

        for( i = 0; i < steps; i++ )
        {
            NAND->NandCmdAddr = pchip->chip_base + page_addr +
                (i * CTRLR_CACHE_SIZE);
            NAND->NandCmdExtAddr = 0;

            nandflash_copy_to_spare(spare + (i * CTRLR_SPARE_SIZE),
                CTRLR_SPARE_SIZE);

            NAND->NandCmdStart = NCMD_PROGRAM_SPARE;
            if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
            {
                unsigned long sts = NAND->NandIntfcStatus;

                if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
                {
                    printf("Error writing to spare area, sts=0x%8.8lx\n", sts);
                    ret = FLASH_API_ERROR;
                }
            }
        }
    }
    else
        ret = FLASH_API_ERROR;

    /* Reset spare area to default value. */
    memset(spare, 0xff, CTRLR_SPARE_SIZE);
    nandflash_copy_to_spare(spare, CTRLR_SPARE_SIZE);

    return( ret );
} /* nandflash_write_spare_area */
#endif

/***************************************************************************
 * Function Name: nandflash_read_page
 * Description  : Reads up to a NAND block of pages into the specified buffer.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_read_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len <= pchip->chip_block_size )
    {
        UINT32 page_addr = start_addr & ~(pchip->chip_page_size - 1);
        UINT32 page_offset = start_addr - page_addr;
        UINT32 size = pchip->chip_page_size - page_offset;
        UINT32 index = 0;

        /* Verify that the spare area contains a JFFS2 cleanmarker. */
        if( nand_is_blk_cleanmarker(pchip, page_addr, 0) )
        {
            UINT32 i;

            if(size > len)
                size = len;

            do
            {
                for( i = 0, ret = FLASH_API_OK; i < pchip->chip_page_size &&
                     ret == FLASH_API_OK; i += CTRLR_CACHE_SIZE)
                {
                    /* clear interrupts, so we can check later for ECC errors */
                    NAND_INTR->NandInterrupt = NINT_STS_MASK;

                    /* send command */
                    NAND->NandCmdAddr = pchip->chip_base + page_addr + i;
                    NAND->NandCmdExtAddr = 0;
                    NAND->NandCmdStart = NCMD_PAGE_READ;

                    if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
                    {
                        /* wait until data is available in the cache */
                        if( (ret = nandflash_wait_cache()) == FLASH_API_OK )
                        {
                            /* TBD. get return status for ECC errors and
                             * process them.
                             */
                            nandflash_check_ecc(); /* check for ECC errors */
                        }

                        if( ret == FLASH_API_OK )
                        {
                            if( i < size )
                            {
                                UINT32 copy_size =
                                    (i + CTRLR_CACHE_SIZE <= size)
                                    ? CTRLR_CACHE_SIZE : size - i;

                                nandflash_copy_from_cache(&buffer[index + i],
                                    page_offset, copy_size);
                            }
                        }
                        else
                        {
                            /* TBD. Do something. */
                        }
                    }
                }

                if(ret != FLASH_API_OK)
                    break;

                page_offset = 0;
                page_addr += pchip->chip_page_size;
                index += size;
                len -= size;
                if(len > pchip->chip_page_size)
                    size = pchip->chip_page_size;
                else
                    size = len;
            } while(len);
        }
        else
        {
            DBG_PRINTF("nandflash_read_page: cleanmarker not found at 0x%8.8lx\n",
                page_addr);
        }
    }

    return( ret ) ;
} /* nandflash_read_page */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_write_page
 * Description  : Writes up to a NAND block of pages from the specified buffer.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_write_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len <= pchip->chip_block_size )
    {
        unsigned char xfer_buf[CTRLR_CACHE_SIZE];
        UINT32 page_addr = start_addr & ~(pchip->chip_page_size - 1);
        UINT32 page_offset = start_addr - page_addr;
        UINT32 size = pchip->chip_page_size - page_offset;
        UINT32 index = 0;

        /* Verify that the spare area contains a JFFS2 cleanmarker. */
        if( nand_is_blk_cleanmarker(pchip, page_addr, 1) )
        {
            UINT32 steps = pchip->chip_page_size / CTRLR_CACHE_SIZE;
            UINT32 i, xfer_ofs, xfer_size;

            if(size > len)
                size = len;

            do
            {
                for( i = 0, xfer_ofs = 0, xfer_size = 0, ret = FLASH_API_OK;
                     i < steps && ret==FLASH_API_OK; i++)
                {
                    memset(xfer_buf, 0xff, sizeof(xfer_buf));

                    if(size - xfer_ofs > CTRLR_CACHE_SIZE)
                        xfer_size = CTRLR_CACHE_SIZE;
                    else
                        xfer_size = size - xfer_ofs;

                    if( xfer_size )
                        memcpy(xfer_buf + page_offset, buffer + index +
                            xfer_ofs, xfer_size);

                    xfer_ofs += xfer_size;

                    NAND->NandCmdAddr = pchip->chip_base + page_addr +
                        (i * CTRLR_CACHE_SIZE);
                    NAND->NandCmdExtAddr = 0;

                    nandflash_copy_to_cache(xfer_buf, 0, CTRLR_CACHE_SIZE);

                    NAND->NandCmdStart = NCMD_PROGRAM_PAGE;
                    if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
                    {
                        unsigned long sts = NAND->NandIntfcStatus;

                        if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
                        {
                            printf("Error writing to block, sts=0x%8.8lx\n", sts);
                            nand_mark_bad_blk(pchip,
                                start_addr & ~(pchip->chip_page_size - 1));
                            ret = FLASH_API_ERROR;
                        }
                    }
                }

                if(ret != FLASH_API_OK)
                    break;

                page_offset = 0;
                page_addr += pchip->chip_page_size;
                index += size;
                len -= size;
                if(len > pchip->chip_page_size)
                    size = pchip->chip_page_size;
                else
                    size = len;
            } while(len);
        }
        else
            DBG_PRINTF("nandflash_write_page: cleanmarker not found at 0x%8.8lx\n",
                page_addr);
    }

    return( ret );
} /* nandflash_write_page */

/***************************************************************************
 * Function Name: nandflash_block_erase
 * Description  : Erases a block.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_block_erase(PCFE_NAND_CHIP pchip, unsigned long blk_addr )
{

    int ret = FLASH_API_OK;

    /* send command */
    NAND->NandCmdAddr = pchip->chip_base + blk_addr;
    NAND->NandCmdExtAddr = 0;
    NAND->NandCmdStart = NCMD_BLOCK_ERASE;
    if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
    {
        unsigned long sts = NAND->NandIntfcStatus;

        if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
        {
            printf("Error erasing block, sts=0x%8.8lx\n", sts);
            ret = FLASH_API_ERROR;
        }
    }

    DBG_PRINTF(">> nandflash_block_erase - addr=0x%8.8lx, ret=%d\n", blk_addr,
        ret);

    return( ret );
} /* nandflash_block_erase */

void dump_spare(void);
void dump_spare(void)
{
    PCFE_NAND_CHIP pchip = &g_chip;
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long i;

    for( i = 0; i < pchip->chip_total_size; i += pchip->chip_block_size )
    {
        if( nandflash_read_spare_area(pchip, i, spare,
            pchip->chip_spare_size) == FLASH_API_OK )
        {
            printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                *(unsigned long *) &spare[0], *(unsigned long *) &spare[4],
                *(unsigned long *) &spare[8], *(unsigned long *) &spare[12]);
            if( pchip->chip_spare_size == SPARE_MAX_SIZE )
            {
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[16],*(unsigned long *)&spare[20],
                    *(unsigned long *)&spare[24],*(unsigned long *)&spare[28]);
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[32],*(unsigned long *)&spare[36],
                    *(unsigned long *)&spare[40],*(unsigned long *)&spare[44]);
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[48],*(unsigned long *)&spare[52],
                    *(unsigned long *)&spare[56],*(unsigned long *)&spare[60]);
            }
        }
        else
            printf("Error reading spare 0x%8.8lx\n", i);
    }
}

int read_spare_data(int blk, unsigned char *buf, int bufsize);
int read_spare_data(int blk, unsigned char *buf, int bufsize)
{
    PCFE_NAND_CHIP pchip = &g_chip;
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long page_addr = blk * pchip->chip_block_size;
    unsigned long i, j;
    int ret;

    if( (ret = nandflash_read_spare_area( pchip, page_addr, spare,
        pchip->chip_spare_size)) == FLASH_API_OK )
    {
        /* Skip spare offsets that are reserved for the ECC.  Make spare data
         * bytes contiguous in the spare buffer.
         */
        for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
            if( pchip->chip_spare_mask[i] == 0 && j < bufsize )
                buf[j++] = spare[i];
    }

    return(ret);
}

#endif

