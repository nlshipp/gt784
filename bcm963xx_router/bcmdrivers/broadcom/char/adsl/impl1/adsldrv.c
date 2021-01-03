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
/***************************************************************************
 * File Name  : AdslDrv.c
 *
 * Description: This file contains Linux character device driver entry points
 *              for the ATM API driver.
 *
 * Updates    : 08/24/2001  lat.  Created.
 ***************************************************************************/


/* Includes. */
#include <linux/version.h>
#include <linux/module.h>

#include <linux/signal.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <bcmtypes.h>
#include <adsldrv.h>
#include <bcmadsl.h>
#include <bcmatmapi.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#include <bcmxtmcfg.h>
#endif
#include <board.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
#include <bcmnetlink.h>
#endif

#include <DiagDef.h>
#include "BcmOs.h"
#include "softdsl/SoftDsl.h"
#include "AdslCore.h"

#ifdef HMI_QA_SUPPORT
#include "BcmXdslHmi.h"
#endif

#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
#include "linux/poll.h"
#endif

#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
#include "BcmAdslCore.h"
#endif

#ifndef DSL_IFNAME
#define	DSL_IFNAME "dsl0"
#endif

#if defined(CONFIG_BCM_ATM_BONDING_ETH) || defined(CONFIG_BCM_ATM_BONDING_ETH_MODULE)
extern int atmbonding_enabled;
extern BCMADSL_STATUS BcmAdsl_SetGfc2VcMapping(int bOn) ;
#endif

#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
#ifndef TRAFFIC_TYPE_PTM_RAW
#define	TRAFFIC_TYPE_PTM_RAW		3
#endif
#ifndef	TRAFFIC_TYPE_PTM_BONDED
#define	TRAFFIC_TYPE_PTM_BONDED	4
#endif
#ifndef	TRAFFIC_TYPE_ATM_BONDED
#define	TRAFFIC_TYPE_ATM_BONDED	0	/* The actual defined value in the new Bcmxtmcfg.h is 3, but since it is
												contradicted with TRAFFIC_TYPE_PTM_RAW, 0 is defined. This should cause
												 no harm b/c if Linux supports it, it would have been defined */
#endif
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
struct net_device_stats *dsl_get_stats(struct net_device *dev);
static const struct net_device_ops dsl_netdev_ops = {
    .ndo_get_stats      = dsl_get_stats,
 };
#endif


/* typedefs. */
typedef void (*ADSL_FN_IOCTL) (unsigned char lineId, unsigned long arg);

/* Externs. */
BCMADSL_STATUS BcmAdsl_SetVcEntry (int gfc, int port, int vpi, int vci);
BCMADSL_STATUS BcmAdsl_SetVcEntryEx (int gfc, int port, int vpi, int vci, int pti);
void BcmAdsl_AtmSetPortId(int path, int portId);
void BcmAdsl_AtmClearVcTable(void);
void BcmAdsl_AtmAddVc(int vpi, int vci);
void BcmAdsl_AtmDeleteVc(int vpi, int vci);
void BcmAdsl_AtmSetMaxSdu(unsigned short maxsdu);

#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
extern void *g_pfnAdslSetVcEntry;
extern void *g_pfnAdslSetAtmLoopbackMode;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
extern void *g_pfnAdslSetVcEntryEx;
#ifdef CONFIG_ATM_EOP_MONITORING
extern void *g_pfnAdslAtmClearVcTable;
extern void *g_pfnAdslAtmAddVc;
extern void *g_pfnAdslAtmDeleteVc;
extern void *g_pfnAdslAtmSetMaxSdu;
#endif
#ifdef BCM_ATM_BONDING
extern void *g_pfnAdslAtmbSetPortId;
#endif
#if defined(BCM_ATM_BONDING) || defined(BCM_ATM_BONDING_ETH)
extern void *g_pfnAdslAtmbGetConnInfo;
static BCMADSL_STATUS BcmAdsl_GetConnectionInfo1(PADSL_CONNECTION_INFO pConnectionInfo)
{
	return (BcmAdsl_GetConnectionInfo(0, pConnectionInfo));
}
#endif
#endif

#else	/* 6368 arch. */
extern Bool BcmCoreDiagLogActive(ulong map);
#ifdef XTM_USE_DSL_MIB
extern void *g_pfnAdslGetObjValue;
#endif
#endif

extern void BcmAdslCoreDiagWriteStatusString(unsigned char lineId, char *fmt, ...);

/* Prototypes. */
static int __init adsl_init( void );
static void __exit adsl_cleanup( void );
static int adsl_open( struct inode *inode, struct file *filp );
static int adsl_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg );
static void DoCheck( unsigned char lineId, unsigned long arg );
static void DoInitialize( unsigned char lineId, unsigned long arg );
static void DoUninitialize( unsigned char lineId, unsigned long arg );
static void DoConnectionStart( unsigned char lineId, unsigned long arg );
static void DoConnectionStop( unsigned char lineId, unsigned long arg );
static void DoGetPhyAddresses( unsigned char lineId, unsigned long arg );
static void DoSetPhyAddresses( unsigned char lineId, unsigned long arg );
static void DoMapATMPortIDs( unsigned char lineId, unsigned long arg );
static void DoGetConnectionInfo( unsigned char lineId, unsigned long arg );
static void DoDiagCommand( unsigned char lineId, unsigned long arg );
static void DoGetObjValue( unsigned char lineId, unsigned long arg );
static void DoSetObjValue( unsigned char lineId, unsigned long arg );
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
static void DoDslWanLineStatus( unsigned char lineId, unsigned long arg );
#endif
static void DoStartBERT( unsigned char lineId, unsigned long arg );
static void DoStopBERT( unsigned char lineId, unsigned long arg );
static void DoConfigure( unsigned char lineId, unsigned long arg );
static void DoTest( unsigned char lineId, unsigned long arg );
static void DoGetConstelPoints( unsigned char lineId, unsigned long arg );
static void DoGetVersion( unsigned char lineId, unsigned long arg );
static void DoSetSdramBase( unsigned char lineId, unsigned long arg );
static void DoResetStatCounters( unsigned char lineId, unsigned long arg );
static void DoSetOemParameter( unsigned char lineId, unsigned long arg );
static void DoBertStartEx( unsigned char lineId, unsigned long arg );
static void DoBertStopEx( unsigned char lineId, unsigned long arg );
static void AdslConnectCb(unsigned char lineId, ADSL_LINK_STATE dslLinkState, ADSL_LINK_STATE dslPrevLinkState, UINT32 ulParm );
#ifdef HMI_QA_SUPPORT
static void DoHmiCommand( unsigned char lineId, unsigned long arg );
#endif

#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
static ssize_t adsl_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos);
static ssize_t adsl_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos);
static unsigned int adsl_poll(struct file *file, poll_table *wait);
extern void dumpaddr( unsigned char *pAddr, int nLen );
#endif /* defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO) */

#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_TOGGLE_DATAPUMP)
extern int datapumpAutodetect;
static void DoDslDatapumpAutoDetect( unsigned char lineId, unsigned long arg );
#endif

#if defined(AEI_VDSL_CUSTOMER_NCS)
static void DoEmpty( unsigned char lineId, unsigned long arg );
#endif

typedef struct
{
  int readEventMask;    
  int writeEventMask;    
  char *buffer;
} ADSL_IO, *pADSL_IO;

/* Globals. */
static struct file_operations adsl_fops =
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    .ioctl      = adsl_ioctl,
    .open       = adsl_open,
#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
    .read       = adsl_read,
    .write      = adsl_write,
    .poll       = adsl_poll,
#endif 
#else
    ioctl:      adsl_ioctl,
    open:       adsl_open,
#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
    read:       adsl_read,
    write:      adsl_write,
    poll:       adsl_poll,
#endif 
#endif
};
struct semaphore semMibShared;
static UINT16 g_usAtmFastPortId;
static UINT16 g_usAtmInterleavedPortId;
static struct net_device_stats g_dsl_if_stats;
static struct net_device *g_dslNetDev;
#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
static int g_eoc_hdr_offset = 0;
static int g_eoc_hdr_len = 0;
#endif

