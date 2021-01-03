/*
<:copyright-broadcom 
 
 Copyright (c) 2007 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          5300 California Avenue
          Irvine, California 92617 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***************************************************************************
 * File Name  : xtmcfgdrv.c
 *
 * Description: This file contains Linux character device driver entry points
 *              for the bcmxtmcfg driver.
 ***************************************************************************/


/* Includes. */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <bcmtypes.h>
#include <xtmcfgdrv.h>
#include <xtmprocregs.h>
#include <board.h>
#include "boardparms.h"
#include "bcm_map.h"

/* Typedefs. */
typedef void (*FN_IOCTL) (unsigned long arg);


/* Prototypes. */
static int __init bcmxtmcfg_init( void );
static void __exit bcmxtmcfg_cleanup( void );
static int bcmxtmcfg_open( struct inode *inode, struct file *filp );
static int bcmxtmcfg_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg );
static void DoInitialize( unsigned long arg );
static void DoUninitialize( unsigned long arg );
static void DoGetTrafficDescrTable( unsigned long arg );
static void DoSetTrafficDescrTable( unsigned long arg );
static void DoGetInterfaceCfg( unsigned long arg );
static void DoSetInterfaceCfg( unsigned long arg );
static void DoGetConnCfg( unsigned long arg );
static void DoSetConnCfg( unsigned long arg );
static void DoGetConnAddrs( unsigned long arg );
static void DoGetInterfaceStatistics( unsigned long arg );
static void DoSetInterfaceLinkInfo( unsigned long arg );
static void DoSendOamCell( unsigned long arg );
static void DoCreateNetworkDevice( unsigned long arg );
static void DoDeleteNetworkDevice( unsigned long arg );
static void DoReInitialize(unsigned long arg);

/* Globals. */
static struct file_operations bcmxtmcfg_fops =
{
    .ioctl  = bcmxtmcfg_ioctl,
    .open   = bcmxtmcfg_open,
};

/* Defined for backward compatibility.  Not used. */
void *g_pfnAdslSetAtmLoopbackMode = NULL;
void *g_pfnAdslSetVcEntry = NULL;
void *g_pfnAdslSetVcEntryEx = NULL;
void *g_pfnAdslGetObjValue = NULL;

/***************************************************************************
 * Function Name: bcmxtmcfg_init
 * Description  : Initial function that is called at system startup that
 *                registers this device.
 * Returns      : None.
 ***************************************************************************/
static int __init bcmxtmcfg_init( void )
{
    printk( "bcmxtmcfg: bcmxtmcfg_init entry\n" );
    register_chrdev( XTMCFGDRV_MAJOR, "bcmxtmcfg", &bcmxtmcfg_fops );
    return( 0 );
} /* bcmxtmcfg_init */


/***************************************************************************
 * Function Name: bcmxtmcfg_cleanup
 * Description  : Final function that is called when the module is unloaded.
 * Returns      : None.
 ***************************************************************************/
static void __exit bcmxtmcfg_cleanup( void )
{
    printk( "bcmxtmcfg: bcmxtmcfg_cleanup entry\n" );
    unregister_chrdev( XTMCFGDRV_MAJOR, "bcmxtmcfg" );
    BcmXtm_Uninitialize();
} /* bcmxtmcfg_cleanup */


/***************************************************************************
 * Function Name: bcmxtmcfg_open
 * Description  : Called when an application opens this device.
 * Returns      : 0 - success
 ***************************************************************************/
static int bcmxtmcfg_open( struct inode *inode, struct file *filp )
{
    return( 0 );
} /* bcmxtmcfg_open */


/***************************************************************************
 * Function Name: bcmxtmcfg_ioctl
 * Description  : Main entry point for an application send issue ATM API
 *                requests.
 * Returns      : 0 - success or error
 ***************************************************************************/
