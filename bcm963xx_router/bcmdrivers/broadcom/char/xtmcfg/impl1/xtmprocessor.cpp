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
 * File Name  : xtmprocessor.cpp
 *
 * Description: This file contains the implementation for the XTM processor
 *              class.  This is the topmost class that contains or points to
 *              all objects needed to handle the processing of the XTM processor.
 ***************************************************************************/

#include "xtmcfgimpl.h"
#include "bcm_map.h"
#include "board.h"

static UINT32	gulBondDslMonitorValid = 0 ;

/***************************************************************************
 * Function Name: XTM_PROCESSOR
 * Description  : Constructor for the XTM processor class.
 * Returns      : None.
 ***************************************************************************/
XTM_PROCESSOR::XTM_PROCESSOR( void )
{
    m_pTrafficDescrEntries = NULL;
    m_ulNumTrafficDescrEntries = 0;
    memset(&m_InitParms, 0x00, sizeof(m_InitParms));
    m_ulConnSem = XtmOsCreateSem(1);
    m_pSoftSar = NULL;
    memset(m_ulRxVpiVciCamShadow, 0x00, sizeof(m_ulRxVpiVciCamShadow));
} /* XTM_PROCESSOR */


/***************************************************************************
 * Function Name: ~XTM_PROCESSOR
 * Description  : Destructor for the XTM processor class.
 * Returns      : None.
 ***************************************************************************/
XTM_PROCESSOR::~XTM_PROCESSOR( void )
{
    Uninitialize(bcmxtmrt_request);
} /* ~XTM_PROCESSOR */

/***************************************************************************
 * Function Name: Initialize (6368, 6362, 6328)
 * Description  : Initializes the object.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::Initialize( PXTM_INITIALIZATION_PARMS pInitParms,
    UINT32 ulBusSpeed, FN_XTMRT_REQ pfnXtmrtReq )

{
    BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
    UINT32 i, j, k, *p;

    if( pInitParms )
        memcpy(&m_InitParms, pInitParms, sizeof(m_InitParms));

    if( pfnXtmrtReq )
        m_pfnXtmrtReq = pfnXtmrtReq;
    
    if( ulBusSpeed )
    {
        m_ulBusSpeed = ulBusSpeed;
        XTM_CONNECTION::SetBusSpeed( ulBusSpeed );
    }

    i = TXSARP_ENET_FCS_INSERT | TXSARP_CRC16_EN | TXSARP_PREEMPT |
        TXSAR_USE_ALT_FSTAT | TXSARP_SOF_WHILE_TX;
    j = XP_REGS->ulRxSarCfg;

	 XP_REGS->ulRxAtmCfg[0] |= RX_DOE_IDLE_CELL ;
	 XP_REGS->ulRxAtmCfg[1] |= RX_DOE_IDLE_CELL ;
	 XP_REGS->ulRxAtmCfg[2] |= RX_DOE_IDLE_CELL ;
	 XP_REGS->ulRxAtmCfg[3] |= RX_DOE_IDLE_CELL ;

    XP_REGS->ulRxAalCfg &= ~RXAALP_CELL_LENGTH_MASK;

    XP_REGS->ulTxSarCfg |= i;
    XP_REGS->ulRxSarCfg = j;

    m_ulTrafficType = TRAFFIC_TYPE_NOT_CONNECTED;

    XP_REGS->ulTxSchedCfg = TXSCH_SHAPER_RESET;
    for( i = 0; i < 1000; i++ )
        ;

    XP_REGS->ulTxSchedCfg = 0;

    for( i = 0; i < XP_MAX_CONNS; i++ )
        XP_REGS->ulSstCtrl[i] = 0;

    for( i = 0; i < XP_MAX_CONNS; i++ )
        XP_REGS->ulSstPcrScr[i] = 0;

    for( i = 0; i < XP_MAX_CONNS; i++ )
        XP_REGS->ulSstBt[i] = 0;

    for( i = 0; i < XP_MAX_CONNS; i++ )
        XP_REGS->ulSstBucketCnt[i] = 0;

    XP_REGS->ulTxUtopiaCfg &= ~TXUTO_MODE_INT_EXT_MASK;

    switch( (m_InitParms.ulPortConfig & PC_INTERNAL_EXTERNAL_MASK) )
    {
    case PC_INTERNAL_EXTERNAL:
        XP_REGS->ulTxUtopiaCfg |= TXUTO_MODE_INT_EXT;
        XP_REGS->ulRxUtopiaCfg |= RXUTO_INTERNAL_BUF0_EN|RXUTO_EXTERNAL_BUF1_EN;
        break;

    case PC_ALL_EXTERNAL:
        XP_REGS->ulTxUtopiaCfg |= TXUTO_MODE_ALL_EXT;
        XP_REGS->ulRxUtopiaCfg |= RXUTO_EXTERNAL_BUF1_EN;
        break;

    /* case PC_ALL_INTERNAL: */
    default:
        XP_REGS->ulTxUtopiaCfg |= TXUTO_MODE_ALL_INT;
        XP_REGS->ulRxUtopiaCfg |= RXUTO_INTERNAL_BUF0_EN;
        break;
    }

    if( (m_InitParms.ulPortConfig & PC_NEG_EDGE) == PC_NEG_EDGE )
    {
        XP_REGS->ulTxUtopiaCfg |= TXUTO_NEG_EDGE;
        XP_REGS->ulRxUtopiaCfg |= RXUTO_NEG_EDGE;
    }
    else
    {
        XP_REGS->ulTxUtopiaCfg &= ~TXUTO_NEG_EDGE;
        XP_REGS->ulRxUtopiaCfg &= ~RXUTO_NEG_EDGE;
    }

    XP_REGS->ulRxAalMaxSdu = 0xffff;

    /* Initialize transmit and receive connection tables. */
    for( i = 0; i < XP_MAX_CONNS; i++ )
        XP_REGS->ulTxVpiVciTable[i] = 0;

    for( i = 0; i < XP_MAX_CONNS * 2; i++ )
        XP_REGS->ulRxVpiVciCam[i] = 0;

    /* Add headers. */
    UINT32 Hdrs[XP_MAX_TX_HDRS][XP_TX_HDR_WORDS] = PKT_HDRS;
    UINT32 ulHdrOffset = 0; /* header offset in packet */

    for( i = 0, k = PKT_HDRS_START_IDX; Hdrs[k][0] != PKT_HDRS_END; i++, k++ )
    {
        /* p[0] = header length, p[1]... = header contents */
        p = Hdrs[k];
        XP_REGS->ulTxHdrInsert[i] =
            ((p[0] & TXHDR_COUNT_MASK) << TXHDR_COUNT_SHIFT) |
            ((ulHdrOffset & TXHDR_OFFSET_MASK) << TXHDR_OFFSET_SHIFT);

        for( j = 0; j < p[0] / sizeof(UINT32); j++ )
            XP_REGS->ulTxHdrValues[i][j] = p[1 + j];

        if( p[0] % sizeof(UINT32) != 0 )
            XP_REGS->ulTxHdrValues[i][j] = p[1 + (p[0] / sizeof(UINT32))];
    }

    /* Read to clear MIB counters. Set "clear on read" as default. */
    XP_REGS->ulMibCtrl = 1;

    for(p = XP_REGS->ulTxPortPktOctCnt; p <= &XP_REGS->ulBondOutputCellCnt; p++)
        i = *p;

    for(p=XP_REGS->ulTxVcPktOctCnt;p<XP_REGS->ulTxVcPktOctCnt+XP_MAX_CONNS; p++)
        i = *p;

    XP_REGS->ulRxPktBufMibMatch = 0;
    for( i = 0; i < XP_RX_MIB_MATCH_ENTRIES; i++ )
    {
        j = XP_REGS->ulRxPktBufMibRxOctet;
        k = XP_REGS->ulRxPktBufMibRxPkt;
    }

    /* Configure Rx Filter Mib control register. */
    XP_REGS->ulRxPktBufMibCtrl = 0;

    /* Mask and clear interrupts. */
    XP_REGS->ulIrqMask = ~INTR_MASK;
    XP_REGS->ulIrqStatus = ~0;

    /* Initialize interfaces. */
    for( i = PHY_PORTID_0; i < MAX_INTERFACES; i++ ) {
       if ((m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) ||
           (m_InitParms.bondConfig.sConfig.atmBond == BC_ATM_BONDING_ENABLE)) {
          if (m_InitParms.bondConfig.sConfig.dualLat == BC_DUAL_LATENCY_ENABLE) {
             if (i < PHY_PORTID_2)
                m_Interfaces[i].Initialize(i, i+PHY_PORTID_2);
             else
                m_Interfaces[i].Initialize(i, i-PHY_PORTID_2);
          }
          else {
             /* No dual latency. Hard code it for the interfaces as per chip
              * definitions
              */
             UINT32 ulBondingPort ;
             switch (i) {
                case PHY_PORTID_0  :
                   ulBondingPort = PHY_PORTID_1 ;
                   break ;
                case PHY_PORTID_1  :
                   ulBondingPort = PHY_PORTID_0 ;
                   break ;
                default    :
                   ulBondingPort = MAX_INTERFACES ;
                   break ;
             }
             m_Interfaces[i].Initialize(i, ulBondingPort) ;
          }
       }
       else {
          m_Interfaces[i].Initialize(i, MAX_INTERFACES);
       }
    }

    if( pInitParms)
    {
        /* First time that this Initialize function has been called.  Call the
         * bcmxtmrt driver initialization function.
         */
        XTMRT_GLOBAL_INIT_PARMS GlobInit;
        memcpy(GlobInit.ulReceiveQueueSizes, m_InitParms.ulReceiveQueueSizes,
            sizeof(GlobInit.ulReceiveQueueSizes));
        GlobInit.bondConfig.uConfig = m_InitParms.bondConfig.uConfig ;
        GlobInit.pulMibTxOctetCountBase = XP_REGS->ulTxVcPktOctCnt;
        GlobInit.ulMibRxClrOnRead = PBMIB_CLEAR;
        GlobInit.pulMibRxCtrl = &XP_REGS->ulRxPktBufMibCtrl;
        GlobInit.pulMibRxMatch = &XP_REGS->ulRxPktBufMibMatch;
        GlobInit.pulMibRxOctetCount = &XP_REGS->ulRxPktBufMibRxOctet;
        GlobInit.pulMibRxPacketCount = &XP_REGS->ulRxPktBufMibRxPkt;
        GlobInit.pulRxCamBase        = &XP_REGS->ulRxVpiVciCam [0];
        (*m_pfnXtmrtReq) (NULL, XTMRT_CMD_GLOBAL_INITIALIZATION, &GlobInit);

        bxStatus = m_OamHandler.Initialize( m_pfnXtmrtReq );

#if defined(CONFIG_BCM96368)
#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
		  m_pAsmHandler = NULL;
		  if (m_InitParms.bondConfig.sConfig.bondAutoDetect == BC_BONDING_AUTODETECT_ENABLE)
#else
		  if (m_InitParms.bondConfig.sConfig.atmBond == BC_ATM_BONDING_ENABLE)
#endif
		  {
			  if (bxStatus == XTMSTS_SUCCESS) {
				  m_pAsmHandler = new XTM_ASM_HANDLER ;
				  if (m_pAsmHandler != NULL) {
					  bxStatus = m_pAsmHandler->Initialize( m_InitParms.bondConfig,
							                m_pfnXtmrtReq, &m_Interfaces[0], &m_ConnTable );
				  }
				  else {
					  bxStatus = XTMSTS_ALLOC_ERROR ;
				  }
			  }
		  } // if (atm bonding enabled)

		  if (m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
			  if (bxStatus == XTMSTS_SUCCESS) {
				  XtmOsStartTimer ((void *) BondDslMonitorTimerCb, (UINT32) this,
						  XTM_BOND_DSL_MONITOR_TIMEOUT_MS) ;
				  gulBondDslMonitorValid = 1 ;
			  }
		  } // if (ptm bonding enabled)
#endif
    }

    return( bxStatus );
} /* Initialize */