#ifdef DSL_KTHREAD
static DECLARE_COMPLETION(dslPollDone);
static atomic_t dslPollLock = ATOMIC_INIT(1);
static pid_t dslPid = -1;

extern void BcmXdslStatusPolling(void);
extern void XdslCoreSetAhifState(unsigned char lineId, ulong state, ulong event);
extern void *XdslCoreGetDslVars(unsigned char lineId);
extern Bool XdslMibIsGinpActive(void *gDslVars, unsigned char direction);

static int dslThread(void * data)
{
    daemonize(DSL_IFNAME);     // remove any user space mem maps and
    
    while (atomic_read(&dslPollLock) > 0) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ);
        BcmXdslStatusPolling();
    }
    
    printk("***%s: Exiting ***\n", __FUNCTION__);
    complete_and_exit(&dslPollDone, 0);
    
    return 0;
}
void dslThreadTerminate(void)
{
    if(dslPid >= 0 ) {
        printk("***%s: Unlocking dslThread dslPid=%d\n", __FUNCTION__, dslPid);
        dslPid = -1;
        atomic_dec(&dslPollLock);
        wait_for_completion(&dslPollDone);
        printk("***%s: Exiting\n", __FUNCTION__);
    }
}
#endif

/* Globals. */

#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)

#define DSL_BONDING_LINE_TIMEOUT 	300

extern unsigned long volatile jiffies;

typedef struct DslLineBondingInfo
{
  int nBondingStatus;    // 0: Disable ; 1: Enable    
  int nLine1Status;	// 0: Line Down;    1: Line Up
  int nLine2Status;	// 0: Line Down;    1: Line Up
  struct semaphore sem;   
  struct timer_list line1Timer;   
  struct timer_list line2Timer;   
} DSL_LINE_BONDING_INFO, *PDSL_LINE_BONDING_INFO;

static DSL_LINE_BONDING_INFO glbDslLineBondingInfo;

//Handel DSL Bonding Line LED Info when DSL Bonding Line timeout 
static void HandleDslBondingLineTimer(ulong arg)
{
	down_interruptible(&glbDslLineBondingInfo.sem);
	if(glbDslLineBondingInfo.nBondingStatus)
	{
		if(0 == arg )
		{
		    if((0 == glbDslLineBondingInfo.nLine2Status) && glbDslLineBondingInfo.nLine1Status)
    		    	kerSysLedCtrl(kLedAdsl,kLedStateOff);
		}
		else
		{
		    if((0 == glbDslLineBondingInfo.nLine1Status) && glbDslLineBondingInfo.nLine2Status)
    			kerSysLedCtrl(kLedSecAdsl,kLedStateOff);
		}
	}
	up(&glbDslLineBondingInfo.sem);
}

//Add DSL Bonding Line Timer when DSL Bonding Line link up.
static void StartDslBondingLineTimer(int nLineID)
{
	ADSL_CONNECTION_INFO ConnectionInfo;
	if(nLineID == 0)
		BcmAdslCoreGetConnectionInfo(nLineID, &ConnectionInfo);
	else if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
		BcmAdslCoreGetConnectionInfo(nLineID, &ConnectionInfo);
	else
		ConnectionInfo.LinkState = ADSL_LINK_DOWN;

	if(glbDslLineBondingInfo.nBondingStatus)
	{
		if (ConnectionInfo.LinkState != ADSL_LINK_UP) return;
		down_interruptible(&glbDslLineBondingInfo.sem);

		if(0 == nLineID && 0 == glbDslLineBondingInfo.nLine2Status)
		{
		    del_timer(&glbDslLineBondingInfo.line1Timer);
		    init_timer(&glbDslLineBondingInfo.line1Timer);
		    glbDslLineBondingInfo.line1Timer.function = HandleDslBondingLineTimer;
		    glbDslLineBondingInfo.line1Timer.expires = jiffies + DSL_BONDING_LINE_TIMEOUT * HZ;
		    glbDslLineBondingInfo.line1Timer.data = nLineID;
		    add_timer(&glbDslLineBondingInfo.line1Timer);
		}
		else if(1 == nLineID && 0 == glbDslLineBondingInfo.nLine1Status)
		{
		    del_timer(&glbDslLineBondingInfo.line2Timer);
		    init_timer(&glbDslLineBondingInfo.line2Timer);
		    glbDslLineBondingInfo.line2Timer.function = HandleDslBondingLineTimer;
		    glbDslLineBondingInfo.line2Timer.expires = jiffies + DSL_BONDING_LINE_TIMEOUT * HZ;
		    glbDslLineBondingInfo.line2Timer.data = nLineID;
		    add_timer(&glbDslLineBondingInfo.line2Timer);
		}
		up(&glbDslLineBondingInfo.sem);
	}
}

//initialize glbDslLineBondingInfo object
static void InitDslLineBondingInfo(PDSL_LINE_BONDING_INFO pInfo)
{
	pInfo->nLine1Status = 0;
	pInfo->nLine2Status = 0;
	init_MUTEX(&pInfo->sem);
        unsigned char dslDatapump = 0;
	kerSysGetDslDatapump(&dslDatapump);
	if(dslDatapump == 2) //non-bonding
		pInfo->nBondingStatus = 0;
	else 
		pInfo->nBondingStatus = 1;
}

#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#if defined(MODULE)
/***************************************************************************
 * Function Name: init_module
 * Description  : Initial function that is called if this driver is compiled
 *                as a module.  If it is not, adsl_init is called in
 *                chr_dev_init() in drivers/char/mem.c.
 * Returns      : None.
 ***************************************************************************/
int init_module(void)
{
    return( adsl_init() );
}


/***************************************************************************
 * Function Name: cleanup_module
 * Description  : Final function that is called if this driver is compiled
 *                as a module.
 * Returns      : None.
 ***************************************************************************/
void cleanup_module(void)
{
    if (MOD_IN_USE)
        printk("adsl: cleanup_module failed because module is in use\n");
    else {
#ifdef DSL_KTHREAD
        dslThreadTerminate();
#endif
        adsl_cleanup();
    }
}
#endif //MODULE 
#endif //LINUX_VERSION_CODE

/***************************************************************************
 * Function Name: dsl_config_netdev
 * Description  : Configure a network device for DSL interface
 * Returns      : None.
 ***************************************************************************/
struct net_device_stats *dsl_get_stats(struct net_device *dev)
{
/*
        UINT32 interfaceId;
        ATM_INTERFACE_CFG Cfg;
        ATM_INTERFACE_STATS KStats;
        PATM_INTF_ATM_STATS pAtmStats = &KStats.AtmIntfStats;

        BcmAtm_GetInterfaceId(0, &interfaceId);
        Cfg.ulStructureId = ID_ATM_INTERFACE_CFG;
        KStats.ulStructureId = ID_ATM_INTERFACE_STATS;
        if ( BcmAtm_GetInterfaceCfg(interfaceId, &Cfg)==STS_SUCCESS &&
            BcmAtm_GetInterfaceStatistics(interfaceId, &KStats, 0)==STS_SUCCESS)
        {
            atm_if_stats.rx_bytes = (unsigned long) pAtmStats->ulIfInOctets;
            atm_if_stats.tx_bytes = (unsigned long) pAtmStats->ulIfOutOctets;
            atm_if_stats.rx_errors = (unsigned long) pAtmStats->ulIfInErrors;
            atm_if_stats.tx_errors = (unsigned long) pAtmStats->ulIfOutErrors;  
        }
*/
        return &g_dsl_if_stats;
}

#ifdef __KERNEL__
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
long DbgAddr(long a) { return((long) (a) < 0 ? (a) : (ulong) (a) + (ulong) init_module); }
#else
long DbgAddr(long a) { return((long) (a) < 0 ? (a) : (ulong) (a) + (ulong) dsl_get_stats); }
#endif
#else	/* __KERNEL__ */
long DbgAddr(long a) { return a; }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static void dsl_setup(struct net_device *dev) {
        
    dev->type = ARPHRD_DSL;
    dev->mtu = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
#else
    dev->get_stats = dsl_get_stats;
#endif
}

