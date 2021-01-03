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
 * File Name  : xtminterface.cpp
 *
 * Description: This file contains the implementation for the XTM interface
 *              class.  This class handles the processing that is associated
 *              with an ATM/PTM port.
 ***************************************************************************/

#include "bcmtypes.h"
#include "xtmcfgimpl.h"

#if defined(CONFIG_BCM96368)

typedef USHORT ushort ;
typedef UINT   uint ;

#include "bcmadsl.h"
typedef int (*FN_ADSL_GET_OBJECT_VALUE) (unsigned char lineId, char *objId,
		                                   int objIdLen, char *dataBuf, long *dataBufLen);

static adslMibInfo MibInfo ;

extern "C" {

extern FN_ADSL_GET_OBJECT_VALUE g_pfnAdslGetObjValue;
}
#endif

/***************************************************************************
 * Function Name: XTM_INTERFACE
 * Description  : Constructor for the XTM interface class.
 * Returns      : None.
 ***************************************************************************/
XTM_INTERFACE::XTM_INTERFACE( void )
{
    m_ulTxPhysPort = 0;
    m_ulRxPhysPort = 0;
    m_ulLogicalPort = 0;
    m_ulLogicalBondingPort = MAX_INTERFACES ;
	 m_ulDataStatus = DATA_STATUS_DISABLED ;
	 m_ulErrTickCount = 0 ;
    m_ulAdslTxDescrTableAddr = 0;
    m_ulAdslRxDescrTableAddr = 0;
    memset(&m_Cfg, 0x00, sizeof(m_Cfg));
    memset( &m_LinkInfo, 0x00, sizeof(m_LinkInfo));
    memset( &m_Stats, 0x00, sizeof(m_Stats));
    m_LinkInfo.ulLinkState = LINK_DOWN;
    m_Cfg.ulIfAdminStatus = ADMSTS_DOWN;
} /* XTM_INTERFACE */


/***************************************************************************
 * Function Name: ~XTM_INTERFACE
 * Description  : Destructor for the XTM interface class.
 * Returns      : None.
 ***************************************************************************/
XTM_INTERFACE::~XTM_INTERFACE( void )
{
    Uninitialize();
} /* ~XTM_INTERFACE */


/***************************************************************************
 * Function Name: Initialize (6368, 6362, 6328)
 * Description  : Initializes the object.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::Initialize( UINT32 ulPort, UINT32 ulBondingPort )
{
    BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;

    m_ulLogicalPort = ulPort;
    m_ulTxPhysPort = PORT_LOG_TO_PHYS(ulPort);
    m_ulRxPhysPort = PORT_PHYS_TX_TO_PHYS_RX(m_ulTxPhysPort);

    m_ulLogicalBondingPort = ulBondingPort ;

    XP_REGS->ulTxSarCfg |= (1 << (m_ulTxPhysPort + TXSARA_CRC10_EN_SHIFT));
    XP_REGS->ulRxSarCfg |= (1 << (m_ulRxPhysPort + RXSARA_CRC10_EN_SHIFT));

		 XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] |= RX_DOE_MASK ;

    XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] |= RXA_GFC_ERROR_IGNORE;

    m_Cfg.ulIfLastChange = XtmOsTickGet() / 10;

    return( bxStatus );
} /* Initialize */