#if defined(CONFIG_BCM96368)
/***************************************************************************
 * Function Name: ReInitialize (6368)
 * Description  : ReInitializes the bonding aspect for the 6368 based on the
 * 					traffictype.
 * 					Bonding type can change dynamically, atleast the SW
 * 					provisions for the same. Only Bonding/Normal modes cannot be
 * 					changed dynamically and requires system reboot for now.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
void XTM_PROCESSOR::ReInitialize( UINT32 ulLogicalPort, UINT32 ulTrafficType )
{
	UINT32	i,j,k ;

	XtmOsPrintf ("bcmxtmcfg: Reinitialize port %d traffictype %d \n", ulLogicalPort,
			       ulTrafficType);

	switch (ulTrafficType) {

		case	TRAFFIC_TYPE_PTM_BONDED	:

			if (m_ulTrafficType != ulTrafficType) {
				i = XP_REGS->ulTxSarCfg;
				i |= TXSARA_BOND_EN;

				/* Unconfigure any ATM bonding related settings */
				j = XP_REGS->ulRxSarCfg;

				i &= ~(TXSARA_SID12_EN | TXSARA_BOND_DUAL_LATENCY);
				j &= ~(RXSARA_BOND_EN | RXSARA_SID12_EN | RXSARA_BOND_DUAL_LATENCY);

            k = XP_REGS->ulTxUtopiaCfg;
				k &= ~TXUTO_CELL_FIFO_DEPTH_1; /* Set to default depth 2 */
				XP_REGS->ulTxUtopiaCfg = k;

				XP_REGS->ulTxSarCfg = i;
				XP_REGS->ulRxSarCfg = j;

				/* In PTM Bonded mode, strip 2 byte PTM CRC-16. Received traffic are
				 * the fragments as opposed to a packet, and only last fragment of a
				 * packet will carry ENET FCS so we cannot strip the FCS per fragment.
				 * It is taken care in the Rx bonding process.
				 */
				m_ulTrafficType = ulTrafficType ;
				XtmOsPrintf ("bcmxtmcfg: PTM Bonding enabled \n");
			}

			//m_Interfaces[ulLogicalPort].SetLinkDataStatus (DATA_STATUS_ENABLED) ;
			break;

		case	TRAFFIC_TYPE_ATM_BONDED	:

			if (m_ulTrafficType != ulTrafficType) {
				i = XP_REGS->ulTxSarCfg;
				j = XP_REGS->ulRxSarCfg;
				k = XP_REGS->ulTxUtopiaCfg;

				i |= TXSARA_BOND_EN;
				j |= RXSARA_BOND_EN;
            k |= TXUTO_CELL_FIFO_DEPTH_1;

				/* Configure for 12-bit SID by default */

				i |= TXSARA_SID12_EN;
				j |= RXSARA_SID12_EN;

				if (m_InitParms.bondConfig.sConfig.dualLat == BC_DUAL_LATENCY_ENABLE) {
					i |= TXSARA_BOND_DUAL_LATENCY;
					j |= RXSARA_BOND_DUAL_LATENCY;
				}
				else {
					i &= ~TXSARA_BOND_DUAL_LATENCY;
					j &= ~RXSARA_BOND_DUAL_LATENCY;
				}

			   j &= ~RXSARA_ASM_CRC_DIS ;

#if 0 //def AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH
            j &= ~RXSARA_BOND_CELL_COUNT_MASK ;
            j |= 0x30 << RXSARA_BOND_CELL_COUNT_SHIFT ;
#endif
				XP_REGS->ulTxSarCfg = i;
				XP_REGS->ulRxSarCfg = j;
				XP_REGS->ulTxUtopiaCfg = k;
				m_ulTrafficType = ulTrafficType ;

				XtmOsPrintf ("bcmxtmcfg: ATM Bonding enabled \n");
			}
			break;

		default								:
			/* Non bonding modes. */
			i = XP_REGS->ulTxSarCfg;
			i &= ~TXSARA_BOND_EN;
			XP_REGS->ulTxSarCfg = i;

			/* In case of ATM mode, Un-configure the following. */
			i = XP_REGS->ulTxSarCfg;
			j = XP_REGS->ulRxSarCfg;

			i &= ~(TXSARA_BOND_EN | TXSARA_SID12_EN | TXSARA_BOND_DUAL_LATENCY);
			j &= ~(RXSARA_BOND_EN | RXSARA_SID12_EN | RXSARA_BOND_DUAL_LATENCY);

			k = XP_REGS->ulTxUtopiaCfg;
			k &= ~TXUTO_CELL_FIFO_DEPTH_1; /* Set to default depth 2 */
			XP_REGS->ulTxUtopiaCfg = k;

			XP_REGS->ulTxSarCfg = i;
			XP_REGS->ulRxSarCfg = j;
			m_ulTrafficType = ulTrafficType ;

			//m_Interfaces[ulLogicalPort].SetLinkDataStatus (DATA_STATUS_ENABLED) ;

			XtmOsPrintf ("bcmxtmcfg: Normal(XTM/PTM) Mode enabled \n");
			break ;
	} /* switch (ulTrafficType) */
}

#else /* Chips other than BCM96368 */

/***************************************************************************
 * Function Name: ReInitialize (6362/6328/6358/6338)
 * Description  : Initializes the bonding aspect for the chipsets.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
void XTM_PROCESSOR::ReInitialize ( UINT32 ulLogicalPort, UINT32 ulTrafficType )
{
	/* Nothing to do, as bonding is not available in these chipsets */
	return ;
}

#endif

/***************************************************************************
 * Function Name: Uninitialize
 * Description  : Undo Initialize.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::Uninitialize( FN_XTMRT_REQ pfnXtmrtReq )
{
    UINT32 i;
    XTM_CONNECTION *pConn;

	 gulBondDslMonitorValid = 0 ;
	 
    /* Set interface link state to down to disable SAR receive */
    for( i = PHY_PORTID_0; i < MAX_INTERFACES; i++ )
    {
        PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[i].GetLinkInfo();

        if (pLi->ulLinkState == LINK_UP)
        {
            XTM_INTERFACE_LINK_INFO linkInfo;

            linkInfo = *pLi;
            linkInfo.ulLinkState = LINK_DOWN;
            SetInterfaceLinkInfo( PORT_TO_PORTID(i), &linkInfo );
        }
    }

    /* Delete all network device instances. */
    i = 0;
    while( (pConn = m_ConnTable.Enum( &i )) != NULL )
    {
        m_ConnTable.Remove( pConn );
        delete pConn;
    }

    /* Uninitialize interfaces. */
    for( i = PHY_PORTID_0; i < MAX_INTERFACES; i++ )
        m_Interfaces[i].Uninitialize();

    if( m_pTrafficDescrEntries )
    {
        XtmOsFree( (char *) m_pTrafficDescrEntries );
        m_pTrafficDescrEntries = NULL;
    }

