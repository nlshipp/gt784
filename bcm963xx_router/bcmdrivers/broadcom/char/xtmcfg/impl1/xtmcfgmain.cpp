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
 * File Name  : xtmcfgmain.cpp
 *
 * Description: This file contains the implementation for the XTM kernel entry
 *              point functions.
 ***************************************************************************/

/* Includes. */
#include "xtmcfgimpl.h"
#include "bcm_intr.h"
#include "bcm_map.h"
#include "board.h"
#include "boardparms.h"


/* Globals. */
static XTM_PROCESSOR *g_pXtmProcessor = NULL;


/* Prototypes. */
extern "C" {
BCMXTM_STATUS BcmXtm_ChipInitialize( PXTM_INITIALIZATION_PARMS pInitParms,
    UINT32 *pulBusSpeed );
}


/***************************************************************************
 * Function Name: BcmXtm_Initialize
 * Description  : ATM/PTM processor initialization entry point function.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_Initialize( PXTM_INITIALIZATION_PARMS pInitParms )
{
    BCMXTM_STATUS bxStatus;

    if( g_pXtmProcessor == NULL )
    {
        XtmOsInitialize();

        if( (g_pXtmProcessor = new XTM_PROCESSOR) != NULL )
        {
            UINT32 ulBusSpeed = 0;

            bxStatus = BcmXtm_ChipInitialize( pInitParms, &ulBusSpeed );
            if( bxStatus == XTMSTS_SUCCESS )
            {
                if( (bxStatus = g_pXtmProcessor->Initialize( pInitParms,
                    ulBusSpeed, bcmxtmrt_request )) != XTMSTS_SUCCESS )
                {
                    BcmXtm_Uninitialize();
                }
            }
        }
        else
            bxStatus = XTMSTS_ALLOC_ERROR;
    }
    else
        bxStatus = XTMSTS_STATE_ERROR;

    return( bxStatus );
} /* BcmXtm_Initialize */


/***************************************************************************
 * Function Name: BcmXtm_Uninitialize
 * Description  : ATM/PTM processor uninitialization entry point function.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_Uninitialize( void )
{
    XTM_PROCESSOR *pXtmProc = g_pXtmProcessor;

    g_pXtmProcessor = NULL;

    if( pXtmProc )
        delete pXtmProc;

    return( XTMSTS_SUCCESS );
} /* BcmXtm_Uninitialize */


/***************************************************************************
 * Function Name: BcmXtm_GetTrafficDescrTable
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_GetTrafficDescrTable( PXTM_TRAFFIC_DESCR_PARM_ENTRY
    pTrafficDescrTable, UINT32 *pulTrafficDescrTableSize )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->GetTrafficDescrTable( pTrafficDescrTable,
            pulTrafficDescrTableSize )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_GetTrafficDescrTable */


/***************************************************************************
 * Function Name: BcmXtm_SetTrafficDescrTable
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_SetTrafficDescrTable( PXTM_TRAFFIC_DESCR_PARM_ENTRY
    pTrafficDescrTable, UINT32  ulTrafficDescrTableSize )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->SetTrafficDescrTable( pTrafficDescrTable,
            ulTrafficDescrTableSize )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_SetTrafficDescrTable */


/***************************************************************************
 * Function Name: BcmXtm_GetInterfaceCfg
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_GetInterfaceCfg( UINT32 ulPortId, PXTM_INTERFACE_CFG
    pInterfaceCfg )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->GetInterfaceCfg( ulPortId, pInterfaceCfg )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_GetInterfaceCfg */


/***************************************************************************
 * Function Name: BcmXtm_SetInterfaceCfg
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_SetInterfaceCfg( UINT32 ulPortId, PXTM_INTERFACE_CFG
    pInterfaceCfg )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->SetInterfaceCfg( ulPortId, pInterfaceCfg )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_SetInterfaceCfg */