/***************************************************************************
 * Function Name: GetStats (6368, 6362, 6328)
 * Description  : Returns statistics for this interface.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::GetStats( PXTM_INTERFACE_STATS pStats,
    UINT32 ulReset )
{
    UINT32 ulCurrMibCtrl = XP_REGS->ulMibCtrl;

    XP_REGS->ulMibCtrl = (ulReset) ? 1 : 0;

    pStats->ulIfInOctets += XP_REGS->ulRxPortPktOctCnt[m_ulRxPhysPort];
    pStats->ulIfOutOctets += XP_REGS->ulTxPortPktOctCnt[m_ulTxPhysPort];
    pStats->ulIfInPackets += XP_REGS->ulRxPortPktCnt[m_ulRxPhysPort];
    pStats->ulIfOutPackets += XP_REGS->ulTxPortPktCnt[m_ulTxPhysPort];
    pStats->ulIfInOamRmCells += (XP_REGS->ulTxRxPortOamCellCnt[m_ulRxPhysPort] &
        OAM_RX_CELL_COUNT_MASK) >> OAM_RX_CELL_COUNT_SHIFT;
    pStats->ulIfOutOamRmCells += (XP_REGS->ulTxRxPortOamCellCnt[m_ulTxPhysPort] &
        OAM_TX_CELL_COUNT_MASK) >> OAM_TX_CELL_COUNT_SHIFT;
    pStats->ulIfInAsmCells += (XP_REGS->ulTxRxPortAsmCellCnt[m_ulRxPhysPort] &
        ASM_RX_CELL_COUNT_MASK) >> ASM_RX_CELL_COUNT_SHIFT;
    pStats->ulIfOutAsmCells += (XP_REGS->ulTxRxPortAsmCellCnt[m_ulTxPhysPort] &
        ASM_TX_CELL_COUNT_MASK) >> ASM_TX_CELL_COUNT_SHIFT;
    pStats->ulIfInCellErrors +=
        (XP_REGS->ulRxPortErrorPktCellCnt[m_ulRxPhysPort] &
        ERROR_RX_CELL_COUNT_MASK) >> ERROR_RX_CELL_COUNT_SHIFT;
    pStats->ulIfInPacketErrors +=
        (XP_REGS->ulRxPortErrorPktCellCnt[m_ulRxPhysPort] &
        ERROR_RX_PKT_COUNT_MASK) >> ERROR_RX_PKT_COUNT_SHIFT;

    XP_REGS->ulMibCtrl = ulCurrMibCtrl;

    return( XTMSTS_SUCCESS );
} /* GetStats */


/***************************************************************************
 * Function Name: SetLinkInfo (6368, 6362, 6328)
 * Description  : Call when there is a change in link status.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::SetLinkInfo( PXTM_INTERFACE_LINK_INFO pLinkInfo, UINT8 rxConfig,
		                                    UINT8 linkInfoConfig, UINT8 txConfig)
{
    if( pLinkInfo->ulLinkState == LINK_UP )
	 {
		 if( m_Cfg.ulIfAdminStatus == ADMSTS_UP )
		 {
			 if (txConfig == XTM_CONFIGURE) {

				 XP_REGS->ulTxSchedCfg |=
					 1 << (m_ulTxPhysPort + TXSCH_PORT_EN_SHIFT);
				 XP_REGS->ulTxUtopiaCfg |= 
					 1 << (m_ulTxPhysPort + TXUTO_PORT_EN_SHIFT);
			 }

         if (linkInfoConfig == XTM_CONFIGURE) {

            memcpy( &m_LinkInfo, pLinkInfo, sizeof(m_LinkInfo));
            m_Cfg.usIfTrafficType = pLinkInfo->ulLinkTrafficType;
         }

			 if (rxConfig == XTM_CONFIGURE) {
				 XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] |= RX_PORT_EN;

				 if (m_Cfg.usIfTrafficType == TRAFFIC_TYPE_ATM_BONDED)
					 XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] |= (RXA_BONDING_VP_MASK |
							 (m_ulRxPhysPort << RX_PORT_MASK_SHIFT) |
							 RX_DOE_IDLE_CELL |
							 RXA_HEC_CRC_IGNORE |
							 RXA_GFC_ERROR_IGNORE) ;
				 XP_REGS->ulRxUtopiaCfg |= 
					 1 << (m_ulRxPhysPort + RXUTO_PORT_EN_SHIFT);

				 if( m_Cfg.usIfTrafficType == TRAFFIC_TYPE_PTM_RAW )
				 {
					 m_Cfg.usIfTrafficType = TRAFFIC_TYPE_PTM;
					 XP_REGS->ulRxUtopiaCfg &= ~RXUTO_TEQ_PORT_MASK;
					 XP_REGS->ulRxUtopiaCfg|=(m_ulRxPhysPort<<RXUTO_TEQ_PORT_SHIFT);
				 }
			 } /* rxConfig */
		 }