#if defined(CONFIG_BCM96368)
	 if (m_pAsmHandler) {
    	 m_pAsmHandler->Uninitialize() ;
		 delete m_pAsmHandler ;
	 }
#endif

    m_OamHandler.Uninitialize();

    m_pfnXtmrtReq = pfnXtmrtReq;
    (*m_pfnXtmrtReq) (NULL, XTMRT_CMD_GLOBAL_UNINITIALIZATION, NULL);

    return( XTMSTS_SUCCESS );

} /* Uninitialize */


/***************************************************************************
 * Function Name: GetTrafficDescrTable
 * Description  : Returns the Traffic Descriptor Table.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::GetTrafficDescrTable(PXTM_TRAFFIC_DESCR_PARM_ENTRY
    pTrafficDescrTable, UINT32 *pulTrafficDescrTableSize)
{
    BCMXTM_STATUS bxStatus = XTMSTS_PARAMETER_ERROR;

    if( *pulTrafficDescrTableSize >= m_ulNumTrafficDescrEntries )
    {
        if( m_pTrafficDescrEntries && m_ulNumTrafficDescrEntries )
        {
            memcpy(pTrafficDescrTable, m_pTrafficDescrEntries,
              m_ulNumTrafficDescrEntries*sizeof(XTM_TRAFFIC_DESCR_PARM_ENTRY));
            bxStatus = XTMSTS_SUCCESS;
        }
    }

    *pulTrafficDescrTableSize = m_ulNumTrafficDescrEntries;

    return( bxStatus );
} /* GetTrafficDescrTable */


/***************************************************************************
 * Function Name: SetTrafficDescrTable
 * Description  : Saves the supplied Traffic Descriptor Table.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::SetTrafficDescrTable(PXTM_TRAFFIC_DESCR_PARM_ENTRY
    pTrafficDescrTable, UINT32  ulTrafficDescrTableSize)
{
    BCMXTM_STATUS bxStatus;

    if( ulTrafficDescrTableSize )
    {
        /* Free an existing table if it exists. */
        if( m_pTrafficDescrEntries )
        {
            XtmOsFree( (char *) m_pTrafficDescrEntries );
            m_pTrafficDescrEntries = NULL;
            m_ulNumTrafficDescrEntries = 0;
        }

        UINT32 ulSize =
            ulTrafficDescrTableSize * sizeof(XTM_TRAFFIC_DESCR_PARM_ENTRY);

        /* Allocate memory for the new table. */
        m_pTrafficDescrEntries =
            (PXTM_TRAFFIC_DESCR_PARM_ENTRY) XtmOsAlloc(ulSize);

        /* Copy the table. */
        if( m_pTrafficDescrEntries )
        {
            m_ulNumTrafficDescrEntries = ulTrafficDescrTableSize;
            memcpy( m_pTrafficDescrEntries, pTrafficDescrTable, ulSize );

            /* Update connections with new values. */
            UINT32 i = 0;
            XTM_CONNECTION *pConn;
            while( (pConn = m_ConnTable.Enum( &i )) != NULL )
            {
                pConn->SetTdt( m_pTrafficDescrEntries,
                    m_ulNumTrafficDescrEntries );
            }

            bxStatus = XTMSTS_SUCCESS;
        }
        else
            bxStatus = XTMSTS_ALLOC_ERROR;
    }
    else
        bxStatus = XTMSTS_PARAMETER_ERROR;

    return( bxStatus );
} /* SetTrafficDescrTable */


/***************************************************************************
 * Function Name: GetInterfaceCfg
 * Description  : Returns the interface configuration record for the specified
 *                port id.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::GetInterfaceCfg( UINT32 ulPortId,
    PXTM_INTERFACE_CFG pInterfaceCfg )
{
    BCMXTM_STATUS bxStatus;
    UINT32 ulPort = PORTID_TO_PORT(ulPortId);

    if( ulPort < MAX_INTERFACES )
        bxStatus = m_Interfaces[ulPort].GetCfg( pInterfaceCfg, &m_ConnTable );
    else
        bxStatus = XTMSTS_PARAMETER_ERROR;

    return( bxStatus ) ;
} /* GetInterfaceCfg */


/***************************************************************************
 * Function Name: SetInterfaceCfg
 * Description  : Sets the interface configuration record for the specified
 *                port.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::SetInterfaceCfg( UINT32 ulPortId,
    PXTM_INTERFACE_CFG pInterfaceCfg )
{
    BCMXTM_STATUS bxStatus;
    UINT32 ulPort = PORTID_TO_PORT(ulPortId);
    UINT32 ulBondingPort ;

    if( ulPort < MAX_INTERFACES ) {
       bxStatus = m_Interfaces[ulPort].SetCfg( pInterfaceCfg );
       ulBondingPort = m_Interfaces[ulPort].GetBondingPortNum () ;
       if (ulBondingPort != MAX_INTERFACES)
          bxStatus = m_Interfaces[ulBondingPort].SetCfg( pInterfaceCfg );
    }
    else
       bxStatus = XTMSTS_PARAMETER_ERROR;

    return( bxStatus );
} /* SetInterfaceCfg */

/* The following routine updates (add/removal) the port mask for the connection
 * for bonding.
 * For ex., if a given port is part of the port mask, its bonding member will
 * also be updated to be part of the same port mask.
 * This happens transparent to the user layer.
 * It also updates (add/removal) the traffic type for the connection
 * for bonding.
 * For ex., if a given connection is PTM type and bonding is enabled, then
 * the traffic type is changed from/to PTM_BONDED type.
 * This happens transparent to the user layer.
 */
void XTM_PROCESSOR::UpdateConnAddrForBonding ( PXTM_ADDR pConnAddr, UINT32 operation )
{
   UINT32 ulPortId, ulPort, ulBondingPort ;
   UINT32 *pulPortMask = &pConnAddr->u.Conn.ulPortMask ;
   UINT32 *pulTrafficType = &pConnAddr->ulTrafficType ;

   if (( m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) ||
       ( m_InitParms.bondConfig.sConfig.atmBond == BC_ATM_BONDING_ENABLE)) {
      for (ulPortId = PORT_PHY0_PATH0 ; ulPortId <= PORT_PHY1_PATH1 ; ulPortId <<= 0x1) {
         if (*pulPortMask & ulPortId) {
            ulPort = PORTID_TO_PORT (ulPortId) ;
            ulBondingPort = m_Interfaces[ulPort].GetBondingPortNum () ;
            if (ulBondingPort != MAX_INTERFACES) {
               if (operation == XTM_ADD)
                  *pulPortMask |= PORT_TO_PORTID (ulBondingPort) ;
               else
                  *pulPortMask &= ~PORT_TO_PORTID (ulBondingPort) ;
            }
         }
      }

		if( (*pulTrafficType & TRAFFIC_TYPE_ATM_MASK) != TRAFFIC_TYPE_ATM ) {
         if (operation == XTM_ADD)
            *pulTrafficType = TRAFFIC_TYPE_PTM_BONDED ;
         else
            *pulTrafficType = TRAFFIC_TYPE_PTM ;
      }
		else {
         if (operation == XTM_ADD)
            *pulTrafficType = TRAFFIC_TYPE_ATM_BONDED ;
         else
            *pulTrafficType = TRAFFIC_TYPE_ATM ;
		}
   }
}

/* The following routine updates (add/removal) the bonding port ID for the
 * transmit Q Parameters section of the connection configuration.
 * For a given portId in the transmit Q Params information, the corresponding
 * bonding member also will be updated.
 * This happens transparent to the user layer.
 */
void XTM_PROCESSOR::UpdateTransmitQParamsForBonding ( XTM_TRANSMIT_QUEUE_PARMS *pTxQParms,
                                           UINT32 ulTxQParmsSize, UINT32 operation)
{
   UINT32 ulPort, ulBondingPort, ulLogicalBondingPort, ulLogicalPort ;
   UINT32 txQIndex ;
   PXTM_INTERFACE_LINK_INFO pLiPort, pLiBondingPort ;

   for (txQIndex = 0 ; txQIndex < ulTxQParmsSize; txQIndex++) {

      ulPort = PORTID_TO_PORT (pTxQParms [txQIndex].ulPortId ) ;
      ulLogicalPort = PORT_PHYS_TO_LOG (ulPort) ;
      ulLogicalBondingPort = m_Interfaces[ulLogicalPort].GetBondingPortNum () ;

      pLiPort =  m_Interfaces[ulLogicalPort].GetLinkInfo();

      if (ulLogicalBondingPort != MAX_INTERFACES) {

         pLiBondingPort =  m_Interfaces[ulLogicalBondingPort].GetLinkInfo();
         ulBondingPort = PORT_LOG_TO_PHYS (ulLogicalBondingPort) ;

         if ((pLiPort->ulLinkTrafficType == TRAFFIC_TYPE_PTM_BONDED) ||
             (pLiBondingPort->ulLinkTrafficType == TRAFFIC_TYPE_PTM_BONDED)) {

         if (operation == XTM_ADD)
            pTxQParms [txQIndex].ulBondingPortId = PORT_TO_PORTID (ulBondingPort) ;
         else
            pTxQParms [txQIndex].ulBondingPortId = PORT_PHY_INVALID ;
      }
      else
         pTxQParms [txQIndex].ulBondingPortId = PORT_PHY_INVALID ;
   }
      else
         pTxQParms [txQIndex].ulBondingPortId = PORT_PHY_INVALID ;
   } /* txQIndex */
}