/***************************************************************************
 * Function Name: BcmXtm_GetConnCfg
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_GetConnCfg( PXTM_ADDR pConnAddr, PXTM_CONN_CFG pConnCfg )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->GetConnCfg( pConnAddr, pConnCfg )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_GetConnCfg */


/***************************************************************************
 * Function Name: BcmXtm_SetConnCfg
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_SetConnCfg( PXTM_ADDR pConnAddr, PXTM_CONN_CFG pConnCfg )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->SetConnCfg( pConnAddr, pConnCfg )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_SetConnCfg */


/***************************************************************************
 * Function Name: BcmXtm_GetConnAddrs
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_GetConnAddrs( PXTM_ADDR pConnAddrs, UINT32 *pulNumConns )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->GetConnAddrs( pConnAddrs, pulNumConns )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_GetConnAddrs */


/***************************************************************************
 * Function Name: BcmXtm_GetInterfaceStatistics
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_GetInterfaceStatistics( UINT32 ulPortId,
    PXTM_INTERFACE_STATS pStatistics, UINT32 ulReset )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->GetInterfaceStatistics(ulPortId, pStatistics, ulReset)
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_GetInterfaceStatistics */


/***************************************************************************
 * Function Name: BcmXtm_SetInterfaceLinkInfo
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_SetInterfaceLinkInfo( UINT32 ulPortId,
    PXTM_INTERFACE_LINK_INFO pLinkInfo )
{
    BCMXTM_STATUS bxStatus = XTMSTS_STATE_ERROR;

    if( g_pXtmProcessor )
    {
        bxStatus = g_pXtmProcessor->SetInterfaceLinkInfo(ulPortId, pLinkInfo); 
        /* If all the available link(s) are down, the SAR will have to be reset
         * for BCM6368 to accommodate possible different traffic types.
         */
        g_pXtmProcessor->CheckAndResetSAR(ulPortId, pLinkInfo);
    }

    return( bxStatus );
} /* BcmXtm_SetInterfaceLinkInfo */


/***************************************************************************
 * Function Name: BcmAtm_SetInterfaceLinkInfo
 * Description  : Backward compatibility function.
 * Returns      : STS_SUCCESS if successful or error status.
 ***************************************************************************/

typedef struct AtmInterfaceLinkInfo
{
    UINT32 ulStructureId;
    UINT32 ulLinkState;
    UINT32 ulLineRate;
    UINT32 ulReserved[2];
} ATM_INTERFACE_LINK_INFO, *PATM_INTERFACE_LINK_INFO;

extern "C"
BCMXTM_STATUS BcmAtm_SetInterfaceLinkInfo( UINT32 ulInterfaceId,
    PATM_INTERFACE_LINK_INFO pInterfaceLinkInfo )
{
    XTM_INTERFACE_LINK_INFO LinkInfo;

    LinkInfo.ulLinkState = pInterfaceLinkInfo->ulLinkState;
    LinkInfo.ulLinkUsRate = pInterfaceLinkInfo->ulLineRate;
    LinkInfo.ulLinkDsRate = pInterfaceLinkInfo->ulLineRate;
    LinkInfo.ulLinkTrafficType = TRAFFIC_TYPE_ATM;

    return( BcmXtm_SetInterfaceLinkInfo( PORT_TO_PORTID(ulInterfaceId),
        &LinkInfo ) );
} /* BcmAtm_SetInterfaceLinkInfo */

extern "C"
BCMXTM_STATUS BcmAtm_GetInterfaceId( UINT8 ucPhyPort, UINT32 *pulInterfaceId )
{
    *pulInterfaceId = 0;
    return( XTMSTS_SUCCESS );
} // BcmAtm_GetInterfaceId

/***************************************************************************
 * Function Name: BcmXtm_SendOamCell
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_SendOamCell( PXTM_ADDR pConnAddr,
    PXTM_OAM_CELL_INFO pOamCellInfo)
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->SendOamCell( pConnAddr, pOamCellInfo )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_SendOamCell */