#if defined(CONFIG_BCM96368)
		 /* Read a current snapshot of the IF Error Counters */

		 if (m_ulLogicalBondingPort != MAX_INTERFACES) {
			 long int size = sizeof (adslMibInfo) ;

          if (g_pfnAdslGetObjValue != NULL)
			   g_pfnAdslGetObjValue (m_ulLogicalPort, NULL, 0, (char *) &MibInfo, &size ) ;

			 m_PrevIfMonitorInfo.rx_loss_of_sync = MibInfo.adslPerfData.perfTotal.adslLOSS ;
			 m_PrevIfMonitorInfo.rx_SES_count_change  = MibInfo.adslPerfData.perfTotal.adslSES ;
			 m_PrevIfMonitorInfo.tx_SES_count_change = MibInfo.adslTxPerfTotal.adslSES ;
			 m_PrevIfMonitorInfo.tx_LOS_change = MibInfo.adslTxPerfTotal.adslLOSS ;

	 		 m_ulErrTickCount = 0 ;
		 }
#endif

		 m_ulDataStatus = DATA_STATUS_ENABLED ;
	 }
	 else /* LINK_DOWN */
	 {
      if (txConfig == XTM_CONFIGURE) {

         XP_REGS->ulTxUtopiaCfg &= 
            ~(1 << (m_ulTxPhysPort + TXUTO_PORT_EN_SHIFT));
         XP_REGS->ulTxSchedCfg &=
            ~(1 << (m_ulTxPhysPort + TXSCH_PORT_EN_SHIFT));
      }
      else {
         /* Workaround for 68 atm bonding. Keep the scheduler enabled and the
          * utopia disabled (as the other bonding link may still be up.)
          */
         if( m_Cfg.usIfTrafficType == TRAFFIC_TYPE_ATM_BONDED )
            XP_REGS->ulTxUtopiaCfg &= 
               ~(1 << (m_ulTxPhysPort + TXUTO_PORT_EN_SHIFT));
      }

      if (linkInfoConfig == XTM_CONFIGURE) {
         memcpy( &m_LinkInfo, pLinkInfo, sizeof(m_LinkInfo));
         m_Cfg.usIfTrafficType = TRAFFIC_TYPE_NOT_CONNECTED;
      }

		 if (rxConfig == XTM_CONFIGURE) {
			 XP_REGS->ulRxUtopiaCfg &= 
				 ~(1 << (m_ulRxPhysPort + RXUTO_PORT_EN_SHIFT));
			 XP_REGS->ulRxUtopiaCfg &= ~RXUTO_TEQ_PORT_MASK;
			 XP_REGS->ulRxUtopiaCfg |= (XP_MAX_PORTS << RXUTO_TEQ_PORT_SHIFT);
			 XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] &= ~RX_PORT_EN;
		 }

		 m_ulDataStatus = DATA_STATUS_DISABLED ;
	 	 m_ulErrTickCount = 0 ;
	 }

    m_Cfg.ulIfLastChange = XtmOsTickGet() / 10;

    return( XTMSTS_SUCCESS );
} /* SetLinkInfo */