static int dsl_config_netdev(void) {

    g_dslNetDev = alloc_netdev(0, DSL_IFNAME, dsl_setup);
    if ( g_dslNetDev )
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)  
         g_dslNetDev->netdev_ops = &dsl_netdev_ops;
#endif
         return register_netdev(g_dslNetDev);
    }
    else    
         return -ENOMEM;
}
#else
static int dsl_config_netdev(void) {
    int status = 0;
    char name[16]="";

    sprintf(name, "dsl0");
    g_dslNetDev = dev_alloc(name,  &status);
    g_dslNetDev->type = ARPHRD_DSL;
    g_dslNetDev->mtu = 0;
    g_dslNetDev->get_stats = dsl_get_stats;
    register_netdev(g_dslNetDev);    
    return status;
}
#endif

/***************************************************************************
 * Function Name: adsl_init
 * Description  : Initial function that is called at system startup that
 *                registers this device.
 * Returns      : None.
 ***************************************************************************/
static int __init adsl_init( void )
{
    printk( "adsl: adsl_init entry\n" );
	sema_init( &semMibShared,1);
    if ( register_chrdev( ADSLDRV_MAJOR, "adsl", &adsl_fops ) )
    {
         printk("adsldd: failed to create adsl\n");
         return( -1 );
    }            
    if ( dsl_config_netdev() )
    {
         printk("adsldd: failed to create %s\n", DSL_IFNAME);
         return( -1 );
    }
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
    InitDslLineBondingInfo(&glbDslLineBondingInfo);
#endif
#ifdef DSL_KTHREAD
    dslPid = kernel_thread(dslThread,      // thread function
                            NULL,                      // data argument
                            CLONE_KERNEL    // flags
                            );
    //printk("*** dslThread dslPid=%d\n", dslPid);
    return ((dslPid < 0)? -ENOMEM: 0);
#else
    return 0;
#endif
} /* adsl_init */


/***************************************************************************
 * Function Name: adsl_cleanup
 * Description  : Final function that is called when the module is unloaded.
 * Returns      : None.
 ***************************************************************************/
static void __exit adsl_cleanup( void )
{
    printk( "adsl: adsl_cleanup entry\n" );
    unregister_chrdev( ADSLDRV_MAJOR, "adsl" );
    if(g_dslNetDev) {
        unregister_netdev(g_dslNetDev);
	 free_netdev(g_dslNetDev);
	 g_dslNetDev = NULL;
    }
} /* adsl_cleanup */


/***************************************************************************
 * Function Name: adsl_open
 * Description  : Called when an application opens this device.
 * Returns      : 0 - success
 ***************************************************************************/
static int adsl_open( struct inode *inode, struct file *filp )
{
    return( 0 );
} /* adsl_open */


/***************************************************************************
 * Function Name: adsl_ioctl
 * Description  : Main entry point for an application send issue ATM API
 *                requests.
 * Returns      : 0 - success or error
 ***************************************************************************/
static int adsl_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg )
{
    int ret = 0;
    unsigned int cmdnr = _IOC_NR(command);
    unsigned char lineId = MINOR(inode->i_rdev);

    ADSL_FN_IOCTL IoctlFuncs[] = {DoCheck, DoInitialize, DoUninitialize,
                DoConnectionStart, DoConnectionStop, DoGetPhyAddresses, DoSetPhyAddresses,
                DoMapATMPortIDs,DoGetConnectionInfo, DoDiagCommand, DoGetObjValue, 
                DoStartBERT, DoStopBERT, DoConfigure, DoTest, DoGetConstelPoints,
                DoGetVersion, DoSetSdramBase, DoResetStatCounters, DoSetOemParameter, 
                DoBertStartEx, DoBertStopEx , DoSetObjValue

#if defined(AEI_VDSL_CUSTOMER_NCS)
/* Since the ioctl number is defined only in adsldrv.h and used both in kernel and userspace,
   don't do any macro especially macro within a macro for IOCTL number there since that makes adding a new one hard and messy...
   Modify this code here to match the IOCTL with correct or DoEmpty dummy handler.
 */

#ifdef HMI_QA_SUPPORT
                ,DoHmiCommand
#else
		,DoEmpty
#endif
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
                ,DoDslWanLineStatus
#else
		,DoEmpty
#endif
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_TOGGLE_DATAPUMP)
                ,DoDslDatapumpAutoDetect
#else
		,DoEmpty
#endif


#else /* AEI_VDSL_CUSTOMER_NCS */


#ifdef HMI_QA_SUPPORT
                ,DoHmiCommand
#endif


#endif /* AEI_VDSL_CUSTOMER_NCS */

                };
#ifdef SUPPORT_DSL_BONDING
    if( lineId > 1 ) {
        printk("%s: Invalid lineId(%d)!\n", __FUNCTION__, lineId);
        return -EINVAL;
    }
#else
    if( lineId > 0 ) {
       printk("%s: Invalid lineId(%d)!\n", __FUNCTION__, lineId);
        return -EINVAL;
    }
#endif

    if( cmdnr >= 0 && cmdnr < MAX_ADSLDRV_IOCTL_COMMANDS )
        (*IoctlFuncs[cmdnr]) (lineId, arg);
    else
        ret = -EINVAL;

    return( ret );
} /* adsl_ioctl */


/***************************************************************************
 * Function Name: DoCheck
 * Description  : Calls BcmAdsl_Check on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoCheck( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_Check();
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
} /* DoCheck */


/***************************************************************************
 * Function Name: DoInitialize
 * Description  : Calls BcmAdsl_Initialize on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoInitialize( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_INITIALIZE      KArg;
        adslCfgProfile          adslCfg, *pAdslCfg;

    /* For now, the user callback function is not used.  Rather,
     * this module handles the connection up/down callback.  The
     * user application will need to call BcmAdsl_GetConnectionStatus
     * in order to determine the state of the connection.
     */

        pAdslCfg = NULL;
        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;

                if (NULL != KArg.pAdslCfg) {
                        if( copy_from_user( &adslCfg, KArg.pAdslCfg, sizeof(adslCfg)) != 0 )
                                break;
                        pAdslCfg = &adslCfg;
                }
        } while (0);

    KArg.bvStatus = BcmAdsl_Initialize(lineId, AdslConnectCb, 0, pAdslCfg);
#ifdef DYING_GASP_API
    kerSysRegisterDyingGaspHandler(DSL_IFNAME, &BcmAdsl_DyingGaspHandler, 0);
#endif
    put_user( KArg.bvStatus, &((PADSLDRV_INITIALIZE) arg)->bvStatus );

    /* This ADSL module is loaded after the ATM API module so the ATM API
     * cannot call the functions below directly.  This modules sets the
     * function address to a global variable which is used by the ATM API
     * module.
     */
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)

    g_pfnAdslSetVcEntry = (void *) BcmAdsl_SetVcEntry;
    g_pfnAdslSetAtmLoopbackMode = (void *) BcmAdsl_SetAtmLoopbackMode;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    g_pfnAdslSetVcEntryEx = (void *) BcmAdsl_SetVcEntryEx;
#ifdef CONFIG_ATM_EOP_MONITORING

    g_pfnAdslAtmClearVcTable = BcmAdsl_AtmClearVcTable;
    g_pfnAdslAtmAddVc = BcmAdsl_AtmAddVc;
    g_pfnAdslAtmDeleteVc = BcmAdsl_AtmDeleteVc;
    g_pfnAdslAtmSetMaxSdu = BcmAdsl_AtmSetMaxSdu;
#endif
#ifdef BCM_ATM_BONDING
	 g_pfnAdslAtmbSetPortId = BcmAdsl_AtmSetPortId;
#endif
#if defined(BCM_ATM_BONDING) || defined(BCM_ATM_BONDING_ETH)
    g_pfnAdslAtmbGetConnInfo = BcmAdsl_GetConnectionInfo1;