#if defined(CONFIG_BCM96368)
void XTM_PROCESSOR::PollLines ()
{
	int  ulInterfaceId ;
	PXTM_INTERFACE_LINK_INFO pLi ;

	for (ulInterfaceId = 0 ; ulInterfaceId < MAX_BONDED_LINES; ulInterfaceId++) {

		pLi = m_Interfaces[ulInterfaceId].GetLinkInfo() ;

		if (pLi->ulLinkState == LINK_UP) {

			if (m_Interfaces[ulInterfaceId].GetLinkErrorStatus () != XTMSTS_SUCCESS) {

				if (m_Interfaces[ulInterfaceId].GetPortDataStatus () == DATA_STATUS_ENABLED) {
					XTMRT_TOGGLE_PORT_DATA_STATUS_CHANGE Tpdsc;
					memset (&Tpdsc, 0, sizeof (Tpdsc)) ;
					Tpdsc.ulPortId         = ulInterfaceId ;
					Tpdsc.ulPortDataStatus = XTMRT_CMD_PORT_DATA_STATUS_DISABLED ;

					(*m_pfnXtmrtReq) (NULL, XTMRT_CMD_TOGGLE_PORT_DATA_STATUS_CHANGE, &Tpdsc) ;
					m_Interfaces[ulInterfaceId].SetPortDataStatus (DATA_STATUS_DISABLED) ;
				}
			}
			else {

				if (m_Interfaces[ulInterfaceId].GetPortDataStatus () == DATA_STATUS_DISABLED) {
					XTMRT_TOGGLE_PORT_DATA_STATUS_CHANGE Tpdsc ;
					memset (&Tpdsc, 0, sizeof (Tpdsc)) ;
					Tpdsc.ulPortId         = ulInterfaceId ;
					Tpdsc.ulPortDataStatus = XTMRT_CMD_PORT_DATA_STATUS_ENABLED ;

					(*m_pfnXtmrtReq) (NULL, XTMRT_CMD_TOGGLE_PORT_DATA_STATUS_CHANGE, &Tpdsc) ;
					m_Interfaces[ulInterfaceId].SetPortDataStatus (DATA_STATUS_ENABLED) ;
				}
			}
		} /* if (pLi->ulLinkState == LINK_UP) */
	} /* for (ulInterfaceId) */
}

void XTM_PROCESSOR::BondDslMonitorTimerCb (UINT32 ulParm)
{
	XTM_PROCESSOR *pXtmProcessor = (XTM_PROCESSOR *) ulParm ;

	if (gulBondDslMonitorValid) {

		pXtmProcessor->PollLines () ;
		XtmOsStartTimer ((void *) pXtmProcessor->BondDslMonitorTimerCb, ulParm,
				           XTM_BOND_DSL_MONITOR_TIMEOUT_MS) ;
	}
}
#endif

/***************************************************************************
 * Function Name: GetConnCfg
 * Description  : Returns the connection configuration record for the
 *                specified connection address.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::GetConnCfg( PXTM_ADDR pConnAddr,
    PXTM_CONN_CFG pConnCfg )
{
    BCMXTM_STATUS bxStatus;
    XTM_CONNECTION *pConn ;
    
    UpdateConnAddrForBonding ( pConnAddr, XTM_ADD ) ;

    pConn = m_ConnTable.Get( pConnAddr );

    if( pConn )
    {
        UINT32 ulPortMask = pConnAddr->u.Conn.ulPortMask ;

        pConn->GetCfg( pConnCfg );

        /* The operational status of the connection is determined by checking
         * the link status.  A connection can use more than one link.  In order
         * for the operational status of the connection to be "up", atleast one
         * of the links it uses must be "up".
         * If all the links are "down", the connection will be operationally
         * down.
         */
        pConnCfg->ulOperStatus = OPRSTS_DOWN;
        for( UINT32 i = PHY_PORTID_0; i < MAX_INTERFACES; i++ )
        {
            if( (ulPortMask & PORT_TO_PORTID(i)) == PORT_TO_PORTID(i) )
            {
                PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[i].GetLinkInfo();
                if( pLi->ulLinkState == LINK_UP &&
                    pLi->ulLinkTrafficType == pConnAddr->ulTrafficType )
                {
                    /* At least one link is UP. */
                    pConnCfg->ulOperStatus = OPRSTS_UP;
                    break;
                }
            }
        }

        bxStatus = XTMSTS_SUCCESS;
    }
    else
        bxStatus = XTMSTS_NOT_FOUND;

    UpdateConnAddrForBonding ( pConnAddr, XTM_REMOVE ) ;

    return( bxStatus );
} /* GetConnCfg */


/***************************************************************************
 * Function Name: SetConnCfg
 * Description  : Sets the connection configuration record for the specified
 *                connection address.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::SetConnCfg( PXTM_ADDR pConnAddr,
    PXTM_CONN_CFG pConnCfg )
{
    BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
    XTM_CONNECTION *pConn ;

    /* Make a port mask taking into account of bonding as well */
    UpdateConnAddrForBonding ( pConnAddr, XTM_ADD ) ;

    /* Make a port mask for xmit params of the conncfg taking into account of bonding as well */
    if (pConnCfg)
      UpdateTransmitQParamsForBonding ( pConnCfg->TransmitQParms, 
                                        pConnCfg->ulTransmitQParmsSize, XTM_ADD) ;
    pConn = m_ConnTable.Get( pConnAddr );

    if( pConn == NULL )
    {
        /* The connection does not exist. Create it and add it to the table. */
        if( (pConn = new XTM_CONNECTION(m_pfnXtmrtReq, &m_ConnTable,
            m_ulConnSem, m_pSoftSar, m_ulRxVpiVciCamShadow)) != NULL )
        {
            pConn->SetTdt(m_pTrafficDescrEntries, m_ulNumTrafficDescrEntries);
            pConn->SetAddr( pConnAddr );
            if( (bxStatus = m_ConnTable.Add( pConn )) != XTMSTS_SUCCESS )
            {
                delete pConn;
                pConn = NULL;
            }
        }
        else
            bxStatus = XTMSTS_ALLOC_ERROR;
    }

    if( pConn )
    {
        bxStatus = pConn->SetCfg( pConnCfg );
        if( pConnCfg == NULL )
        {
            /* A NULL configuration pointer means to remove the connection.
             * However, an active connection (and its configuration will stay
             * active until its network device instance is deleted.
             */
            if( !pConn->Connected() )
            {
                m_ConnTable.Remove( pConn );
                delete pConn;
            }
        }
    }

    if (pConnCfg)
      UpdateTransmitQParamsForBonding ( pConnCfg->TransmitQParms, 
                                        pConnCfg->ulTransmitQParmsSize, XTM_REMOVE) ;

    UpdateConnAddrForBonding ( pConnAddr, XTM_REMOVE ) ;
    return( bxStatus );
} /* SetConnCfg */


/***************************************************************************
 * Function Name: GetConnAddrs
 * Description  : Returns an array of all configured connection addresses.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::GetConnAddrs( PXTM_ADDR pConnAddrs,
    UINT32 *pulNumConns )
{
    BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
    UINT32 ulInNum = *pulNumConns;
    UINT32 ulOutNum = 0;
    UINT32 i = 0;
    XTM_CONNECTION *pConn;
    XTM_ADDR Addr;

    while( (pConn = m_ConnTable.Enum( &i )) != NULL )
    {
        if( ulOutNum < ulInNum )
        {
            pConn->GetAddr( &Addr );
            UpdateConnAddrForBonding ( &Addr, XTM_REMOVE ) ;
            memcpy(&pConnAddrs[ulOutNum++], &Addr, sizeof(Addr));
        }
        else
        {
            bxStatus = XTMSTS_PARAMETER_ERROR;
            ulOutNum++;
        }
    }

    *pulNumConns = ulOutNum;
    return( bxStatus );
} /* GetConnAddrs */


/***************************************************************************
 * Function Name: GetInterfaceStatistics
 * Description  : Returns the interface statistics for the specified port.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::GetInterfaceStatistics( UINT32 ulPortId,
    PXTM_INTERFACE_STATS pIntfStats, UINT32 ulReset )
{
    BCMXTM_STATUS bxStatus;
    UINT32 ulPort = PORTID_TO_PORT(ulPortId);
    UINT32 ulBondingPort ;

    memset (pIntfStats, 0, sizeof (XTM_INTERFACE_STATS)) ;

    if( ulPort < MAX_INTERFACES ) {
       bxStatus = m_Interfaces[ulPort].GetStats( pIntfStats, ulReset );
       ulBondingPort = m_Interfaces[ulPort].GetBondingPortNum () ;
       if (ulBondingPort != MAX_INTERFACES)
          bxStatus = m_Interfaces[ulBondingPort].GetStats( pIntfStats, ulReset );
    }
    else
        bxStatus = XTMSTS_PARAMETER_ERROR;

    return( bxStatus );
} /* GetInterfaceStatistics */