/***************************************************************************
 * Function Name: GetLinkErrorStatus (6368, 6362, 6328)
 * Description  : Call for detecting Link Errors. Only called for 6368 bonding
 * through run-time configurations.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::GetLinkErrorStatus ( )
{
	BCMXTM_STATUS nRet = XTMSTS_SUCCESS ;

#if defined(CONFIG_BCM96368)
	if (m_ulLogicalBondingPort != MAX_INTERFACES) {

		if (m_ulErrTickCount == 0) {

			long int size = sizeof (adslMibInfo) ;
			IfMonitorErrorCounters CurrIfMonitorInfo ;

         if (g_pfnAdslGetObjValue != NULL)
			   g_pfnAdslGetObjValue (m_ulLogicalPort, NULL, 0, (char *) &MibInfo, &size) ;

			CurrIfMonitorInfo.rx_loss_of_sync = MibInfo.adslPerfData.perfTotal.adslLOSS ;
			CurrIfMonitorInfo.rx_SES_count_change  = MibInfo.adslPerfData.perfTotal.adslSES ;
			CurrIfMonitorInfo.tx_SES_count_change = MibInfo.adslTxPerfTotal.adslSES ;
			CurrIfMonitorInfo.tx_LOS_change = MibInfo.adslTxPerfTotal.adslLOSS ;

			if ((m_PrevIfMonitorInfo.rx_loss_of_sync != CurrIfMonitorInfo.rx_loss_of_sync) ||
					(m_PrevIfMonitorInfo.rx_SES_count_change != CurrIfMonitorInfo.rx_SES_count_change) ||
					(m_PrevIfMonitorInfo.tx_SES_count_change != CurrIfMonitorInfo.tx_SES_count_change) ||
					(m_PrevIfMonitorInfo.tx_LOS_change != CurrIfMonitorInfo.tx_LOS_change)) {

				memcpy (&m_PrevIfMonitorInfo, &CurrIfMonitorInfo, sizeof (IfMonitorErrorCounters)) ;
				m_ulErrTickCount = XTM_BOND_DSL_ERR_DURATION_TIMEOUT_MS ;
				nRet = XTMSTS_ERROR ;
			}
		}
		else {
			m_ulErrTickCount -= XTM_BOND_DSL_MONITOR_TIMEOUT_MS ;
			nRet = XTMSTS_ERROR ;
		}
	} /* if (m_ulLogicalBondingPort != MAX_INTERFACES) */
#endif

	return (nRet) ;
}

/***************************************************************************
 * Function Name: SetPortDataStatus (6368)
 * Description  : Call for setting the port data status.
 *                Also effects the port status in the SAR Rx registers.
 *                This is necessary to avoid SAR lockup from undesired traffic
 *                in DS direction during line micro-interruption.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
void XTM_INTERFACE::SetPortDataStatus ( UINT32 status )
{
#ifdef AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH
   /* if Phy does stop & start the flow, then we do not need this here in XTM.
    * Currently in experimentation phase
    */
	if (m_Cfg.usIfTrafficType == TRAFFIC_TYPE_ATM_BONDED) {
		/* For PTM Bonding, XTM Network driver will take care in SW. This is for
		 * HW control.
		 * We only effect Rx direction, as there may be pending traffic for
		 * concerned port in tx direction in the DMA queues, which should not be
		 * blocked.
		 */
   if (status == DATA_STATUS_ENABLED) {
      XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] |= RX_PORT_EN;
		   XP_REGS->ulRxUtopiaCfg |= 1 << (m_ulRxPhysPort + RXUTO_PORT_EN_SHIFT);

   }
   else {
      XP_REGS->ulRxAtmCfg[m_ulRxPhysPort] &= ~RX_PORT_EN;
		   XP_REGS->ulRxUtopiaCfg &= ~(1 << (m_ulRxPhysPort + RXUTO_PORT_EN_SHIFT));
      }
   }
#endif
   m_ulDataStatus = status;
}

/***************************************************************************
 * Function Name: AdslInit (6368, 6362, 6328)
 * Description  : Not used on ths chip.
 * Returns      : XTMSTS_SUCCESS
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::AdslInit( void )
{
    return( XTMSTS_SUCCESS );
} /* AdslInit */


/***************************************************************************
 * Function Name: ConfigureAtmProcessor (6368, 6362, 6328)
 * Description  : Not used on ths chip.
 * Returns      : None.
 ***************************************************************************/