static int bcmxtmcfg_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg )
{
    int ret = 0;
    unsigned int cmdnr = _IOC_NR(command);

    FN_IOCTL IoctlFuncs[] = {DoInitialize, DoUninitialize,
        DoGetTrafficDescrTable, DoSetTrafficDescrTable, DoGetInterfaceCfg,
        DoSetInterfaceCfg, DoGetConnCfg, DoSetConnCfg, DoGetConnAddrs,
        DoGetInterfaceStatistics, DoSetInterfaceLinkInfo, DoSendOamCell,
        DoCreateNetworkDevice, DoDeleteNetworkDevice, DoReInitialize, NULL};

    if( cmdnr >= 0 && cmdnr < MAX_XTMCFGDRV_IOCTL_COMMANDS &&
        IoctlFuncs[cmdnr] != NULL )
    {
        (*IoctlFuncs[cmdnr]) (arg);
    }
    else
        ret = -EINVAL;

    return( ret );
} /* bcmxtmcfg_ioctl */


/***************************************************************************
 * Function Name: DoInitialize
 * Description  : Calls BcmXtm_Initialize on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoInitialize( unsigned long arg )
{
    XTMCFGDRV_INITIALIZE KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_Initialize( &KArg.Init );
        put_user( KArg.bxStatus, &((PXTMCFGDRV_INITIALIZE) arg)->bxStatus );
    }
} /* DoInitialize */


/***************************************************************************
 * Function Name: DoUninitialize
 * Description  : Calls BcmXtm_Uninitialize on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoUninitialize( unsigned long arg )
{
    BCMXTM_STATUS bxStatus = BcmXtm_Uninitialize();
    put_user( bxStatus, &((PXTMCFGDRV_STATUS_ONLY) arg)->bxStatus );
} /* DoUninitialize */


/***************************************************************************
 * Function Name: DoGetTrafficDescrTable
 * Description  : Calls BcmXtm_GetTrafficDescrTable on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetTrafficDescrTable( unsigned long arg )
{
    XTMCFGDRV_TRAFFIC_DESCR_TABLE KArg;
    PXTM_TRAFFIC_DESCR_PARM_ENTRY pKTbl = NULL;
    UINT32 ulSize;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        ulSize = KArg.ulTrafficDescrTableSize *
            sizeof(XTM_TRAFFIC_DESCR_PARM_ENTRY);
        if( ulSize )
            pKTbl = (PXTM_TRAFFIC_DESCR_PARM_ENTRY) kmalloc(ulSize, GFP_KERNEL);

        if( pKTbl || ulSize == 0 )
        {
            KArg.bxStatus = BcmXtm_GetTrafficDescrTable( pKTbl,
                &KArg.ulTrafficDescrTableSize );

            if( KArg.bxStatus == XTMSTS_SUCCESS )
                copy_to_user((void *) KArg.pTrafficDescrTable,pKTbl,ulSize);

            put_user( KArg.ulTrafficDescrTableSize,
              &((PXTMCFGDRV_TRAFFIC_DESCR_TABLE)arg)->ulTrafficDescrTableSize);

            put_user( KArg.bxStatus,
                &((PXTMCFGDRV_TRAFFIC_DESCR_TABLE) arg)->bxStatus );

            if( pKTbl )
                kfree( pKTbl );
        }
    }
} /* DoGetTrafficDescrTable */


/***************************************************************************
 * Function Name: DoSetTrafficDescrTable
 * Description  : Calls BcmXtm_SetTrafficDescrTable on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetTrafficDescrTable( unsigned long arg )
{
    XTMCFGDRV_TRAFFIC_DESCR_TABLE KArg;
    PXTM_TRAFFIC_DESCR_PARM_ENTRY pKTbl;
    UINT32 ulSize;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        ulSize = KArg.ulTrafficDescrTableSize *
            sizeof(XTM_TRAFFIC_DESCR_PARM_ENTRY);
        pKTbl = (PXTM_TRAFFIC_DESCR_PARM_ENTRY) kmalloc( ulSize, GFP_KERNEL );

        if( pKTbl )
        {
            if( copy_from_user( pKTbl, KArg.pTrafficDescrTable, ulSize ) == 0 )
            {
                KArg.bxStatus = BcmXtm_SetTrafficDescrTable( pKTbl,
                    KArg.ulTrafficDescrTableSize );

                put_user( KArg.bxStatus,
                    &((PXTMCFGDRV_TRAFFIC_DESCR_TABLE) arg)->bxStatus );

            }

            kfree( pKTbl );
        }
    }
} /* DoSetTrafficDescrTable */