/***************************************************************************
 * Function Name: SetInterfaceLinkInfo (6368, 6362, 6328)
 * Description  : Called when an ADSL/VDSL connection has come up or gone down.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::SetInterfaceLinkInfo( UINT32 ulPortId,
    PXTM_INTERFACE_LINK_INFO pLinkInfo )
{
	BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
	UINT32 ulPhysPort = PORTID_TO_PORT(ulPortId);
	UINT32 ulLogicalPort = PORT_PHYS_TO_LOG(ulPhysPort);
	UINT32 ulBondingPort;
	UINT32 ulTrafficType = (pLinkInfo->ulLinkTrafficType==TRAFFIC_TYPE_PTM_RAW)
		? TRAFFIC_TYPE_PTM : pLinkInfo->ulLinkTrafficType;
	PXTM_INTERFACE_LINK_INFO pOtherLinkInfo = NULL ;
	PXTM_INTERFACE_LINK_INFO pCurrLinkInfo ;

	XtmOsPrintf ("bcmxtmcfg: SetInterfaceLinkInfo Entry PortId %d T-%d\n", ulPortId, XtmOsTickGet()) ;

/* this was a workaround for Adtran bonding DSLAMS */
#if defined(AEI_VDSL_CUSTOMER_NCS)
   if (
#if defined(AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH)
       (m_InitParms.bondConfig.sConfig.bondAutoDetect != BC_BONDING_AUTODETECT_ENABLE)  && 
#endif
       (m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) &&
            (ulTrafficType == TRAFFIC_TYPE_PTM)) {
               /* To support some DSLAMs which dont support G.994 handshake
                * information for bonding.
                */
             ulTrafficType = pLinkInfo->ulLinkTrafficType = TRAFFIC_TYPE_PTM_BONDED ;
   }
#endif
   if ((m_InitParms.bondConfig.sConfig.atmBond == BC_ATM_BONDING_ENABLE) &&
	    (ulTrafficType == TRAFFIC_TYPE_ATM)) {
		/* To support legacy DSLAMs which dont support G.994 handshake
		 * information for bonding.
		 */
		ulTrafficType = pLinkInfo->ulLinkTrafficType = TRAFFIC_TYPE_ATM_BONDED ;
	}

	XtmOsPrintf ("bcmxtmcfg: XTM Link Information, port = %d, State = %s, Service Support = %s \n", 
			ulLogicalPort, 
			((pLinkInfo->ulLinkState == LINK_UP) ? "UP" : "DOWN"),
			((ulTrafficType == TRAFFIC_TYPE_ATM) ? "ATM" :
			 ((ulTrafficType == TRAFFIC_TYPE_PTM) ? "PTM" :
			  ((ulTrafficType == TRAFFIC_TYPE_PTM_BONDED) ? "PTM_BONDED" :
				((ulTrafficType == TRAFFIC_TYPE_ATM_BONDED) ? "ATM_BONDED" : "RAW"))))) ;

#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
	if (ulTrafficType == TRAFFIC_TYPE_ATM || ulTrafficType == TRAFFIC_TYPE_ATM_BONDED)
	{
		if (m_pAsmHandler) {
			m_pAsmHandler->m_linkup = (pLinkInfo->ulLinkState == LINK_UP) ;
			m_pAsmHandler->m_lastAsmTimestamp = XtmOsLinuxUptime();
		} 
	}
