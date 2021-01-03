#ifdef WLC_LOW
#ifndef LINUX_VERSION_CODE 
#include <linuxver.h>
#endif

#define MAX_SROM_FILE_SIZE SROM_MAX

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <osl.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <typedefs.h>
#include <bcmdevs.h>
#include "boardparms.h"
#include "bcmsrom_fmt.h"
#include "siutils.h"
#include "bcmutils.h"

int BCMATTACHFN(sprom_update_params)(si_t *sih, uint16 *buf );

int BCMATTACHFN(init_srom_sw_map)(si_t *sih, uint chipId, void *buf, uint nbytes) 
{	
	struct file *fp = NULL;
	char fname[32];
	int ret = -1;
	char base[MAX_SROM_FILE_SIZE]={0};
	mm_segment_t fs;
	int size;
	int patch_status = BP_BOARD_ID_NOT_FOUND;

	if ((chipId & 0xff00) == 0x4300 || (chipId & 0xff00) == 0x6300 ) /* DSLCPE_WOC */
		sprintf(fname, "/etc/wlan/bcm%04x_map.bin", chipId);
	else if ((chipId / 1000) == 43) 
		sprintf(fname, "/etc/wlan/bcm%d_map.bin", chipId);
	else
		return ret;

#if defined(CONFIG_BRCM_IKOS) /* DSLCPE_WOC */
	goto direct_read;
#endif	
	ASSERT(nbytes <= MAX_SROM_FILE_SIZE);

	printk("wl:loading %s\n", fname);		
	fp = filp_open(fname, O_RDONLY, 0);	

	if(!IS_ERR(fp) && fp->f_op && fp->f_op->read ) {
		fs = get_fs();
		set_fs(get_ds());
		
		fp->f_pos = 0;
	
		size = fp->f_op->read(fp, (void *)base, MAX_SROM_FILE_SIZE, &fp->f_pos);
		if (size <= 0) {
			printk("Failed to read image header from '%s'.\n", fname);
		} else {		
			/* patch srom map with info from boardparams */
			patch_status = BpUpdateWirelessSromMap(chipId, (uint16*)base, size/sizeof(uint16));
				
			/* patch srom map with info from CPE's flash */
			sprom_update_params(sih, (uint16 *)base);
			bcopy(base, buf, MIN(size,nbytes));
			ret = 0;
		}
		set_fs(fs);		
		
		filp_close(fp, NULL);							
	} else {

#if defined(CONFIG_BRCM_IKOS) /* DSLCPE_WOC */
direct_read:
#endif		
		printk("Failed to open srom image from '%s'.\n", fname);

#ifdef DSLCPE_WOMBO_BUILTIN_SROM
		{
			uint16 *wl_srom_map = NULL;
			extern uint16 wl_srom_map_4306[64];
#ifdef DSLCPE_SDIO
			extern uint16 wl_srom_map_4318[256];
			extern uint16 wl_srom_map_4312[256];		
#else		
			extern uint16 wl_srom_map_4318[64];		
#endif						
			extern uint16 wl_srom_map_4321[220];
			extern uint16 wl_srom_map_4322[220];
			extern uint16 wl_srom_map_a8d6[220]; /* 43222 */
			extern uint16 wl_srom_map_6362[220]; /* 43226, DSLCPE_WOC */
			int len = 0;
			if(ret == -1) {
				
				switch (chipId) {
					case 0x4306:
						wl_srom_map = wl_srom_map_4306;
						len = sizeof(wl_srom_map_4306);
						break;
					case 0x4318:
						wl_srom_map = wl_srom_map_4318;
						len = sizeof(wl_srom_map_4318);
						break;				
					case 0x4321:
						wl_srom_map = wl_srom_map_4321;
						len = sizeof(wl_srom_map_4321);
						break;
					case 0xa8d6: /* 43222 */
						wl_srom_map = wl_srom_map_a8d6;
						len = sizeof(wl_srom_map_a8d6);
						break;						
					case 0x4322:
						wl_srom_map = wl_srom_map_4322;				
						len = sizeof(wl_srom_map_4322);
						break;
#ifdef DSLCPE_SDIO	
				 	case 0x4312:
						wl_srom_map = wl_srom_map_4312;
						len = sizeof(wl_srom_map_4312);
						break;
#endif					
					case 0x6362: /* 43226, DSLCPE_WOC */
						wl_srom_map = wl_srom_map_6362;
						len = sizeof(wl_srom_map_6362);
						break;
					default:
						break;
				}
			}
			printk("Reading srom image from kernel\n");		
			if(wl_srom_map) {
				bcopy(wl_srom_map, buf, MIN(len, nbytes));
				ret = 0;
			} else {
				printk("Error reading srom image from kernel\n");
				ret = -1;
			}		
		}			
#endif	/* DSLCPE_WOMBO_BUILTIN_SROM */
	}

	return ret;	
		
}


int read_sromfile(void *swmap, void *buf, uint offset, uint nbytes)
{
	bcopy((char*)swmap+offset, (char*)buf, nbytes);
	return 0;
}


extern int kerSysGetWlanSromParams( unsigned char *wlanCal, unsigned short len);
#define SROM_PARAMS_LEN 256

typedef struct entry_struct {
unsigned short offset;
unsigned short value;
} Entry_struct;

typedef struct adapter_struct {
unsigned char id[SI_DEVPATH_BUFSZ];
unsigned short entry_num;
struct entry_struct  entry[1];
} Adapter_struct;

int BCMATTACHFN(sprom_update_params)(si_t *sih, uint16 *buf )
{
	char params[SROM_PARAMS_LEN];
	uint16 adapter_num=0, entry_num=0, pos=0;
	
	struct adapter_struct *adapter_ptr = NULL;
	struct entry_struct *entry_ptr = NULL;
	
	char id[SI_DEVPATH_BUFSZ];
	int i=0, j=0;
	int ret = 0;
	   
	char devpath[SI_DEVPATH_BUFSZ];

	si_devpath(sih, devpath, sizeof(devpath));
	kerSysGetWlanSromParams( params, SROM_PARAMS_LEN);
	
	/* params format
	adapter_num uint16
	devId1            char 16
	entry_num      uint 16
	offset1            uint16
	value1            uint16
	offset2            uint16
	value2            uint16
	...
	devId2           char 16
	entry_num     uint 16
	offset1           uint16
	value2           uint16
	
	*/
       adapter_num = *(uint16 *)(params);
       pos = 2;

	for ( i=0; (i< adapter_num) && (pos <SROM_PARAMS_LEN); i++ ) {
        adapter_ptr = (struct adapter_struct *)(params +pos);
		strncpy( id, adapter_ptr->id, SI_DEVPATH_BUFSZ );			  

 		entry_num=  adapter_ptr->entry_num;
			  
		if ( !strncmp(id, devpath, strlen(devpath) ) ) {
 		       entry_ptr = (struct entry_struct *) &(adapter_ptr->entry);
 		       printk("wl: updating srom from flash...\n");
 		       for ( j=0; j< entry_num; j++, entry_ptr++ ) {
				buf[entry_ptr->offset] = entry_ptr->value;
		      }

			ret = 1;
			break;
		}

		/*goto next adapter parameter*/
		pos += SI_DEVPATH_BUFSZ + sizeof(uint16)+ entry_num*sizeof(uint16)*2; 
       }
	return ret;
}

#else
/* no longer maintained for linux 2.4, compare above */
#error "kernel version not supported"

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) */
#endif /* WLC_LOW */