#endif

#if defined(CONFIG_BCM_ATM_BONDING_ETH) || defined(CONFIG_BCM_ATM_BONDING_ETH_MODULE)
    if (atmbonding_enabled == 1)
    {
      printk("Disabling Gfc2VcMapping\n");
      BcmAdsl_SetGfc2VcMapping(0);
    }
#endif
#endif

#endif /* 6348 arch. */

#if defined(XTM_USE_DSL_MIB)
    g_pfnAdslGetObjValue = (void *) BcmAdsl_GetObjectValue;
#endif

#if defined(AEI_VDSL_CUSTOMER_QWEST)
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
    if(glbDslLineBondingInfo.nBondingStatus)
    {
    	kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
   	kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
    }
    else
    {
	if(lineId == 1)
	{
	    kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
    	    kerSysLedCtrl(kLedSecAdsl, kLedStateFail);
	}	
	else
	{
    	    kerSysLedCtrl(kLedAdsl, kLedStateFail);
    	    kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
	}	
    }
#else
    /* Set LED into mode that is looking for carrier. */
    kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
#ifdef SUPPORT_DSL_BONDING
    kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
#endif
#endif
#else
    /* Set LED into mode that is looking for carrier. */
    kerSysLedCtrl(kLedAdsl, kLedStateFail);
#ifdef SUPPORT_DSL_BONDING
    kerSysLedCtrl(kLedSecAdsl, kLedStateFail);
#endif
#endif
} /* DoInitialize */


/***************************************************************************
 * Function Name: DoUninitialize
 * Description  : Calls BcmAdsl_Uninitialize on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoUninitialize( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_Uninitialize(lineId);
#ifdef DYING_GASP_API
    kerSysDeregisterDyingGaspHandler(DSL_IFNAME);
#endif
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
} /* DoUninitialize */


/***************************************************************************
 * Function Name: DoConnectionStart
 * Description  : Calls BcmAdsl_ConnectionStart on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoConnectionStart( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_ConnectionStart(lineId);
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
} /* DoConnectionStart */


/***************************************************************************
 * Function Name: DoConnectionStop
 * Description  : Calls BcmAdsl_ConnectionStop on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoConnectionStop( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_ConnectionStop(lineId);
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
} /* DoConnectionStop */


/***************************************************************************
 * Function Name: DoGetPhyAddresses
 * Description  : Calls BcmAdsl_GetPhyAddresses on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetPhyAddresses( unsigned char lineId, unsigned long arg )
{
    ADSLDRV_PHY_ADDR KArg;

    KArg.bvStatus = BcmAdsl_GetPhyAddresses( &KArg.ChannelAddr );
    copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
} /* DoGetPhyAddresses */


/***************************************************************************
 * Function Name: DoSetPhyAddresses
 * Description  : Calls BcmAdsl_SetPhyAddresses on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetPhyAddresses( unsigned char lineId, unsigned long arg )
{
    ADSLDRV_PHY_ADDR KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bvStatus = BcmAdsl_SetPhyAddresses( &KArg.ChannelAddr );
        put_user( KArg.bvStatus, &((PADSLDRV_PHY_ADDR) arg)->bvStatus );
    }
} /* DoSetPhyAddresses */


/***************************************************************************
 * Function Name: DoMapATMPortIDs
 * Description  : Maps ATM Port IDs to DSL PHY Utopia Addresses.
 * Returns      : None.
 ***************************************************************************/
static void DoMapATMPortIDs( unsigned char lineId, unsigned long arg )
{
    ADSLDRV_MAP_ATM_PORT KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 ) {
                g_usAtmFastPortId = KArg.usAtmFastPortId;
                g_usAtmInterleavedPortId = KArg.usAtmInterleavedPortId;
            put_user( BCMADSL_STATUS_SUCCESS, &((PADSLDRV_MAP_ATM_PORT) arg)->bvStatus );
        }
} /* DoMapATMPortIDs */


/***************************************************************************
 * Function Name: DoGetConnectionInfo
 * Description  : Calls BcmAdsl_GetConnectionInfo on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
#if defined(PHY_BLOCK_TEST) && defined(PHY_BLOCK_TEST_SAR)
int phyLinkUp = 0;
#define	US_DIRECTION	1
#define	DS_DIRECTION	0
#endif
static void DoGetConnectionInfo( unsigned char lineId, unsigned long arg )
{
    ADSLDRV_CONNECTION_INFO KArg;

    KArg.bvStatus = BcmAdsl_GetConnectionInfo(lineId, &KArg.ConnectionInfo );
#if defined(PHY_BLOCK_TEST) && defined(PHY_BLOCK_TEST_SAR)
	if (phyLinkUp) {
		PADSL_CONNECTION_INFO pConnectionInfo = &KArg.ConnectionInfo;
		ulong	mibLen;
		adslMibInfo	*pMib;

		mibLen = sizeof(adslMibInfo);
		pMib = (void *) AdslCoreGetObjectValue(0, NULL, 0, NULL, &mibLen);

		pConnectionInfo->LinkState = BCM_ADSL_LINK_UP;
		pConnectionInfo->ulFastDnStreamRate = pMib->adsl2Info2lp[0].rcvRate * 1000;
		pConnectionInfo->ulInterleavedDnStreamRate = 0;
		pConnectionInfo->ulFastUpStreamRate = pMib->adsl2Info2lp[0].xmtRate * 1000;
		pConnectionInfo->ulInterleavedUpStreamRate = 0;
	}
#endif
    copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
} /* DoGetConnectionInfo */


/***************************************************************************
 * Function Name: DoDiagCommand
 * Description  : Calls BcmAdsl_DiagCommand on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoDiagCommand ( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_DIAG        KArg;
        char                cmdData[LOG_MAX_DATA_SIZE];

        KArg.bvStatus = BCMADSL_STATUS_ERROR;
        do {
                if (copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0)
                        break;

                if ((LOG_CMD_CONNECT != KArg.diagCmd) && (LOG_CMD_DISCONNECT != KArg.diagCmd) && (0 != KArg.diagMap)) {
                        if (KArg.logTime > sizeof(cmdData))
                                break;
                        if (copy_from_user(cmdData, (char*)KArg.diagMap, KArg.logTime) != 0)
                                break;
                        if(KArg.logTime < sizeof(cmdData))
                                memset((void*)(cmdData+KArg.logTime), 0, sizeof(cmdData)-KArg.logTime);
                        KArg.diagMap = (int) cmdData;
                }

        KArg.bvStatus = BcmAdsl_DiagCommand (lineId, (PADSL_DIAG) &KArg );
        } while (0);
        put_user( KArg.bvStatus, &((PADSLDRV_DIAG) arg)->bvStatus );
} /* DoGetConnectionInfo */