#endif
	pCurrLinkInfo = m_Interfaces[ulLogicalPort].GetLinkInfo() ;

   if ((pCurrLinkInfo) && (pCurrLinkInfo->ulLinkState == pLinkInfo->ulLinkState)) /* duplicate */ {
	   XtmOsPrintf ("bcmxtmcfg: SetInterfaceLinkInfo Exit-1 PortId %d T-%d\n", ulPortId, XtmOsTickGet()) ;
      return (bxStatus) ;
   }

	/* Reinitialize SAR based on the line traffic type.
	 * We support Bonding modes (PTM-ATM) change on the fly.
	 * Bonding to Normal mode requires reboot, stipulated by the Configuration
	 * stack on the system.
	 **/
	if (pLinkInfo->ulLinkState == LINK_UP) {
      if (CheckTrafficCompatibility (ulTrafficType) != XTMSTS_SUCCESS) {
         XtmOsPrintf ("\nbcmxtmcfg: Traffic Type incompatibility within BONDING & NON-BONDING between"
               " CO & CPE. Rebooting is necessary to match the traffic types.!!!! \n") ;
#ifdef AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH
	if (m_InitParms.bondConfig.sConfig.bondAutoDetect == BC_BONDING_AUTODETECT_ENABLE)
         XtmOsSendSysEvent (XTM_EVT_TRAFFIC_TYPE_MISMATCH) ;
#endif
         return (bxStatus) ;
      }
		ReInitialize (ulLogicalPort, ulTrafficType);
   }

	switch( pLinkInfo->ulLinkState )
	{
		case LINK_UP:
		case LINK_DOWN:

			ulBondingPort = m_Interfaces[ulLogicalPort].GetBondingPortNum () ;

			if (pLinkInfo->ulLinkState == LINK_DOWN) {
				pLinkInfo->ulLinkUsRate = pLinkInfo->ulLinkDsRate = 0 ;
			}

			if (ulBondingPort != MAX_INTERFACES)
				pOtherLinkInfo = m_Interfaces[ulBondingPort].GetLinkInfo() ;

         /* In case of normal mode, this will default to 0, but will have the
          * effect in bonded mode.
          */
			{
				const UINT32 ulUsPerSec = 1000000;
				const UINT32 ul100NsPerUs = 10;
				const UINT32 ulBitsPerCell = (53 * 8);
				const UINT32 ulBitsPerCodeWord = (65 * 8);

				UINT32 ulBits = ((ulTrafficType & TRAFFIC_TYPE_ATM_MASK) == TRAFFIC_TYPE_ATM)
					? ulBitsPerCell : ulBitsPerCodeWord;
            UINT32 ulLinkRate100Ns = (pOtherLinkInfo) ? 
               (pLinkInfo->ulLinkUsRate + pOtherLinkInfo->ulLinkUsRate)/ ul100NsPerUs :
               pLinkInfo->ulLinkUsRate/ul100NsPerUs ;
            UINT32 ulSlr ;

            ulSlr = (ulLinkRate100Ns != 0) ? ((ulUsPerSec * ulBits) / ulLinkRate100Ns) : 0 ;

				/* The shaper interval timer (SIT) can be set to a fix value rather than a
				 * calculated value based on the link rate. In ATM mode, with SIT value
				 * of 2, we will be able to specify minimum peak cell rate of 32kbps and
				 * it will give us very good accuracy when computing PCR for cell rates
				 * close to line rate.
				 * 
				 * For PTM mode, uses the "fast mode" setting.
				 */
				const UINT32 ulSit = 2;

				XTM_CONNECTION::SetSitUt( ulSit );

            if( pLinkInfo->ulLinkState == LINK_UP )
            {
				XP_REGS->ulTxLineRateTimer = TXLRT_EN |
					(ulSlr << TXLRT_COUNT_VALUE_SHIFT);

				/* Set SIT value.  For PTM mode, use "fast mode". */
				UINT32 ulTxSchedCfg = XP_REGS->ulTxSchedCfg & ~(TXSCH_SIT_COUNT_EN |
						TXSCH_SIT_COUNT_VALUE_MASK | TXSCHP_FAST_SCHED | TXSCH_SOFWT_PRIORITY_EN |
						TXSCHA_ALT_SHAPER_MODE);
				if( (ulTrafficType & TRAFFIC_TYPE_ATM_MASK) == TRAFFIC_TYPE_ATM )
				{
					ulTxSchedCfg |= TXSCH_SIT_COUNT_EN | TXSCHA_ALT_SHAPER_MODE |
						(ulSit<< TXSCH_SIT_COUNT_VALUE_SHIFT);
				}
				else
					ulTxSchedCfg |= TXSCHP_FAST_SCHED | TXSCH_SOFWT_PRIORITY_EN;
				XP_REGS->ulTxSchedCfg = ulTxSchedCfg;

				UINT32 ulSwitchPktTxChan;
				if( (ulTrafficType & TRAFFIC_TYPE_ATM_MASK) == TRAFFIC_TYPE_ATM )
				{
					XP_REGS->ulTxSarCfg &= ~TXSAR_MODE_PTM;
					XP_REGS->ulRxSarCfg &= ~RXSAR_MODE_PTM;
					XP_REGS->ulTxSarCfg |= TXSAR_MODE_ATM;
					XP_REGS->ulRxSarCfg |= RXSAR_MODE_ATM;
					ulSwitchPktTxChan = SPB_ATM_CHANNEL;
				}
				else
				{
					XP_REGS->ulTxSarCfg &= ~TXSAR_MODE_ATM;
					XP_REGS->ulRxSarCfg &= ~RXSAR_MODE_ATM;
					XP_REGS->ulTxSarCfg |= TXSAR_MODE_PTM;
					XP_REGS->ulRxSarCfg |= RXSAR_MODE_PTM;
					ulSwitchPktTxChan = SPB_PTM_CHANNEL;
				}

#if defined(CONFIG_BCM_PKTCMF_MODULE) || defined(CONFIG_BCM_PKTCMF)
				/* Configure switch packet bus. */
				const UINT32 ulMaxPktCnt = 3;
				const UINT32 ulMaxPktSize = 16;
				const UINT32 ulSrcId = 1;
				const UINT32 ulRxChan = 3;

				XP_REGS->ulSwitchPktCfg = ulMaxPktCnt |
					(ulMaxPktSize << SWP_MAX_PKT_SIZE_SHIFT) |
					(ulSrcId << SWP_SRC_ID_SHIFT) |
					(ulRxChan << SWP_RX_CHAN_SHIFT) |
					(ulSwitchPktTxChan << SWP_TX_CHAN_SHIFT) |
#ifdef AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH
					SWP_RX_EN;  /* don't enable switch packet bus tx
                            * so that tx ch 15 (ATM) and ch 7 (PTM)
                            * can be used for normal transmission.
                            */  
#else
                                        SWP_TX_EN | SWP_RX_EN;
#endif

				XP_REGS->ulSwitchPktTxCtrl = (XCT_AAL5 << SWP_CT_VALUE_SHIFT) |
					(0 << SWP_FSTAT_CFG_VALUE_SHIFT) | SWP_MUX_MODE_VALUE |
					SWP_CT_MASK_MASK | SWP_FSTAT_CFG_MASK_MASK | SWP_MUX_MODE_MASK;

				XP_REGS->ulSwitchPktRxCtrl = SWP_CRC32 | SWP_MUX_CRC32;

				XP_REGS->ulPktModCtrl = PKTM_RXQ_EN_MASK | PKTM_EN;
#endif

#if defined(CONFIG_BCM96368)
				/* Set default Match IDs in Packet CMF Rx Filter block. */
				*(UINT32 *) (PKTCMF_OFFSET_ENGINE_SAR + PKTCMF_OFFSET_RXFILT +
						RXFILT_REG_MATCH0_DEF_ID) = 0x03020100;
				*(UINT32 *) (PKTCMF_OFFSET_ENGINE_SAR + PKTCMF_OFFSET_RXFILT +
						RXFILT_REG_MATCH1_DEF_ID) = 0x07060504;
				*(UINT32 *) (PKTCMF_OFFSET_ENGINE_SAR + PKTCMF_OFFSET_RXFILT +
						RXFILT_REG_MATCH2_DEF_ID) = 0x0b0a0908;
				*(UINT32 *) (PKTCMF_OFFSET_ENGINE_SAR + PKTCMF_OFFSET_RXFILT +
						RXFILT_REG_MATCH3_DEF_ID) = 0x0f0e0d0c;
#endif

#if defined(CONFIG_BCM_PKTCMF_MODULE) || defined(CONFIG_BCM_PKTCMF)
				{
					int ulHwFwdTxChannel = 0;
					/*
					 * Pkts to ulHwFwdTxChannel are HW fwded to ulSwitchPktTxChan.
					 * All other packets, fwded via (MIPS assisted) fast handler.
					 * Default: ulHwFwdTxChannel=0
					 * Web gui based setting of ulHwFwdTxChannel (PVC based): TBD?
					 *
					 * pktCmfSarConfig
					 * - SAR CMF RxFilt and Modification Blocks RESET.
					 * - SAR CMF Default VCID (DestQ, MatchTag) tables INITIALIZED.
					 * - SAR CMF requests Switch driver to configure Switch SAR Port
					 *   - Switch SAR Port enabled for downstream (Switch RX)
					 *   - Switch SAR Port disabled for upstream (Switch TX)
					 *   - Learning disabled on Switch SAR Port.
					 *
					 * TBD: Do we need an explicit mechanism to disable SAR CMF
					 *      on Link Down?
					 */
                pktCmfSarConfig( ulHwFwdTxChannel, ulTrafficType );
				}
#endif
				//*(UINT32 *) (PKTCMF_OFFSET_ENGINE_SAR + PKTCMF_OFFSET_RXFILT +
				//RXFILT_REG_VCID0_QID) = 0x00000004 ;  
				/* Needed for multiple DMAs each per vcid. */
			}
			else
			{
				if (!pOtherLinkInfo ||
						((pOtherLinkInfo->ulLinkState == LINK_DOWN))) {

					XP_REGS->ulTxLineRateTimer = 0;

#if defined(CONFIG_BCM_PKTCMF_MODULE) || defined(CONFIG_BCM_PKTCMF)
					pktCmfSarAbort();   /* Flushes Switch CMF entries that redirect to SAR */
#endif

					ReInitialize (ulLogicalPort, ulTrafficType);
				}
               else
                  XP_REGS->ulTxLineRateTimer = TXLRT_EN | (ulSlr << TXLRT_COUNT_VALUE_SHIFT) ;
            }
			}

			if( ulPhysPort < MAX_INTERFACES )
			{
				UINT32 i;
				UINT32 ulPortMask, ulLinkMask;
				XTM_ADDR Addr;

				/* Create a "link up" bit mask of connected interfaces. */
				for( i = PHY_PORTID_0, ulLinkMask = 0; i < MAX_INTERFACES; i++ )
				{
					PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[i].GetLinkInfo();
					if( pLi->ulLinkState == LINK_UP )
						ulLinkMask |= PORT_TO_PORTID(i);
				}

				/* If link down, subtract the port id of the interface that just
				 * changed.  If link up, add the port id of the interface.
				 */
				if( pLinkInfo->ulLinkState == LINK_DOWN )
					ulLinkMask &= ~ulPortId;
				else
					ulLinkMask |= ulPortId;

				/* Call connections that are affected by the link change. A connection
				 * is affected if it is connected and none of its interfaces are
				 * currently up or it is not connected and atleast one of its
				 * interfaces is currently up.
				 * In addition, if the link status changes either way as long as
				 * the link is part of the port mask of the connection.
				 */
				i = 0;
				XTM_CONNECTION *pConn;

                while( (pConn = m_ConnTable.Enum( &i )) != NULL )
                {
                    UINT32 ulDoConnSetLinkInfo = 0;

                    pConn->GetAddr( &Addr );
                    ulPortMask = Addr.u.Conn.ulPortMask ;

			        if( ulBondingPort != MAX_INTERFACES )
                        ulDoConnSetLinkInfo = (ulPortId & ulPortMask) ? 1 : 0;
                    else
                        ulDoConnSetLinkInfo =
                            (((ulPortMask & ulLinkMask) == 0 &&
                              pConn->Connected()) ||
                             (ulTrafficType == Addr.ulTrafficType &&
                              (ulPortMask & ulLinkMask) == ulPortMask &&
                              !pConn->Connected())) ? 1 : 0;

                    if (ulDoConnSetLinkInfo)
                    {
                  bxStatus = pConn->SetLinkInfo( (ulLinkMask & ulPortMask), ulLogicalPort,
                        pLinkInfo, ulBondingPort, pOtherLinkInfo );

                        if( bxStatus != XTMSTS_SUCCESS )
                            break;
                    }
                }

				/* Call the interface that there is a link change. */
				CheckAndSetIfLinkInfo (ulLogicalPort, pLinkInfo, ulBondingPort,
						                 pOtherLinkInfo) ;

#if defined(CONFIG_BCM96368)
#ifndef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
   			if (m_InitParms.bondConfig.sConfig.atmBond == BC_ATM_BONDING_ENABLE)
#endif
#ifdef AEI_VDSL_CUSTOMER_NCS
				if (m_pAsmHandler)	
#endif
					m_pAsmHandler->LinkStateNotify(ulLogicalPort) ;
#endif

				if( pLinkInfo->ulLinkState == LINK_DOWN )
				{
					/* Call bcmxtmrt driver with NULL device context to stop the
					 * receive DMA.
					 */
					(*m_pfnXtmrtReq) (NULL, XTMRT_CMD_LINK_STATUS_CHANGED, 
											(char *) ulPortId);
				}
			}
			else
				bxStatus = XTMSTS_PARAMETER_ERROR;
			break;

		case LINK_START_TEQ_DATA:
			XP_REGS->ulRxAtmCfg[ulPhysPort] = RX_PORT_EN |
				RXA_VCI_MASK | RXA_VC_BIT_MASK;
			XP_REGS->ulRxUtopiaCfg &= ~RXUTO_TEQ_PORT_MASK;
			XP_REGS->ulRxUtopiaCfg |= (ulPhysPort << RXUTO_TEQ_PORT_SHIFT) |
				(1 << ulPhysPort);
			XP_REGS->ulRxVpiVciCam[(TEQ_DATA_VCID * 2) + 1] =
				RXCAM_CT_TEQ | (TEQ_DATA_VCID << RXCAM_VCID_SHIFT);
			XP_REGS->ulRxVpiVciCam[TEQ_DATA_VCID * 2] =
				ulPhysPort | RXCAM_TEQ_CELL | RXCAM_VALID;
#if defined(CONFIG_BCM_PKTCMF_MODULE) || defined(CONFIG_BCM_PKTCMF)
			pktCmfDslDiags(TEQ_DATA_VCID /* | (TBD switch port << 16) */);
#endif
			break;

		case LINK_STOP_TEQ_DATA:
			XP_REGS->ulRxAtmCfg[ulPhysPort] = 0;
			XP_REGS->ulRxUtopiaCfg &= ~(RXUTO_TEQ_PORT_MASK |
					(1 << ulPhysPort));
			XP_REGS->ulRxUtopiaCfg |= (XP_MAX_PORTS << RXUTO_TEQ_PORT_SHIFT);
			XP_REGS->ulRxVpiVciCam[(TEQ_DATA_VCID * 2) + 1] = 0;
			XP_REGS->ulRxVpiVciCam[TEQ_DATA_VCID * 2] = 0;
			break;
	}

	return( bxStatus );
} /* SetInterfaceLinkInfo */