/***************************************************************************
 * Function Name: DoGetInterfaceCfg
 * Description  : Calls BcmXtm_GetInterfaceCfg on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetInterfaceCfg( unsigned long arg )
{
    XTMCFGDRV_INTERFACE_CFG KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_GetInterfaceCfg( KArg.ulPortId,
            &KArg.InterfaceCfg );

        if( KArg.bxStatus == XTMSTS_SUCCESS )
        {
            copy_to_user( &((PXTMCFGDRV_INTERFACE_CFG) arg)->InterfaceCfg,
                &KArg.InterfaceCfg, sizeof(KArg.InterfaceCfg) );
        }

        put_user( KArg.bxStatus, &((PXTMCFGDRV_INTERFACE_CFG) arg)->bxStatus );
    }
} /* DoGetInterfaceCfg */


/***************************************************************************
 * Function Name: DoSetInterfaceCfg
 * Description  : Calls BcmXtm_SetInterfaceCfg on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetInterfaceCfg( unsigned long arg )
{
    XTMCFGDRV_INTERFACE_CFG KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_SetInterfaceCfg( KArg.ulPortId,
            &KArg.InterfaceCfg );
        put_user( KArg.bxStatus, &((PXTMCFGDRV_INTERFACE_CFG) arg)->bxStatus );
    }
} /* DoSetInterfaceCfg */


/***************************************************************************
 * Function Name: DoGetConnCfg
 * Description  : Calls BcmXtm_GetConnCfg on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetConnCfg( unsigned long arg )
{
    XTMCFGDRV_CONN_CFG KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_GetConnCfg( &KArg.ConnAddr, &KArg.ConnCfg );

        if( KArg.bxStatus == XTMSTS_SUCCESS )
        {
            copy_to_user( &((PXTMCFGDRV_CONN_CFG) arg)->ConnCfg,
                &KArg.ConnCfg, sizeof(KArg.ConnCfg) );
        }

        put_user( KArg.bxStatus, &((PXTMCFGDRV_CONN_CFG) arg)->bxStatus );
    }
} /* DoGetConnCfg */


/***************************************************************************
 * Function Name: DoSetConnCfg
 * Description  : Calls BcmXtm_SetConnCfg on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetConnCfg( unsigned long arg )
{
    XTMCFGDRV_CONN_CFG KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        /* If an invalid AAL type field is set, delete the configuration.  The
         * caller passed a NULL pointer for the configuration record which means
         * to delete the configuration for the specified connection address.
         */
        if( KArg.ConnCfg.ulAtmAalType == XTMCFG_INVALID_FIELD )
        {
            KArg.bxStatus = BcmXtm_SetConnCfg( &KArg.ConnAddr, NULL );
            put_user( KArg.bxStatus, &((PXTMCFGDRV_CONN_CFG) arg)->bxStatus );
        }
        else
        {
            KArg.bxStatus = BcmXtm_SetConnCfg( &KArg.ConnAddr, &KArg.ConnCfg );
            put_user( KArg.bxStatus, &((PXTMCFGDRV_CONN_CFG) arg)->bxStatus );
        }
    }
} /* DoSetConnCfg */