/***************************************************************************
 * Function Name: BcmXtm_CreateNetworkDevice
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_CreateNetworkDevice( PXTM_ADDR pConnAddr,
    char *pszNetworkDeviceName )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->CreateNetworkDevice( pConnAddr,
            pszNetworkDeviceName )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_CreateNetworkDevice */


/***************************************************************************
 * Function Name: BcmXtm_DeleteNetworkDevice
 * Description  : 
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_DeleteNetworkDevice( PXTM_ADDR pConnAddr )
{
    return( (g_pXtmProcessor)
        ? g_pXtmProcessor->DeleteNetworkDevice( pConnAddr )
        : XTMSTS_STATE_ERROR );
} /* BcmXtm_DeleteNetworkDevice */

/***************************************************************************
 * Function Name: BcmXtm_ReInitialize
 * Description  : ATM/PTM processor ReInitialization entry point function.
 *                This function is mainly a debug but ueful routine that
 *                reinitializes the SAR (if SAR were to be in a dizzy state) by
 *                mimicing the link down/up conditions as well reconfiguring
 *                the SAR HW with any application layer configuration
   *                available.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_ReInitialize( void )
{
   BCMXTM_STATUS bxStatus ;

   if (g_pXtmProcessor) {
      bxStatus = g_pXtmProcessor->DebugReInitialize() ;
   }
   else
      bxStatus = XTMSTS_STATE_ERROR ;

   return( bxStatus );
} /* BcmXtm_ReInitialize */


#if defined(CONFIG_BCM96362)
/***************************************************************************
 * Function Name: BcmXtm_ChipInitialization (6362)
 * Description  : Chip specific initialization.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_ChipInitialize( PXTM_INITIALIZATION_PARMS pInitParms,
    UINT32 *pulBusSpeed )
{
    UINT32 miscStrapBus;

    /* TBD. */
    PERF->blkEnables |= SAR_CLK_EN;

    /* Find the VCO frequency value */
    miscStrapBus = MISC->miscStrapBus ;
    miscStrapBus &= MISC_STRAP_BUS_MIPS_PLL_FVCO_MASK;
    miscStrapBus >>= MISC_STRAP_BUS_MIPS_PLL_FVCO_SHIFT;
    *pulBusSpeed = kerSysGetUbusFreq(miscStrapBus);

    return( XTMSTS_SUCCESS );
} /* BcmXtm_ChipInitialization */
#elif defined(CONFIG_BCM96328)
/***************************************************************************
 * Function Name: BcmXtm_ChipInitialization (6328)
 * Description  : Chip specific initialization.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_ChipInitialize( PXTM_INITIALIZATION_PARMS pInitParms,
    UINT32 *pulBusSpeed )
{
    /* TBD. */
    PERF->blkEnables |= SAR_CLK_EN;
    *pulBusSpeed = UBUS_BASE_FREQUENCY_IN_MHZ;  /* TBD */

    return( XTMSTS_SUCCESS );
} /* BcmXtm_ChipInitialization */
#elif defined(CONFIG_BCM96368)
/***************************************************************************
 * Function Name: BcmXtm_ChipInitialize (6368)
 * Description  : Chip specific initialization.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
extern "C"
BCMXTM_STATUS BcmXtm_ChipInitialize( PXTM_INITIALIZATION_PARMS pInitParms,
    UINT32 *pulBusSpeed )
{
    PERF->blkEnables |= SAR_CLK_EN | SWPKT_SAR_CLK_EN;

    if( (pInitParms->ulPortConfig & PC_INTERNAL_EXTERNAL_MASK) !=
        PC_ALL_INTERNAL )
    {
        PERF->blkEnables |= UTOPIA_CLK_EN;
        GPIO->GPIOBaseMode |= EN_UTO;
    }

    *pulBusSpeed = UBUS_BASE_FREQUENCY_IN_MHZ; /* not used */

    return( XTMSTS_SUCCESS );
} /* BcmXtm_ChipInitialization */
#endif