/***************************************************************************
 * Function Name: DoGetObjValue
 * Description  : Calls BcmAdsl_GetObjectValue on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetObjValue( unsigned char lineId, unsigned long arg )
{
	ADSLDRV_GET_OBJ	KArg;
	char				objId[kOidMaxObjLen];
	int				retObj;
	long				userObjLen;
	int					semUsed=0;
	do {
		if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
			KArg.bvStatus = BCMADSL_STATUS_ERROR;
			break;
		}
		
		KArg.bvStatus = BCMADSL_STATUS_ERROR;
		userObjLen = KArg.dataBufLen;

		if ((NULL != KArg.objId) && (KArg.objIdLen)) {
			if (KArg.objIdLen > kOidMaxObjLen)
				break;
			if( copy_from_user( objId, KArg.objId, KArg.objIdLen ) != 0 )
				break;
			if (down_interruptible(&semMibShared) == 0){
				retObj = BcmAdsl_GetObjectValue (lineId, objId, KArg.objIdLen, NULL, &KArg.dataBufLen);
				semUsed=1;
			}
			else 
				break;
		}
		else
			retObj = BcmAdsl_GetObjectValue (lineId, NULL, 0, NULL, &KArg.dataBufLen);

		if ((retObj >= kAdslMibStatusLastError) && (retObj < 0))
			break;		

		if( KArg.dataBufLen > userObjLen )
			KArg.dataBufLen = userObjLen;

		copy_to_user( KArg.dataBuf, (void *) retObj, KArg.dataBufLen );
		KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
	} while (0);

	if (BCMADSL_STATUS_ERROR == KArg.bvStatus)
		KArg.dataBufLen = 0;
	copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
	if (semUsed)
		up(&semMibShared);
}

/***************************************************************************
 * Function Name: DoDslWanLineStatus
 * Description  : 
 * Returns      : None.
 ***************************************************************************/
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
static void DoDslWanLineStatus( unsigned char lineId, unsigned long arg )
{
    int nDslLineStatus = (int) arg;
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)

    if(1 == nDslLineStatus)    //Line Up
    {
        if(glbDslLineBondingInfo.nLine1Status)
	{    
            if(!glbDslLineBondingInfo.nLine2Status)
                kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
            else
                kerSysLedCtrl(kLedAdsl, kLedStateOn);
            StartDslBondingLineTimer(lineId); 
	}
	else if(glbDslLineBondingInfo.nLine2Status)
	{    
            if(!glbDslLineBondingInfo.nLine1Status)
                kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
            else
                kerSysLedCtrl(kLedSecAdsl, kLedStateOn);
	    StartDslBondingLineTimer(lineId + 1); 
	}
    }
    else if(0 == nDslLineStatus)  //Line Down
    {    
        if(glbDslLineBondingInfo.nBondingStatus)
        {
           if(!glbDslLineBondingInfo.nLine2Status && !glbDslLineBondingInfo.nLine1Status)
           {		   
              kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
              kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
	   }
	   else if(!glbDslLineBondingInfo.nLine1Status)
	   {
               kerSysLedCtrl(kLedAdsl, kLedStateOn);
               kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
               StartDslBondingLineTimer(lineId + 1); 
	   }
	   else if(!glbDslLineBondingInfo.nLine2Status)
	   {
               kerSysLedCtrl(kLedSecAdsl, kLedStateOn);
               kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
               StartDslBondingLineTimer(lineId); 
	   }
	}
	else
            kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
    }
    else // Line Disable
    {    
        if(glbDslLineBondingInfo.nBondingStatus)
        {   
            del_timer(&glbDslLineBondingInfo.line1Timer);
            del_timer(&glbDslLineBondingInfo.line2Timer);
            kerSysLedCtrl(kLedAdsl, kLedStateFail);
        }
        //kerSysLedCtrl(kLedSecAdsl, kLedStateFail);
    }
#else
    if(0 == nDslLineStatus)
    {
#if defined(AEI_VDSL_CUSTOMER_QWEST)
		   kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
#else
		   kerSysLedCtrl(kLedSecAdsl, kLedStateFail);
#endif
    }
#endif
}
#endif

/***************************************************************************
 * Function Name: DoSetObjValue
 * Description  : Calls BcmAdsl_SetObjectValue on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetObjValue( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_GET_OBJ KArg;
        char                    objId[kOidMaxObjLen];
        char                    *dataBuf = NULL;
        int                     retObj = kAdslMibStatusFailure;

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;
                if ((NULL != KArg.objId) && (KArg.objIdLen)) {
                        if (KArg.objIdLen > kOidMaxObjLen)
                                break;
                        if( copy_from_user( objId, KArg.objId, KArg.objIdLen ) != 0 )
                                break;
                        dataBuf = kmalloc(KArg.dataBufLen, GFP_ATOMIC);
                        if(NULL == dataBuf)
                            break;
                        else if (copy_from_user( dataBuf, KArg.dataBuf, KArg.dataBufLen ) != 0 )
                            break;
                        retObj = BcmAdsl_SetObjectValue (lineId, objId, KArg.objIdLen, dataBuf, &KArg.dataBufLen);
                }

                if ((retObj >= kAdslMibStatusLastError) && (retObj < 0))
                        break;
                KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
        } while (0);

        if (BCMADSL_STATUS_ERROR == KArg.bvStatus)
                KArg.dataBufLen = 0;
        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
        
        if(dataBuf)
            free(dataBuf);
}

/***************************************************************************
 * Function Name: DoStartBERT
 * Description  : Calls BcmAdsl_StartBERT on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoStartBERT( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_BERT    KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 ) {
        KArg.bvStatus = BcmAdsl_StartBERT (lineId, KArg.totalBits);
        put_user( KArg.bvStatus, &((PADSLDRV_BERT) arg)->bvStatus );
    }
}

/***************************************************************************
 * Function Name: DoStopBERT
 * Description  : Calls BcmAdsl_StopBERT on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoStopBERT( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_StopBERT(lineId);
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
}

/***************************************************************************
 * Function Name: DoConfigure
 * Description  : Calls BcmAdsl_Configureon behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoConfigure( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_CONFIGURE       KArg;
        adslCfgProfile          adslCfg;

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;

                if (NULL != KArg.pAdslCfg) {
                        if( copy_from_user( &adslCfg, KArg.pAdslCfg, sizeof(adslCfg)) != 0 )
                                break;
                        KArg.bvStatus = BcmAdsl_Configure(lineId, &adslCfg);
                }
                else
                        KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
        } while (0);

        put_user( KArg.bvStatus, &((PADSLDRV_CONFIGURE) arg)->bvStatus );
}

/***************************************************************************
 * Function Name: DoTest
 * Description  : Controls ADSl driver/PHY special test modes
 * Returns      : None.
 ***************************************************************************/
static void DoTest( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_TEST            KArg;
        char                            rcvToneMap[512/8];
        char                            xmtToneMap[512/8];
        int                                     nToneBytes;

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;

                if ((NULL != KArg.xmtToneMap) && (0 != KArg.xmtNumTones)) {
                        nToneBytes = (KArg.xmtNumTones + 7) >> 3;
                        if (nToneBytes > sizeof(xmtToneMap)) {
                                nToneBytes = sizeof(xmtToneMap);
                                KArg.xmtNumTones = nToneBytes << 3;
                        }
                        if( copy_from_user( xmtToneMap, KArg.xmtToneMap, nToneBytes) != 0 ) {
                                KArg.xmtNumTones = 0;
                                break;
                        }
                        KArg.xmtToneMap = xmtToneMap;
                }

                if ((NULL != KArg.rcvToneMap) && (0 != KArg.rcvNumTones)) {
                        nToneBytes = (KArg.rcvNumTones + 7) >> 3;
                        if (nToneBytes > sizeof(rcvToneMap)) {
                                nToneBytes = sizeof(rcvToneMap);
                                KArg.rcvNumTones = nToneBytes << 3;
                        }
                        if( copy_from_user( rcvToneMap, KArg.rcvToneMap, nToneBytes) != 0 ) {
                                KArg.rcvNumTones = 0;
                                break;
                        }
                        KArg.rcvToneMap = rcvToneMap;
                }
                
                if (ADSL_TEST_SELECT_TONES != KArg.testCmd)
                        KArg.bvStatus = BcmAdsl_SetTestMode(lineId, KArg.testCmd);
                else
                        KArg.bvStatus = BcmAdsl_SelectTones(
                                lineId,
                                KArg.xmtStartTone, KArg.xmtNumTones,
                                KArg.rcvStartTone, KArg.rcvNumTones,
                                xmtToneMap, rcvToneMap);
        } while (0);

        put_user( KArg.bvStatus, &((PADSLDRV_TEST) arg)->bvStatus );
}