/***************************************************************************
 * Function Name: DoGetConnAddrs
 * Description  : Calls BcmXtm_GetConnAddrs on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetConnAddrs( unsigned long arg )
{
    XTMCFGDRV_CONN_ADDRS KArg;
    PXTM_ADDR pKAddrs = NULL;
    UINT32 ulSize;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        ulSize = KArg.ulNumConns * sizeof(XTM_ADDR);
        if( ulSize )
            pKAddrs = (PXTM_ADDR) kmalloc( ulSize, GFP_KERNEL );

        if( pKAddrs || ulSize == 0 )
        {
            KArg.bxStatus = BcmXtm_GetConnAddrs( pKAddrs, &KArg.ulNumConns );

            if( ulSize )
                copy_to_user( KArg.pConnAddrs, pKAddrs, ulSize );
            put_user( KArg.ulNumConns,
                &((PXTMCFGDRV_CONN_ADDRS) arg)->ulNumConns );

            put_user(KArg.bxStatus, &((PXTMCFGDRV_CONN_ADDRS) arg)->bxStatus);

            if( pKAddrs )
                kfree( pKAddrs );
        }
    }
} /* DoGetConnAddrs */


/***************************************************************************
 * Function Name: DoGetInterfaceStatistics
 * Description  : Calls BcmXtm_GetInterfaceStatistics on behalf of a user
 *                program.
 * Returns      : None.
 ***************************************************************************/
static void DoGetInterfaceStatistics( unsigned long arg )
{
    XTMCFGDRV_INTERFACE_STATISTICS KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_GetInterfaceStatistics( KArg.ulPortId,
            &KArg.Statistics, KArg.ulReset );

        if( KArg.bxStatus == XTMSTS_SUCCESS )
        {
            copy_to_user( &((PXTMCFGDRV_INTERFACE_STATISTICS) arg)->Statistics,
                &KArg.Statistics, sizeof(KArg.Statistics) );
        }

        put_user( KArg.bxStatus,
            &((PXTMCFGDRV_INTERFACE_STATISTICS)arg)->bxStatus );
    }
} /* DoGetInterfaceStatistics */


/***************************************************************************
 * Function Name: DoSetInterfaceLinkInfo
 * Description  : Calls BcmXtm_SetInterfaceLinkInfo on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSetInterfaceLinkInfo( unsigned long arg )
{
    XTMCFGDRV_INTERFACE_LINK_INFO KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_SetInterfaceLinkInfo( KArg.ulPortId,
            &KArg.LinkInfo );
        put_user( KArg.bxStatus,
            &((PXTMCFGDRV_INTERFACE_LINK_INFO) arg)->bxStatus );
    }
} /* DoSetInterfaceLinkInfo */


/***************************************************************************
 * Function Name: DoSendOamCell
 * Description  : Calls BcmXtm_SendOamCell on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoSendOamCell( unsigned long arg )
{
    XTMCFGDRV_SEND_OAM_CELL KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_SendOamCell(&KArg.ConnAddr, &KArg.OamCellInfo);
        copy_to_user( &((PXTMCFGDRV_SEND_OAM_CELL) arg)->OamCellInfo,
            &KArg.OamCellInfo, sizeof(KArg.OamCellInfo) );
        put_user( KArg.bxStatus, &((PXTMCFGDRV_SEND_OAM_CELL) arg)->bxStatus );
    }
} /* DoSendOamCell */


/***************************************************************************
 * Function Name: DoCreateNetworkDevice
 * Description  : Calls BcmXtm_CreateNetworkDevice on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoCreateNetworkDevice( unsigned long arg )
{
    XTMCFGDRV_CREATE_NETWORK_DEVICE KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_CreateNetworkDevice( &KArg.ConnAddr,
            KArg.szNetworkDeviceName );
        put_user( KArg.bxStatus,
            &((PXTMCFGDRV_CREATE_NETWORK_DEVICE) arg)->bxStatus );
    }
} /* DoCreateNetworkDevice */