void XTM_INTERFACE::ConfigureAtmProcessor( UINT32 ulEnable )
{
} /* ConfigureAtmProcessor */

/***************************************************************************
 * Function Name: Uninitialize
 * Description  : Undo Initialize.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::Uninitialize( void )
{
    m_ulLogicalBondingPort = MAX_INTERFACES ;
    return( XTMSTS_SUCCESS );
} /* Uninitialize */


/***************************************************************************
 * Function Name: GetCfg
 * Description  : Returns the interface configuration record.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::GetCfg( PXTM_INTERFACE_CFG pCfg,
    XTM_CONNECTION_TABLE *pConnTbl )
{
    BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
    XTM_CONNECTION *pConn;
    XTM_ADDR Addr;

    memcpy(pCfg, &m_Cfg, sizeof(XTM_INTERFACE_CFG));

    pCfg->ulIfOperStatus = (m_LinkInfo.ulLinkState == LINK_UP &&
        IsInterfaceUp()) ? OPRSTS_UP : OPRSTS_DOWN;

#ifdef AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH
#if defined(CONFIG_BCM96368)
       pCfg->usIfSupportedTrafficTypes = (SUPPORT_TRAFFIC_TYPE_ATM |
             SUPPORT_TRAFFIC_TYPE_PTM | SUPPORT_TRAFFIC_TYPE_PTM_RAW | SUPPORT_TRAFFIC_TYPE_PTM_BONDED
				 | SUPPORT_TRAFFIC_TYPE_ATM_BONDED);
#else
       pCfg->usIfSupportedTrafficTypes = (SUPPORT_TRAFFIC_TYPE_ATM |
             SUPPORT_TRAFFIC_TYPE_PTM | SUPPORT_TRAFFIC_TYPE_PTM_RAW);
#endif
#else
    if (m_ulLogicalBondingPort != MAX_INTERFACES)
       pCfg->usIfSupportedTrafficTypes = (SUPPORT_TRAFFIC_TYPE_ATM |
             SUPPORT_TRAFFIC_TYPE_PTM | SUPPORT_TRAFFIC_TYPE_PTM_RAW | SUPPORT_TRAFFIC_TYPE_PTM_BONDED
				 | SUPPORT_TRAFFIC_TYPE_ATM_BONDED);
    else
       pCfg->usIfSupportedTrafficTypes = (SUPPORT_TRAFFIC_TYPE_ATM |
             SUPPORT_TRAFFIC_TYPE_PTM | SUPPORT_TRAFFIC_TYPE_PTM_RAW);

#endif
    /* Calculate the number of configured VCCs for this interface. */
    pCfg->ulAtmInterfaceConfVccs = 0;
    UINT32 i = 0;
    while( (pConn = pConnTbl->Enum( &i )) != NULL )
    {
        pConn->GetAddr( &Addr );
		  if(( (Addr.ulTrafficType & TRAFFIC_TYPE_ATM_MASK) == TRAFFIC_TYPE_ATM ) &&
            (Addr.u.Vcc.ulPortMask & PORT_TO_PORTID(m_ulTxPhysPort)) ==
             PORT_TO_PORTID(m_ulTxPhysPort) )
        {
            pCfg->ulAtmInterfaceConfVccs++;
        }
    }

    return( bxStatus );
} /* GetCfg */


/***************************************************************************
 * Function Name: SetCfg
 * Description  : Sets the configuration record.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_INTERFACE::SetCfg( PXTM_INTERFACE_CFG pCfg )
{
    m_Cfg.ulIfAdminStatus = pCfg->ulIfAdminStatus ;

    if (m_Cfg.ulIfAdminStatus == ADMSTS_UP ) {
        ConfigureAtmProcessor( 1 );
    }
    else {
        ConfigureAtmProcessor( 0 );
    }

    return( XTMSTS_SUCCESS );
} /* SetCfg */