/***************************************************************************
 * Function Name: DoGetConstelPoints
 * Description  : Calls BcmAdsl_GetObjectValue on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetConstelPoints( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_GET_CONSTEL_POINTS      KArg;
        ADSL_CONSTELLATION_POINT        pointBuf[64];
        int                                                     numPoints;

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;

                numPoints = (KArg.numPoints > (sizeof(pointBuf)/sizeof(pointBuf[0])) ? sizeof(pointBuf)/sizeof(pointBuf[0]) : KArg.numPoints);
                numPoints = BcmAdsl_GetConstellationPoints (KArg.toneId, pointBuf, KArg.numPoints);
                if (numPoints > 0)
                        copy_to_user( KArg.pointBuf, (void *) pointBuf, numPoints * sizeof(ADSL_CONSTELLATION_POINT) );
                KArg.numPoints = numPoints;
                KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
        } while (0);

        if (BCMADSL_STATUS_ERROR == KArg.bvStatus)
                KArg.numPoints = 0;
        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
}

static void DoGetVersion( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_GET_VERSION             KArg;
        adslVersionInfo                 adslVer;

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }

                BcmAdsl_GetVersion(&adslVer);
                copy_to_user( KArg.pAdslVer, (void *) &adslVer, sizeof(adslVersionInfo) );
                KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
        } while (0);

        put_user( KArg.bvStatus , &((PADSLDRV_GET_VERSION) arg)->bvStatus );
}

static void DoSetSdramBase( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_SET_SDRAM_BASE          KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 ) {
        KArg.bvStatus = BcmAdsl_SetSDRAMBaseAddr((void *)KArg.sdramBaseAddr);
        put_user( KArg.bvStatus, &((PADSLDRV_SET_SDRAM_BASE) arg)->bvStatus );
    }
}

static void DoResetStatCounters( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_ResetStatCounters(lineId);
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
}

static void DoSetOemParameter( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_SET_OEM_PARAM   KArg;
        char                                    dataBuf[256];

        do {
                if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) != 0 ) {
                        KArg.bvStatus = BCMADSL_STATUS_ERROR;
                        break;
                }
                KArg.bvStatus = BCMADSL_STATUS_ERROR;

                if ((NULL != KArg.buf) && (KArg.len > 0)) {
                        if (KArg.len > sizeof(dataBuf))
                                KArg.len = sizeof(dataBuf);
                        if( copy_from_user( dataBuf, (void *)KArg.buf, KArg.len) != 0 )
                                break;
                        KArg.bvStatus = BcmAdsl_SetOemParameter (KArg.paramId, dataBuf, KArg.len);
                }
                else
                        KArg.bvStatus = BCMADSL_STATUS_SUCCESS;
        } while (0);

        put_user( KArg.bvStatus, &((PADSLDRV_SET_OEM_PARAM) arg)->bvStatus );
}

/***************************************************************************
 * Function Name: DoBertStartEx
 * Description  : Calls BcmAdsl_BertStartEx on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoBertStartEx( unsigned char lineId, unsigned long arg )
{
        ADSLDRV_BERT_EX         KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 ) {
        KArg.bvStatus = BcmAdsl_BertStartEx (lineId, KArg.totalSec);
        put_user( KArg.bvStatus, &((PADSLDRV_BERT_EX) arg)->bvStatus );
    }
}

/***************************************************************************
 * Function Name: DoBertStopEx
 * Description  : Calls BcmAdsl_BertStopEx on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoBertStopEx( unsigned char lineId, unsigned long arg )
{
    BCMADSL_STATUS bvStatus = BcmAdsl_BertStopEx(lineId);
    put_user( bvStatus, &((PADSLDRV_STATUS_ONLY) arg)->bvStatus );
}
#ifdef HMI_QA_SUPPORT

/***************************************************************************
 * Function Name: DoHmiCommand
 * Description  : Calls BcmAdslHmiMsgProcess on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoHmiCommand ( unsigned char lineId, unsigned long arg )
{
    BcmAdslHmiMsgProcess ( (void *)arg );
}
#endif

#if defined(PHY_BLOCK_TEST) && defined(PHY_BLOCK_TEST_SAR)

#define US_RATE		567
#define DS_RATE		23456

void AdslDrvXtmLinkDown(void)
{
	ulong	mibLen;
	adslMibInfo	*pMib;

	printk("AdslDrvXtmLinkDown:\n");
	phyLinkUp = 0;

	mibLen = sizeof(adslMibInfo);
	pMib = (void *) AdslCoreGetObjectValue (0, NULL, 0, NULL, &mibLen);

	pMib->adslTrainingState = kAdslTrainingIdle;

#if 0
	{
	XTM_INTERFACE_LINK_INFO	linkInfo;
	UINT32					ahifChanId = 0;

	linkInfo.ulLinkState = LINK_DOWN;
	linkInfo.ulLinkUsRate =	0;
	linkInfo.ulLinkDsRate =	0;
	linkInfo.ulLinkTrafficType = TRAFFIC_TYPE_ATM;
	BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID(ahifChanId), &linkInfo);
	}
#endif

#ifdef DYING_GASP_API
	/* Send Msg to the user mode application that monitors link status. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
	kerSysSendtoMonitorTask(MSG_NETLINK_BRCM_LINK_STATUS_CHANGED,NULL,0);
#else
	kerSysWakeupMonitorTask();
#endif

#endif
}

void AdslDrvXtmLinkUp(void)
{
	ulong	mibLen;
	adslMibInfo	*pMib;
	XTM_INTERFACE_LINK_INFO	linkInfo;
	UINT32					ahifChanId = 0;

	phyLinkUp = 1;
	mibLen = sizeof(adslMibInfo);
	pMib = (void *) AdslCoreGetObjectValue (0, NULL, 0, NULL, &mibLen);
    pMib->adsl2Info2lp[0].xmtRate = US_RATE;
    pMib->adsl2Info2lp[0].rcvRate = DS_RATE;

	pMib->adsl2Info2lp[0].rcvChanInfo.connectionType = kXdslDataAtm;
	pMib->adsl2Info2lp[0].xmtChanInfo.connectionType = kXdslDataAtm;

	pMib->adslTrainingState = kAdslTrainingConnected;
	pMib->adslConnection.modType = kAdslModAdsl2p;

#ifdef DYING_GASP_API
	/* Send Msg to the user mode application that monitors link status. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
	kerSysSendtoMonitorTask(MSG_NETLINK_BRCM_LINK_STATUS_CHANGED,NULL,0);
#else
	kerSysWakeupMonitorTask();
#endif

#endif

	linkInfo.ulLinkState = LINK_UP;
	linkInfo.ulLinkUsRate =	1000 * US_RATE;
	linkInfo.ulLinkDsRate =	1000 * DS_RATE;
	linkInfo.ulLinkTrafficType = TRAFFIC_TYPE_ATM;
	BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID(ahifChanId), &linkInfo);
	XdslCoreSetAhifState(lineId, 1, 1);
}
#endif /* PHY_BLOCK_TEST_SAR */

/***************************************************************************
 * Function Name: AdslConnectCb
 * Description  : Callback function that is called when by the ADSL driver
 *                when there is a change in status.
 * Returns      : None.
 ***************************************************************************/
static void AdslConnectCb(unsigned char lineId, ADSL_LINK_STATE dslLinkState, ADSL_LINK_STATE dslPrevLinkState, UINT32 ulParm )
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
	XTM_INTERFACE_LINK_INFO	linkInfo;
#else
	ATM_INTERFACE_LINK_INFO	linkInfo;
#endif
	adslMibInfo		*adslMib = NULL;
	long				size = sizeof(adslMibInfo);
	UINT32			ahifChanId;
	int				ledType ;
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
	int				connectionType, i;
#endif