/***************************************************************************
 * Function Name: DoDeleteNetworkDevice
 * Description  : Calls BcmXtm_DeleteNetworkDevice on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoDeleteNetworkDevice( unsigned long arg )
{
    XTMCFGDRV_DELETE_NETWORK_DEVICE KArg;

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        KArg.bxStatus = BcmXtm_DeleteNetworkDevice( &KArg.ConnAddr );
        put_user( KArg.bxStatus,
            &((PXTMCFGDRV_DELETE_NETWORK_DEVICE) arg)->bxStatus );
    }
} /* DoDeleteNetworkDevice */


/***************************************************************************
 * Function Name: DoReInitialize
 * Description  : Calls BcmXtm_ReInitialize on behalf of a user program.
 * Returns      : None.
 ***************************************************************************/
static void DoReInitialize( unsigned long arg )
{
    BCMXTM_STATUS bxStatus ; // = BcmXtm_ReInitialize(void);
    put_user( bxStatus, &((PXTMCFGDRV_STATUS_ONLY) arg)->bxStatus );
} /* DoReInitialize */


/***************************************************************************
 * Function Name: WanDataLedCtrl
 * Description  : Callback function that is called when by the LED driver
 *                to handle the ADSL hardware LED.
 * Returns      : None.
 ***************************************************************************/
#if defined(CONFIG_BCM96368)
static void WanDataLedCtrl(BOARD_LED_STATE ledState)
{
    switch (ledState)
    {
        case kLedStateOn:
            GPIO->LEDCtrl |= LED_ON_2;
            break;

        case kLedStateOff:
        case kLedStateFail:
            GPIO->LEDCtrl &= ~LED_ON_2;
            break;

        default:
            break;
    }
}
#endif

/***************************************************************************
 * MACRO to call driver initialization and cleanup functions.
 ***************************************************************************/
module_init( bcmxtmcfg_init );
module_exit( bcmxtmcfg_cleanup );
MODULE_LICENSE("Proprietary");

EXPORT_SYMBOL(BcmXtm_Initialize);
EXPORT_SYMBOL(BcmXtm_Uninitialize);
EXPORT_SYMBOL(BcmXtm_GetTrafficDescrTable);
EXPORT_SYMBOL(BcmXtm_SetTrafficDescrTable);
EXPORT_SYMBOL(BcmXtm_GetInterfaceCfg);
EXPORT_SYMBOL(BcmXtm_SetInterfaceCfg);
EXPORT_SYMBOL(BcmXtm_GetConnCfg);
EXPORT_SYMBOL(BcmXtm_SetConnCfg);
EXPORT_SYMBOL(BcmXtm_GetConnAddrs);
EXPORT_SYMBOL(BcmXtm_GetInterfaceStatistics);
EXPORT_SYMBOL(BcmXtm_SetInterfaceLinkInfo);
EXPORT_SYMBOL(BcmXtm_SendOamCell);
EXPORT_SYMBOL(BcmXtm_CreateNetworkDevice);
EXPORT_SYMBOL(BcmXtm_DeleteNetworkDevice);
//EXPORT_SYMBOL(BcmXtm_ReInitialize);

/* Backward compatibility. */
BCMXTM_STATUS BcmAtm_SetInterfaceLinkInfo( UINT32 ulInterfaceId,
    void *pInterfaceLinkInfo );
BCMXTM_STATUS BcmAtm_GetInterfaceId( UINT8 ucPhyPort, UINT32 *pulInterfaceId );

EXPORT_SYMBOL(BcmAtm_SetInterfaceLinkInfo);
EXPORT_SYMBOL(BcmAtm_GetInterfaceId);
EXPORT_SYMBOL(g_pfnAdslSetAtmLoopbackMode);
EXPORT_SYMBOL(g_pfnAdslSetVcEntry);
EXPORT_SYMBOL(g_pfnAdslSetVcEntryEx);
EXPORT_SYMBOL(g_pfnAdslGetObjValue);