/***************************************************************************
 * Function Name: SendOamCell
 * Description  : Sends an OAM loopback cell.  For request cells, it waits for
 *                the OAM loopback response.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::SendOamCell( PXTM_ADDR pConnAddr,
    PXTM_OAM_CELL_INFO pOamCellInfo )
{
    BCMXTM_STATUS bxStatus = XTMSTS_STATE_ERROR;
    UINT32 ulPort = PORTID_TO_PORT(pConnAddr->u.Vcc.ulPortMask);
    PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[ulPort].GetLinkInfo();

    if( ulPort < MAX_INTERFACES && pLi->ulLinkState == LINK_UP )
        bxStatus = m_OamHandler.SendCell(pConnAddr, pOamCellInfo, &m_ConnTable);

    return( bxStatus );
} /* SendOamCell */


/***************************************************************************
 * Function Name: CreateNetworkDevice
 * Description  : Call the bcmxtmrt driver to create a network device instance.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::CreateNetworkDevice( PXTM_ADDR pConnAddr,
    char *pszNetworkDeviceName )
{
    BCMXTM_STATUS bxStatus;
    XTM_CONNECTION *pConn ;
    XTM_INTERFACE_LINK_INFO Li ;
    PXTM_INTERFACE_LINK_INFO pOtherLi = NULL ;
    UINT32 ulPort, ulBondingPort = MAX_INTERFACES;

    UpdateConnAddrForBonding ( pConnAddr, XTM_ADD ) ;
    pConn = m_ConnTable.Get( pConnAddr );

    if( pConn )
    {
        pConn->CreateNetworkDevice( pszNetworkDeviceName );

        /* Call connection to indicate the link is up where appropriate. */
        memset (&Li, 0, sizeof (XTM_INTERFACE_LINK_INFO)) ;
        UINT32 ulPortMask = pConnAddr->u.Conn.ulPortMask ;
        UINT32 ulLinkMask = 0 ;
        for( UINT32 i = PHY_PORTID_0; i < MAX_INTERFACES; i++ )
        {
            ulPort = i ;
            if( (ulPortMask & PORT_TO_PORTID(i)) == PORT_TO_PORTID(i) )
            {
                PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[i].GetLinkInfo();
                ulBondingPort = m_Interfaces[i].GetBondingPortNum() ;
                if( pLi->ulLinkState == LINK_UP &&
                    pLi->ulLinkTrafficType == pConnAddr->ulTrafficType )
                {
                   ulLinkMask |= PORT_TO_PORTID(i) ;
                   Li.ulLinkUsRate += pLi->ulLinkUsRate ;
                   Li.ulLinkDsRate += pLi->ulLinkDsRate ;
                   Li.ulLinkTrafficType = pLi->ulLinkTrafficType ;

                   if (ulBondingPort != MAX_INTERFACES) {
                     pOtherLi = m_Interfaces[ulBondingPort].GetLinkInfo();
                     if (pOtherLi->ulLinkState == LINK_UP)
                        ulLinkMask |= PORT_TO_PORTID(ulBondingPort) ;
                     break ;   /* More than 2 ports in bonding not supported, so exiting */
                   }
                }
            }
        }

        if (ulLinkMask != 0) {
           Li.ulLinkState = LINK_UP ;
           pConn->SetLinkInfo( ulLinkMask, ulPort, &Li, ulBondingPort, pOtherLi ) ;
        }

        bxStatus = XTMSTS_SUCCESS;
    }
    else
        bxStatus = XTMSTS_NOT_FOUND;

    UpdateConnAddrForBonding ( pConnAddr, XTM_REMOVE ) ;

    /* Compile Out for Loopback */
    UINT32 ulLinkState = LINK_UP;
    UINT32 ulLpPort = PORTID_TO_PORT(pConnAddr->u.Vcc.ulPortMask);

    if( ulLpPort < MAX_INTERFACES )
    {
        PXTM_INTERFACE_LINK_INFO pLi = m_Interfaces[ulLpPort].GetLinkInfo();
        ulLinkState = pLi->ulLinkState;
    }

#ifndef PHY_LOOPBACK
    if(bxStatus == XTMSTS_SUCCESS && ulLinkState == LINK_DOWN &&
       (XP_REGS->ulTxUtopiaCfg & TXUTO_MODE_INT_EXT_MASK) == TXUTO_MODE_ALL_EXT)
#endif
    {
        XTM_INTERFACE_LINK_INFO Li;

        Li.ulLinkState = LINK_UP;
        Li.ulLinkUsRate = 54000000;
        Li.ulLinkDsRate = 99000000;
        Li.ulLinkTrafficType = pConnAddr->ulTrafficType ;

        XtmOsPrintf("bcmxtmcfg: Force link up, port=%lu, traffictype=%lu\n",
            ulLpPort, Li.ulLinkTrafficType);
        BcmXtm_SetInterfaceLinkInfo( pConnAddr->u.Vcc.ulPortMask, &Li );
#ifdef PHY_LOOPBACK
        if( m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
           XTM_INTERFACE_LINK_INFO Li;

           Li.ulLinkState = LINK_UP;
           Li.ulLinkUsRate = 54000000;
           Li.ulLinkDsRate = 99000000;
           Li.ulLinkTrafficType = pConnAddr->ulTrafficType ;

           XtmOsPrintf("bcmxtmcfg: Force link up, port=%lu, traffictype=%lu\n",
                 ulLpPort, Li.ulLinkTrafficType);
           BcmXtm_SetInterfaceLinkInfo( 0x2, &Li );
        }
#endif
    }

    return( bxStatus );
} /* CreateNetworkDevice */


/***************************************************************************
 * Function Name: DeleteNetworkDevice
 * Description  : Remove a bcmxtmrt network device.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::DeleteNetworkDevice( PXTM_ADDR pConnAddr )
{
    BCMXTM_STATUS bxStatus;
    XTM_CONNECTION *pConn ;

    UpdateConnAddrForBonding ( pConnAddr, XTM_ADD ) ;

    pConn = m_ConnTable.Get( pConnAddr );

    if( pConn )
    {
        pConn->DeleteNetworkDevice();

        /* The connection's configuration was previously removed. */
        if( pConn->RemovePending() )
        {
            m_ConnTable.Remove( pConn );
            delete pConn;
        }
        bxStatus = XTMSTS_SUCCESS;
    }
    else
        bxStatus = XTMSTS_NOT_FOUND;

    UpdateConnAddrForBonding ( pConnAddr, XTM_REMOVE ) ;
    return( bxStatus );
} /* DeleteNetworkDevice */

/***************************************************************************
 * Function Name: CheckTrafficCompatibility (6368/62/28)
 * Description  : Called for 6368 as currently we cannot support on the fly
 *                conversion from non-bonded to bonded and vice versa
 *                traffic types. This function enforces this condition.
 * Returns      : None.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::CheckTrafficCompatibility ( UINT32 ulTrafficType )
{
	BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS ;

   switch (ulTrafficType) {
      case TRAFFIC_TYPE_PTM_BONDED     :
         if ( m_InitParms.bondConfig.sConfig.ptmBond != BC_PTM_BONDING_ENABLE)
            bxStatus = XTMSTS_PROTO_ERROR ;
         break ;
      case TRAFFIC_TYPE_ATM_BONDED     :
         if ( m_InitParms.bondConfig.sConfig.atmBond != BC_ATM_BONDING_ENABLE)
            bxStatus = XTMSTS_PROTO_ERROR ;
         break ;
      case TRAFFIC_TYPE_PTM            :
         if ( m_InitParms.bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
            bxStatus = XTMSTS_PROTO_ERROR ;
         break ;
      case TRAFFIC_TYPE_ATM            :
         if ( m_InitParms.bondConfig.sConfig.atmBond == BC_PTM_BONDING_ENABLE)
            bxStatus = XTMSTS_PROTO_ERROR ;
         break ;
      default                          :
         break ;
   }

	return (bxStatus) ;
} /* CheckAndSetIfLinkInfo */


/***************************************************************************
 * Function Name: CheckAndSetIfLinkInfo (6368/62/28)
 * Description  : Called for 6368 as bonding requires rx side to be configured 
 *                intelligently based on the other bonding pair's activeness.
 *		            If bonding enabled, (only for 6368), set the rx port status only
 *			         f all the bonded ports are down, otherwise, do not disable any
 *			         ort. This is due to the SAR workaround we have in the phy for
 *			         ptm bonding that all traffic will always come on channel 0.
 *			         Also a case for atm bonding, where port 0 goes down, the
 *			         entire traffic stops as the scheduler gets disabled on the
 *			         port 0. We cannot disable the scheduler for atm bonding
 *			         operating port until all the ports are down.
 *			   
 * Returns      : None.
 ***************************************************************************/