#ifdef DYING_GASP_API
	/* Send Msg to the user mode application that monitors link status. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
	kerSysSendtoMonitorTask(MSG_NETLINK_BRCM_LINK_STATUS_CHANGED,NULL,0);
#else
	kerSysWakeupMonitorTask();
#endif

#endif

#ifdef SUPPORT_DSL_BONDING
#ifdef AEI_VDSL_CUSTOMER_NCS
	ledType = (lineId == 1) ? kLedAdsl : kLedSecAdsl;
#else
	ledType = (lineId == 0) ? kLedAdsl : kLedSecAdsl;
#endif
#else
	ledType = kLedAdsl;
#endif

	linkInfo.ulLinkState = LINK_DOWN;

	switch (dslLinkState) {
		case ADSL_LINK_UP:
			linkInfo.ulLinkState = LINK_UP;
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
			if(glbDslLineBondingInfo.nBondingStatus)   StartDslBondingLineTimer(lineId);
#endif
			kerSysLedCtrl(ledType, kLedStateOn);

#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
			snmp_adsl_eoc_event(lineId);
#endif
			break;
		case ADSL_LINK_DOWN:
		case BCM_ADSL_ATM_IDLE:
			if (dslLinkState == ADSL_LINK_DOWN)
		        {		
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
				if(glbDslLineBondingInfo.nBondingStatus)
				{
					kerSysLedCtrl(ledType,kLedStateSlowBlinkContinues);
					if(0 == lineId )
					{
					    down_interruptible(&glbDslLineBondingInfo.sem);
					    glbDslLineBondingInfo.nLine1Status = 0;
					    up(&glbDslLineBondingInfo.sem);
					    if(0 == glbDslLineBondingInfo.nLine2Status)
						kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);
					    else
						StartDslBondingLineTimer(lineId + 1);
					}
					else
					{
					    down_interruptible(&glbDslLineBondingInfo.sem);
					    glbDslLineBondingInfo.nLine2Status = 0;
					    up(&glbDslLineBondingInfo.sem);
					    if(0 == glbDslLineBondingInfo.nLine1Status)
						kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
					    else
						StartDslBondingLineTimer(lineId - 1);
					}
				}
				else
					kerSysLedCtrl(ledType,kLedStateSlowBlinkContinues);
#else
#if defined(AEI_VDSL_CUSTOMER_QWEST)
					kerSysLedCtrl(ledType, kLedStateSlowBlinkContinues);
#else
					kerSysLedCtrl(ledType, kLedStateFail);
#endif
#endif
			}
			break;
		case BCM_ADSL_TRAINING_G992_EXCHANGE:
		case BCM_ADSL_TRAINING_G992_CHANNEL_ANALYSIS:
		case BCM_ADSL_TRAINING_G992_STARTED:
		case BCM_ADSL_TRAINING_G993_EXCHANGE:
		case BCM_ADSL_TRAINING_G993_CHANNEL_ANALYSIS:
		case BCM_ADSL_TRAINING_G993_STARTED:
#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
			if(glbDslLineBondingInfo.nBondingStatus)
			{
				down_interruptible(&glbDslLineBondingInfo.sem);
				if(0 == lineId )
				{
				    glbDslLineBondingInfo.nLine1Status = 1;
                                    if(glbDslLineBondingInfo.nLine2Status  == 0)
                                        kerSysLedCtrl(kLedAdsl, kLedStateSlowBlinkContinues);

				}
				else
				{
				    glbDslLineBondingInfo.nLine2Status = 1;
                                    if(glbDslLineBondingInfo.nLine1Status  == 0)
                                        kerSysLedCtrl(kLedSecAdsl, kLedStateSlowBlinkContinues);
				}
				up(&glbDslLineBondingInfo.sem);
			}
#endif
			kerSysLedCtrl(ledType, kLedStateFastBlinkContinues);
			break;
#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
		case BCM_ADSL_G997_FRAME_RECEIVED:
			return;
		case BCM_ADSL_G997_FRAME_SENT:
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
			printk("BCM_ADSL_G997_FRAME_SENT \n");
#endif
			return;
#endif /* defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO) */
		default:
			return;
	}
	
	/* record time stamp of link state change (in 1/100ths of a second unit per MIB spec.) */
	if (g_dslNetDev != NULL) 
		g_dslNetDev->trans_start = jiffies * 100 / HZ;
	if((ADSL_LINK_DOWN == dslLinkState) && (ADSL_LINK_UP != dslPrevLinkState))
		return;
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
	if((ADSL_LINK_UP != dslLinkState) && (ADSL_LINK_DOWN != dslLinkState))
		return;
#else
	if((ADSL_LINK_UP != dslLinkState) && (ADSL_LINK_DOWN != dslLinkState) && (BCM_ADSL_ATM_IDLE != dslLinkState))
		return;
#endif
	
	adslMib = (void *) BcmAdsl_GetObjectValue (lineId, NULL, 0, NULL, &size);
	if( NULL == adslMib) {
		BcmAdslCoreDiagWriteStatusString(lineId, "AdslConnectCb: BcmAdsl_GetObjectValue() failed!!!\n" );
		return;
	}

#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	/* For now, only ATM port 0 is supported */
	BcmAtm_GetInterfaceId( 0, &ahifChanId );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
	linkInfo.ulLinkUsRate = 1000 * adslMib->adsl2Info.xmtRate;
	linkInfo.ulLinkDsRate = 1000 * adslMib->adsl2Info.rcvRate;
	linkInfo.ulLinkTrafficType = TRAFFIC_TYPE_ATM;
	BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID(ahifChanId), &linkInfo);
#else
	linkInfo.ulStructureId = ID_ATM_INTERFACE_LINK_INFO;
	linkInfo.ulLineRate = 1000 * adslMib->adsl2Info.xmtRate;
	BcmAtm_SetInterfaceLinkInfo( ahifChanId, &linkInfo );
#endif
	if (dslLinkState == BCM_ADSL_ATM_IDLE) {
		linkInfo.ulLinkState = LINK_UP;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
		BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID(ahifChanId), &linkInfo);
#else
		BcmAtm_SetInterfaceLinkInfo( ahifChanId, &linkInfo );
#endif
	}
	if(ADSL_LINK_UP == dslLinkState)
		BcmAdslCoreDiagWriteStatusString(lineId,"Line: %d Path: 0  Link Up UsRate %lu DsRate %lu Connection type %s ahifChanId %d\n",
					lineId, 1000 * adslMib->adsl2Info.xmtRate, 1000 * adslMib->adsl2Info.rcvRate, "ATM", ahifChanId);
	else
		BcmAdslCoreDiagWriteStatusString(lineId, "Line: %d Path: 0  Link Down\n", lineId);
#else /* 6368 arch. */

	for(i=0; i<MAX_LP_NUM; i++) {
		if( (0==i) || (adslMib->lp2Active || adslMib->lp2TxActive) ) {
			
			xdslFramingInfo *pXdslFramingParam = &adslMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[i];
			
			ahifChanId = pXdslFramingParam->ahifChanId[0];
			connectionType = pXdslFramingParam->tmType[0];
			linkInfo.ulLinkUsRate = 1000 * pXdslFramingParam->dataRate;
			linkInfo.ulLinkDsRate = 1000 * adslMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[i].dataRate;

			linkInfo.ulLinkTrafficType = ((kXdslDataPtm == connectionType ) ? (adslMib->xdslStat[i].bondingStat.status? TRAFFIC_TYPE_PTM_BONDED: TRAFFIC_TYPE_PTM) :
								(kXdslDataAtm == connectionType )? (adslMib->xdslStat[i].bondingStat.status? TRAFFIC_TYPE_ATM_BONDED: TRAFFIC_TYPE_ATM):
								( kXdslDataNitro == connectionType) ? TRAFFIC_TYPE_ATM:
								(kXdslDataRaw == connectionType) ? TRAFFIC_TYPE_PTM_RAW:TRAFFIC_TYPE_NOT_CONNECTED);
			
			if(ADSL_LINK_UP == dslLinkState)
				BcmAdslCoreDiagWriteStatusString(lineId,"Line: %d Path: %d  Link Up UsRate %lu DsRate %lu Connection type %s ahifChanId %d\n",
					lineId, i, linkInfo.ulLinkUsRate, linkInfo.ulLinkDsRate,
					(TRAFFIC_TYPE_PTM == linkInfo.ulLinkTrafficType) ? "PTM":
					(TRAFFIC_TYPE_PTM_BONDED == linkInfo.ulLinkTrafficType) ? "PTM_BONDED":
					(TRAFFIC_TYPE_ATM == linkInfo.ulLinkTrafficType) ? "ATM":
					(TRAFFIC_TYPE_ATM_BONDED == linkInfo.ulLinkTrafficType) ? "ATM_BONDED":
					(TRAFFIC_TYPE_PTM_RAW == linkInfo.ulLinkTrafficType) ? "RAW ETHERNET":"Not connected", ahifChanId);
			else
				BcmAdslCoreDiagWriteStatusString(lineId, "Line: %d Path: %d  Link Down\n", lineId, i);
			
			/* Temporarily skip notifying the XTM driver to prevent it from messing up the TEQ channel */
			if(BcmCoreDiagLogActive(DIAG_DATA_LOG)) {
				BcmAdslCoreDiagWriteStatusString(lineId, "Line: %d Path: %d  in TEQ data logging mode. Skip notifying XTM driver\n", lineId, i);
				return;
			}
			
#ifndef PHY_LOOPBACK
			BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID(ahifChanId), &linkInfo);
			XdslCoreSetAhifState(lineId, 1, LINK_DOWN == linkInfo.ulLinkState ? 0 : 1);
#endif
		}
		if(XdslMibIsGinpActive(XdslCoreGetDslVars(lineId), US_DIRECTION) ||
			XdslMibIsGinpActive(XdslCoreGetDslVars(lineId), DS_DIRECTION))
			break;
	}	
#endif
} /* AdslConnectCb */


#if defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO)
/* routine copied from tiny bridge written by lt */
void snmp_adsl_eoc_event(unsigned char lineId)
{
	adslMibInfo *adslMib;
	long size = sizeof(adslMibInfo);

	adslMib = (void *) BcmAdsl_GetObjectValue (lineId, NULL, 0, NULL, &size);

	if( NULL != adslMib ) {
		if( (kAdslModAdsl2p == adslMib->adslConnection.modType) ||
			(kAdslModAdsl2  == adslMib->adslConnection.modType) ||
			(kVdslModVdsl2  == adslMib->adslConnection.modType) ) {
			if(kVdslModVdsl2  == adslMib->adslConnection.modType)
				BcmAdslCoreDiagWriteStatusString(lineId, "Line %d: VDSL2 connection\n", lineId);
			else
				BcmAdslCoreDiagWriteStatusString(lineId, "Line %d: ADSL2/ADSL2+ connection\n", lineId);
			g_eoc_hdr_offset = ADSL_2P_HDR_OFFSET;
			g_eoc_hdr_len    = ADSL_2P_EOC_HDR_LEN; 
		}
		else {
			BcmAdslCoreDiagWriteStatusString("lineId, Line %d: ADSL1 connection\n", lineId);
			g_eoc_hdr_offset = ADSL_HDR_OFFSET;
			g_eoc_hdr_len    = ADSL_EOC_HDR_LEN;
		}
	}
}

/***************************************************************************
 * Function Name: adsl_read
 * The EOC read code (slightly modified) is copied from cfeBridge written by lat
 ***************************************************************************/
static ssize_t adsl_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
	int ret = 0, len, request_length=0;
	char *p;
	unsigned char adsl_eoc_hdr[] = ADSL_EOC_HDR;
	unsigned char adsl_eoc_enable[] = ADSL_EOC_ENABLE;
	char inputBuffer[520];
	unsigned char lineId = 0;
	/* TO DO: Need to figure out how to extract lineId(Device Minor Number) for bonding target */
	
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
	printk("adsl_read entry\n");
#endif

	/* Receive SNMP request. */
	p = BcmAdsl_G997FrameGet(lineId, &len );
	if( p && len > 0 )
	{
		if( memcmp(p, adsl_eoc_enable, sizeof(adsl_eoc_enable)) == 0 ) {
			/* return now and let the upper layer know that link up is received */
			/* don't clear the FRAME_RCVD bit, just in case there are things to read */
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
			printk("*** Got message up on ADSL EOC connection, len %d ***\n",len);
			dumpaddr(p,len);
#endif
			BcmAdsl_G997FrameFinished(lineId);
			return len;
		}

		if (memcmp(p, adsl_eoc_hdr+g_eoc_hdr_offset, g_eoc_hdr_len) == 0) {
			  p += g_eoc_hdr_len;
			  len -= g_eoc_hdr_len;
		}
		else {
			  BcmAdslCoreDiagWriteStatusString(lineId, "something is wrong, len %d, no adsl_eoc_hdr found\n",len);
			  dumpaddr(p,len);
		}

		do
		{
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
			printk("***count %d, processing rx from EOC link, len %d/requestLen %d ***\n",
			count,len,request_length);
			dumpaddr(p,len);
#endif
			if( len + request_length < count ) {
				memcpy(inputBuffer + request_length, p, len);
				request_length += len;
			}
			p = BcmAdsl_G997FrameGetNext(lineId, &len );
		  } while( p && len > 0 );

		BcmAdsl_G997FrameFinished(lineId);
		if (!(copy_to_user(buf, inputBuffer, request_length)))
			ret = request_length;

#ifdef BUILD_SNMP_TRANSPORT_DEBUG
		printk("adsl_read(): end, request_length %d\n", request_length);
		printk("inputBuffer:\n");
		dumpaddr(inputBuffer,request_length);
#endif
	} /* (p  & len > 0) */
	
	return( ret );
} /* adsl_read */

/***************************************************************************
 * Function Name: adsl_write
 * The EOC write code is copied from cfeBridge written by lat
 ***************************************************************************/
static ssize_t adsl_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
	unsigned char adsl_eoc_hdr[] = ADSL_EOC_HDR;
	int ret = 0, len;
	char *pBuf;
	unsigned char lineId = 0;
	/* TO DO: Need to figure out how to extract lineId(Device Minor Number) for bonding target */
	
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
	printk("adsl_write(entry): count %d\n",count);
#endif

	/* Need to include adsl_eoc_hdr */
	len = count + g_eoc_hdr_len;

	if( (pBuf = calloc(len, 1)) == NULL ) {
		BcmAdslCoreDiagWriteStatusString(lineId, "ADSL send failed, out of memory");
		return ret;
	}
	
	memcpy(pBuf, adsl_eoc_hdr+g_eoc_hdr_offset, g_eoc_hdr_len);

	if (copy_from_user((pBuf+g_eoc_hdr_len), buf, count) == 0)	{
#ifdef BUILD_SNMP_TRANSPORT_DEBUG
		printk("adsl_write ready to send data over EOC, len %d\n", len);
		dumpaddr(pBuf, len);
#endif
		
		if( BCMADSL_STATUS_SUCCESS == BcmAdsl_G997SendData(lineId, pBuf, len) ) {
			ret = len;
		}
		else {
			free(pBuf);
			BcmAdslCoreDiagWriteStatusString(lineId, "ADSL send failed, len %d",   len);
		}
	}
	else {
		free(pBuf);
		BcmAdslCoreDiagWriteStatusString(lineId, "ADSL send copy_from_user failed, len %d",   len);
	}		
  
	return ret;
	
} 

static unsigned int adsl_poll(struct file *file, poll_table *wait)
{
	unsigned char lineId = 0;
	/* TO DO: Need to figure out how to extract lineId(Device Minor Number) for bonding target */

	return AdslCoreG997FrameReceived(lineId);
}
#endif /* defined(BUILD_SNMP_EOC) || defined(BUILD_SNMP_AUTO) */

#if defined(AEI_VDSL_CUSTOMER_NCS)
static void DoEmpty( unsigned char lineId, unsigned long arg )
{
    return;
}
#endif

#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_TOGGLE_DATAPUMP)
static void DoDslDatapumpAutoDetect( unsigned char lineId, unsigned long arg )
{
    if(1 == arg)
    {
        datapumpAutodetect = 1;
    }
    else
    {    
        datapumpAutodetect = 0;
    }
}
#endif

/***************************************************************************
 * MACRO to call driver initialization and cleanup functions.
 ***************************************************************************/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) || !defined(MODULE)
module_init( adsl_init );
module_exit( adsl_cleanup );
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
MODULE_LICENSE("Proprietary");
#endif