BCMXTM_STATUS XTM_PROCESSOR::CheckAndSetIfLinkInfo ( UINT32 ulLogicalPort,
	 			PXTM_INTERFACE_LINK_INFO pLinkInfo, UINT32 ulBondingPort,
	 			PXTM_INTERFACE_LINK_INFO pOtherLinkInfo)
{
	BCMXTM_STATUS bxStatus ;

	if (ulBondingPort != MAX_INTERFACES) {
      if (pLinkInfo->ulLinkTrafficType != TRAFFIC_TYPE_ATM_BONDED) {

         if( pLinkInfo->ulLinkState == LINK_UP ) {


            if (ulLogicalPort == DS_PTMBOND_CHANNEL) {
            bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, 
                  XTM_CONFIGURE, XTM_CONFIGURE );
            }
            else {

               bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_NO_CONFIGURE,
                     XTM_CONFIGURE, XTM_CONFIGURE );

            /* Use the same link information (spoof for rx side config) */
            if (pOtherLinkInfo->ulLinkState == LINK_DOWN)
               bxStatus = m_Interfaces[ulBondingPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, 
                     XTM_NO_CONFIGURE, XTM_NO_CONFIGURE );
         }
         }
         else {
            /* Link down */
            bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, 
                  XTM_CONFIGURE, XTM_CONFIGURE );
            if ((ulLogicalPort == DS_PTMBOND_CHANNEL) && (pOtherLinkInfo->ulLinkState == LINK_UP)) {
               bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pOtherLinkInfo, XTM_CONFIGURE, 
                     XTM_NO_CONFIGURE, XTM_NO_CONFIGURE );
            }
         }
      }
      else {
         if( pLinkInfo->ulLinkState == LINK_UP ) {
            bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, XTM_CONFIGURE, 
                                          XTM_CONFIGURE );
         }
         else {
            /* Only for single latency bonding.
             * For dual latency bonding, we need to pair 0 & 2 for a group and
             * 1 & 3 for another group.
             * For single latency, 0 & 1 are paired. These are derived from
             * HW design/assumptions.
             */
            if ((ulLogicalPort == 0) && (pOtherLinkInfo->ulLinkState == LINK_UP)) {
               bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, XTM_CONFIGURE, 
                                          XTM_NO_CONFIGURE );
            }
            else {
               bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, XTM_CONFIGURE,
                                          XTM_CONFIGURE);
            }
			}
		}
	}
	else {
      bxStatus = m_Interfaces[ulLogicalPort].SetLinkInfo( pLinkInfo, XTM_CONFIGURE, XTM_CONFIGURE, XTM_CONFIGURE );
	}

	return (bxStatus) ;
} /* CheckAndSetIfLinkInfo */

#if defined(CONFIG_BCM96368)

/***************************************************************************
 * Function Name: CheckAndResetSAR (6368)
 * Description  : Called when all the available link(s) are DOWN to reset SAR
 *                block so that it is ready and accommodate differen traffic
 *                types when the available link(s) come UP.
 * Returns      : None.
 ***************************************************************************/
void XTM_PROCESSOR::CheckAndResetSAR ( UINT32 ulPortId,
	 PXTM_INTERFACE_LINK_INFO pLinkInfo )
{
	 UINT32 ulPhysPort = PORTID_TO_PORT(ulPortId);
	 UINT32 ulLogicalPort = PORT_PHYS_TO_LOG(ulPhysPort);
	 UINT32 ulBondingPort ;
	 PXTM_INTERFACE_LINK_INFO pOtherLinkInfo = NULL ;
         int i;

	 if( pLinkInfo->ulLinkState == LINK_DOWN )
	 {
		 ulBondingPort = m_Interfaces[ulLogicalPort].GetBondingPortNum () ;

		 if (ulBondingPort != MAX_INTERFACES) {
			 pOtherLinkInfo = m_Interfaces[ulBondingPort].GetLinkInfo() ;
		 }

		 if ((pOtherLinkInfo == NULL) ||
			  (pOtherLinkInfo->ulLinkState == LINK_DOWN)) {
			 PERF->softResetB &= ~SOFT_RST_SAR;
                         for (i=50 ; i >0 ; i--)
                         ;
			 PERF->softResetB |= SOFT_RST_SAR;
			 Initialize( NULL, 0, NULL );
		 }
	 }

	 return ;
} /* CheckAndResetSAR */
#else
void XTM_PROCESSOR::CheckAndResetSAR( UINT32 ulPortId,
    PXTM_INTERFACE_LINK_INFO pLinkInfo )
{
   int i;

    if( pLinkInfo->ulLinkState == LINK_DOWN )
    {
        /* Reset in order to clear internal buffers. */
        PERF->softResetB &= ~SOFT_RST_SAR;

        for (i=50 ; i >0 ; i--)
           ;

        PERF->softResetB |= SOFT_RST_SAR;
        Initialize( NULL, 0, NULL );
    }
}
#endif


/* Reinitializes the SAR with configuration. Used in debugging SAR issue(s)
 * with errorneous traffic in US/DS direction from/to phy.
 * Assumes, SAR with one/two ports, 1 PTM LOW priority interface with a
 * corresponding connection.
 * Number of Tx Qs can be any. (from pre-existing)
 */
BCMXTM_STATUS XTM_PROCESSOR::DebugReInitialize ()
{
   UINT32 i ;
   UINT32 ulPort = 0x0, ulOtherPort, ulOrigLinkState, ulOrigOtherLinkState ;
   PXTM_INTERFACE_LINK_INFO pLiPort, pLiOtherPort ;
   XTM_INTERFACE_LINK_INFO origLiPort, origLiOtherPort ;
   XTM_ADDR xtmAddr ;
   XTM_CONN_CFG origConnCfg ;
   BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS ;

   pLiPort      = m_Interfaces [ulPort].GetLinkInfo() ;

   if (pLiPort) {

      memcpy (&origLiPort, pLiPort, sizeof (XTM_INTERFACE_LINK_INFO)) ;
      ulOrigLinkState      = pLiPort->ulLinkState ;
      ulOtherPort = m_Interfaces [ulPort].GetBondingPortNum() ;
      if (ulOtherPort != MAX_INTERFACES) {
         pLiOtherPort = m_Interfaces [ulOtherPort].GetLinkInfo() ;
         memcpy (&origLiOtherPort, pLiOtherPort, sizeof (XTM_INTERFACE_LINK_INFO)) ;
         ulOrigOtherLinkState = pLiOtherPort->ulLinkState ;
      }
      else {
         pLiOtherPort = NULL ;
         ulOrigOtherLinkState = LINK_DOWN ;
      }
   }
   else {
      ulOrigLinkState = LINK_DOWN ;
      ulOtherPort = MAX_INTERFACES ;
      pLiOtherPort = NULL ;
      ulOrigOtherLinkState = LINK_DOWN ;
   }

   if (ulOrigLinkState == LINK_UP) {
      pLiPort->ulLinkState = LINK_DOWN ;
      pLiPort->ulLinkTrafficType = TRAFFIC_TYPE_PTM ;
      bxStatus = BcmXtm_SetInterfaceLinkInfo (PORT_TO_PORTID(ulPort), pLiPort) ;
   }

   if ((bxStatus == XTMSTS_SUCCESS) && (ulOrigOtherLinkState == LINK_UP)) {
      pLiOtherPort->ulLinkState = LINK_DOWN ;
      pLiOtherPort->ulLinkTrafficType = TRAFFIC_TYPE_PTM ;
      bxStatus = BcmXtm_SetInterfaceLinkInfo (PORT_TO_PORTID(ulOtherPort), pLiOtherPort) ;
   }

   xtmAddr.ulTrafficType =  TRAFFIC_TYPE_PTM ;
   xtmAddr.u.Flow.ulPortMask = PORT_PHY0_PATH0 ;
   xtmAddr.u.Flow.ulPtmPriority = PTM_PRI_LOW ;

   if (bxStatus == XTMSTS_SUCCESS)
      bxStatus = BcmXtm_DeleteNetworkDevice (&xtmAddr) ;

   if (bxStatus == XTMSTS_SUCCESS) {
      bxStatus = BcmXtm_GetConnCfg (&xtmAddr, &origConnCfg) ;
      if (bxStatus == XTMSTS_SUCCESS) 
         bxStatus = BcmXtm_SetConnCfg (&xtmAddr, NULL) ;  /* Remove the connection */
   }

   for (i=99999999; i > 0 ; i--)
      ;

   if ((bxStatus == XTMSTS_SUCCESS) && (ulOrigOtherLinkState == LINK_UP)) {

      pLiOtherPort->ulLinkState = LINK_UP ;
      pLiOtherPort->ulLinkTrafficType = TRAFFIC_TYPE_PTM_BONDED ;
      bxStatus = BcmXtm_SetInterfaceLinkInfo (PORT_TO_PORTID(ulOtherPort), &origLiOtherPort) ;

      if (bxStatus == XTMSTS_SUCCESS) 
         bxStatus = BcmXtm_SetConnCfg (&xtmAddr, &origConnCfg) ; /* ReConfigure the connection */

      if (bxStatus == XTMSTS_SUCCESS)
         bxStatus = BcmXtm_CreateNetworkDevice (&xtmAddr, (char *) "ptm0") ;
   }

   for (i=99999; i > 0 ; i--)
      ;

   if ((bxStatus == XTMSTS_SUCCESS) && (ulOrigLinkState == LINK_UP)) {

      pLiPort->ulLinkState = LINK_UP ;
      pLiPort->ulLinkTrafficType = TRAFFIC_TYPE_PTM_BONDED ;
      bxStatus = BcmXtm_SetInterfaceLinkInfo (PORT_TO_PORTID(ulPort), &origLiPort) ;

      if (ulOrigOtherLinkState != LINK_UP) {

         if (bxStatus == XTMSTS_SUCCESS) 
            bxStatus = BcmXtm_SetConnCfg (&xtmAddr, &origConnCfg) ; /* ReConfigure the connection */

         if (bxStatus == XTMSTS_SUCCESS)
            bxStatus = BcmXtm_CreateNetworkDevice (&xtmAddr, (char *) "ptm0") ;
      }
   }

   return (bxStatus) ;
}
