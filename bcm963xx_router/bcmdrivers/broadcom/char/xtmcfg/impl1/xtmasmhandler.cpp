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
 * File Name  : xtmasmhandler.cpp
 *
 * Description: This file contains the implementation for the XTM ASM handler
 *              class.  
 *              This class handles sending an ASM traffic for bonding.
 *              It also handles the ASMs being recieved for further processing.
 ***************************************************************************/

#include "xtmcfgimpl.h"

#if defined(CONFIG_BCM96368)
#define MapXtmOsPrintf				DummyXtmOsPrintf

/* Globals */
static UINT32 bondMgmtValid = 0;

#if 1
#define RX_ASM_PRINT_DEBUG    0xFFFFFFFF
#define TX_ASM_PRINT_DEBUG    0xFFFFFFFF
#else
#define RX_ASM_PRINT_DEBUG    0x1
#define TX_ASM_PRINT_DEBUG    0x1
#endif

#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
#define ASM_MAX_TIMEOUT  60 //seconds since last ASM receive
#endif

#ifdef AEI_VDSL_TOGGLE_DATAPUMP
#include "board.h"
#endif
/***************************************************************************
 * Function Name: XTM_ASM_HANDLER
 * Description  : Constructor for the XTM processor class.
 * Returns      : None.
 ***************************************************************************/
XTM_ASM_HANDLER::XTM_ASM_HANDLER( void )
{
	m_pfnXtmrtReq = NULL;
	m_ulRegistered = 0;
#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
	m_linkup = FALSE;
	m_lastAsmTimestamp = 0;
#endif
} /* XTM_ASM_HANDLER */


/***************************************************************************
 * Function Name: ~XTM_ASM_HANDLER
 * Description  : Destructor for the XTM processor class.
 * Returns      : None.
 ***************************************************************************/
XTM_ASM_HANDLER::~XTM_ASM_HANDLER( void )
{
    Uninitialize();
} /* ~XTM_ASM_HANDLER */

void XTM_ASM_HANDLER::resetBondingGroup ()
{
	int i ;

	MapXtmOsPrintf ("bcmxtmcfg: resetBondingGroup \n") ;
	m_BondMgmt.knownMapping = 0 ;
	m_BondMgmt.bondingId = 0 ;
	m_BondMgmt.asmLastRxId = 0 ;
	m_BondMgmt.numberOfLines = 0 ;
	m_BondMgmt.nextLineToPoll = MAX_BONDED_LINES ;
	m_BondMgmt.remoteRxState = 0 ;
	m_BondMgmt.remoteTxState = 0 ;
	m_BondMgmt.resetState = BND_IDLE ;
	m_BondMgmt.vt = XtmOsGetTimeStamp () ;
	m_BondMgmt.refLink = ~0 ; //* Currently no ref link */
	memset (&m_BondMgmt.asmCell, 0, sizeof (Asm)) ;
   m_BondMgmt.asmCell.msgLen = 0x28;
   m_BondMgmt.asmCell.rxAsmStatus[0] = 0xC0 ;

	for (i=0; i<MAX_BONDED_LINES; i++) {
		mappingReset (i, 0);
	}
}

void XTM_ASM_HANDLER::mappingReset(UINT32 ulLogicalPort, int timeout)
{
	LineMapping *mappingp = &m_BondMgmt.mapping[ulLogicalPort];
	int *remoteIdToLocalp = m_BondMgmt.remoteIdToLocal;
	Asm *asmp = &m_BondMgmt.asmCell;
	LineInfo *linep = &m_BondMgmt.lines[ulLogicalPort];
	UINT32 knownMapping = m_BondMgmt.knownMapping;
	UINT32 newKnownMapping;

	/* remove the line from set of known mapping */
	newKnownMapping = knownMapping & ~(1<<ulLogicalPort);
	m_BondMgmt.knownMapping = newKnownMapping;

	if ((newKnownMapping == 0) && (knownMapping != 0))
	{
		/* last line in the bonding => we reset complete bonding group */ 
		resetBondingGroup () ;
		return;
	}

	/* remote Tx cannot be active .... so remove it from the reoredering queue */

	MapXtmOsPrintf ("bcmxtmcfg: tx %d disabled \n", ulLogicalPort) ;
//	m_pInterfaces [ulLogicalPort].SetLinkDataStatus (DATA_STATUS_DISABLED) ;
	/* ToDo -  should we reset rx */

	/* if line already associated with a remote idx, reset it. */
	if (mappingp->localIdToRemoteId != -1) {

		UINT16 newLocalTx =  asmp->txLinkStatus[0];
		UINT16 newLocalRx =  asmp->rxLinkStatus[0];
		UINT32 shiftArg = 14-(mappingp->localIdToRemoteId<<1);

		remoteIdToLocalp[mappingp->localIdToRemoteId] = -1;
		newLocalRx = (newLocalRx & ~(LK_ST_MASK << shiftArg)) | (LK_ST_SHOULD_NOT_USE<< shiftArg);
		newLocalTx = (newLocalTx & ~(LK_ST_MASK << shiftArg)) | (LK_ST_SHOULD_NOT_USE<< shiftArg);
		linep->inUseRx = linep->inUseTx = 0;
		asmp->txLinkStatus[0] = newLocalTx;
		asmp->rxLinkStatus[0] = newLocalRx;
	}

	mappingp->localIdToRemoteId = -1;
	mappingp->requestedDelay = 0;
	mappingp->lastAsmTimestamp = 0;
	//mappingp->receivedAsm = 0;
	//mappingp->transmittedAsm = 0;
	mappingp->stateChanges = 0;
	mappingp->localStateChanges = 0;
}

void XTM_ASM_HANDLER::LineInfo_init(LineInfo *objp)
{
  objp->inUseRx = objp->inUseTx = 0;
  objp->asmReady = 0;
  objp->lastAsmElapsed = 0;
  objp->usecTxAsm = ~0;  
}

int XTM_ASM_HANDLER::IsElapsedSinceLastAsm (LineMapping *mappingp)
{
  return (XtmOsGetTimeStamp () - mappingp->lastAsmTimestamp) ;
}

void XTM_ASM_HANDLER::PollLines ()
{
	PXTM_INTERFACE_LINK_INFO pLi ;

	int localId =  m_BondMgmt.nextLineToPoll ;

#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
	/* cover case where DSLAM not bonding but CPE set to bonding
	 * if pass say 1 minute after link in ATM and not received ASM, then can assume it is not bonding
	 */
        unsigned long timediff = XtmOsLinuxUptime() - m_lastAsmTimestamp;
	if (m_BondCfg.sConfig.atmBond == BC_ATM_BONDING_ENABLE && m_linkup==TRUE && 
		(timediff > ASM_MAX_TIMEOUT)
	) 
	{
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
		/* do optimization, if no asm cells, go ahead to change to correct single line datapump also */
		unsigned char dslDatapump = 0;
		kerSysGetDslDatapump(&dslDatapump);
		if (dslDatapump !=2) {
			restoreDatapump(2); //set to single line magic number
			XtmOsPrintf ("@@@@@@@ Setting Supercombo Single line Datapump. @@@@@@@@@@\n");
		}
#endif

		XtmOsPrintf ("@@@@@@@ No ASM messages detected for more than %d seconds but ATM BONDING mode is enable, so switch mode.@@@@@@@@@@\n",ASM_MAX_TIMEOUT);
		m_BondCfg.sConfig.atmBond = BC_ATM_BONDING_DISABLE; //toggle it so don't send message again in short time before reboot
		XtmOsSendSysEvent (XTM_EVT_TRAFFIC_TYPE_MISMATCH) ;
	}
#endif

	if (localId != MAX_BONDED_LINES) {
		pLi = m_pInterfaces[localId].GetLinkInfo() ;

		if (pLi->ulLinkState == LINK_UP) {

#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
			/* cover case where detected bonding state from DSLAM but CPE not set to do bonding */
			if (m_BondCfg.sConfig.atmBond != BC_ATM_BONDING_ENABLE) {
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
				/* do optimization, if detect adsl bonding, go ahead to change to correct adsl bonding datapump also */
				unsigned char dslDatapump = 0;
				kerSysGetDslDatapump(&dslDatapump);
				if (dslDatapump !=1) {
					restoreDatapump(1); //set to adsl bonding datapump magic number
					XtmOsPrintf ("@@@@@@@ Setting ADSL Bonding Datapump. @@@@@@@@@@\n");
				}
#endif
				XtmOsPrintf ("@@@@@@@ ASM messages detected but ATM BONDING mode is disabled, so switch mode.@@@@@@@@@@\n");				
				XtmOsSendSysEvent (XTM_EVT_TRAFFIC_TYPE_MISMATCH);
	                }
#endif

			/* in case the CPE link id is known, we monitor if we still receive
				ASM.
				In case no ASM has been received for a certain amount of time,
				we reset the mapping (which will unregister from the reordering queue)
				*/

			if (m_BondMgmt.knownMapping &(1<<localId)) {

				if (IsElapsedSinceLastAsm(&m_BondMgmt.mapping[localId]) > ASM_DOWN_TIMEOUT) {

					m_BondMgmt.mapping[localId].localStateChanges++;
					/* reset bonding mapping */
					mappingReset (localId, 0);
				}
			}
		} /* if (pLi) */

		m_BondMgmt.nextLineToPoll++ ;

		if (m_BondMgmt.nextLineToPoll > MAX_BONDED_LINES)
			m_BondMgmt.nextLineToPoll = 0;
	} /* if (localId) */
}

/* return the tranmit ASM tx time */
void  XTM_ASM_HANDLER::LinkStateNotify (UINT32 ulLogicalPort)
{
	//UINT32    usecTx ;
	LineInfo  *objp ;
	PXTM_INTERFACE_LINK_INFO pLi = m_pInterfaces [ulLogicalPort].GetLinkInfo() ;
	UINT32 usRate = pLi->ulLinkUsRate ;
	UINT16 trainingState = pLi->ulLinkState ;

	MapXtmOsPrintf ("bcmxtmcfg: LinkStateNotify %d  us %d  ds %d \n",
					 ulLogicalPort, usRate, pLi->ulLinkDsRate) ;

	/* action to be done:
		=> need a pointer to the global ASM in order
		to change it if required (can be changed at any time)
	*/
	objp = &m_BondMgmt.lines [ulLogicalPort];

	if (trainingState == LINK_DOWN)
	{
		/* in case there is no showtime, reset everything related to ASM */
		objp->usecTxAsm = ~0;
		objp->asmReady = 0;
	   //XtmOsPrintf ("bcmxtmcfg: tx link %d disabled \n", ulLogicalPort) ;
		//m_pInterfaces[ulLogicalPort].SetLinkDataStatus (DATA_STATUS_DISABLED) ;
	}
	else
	{
		if (objp->usecTxAsm == (UINT32) ~0)
			objp->lastAsmElapsed = 0;
		objp->usecTxAsm = MAX(100000,(((8*1050*150*1000)/*slow down asm rate *40*/ )/usRate)*53);
		//usecTx = (53*8*1010*1000)/usRate ;
		//m_BondMgmt.timeInterval = MIN(usecTx, MAX_TIME_INTERVAL) ;
	   //XtmOsPrintf ("bcmxtmcfg: tx link %d enabled \n", ulLogicalPort) ;
		//m_pInterfaces[ulLogicalPort].SetLinkDataStatus (DATA_STATUS_ENABLED) ;
	}
}

void XTM_ASM_HANDLER::TxAsm ()
{
	int i;
	UINT8 *payload ;
	PXTM_INTERFACE_LINK_INFO pLi ;

	i = MAX_BONDED_LINES ;

	/* Search line to be used for transmit .... */

	while (i--)
	{
		m_BondMgmt.lines[i].lastAsmElapsed += m_BondMgmt.timeInterval;
		if (m_BondMgmt.lines[i].lastAsmElapsed > m_BondMgmt.lines[i].usecTxAsm)
		{
			m_BondMgmt.lines[i].asmReady = 1;
			m_BondMgmt.lines[i].lastAsmElapsed -= m_BondMgmt.lines[i].usecTxAsm;
		}

		if (m_BondMgmt.lines[i].asmReady) {

			pLi = m_pInterfaces[i].GetLinkInfo () ;
			if (
				 (pLi->ulLinkState == LINK_UP) && (pLi->ulLinkUsRate != 0)
					&&
				 (m_BondMgmt.mapping[i].localIdToRemoteId != -1)
#if 1
					&&
				 (m_BondMgmt.resetState != BND_IDLE)
#endif
				) {

				/* only send the ASM is the remote line Id is known */
				/* fill in the asm */

				m_BondMgmt.asmCell.asmId ++;
				m_BondMgmt.asmCell.txLinkId = m_BondMgmt.mapping[i].localIdToRemoteId;
				m_BondMgmt.asmCell.groupId = m_BondMgmt.bondingId;
				m_BondMgmt.asmCell.delay = 0; /* for timebeing do not yet request
															delay on the line (CPE will only do
															this for test purposes */
				m_BondMgmt.asmCell.appliedDelay = m_BondMgmt.mapping[i].requestedDelay;
				{
					UINT32 ct;
					UINT32 dt;

      			UINT8 tv1[TIMEOUT_VALUE_SIZE] ;
					XtmOsGetTimeOfDay(tv1,NULL);
					ct = XtmOsTimeInUsecs(tv1) ;
					dt = ct-m_BondMgmt.vt ;

					/* convert timestamp so that unit is 100usec */
					m_BondMgmt.asmCell.timestamp = dt/100;
				}

				/* CRC will be done by HW. */
#if 0
			//	m_BondMgmt.asmCell.crc = 0 ;
				/* compute the CRC */
				{
					UINT32 crc ;
					crc = getCrc32_rev32Bit((UINT32 *)&m_BondMgmt.asmCell, 44, CRC32_INIT_VALUE) ;
					crc = ~crc ;
					m_BondMgmt.asmCell.crc = TO_FROM_LE_32(crc) ;
				}
#else
					m_BondMgmt.asmCell.crc = 0 ;
#endif

				payload = (UINT8 *)&m_BondMgmt.asmCell ;

				if (!(m_BondMgmt.mapping[i].transmittedAsm % TX_ASM_PRINT_DEBUG))
					printNewAsm ((Asm*) payload, "Tx") ;

				if (SendAsmCell (i, payload) == XTMSTS_SUCCESS) {
					/* Increase transmitted cells counter */
					m_BondMgmt.mapping[i].transmittedAsm++;
				}

			} /* if remote mapping known */
			m_BondMgmt.lines[i].asmReady = 0;
		} /* if (asmready) */

	} /* (while i) */
}

void XTM_ASM_HANDLER::MgmtTimerCb (UINT32 ulParm)
{
	XTM_ASM_HANDLER *pAsmHandle = (XTM_ASM_HANDLER *) ulParm ;

	if (bondMgmtValid) {

		pAsmHandle->TxAsm () ;
		pAsmHandle->PollLines () ;
		XtmOsStartTimer ((void *) pAsmHandle->MgmtTimerCb, ulParm, XTM_BOND_MGMT_TIMEOUT_MS) ;
	}
}

/***************************************************************************
 * Function Name: SendCell
 * Description  : Sends an ASM cell.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_ASM_HANDLER::SendAsmCell ( UINT32 ulLogicalPort, UINT8 *pAsmData)
{
	UINT8       ucCircuitType ;
	UINT32 ulPhysPort = PORT_LOG_TO_PHYS(ulLogicalPort);
	BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
	XTM_CONNECTION *pConn = NULL;
	XTMRT_HANDLE hDev = INVALID_HANDLE;

	MapXtmOsPrintf ("SendAsmCell %d \n", ulLogicalPort) ;

	switch (ulPhysPort) {
		case PHY_PORTID_0		:
			ucCircuitType = CTYPE_ASM_P0 ;
			break ;
		case PHY_PORTID_1		:
			ucCircuitType = CTYPE_ASM_P1 ;
			break ;
		case PHY_PORTID_2		:
			ucCircuitType = CTYPE_ASM_P2 ;
			break ;
		case PHY_PORTID_3		:
			ucCircuitType = CTYPE_ASM_P3 ;
			break ;
		default					:
			XtmOsPrintf ("bcmxtmcfg: Tx'ed ASM Unknown Port %d \n", ulPhysPort) ;
			bxStatus = XTMSTS_PARAMETER_ERROR ;
			goto _End ;
			break ;
	} /* switch (ulPhysPort) */

	/* ASM traffic.  Send it on the first connection that has a valid
	 * bcmxtmrt driver instance handle and the port matches.
	 */
	{
		XTM_ADDR Addr;
		UINT32 i = 0;

		while( (pConn = m_pConnTable->Enum( &i )) != NULL ) {
			pConn->GetAddr( &Addr );
			if(((Addr.u.Vcc.ulPortMask & PORT_TO_PORTID(ulPhysPort)) == PORT_TO_PORTID(ulPhysPort) )
					&&
				((hDev = pConn->GetHandle()) != INVALID_HANDLE ))
			{
				bxStatus = XTMSTS_SUCCESS;
				break;
			}
		} /* while (pConn) */

		if (pConn == NULL)
			bxStatus = XTMSTS_NOT_FOUND;
	}

	if( bxStatus == XTMSTS_SUCCESS ) {
		XTMRT_CELL Cell;

		Cell.ConnAddr.u.Conn.ulPortMask = 0x1 ;  /* ToDo */
		Cell.ucCircuitType = ucCircuitType;
		memcpy (Cell.ucData, pAsmData, sizeof (Asm)) ;

		/* Send to ASM cell to the bcmxtmrt driver to send. */
		if ((*m_pfnXtmrtReq) (hDev, XTMRT_CMD_SEND_CELL, &Cell) != 0)
			bxStatus = XTMSTS_ERROR ;
	} /* if (bxStatus) */

_End	:
		return( bxStatus ) ;
} /* SendAsmCell */

void XTM_ASM_HANDLER::printNewAsm (Asm *pAsm, const char* pMsg)
{
#if 0
   XtmOsPrintf ("------------------%s-------------------\n",pMsg) ;
   XtmOsPrintf ("ASM Message \n") ;
	XtmOsPrintf ("msgType        = 0x%x \n", pAsm->msgType) ;
   XtmOsPrintf ("asmId          = %d \n", pAsm->asmId) ;
   XtmOsPrintf ("txLinkId       = %d \n", pAsm->txLinkId) ;
   XtmOsPrintf ("txLinkNumber   = %d \n", pAsm->txLinkNumber) ;
   XtmOsPrintf ("rxLinkStatus   = 0x%x 0x%x \n", pAsm->rxLinkStatus[0], pAsm->rxLinkStatus[1]);
   XtmOsPrintf ("txLinkStatus   = 0x%x 0x%x \n", pAsm->txLinkStatus[0], pAsm->txLinkStatus[1]);
   XtmOsPrintf ("groupId        = %d \n", pAsm->groupId) ;
   XtmOsPrintf ("rxAsmStatus    = 0x%x 0x%x \n", pAsm->rxAsmStatus [0], pAsm->rxAsmStatus [1]) ;
   XtmOsPrintf ("groupLostCell  = %d \n", pAsm->groupLostCell) ;
   XtmOsPrintf ("timestamp      = 0x%x \n", pAsm->timestamp) ;
   XtmOsPrintf ("delay          = %d \n", pAsm->delay) ;
   XtmOsPrintf ("appliedDelay   = %d \n", pAsm->appliedDelay) ;
   XtmOsPrintf ("msgLen			= %d \n", pAsm->msgLen) ;
   XtmOsPrintf ("crc				= %x \n", pAsm->crc) ;
   XtmOsPrintf ("---------------------------------------\n") ;
#else
   XtmOsPrintf ("%s\n",pMsg) ;
	XtmOsPrintf ("mt = 0x%x \n", pAsm->msgType) ;
   XtmOsPrintf ("id = %d \n", pAsm->txLinkId) ;
   XtmOsPrintf ("rx = 0x%x 0x%x \n", pAsm->rxLinkStatus[0], pAsm->rxLinkStatus[1]);
   XtmOsPrintf ("tx = 0x%x 0x%x \n", pAsm->txLinkStatus[0], pAsm->txLinkStatus[1]);
#endif
}

UINT32 XTM_ASM_HANDLER::LineInfo_getUpdateLocalRxState (UINT32 ulLogicalPort, UINT32 rTx)
{
  int inUse = 0;
  UINT32 lRx, rRx;
  LineInfo *objp = &m_BondMgmt.lines[ulLogicalPort] ;
  PXTM_INTERFACE_LINK_INFO pLi ;
  
  /* check toggling local rx state .... only impacted by our local status.
   * The rx state may only be changed if the differential delay is small enough  !!!
   */

  pLi = m_pInterfaces [ulLogicalPort].GetLinkInfo() ;

  if (pLi->ulLinkState == LINK_DOWN || pLi->ulLinkDsRate == 0)
  {
    lRx = LK_ST_SHOULD_NOT_USE;
  }
  else
  {
    lRx = rTx;
	 rRx = m_BondMgmt.remoteRxState ;
    if (rRx == LK_ST_SELECTED) {
        inUse = 1;
    }
  }
  
  objp->inUseRx = inUse;
  return lRx;
    
}

UINT32 XTM_ASM_HANDLER::LineInfo_getUpdateLocalTxState (UINT32 ulLogicalPort, UINT32 rRx)
{
  int inUse = 0;
  int txState;
  LineInfo *objp = &m_BondMgmt.lines[ulLogicalPort] ;
  PXTM_INTERFACE_LINK_INFO pLi ;

  txState = LK_ST_SHOULD_NOT_USE;

  pLi = m_pInterfaces [ulLogicalPort].GetLinkInfo() ;
  /* modify the Tx state if necessary */
  if (pLi->ulLinkState == LINK_UP && pLi->ulLinkUsRate != 0)
  {
    if (rRx != LK_ST_SHOULD_NOT_USE)
    {
      txState = LK_ST_SELECTED;
      if (rRx == LK_ST_SELECTED)
      {
        inUse = 1;
      }
    }
    else
      txState = LK_ST_ACCEPTABLE;
  }
  
  objp->inUseTx = inUse;
  return txState;
}


/* Following function verify whether the complete mapping is known
   For this purpose, we need to know the total line numbers
*/
int XTM_ASM_HANDLER::mappingKnown ()
{
  int mappingKnown = 0 ;

  if (m_BondMgmt.numberOfLines != 0) {

     int i;

	  if (m_BondingFallback == XTM_BONDING_FALLBACK_DISABLED) {
		  for (i=0;i<m_BondMgmt.numberOfLines;i++) {

			  if (m_BondMgmt.remoteIdToLocal[i] == -1) {
				  goto _End ;
			  }
		  }
		  mappingKnown = 1 ;
	  }
	  else {
		  for (i=0;i<m_BondMgmt.numberOfLines;i++) {

			  if (m_BondMgmt.remoteIdToLocal[i] != -1) {
				  mappingKnown = 1 ;
				  break ;
			  }
		  }
	  }
  } /* Number of lines != 0 */
  else
	  XtmOsPrintf ("bcmxtmcfg: atm bond mgmt number of lines is 0 \n") ;

_End :
  return mappingKnown ;
}

/*
  Reset FSM 
     wrong check   - xxxx  - xxxxx  => go error & reset
     correct check -  0xFF - active => reset fsm,  go idle
     correct check - !0xFF - active => no change
            
     correct check -  OxFF - error  => reset FSM, go idle
     correct check - !0xFF - error  time>timeout => go idle
     correct check - !0xFF - error  time<timeout => no change

     correct check -  0xFF - idle    => no change
     correct check - !0xFF - idle (+all mapping known) => go running
     correct check - !0xFF - idle (not all mapping known) => no change
*/

UINT32 XTM_ASM_HANDLER::UpdateResetFsm (int wrongCheck, int newMsgType)
{
  int resetState = m_BondMgmt.resetState ;

  MapXtmOsPrintf ("bcmxtmcfg: update Reset FSM newMsgType = %x \n", newMsgType) ;

  if (wrongCheck) {

    /* perform reset */
    resetBondingGroup () ;
    XtmOsGetTimeOfDay (m_BondMgmt.resetTimeout,NULL) ;
    XtmOsAddTimeOfDay (m_BondMgmt.resetTimeout, 5) ; /* stays at max 5 seconds in reset state */
    m_BondMgmt.asmCell.msgType = 0xFF ;
    resetState = BND_FORCE_RESET ;
  }
  else {
    if (resetState == BND_RUNNING) {
      if (newMsgType == 0xFF) {
        /* we receive an 0xFF, we just need to re-initialise, no need to ask
         * the other party to reinit */
        /* reset FSM go idle */
        /* perform reset */

        resetBondingGroup ();
        resetState = BND_IDLE;
      }
    }
    else if  (resetState == BND_FORCE_RESET) {
      /* get ASM arrival time ... */
      UINT8 tv0[TIMEOUT_VALUE_SIZE] ;
      
      int timeoutPassed;
      XtmOsGetTimeOfDay(tv0,NULL);
      timeoutPassed = XtmOsIsTimerGreater (tv0,m_BondMgmt.resetTimeout);

      if ((newMsgType == 0xFF) || timeoutPassed) {
        /* note that timeout is a protection against misbehaving CO that do not
           send 0xFF when they get a "0xFF" */
        /* go idle */
        resetState = BND_IDLE; /* no need to reset as the reset take place
                                * when we enter force reset */
        /* normqlly in IDLE, we do not send asm. Just set correctly msgType in
         * case this feature is disabled */
        m_BondMgmt.asmCell.msgType = ATMBOND_ASM_MESSAGE_TYPE_12BITSID ;
      }
    }
    else if  (resetState == BND_IDLE) {

      if (mappingKnown() && (newMsgType != 0xFF)) {

        resetState = BND_RUNNING ;
		  XtmOsPrintf ("bcmxtmcfg: bonding state is BND_RUNNING \n") ;
      }
    }
  } /* if good asm */

  if (resetState == BND_RUNNING) {
	 UINT32 i,j ;

    /* Verify the Rx SID mapping and Tx SID mapping !!! */
    m_BondMgmt.asmCell.msgType = newMsgType ;

	 /* Check type of sid and adapt */
	 i = XP_REGS->ulTxSarCfg ;
	 j = XP_REGS->ulRxSarCfg ;

	 /* Check the message type and accordingly set the sid mask(s). */
	 if ((UINT32) newMsgType != m_BondMgmt.sidType) {
		 if (newMsgType == ATMBOND_ASM_MESSAGE_TYPE_8BITSID) {
			 i &= ~TXSARA_SID12_EN ;
			 j &= ~RXSARA_SID12_EN ;
		 }
		 else {
			 i |= TXSARA_SID12_EN ;
			 j |= RXSARA_SID12_EN ;
		 }

		 XP_REGS->ulTxSarCfg = i ;
		 XP_REGS->ulRxSarCfg = j ;
		 m_BondMgmt.sidType = newMsgType ;
	 }
  }

  m_BondMgmt.resetState = resetState ;
  return resetState ;
}

void XTM_ASM_HANDLER::UpdateLineState (UINT32 ulLogicalPort)
{
  LineInfo *objp = &m_BondMgmt.lines[ulLogicalPort] ;

  if (!objp->inUseTx) {

    /* remove line from the active lines */
	 MapXtmOsPrintf ("bcmxtmcfg: tx %d disabled \n", ulLogicalPort) ;
    //m_pInterfaces [ulLogicalPort].SetLinkDataStatus (DATA_STATUS_DISABLED) ;
  }
  else {
	 MapXtmOsPrintf ("bcmxtmcfg: tx %d enabled \n", ulLogicalPort) ;
    //m_pInterfaces [ulLogicalPort].SetLinkDataStatus (DATA_STATUS_ENABLED) ;
  }
}

int XTM_ASM_HANDLER::ProcessReceivedAsm (Asm *pAsm, UINT32 ulLogicalPort)
{
   int txLinkId = pAsm->txLinkId & 0xF ;
   int checkFailed = 0 ;

	MapXtmOsPrintf ("bcmxtmcfg: Rx ASM port : %d \n", ulLogicalPort) ;

   if (!(m_BondMgmt.mapping[ulLogicalPort].receivedAsm % RX_ASM_PRINT_DEBUG))
		printNewAsm (pAsm, "Rx") ;

   if ((pAsm->msgType != 0xFF) && ((pAsm->msgType & ~1) != 0)) {
      /* unknown message type just discard the ASM */
		XtmOsPrintf ("bcmxtmcfg: ASM msgType %d received \n", pAsm->msgType) ;
      m_BondMgmt.mapping[ulLogicalPort].receivedUnKnowns++ ;
	   *((UINT32 *) &m_BondMgmt.asmCell.rxAsmStatus [0]) |= (0x1<<(31-ulLogicalPort)) ;
      return 0;
   }

   /* Avoid line reset because no asm received on a line */
   m_BondMgmt.mapping[ulLogicalPort].lastAsmTimestamp = XtmOsGetTimeStamp() ;

#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
      m_lastAsmTimestamp = XtmOsLinuxUptime();
      //XtmOsPrintf ("@@@@@@@ asm detected %u @@@@@@@@@@\n",m_lastAsmTimestamp);
#endif

   /* Increase ASM cell counter */
   m_BondMgmt.mapping[ulLogicalPort].receivedAsm++;

	*((UINT32 *) &m_BondMgmt.asmCell.rxAsmStatus [0]) &= ~(0x1<<(31-ulLogicalPort)) ;

   /* handle reception of msg type -> update the potential reset

      In case an old ASM with 0xFF is received. It will reset the bonding
      state. We think this is ok as it does not harm it just slow down the
      startup.

      Actions to be done:
      - learn bonding info
      + learn group id
      + learn link id

      - handle resetFSM
      + take into account a wrong check of groupid and link id
      (depending on the current state, it will lead to a reset or to a
      request to the CO to force a reset)
      + new msg type which could lead to a reset (just reset and go back
      to idle. In this case of starting up. When we are out of idle,
      it means that we have received an non 0xFF on each link. And
      consequently we should not oscillate between running and idle.

      - if new state is not "active", return and do not further handle the
      link states.
      */

   if ((m_BondMgmt.knownMapping & (1<<ulLogicalPort)) == 0)
   {
      /* if this is the first line to be mapped, we have to reset the bonding
       * group (reordering queue and tx sid) */

      if (m_BondMgmt.remoteIdToLocal[txLinkId] != -1) {

			XtmOsPrintf ("bcmxtmcfg: Asm Check Failed \n") ;
         checkFailed = 1;
		}
      else
      {
         m_BondMgmt.knownMapping |= 0x1<<ulLogicalPort ;

         /* check range txLinkId */
         /* we receive the reset message => we learn the external id */
         m_BondMgmt.mapping [ulLogicalPort].localIdToRemoteId = txLinkId ;
         m_BondMgmt.remoteIdToLocal [txLinkId] = ulLogicalPort;

         /* first line passing here, will set the default tx/rx state in LINK_ID_KNOWN */
         if (m_BondMgmt.numberOfLines == 0)
         {
            UINT32 defaultLinkState ;

            m_BondMgmt.numberOfLines = pAsm->txLinkNumber;
				m_BondMgmt.nextLineToPoll = ulLogicalPort ;
            m_BondMgmt.bondingId = pAsm->groupId;
            m_BondMgmt.asmCell.txLinkNumber = pAsm->txLinkNumber;
            m_BondMgmt.asmCell.asmId = 0x0;
            /* reset local status */
            defaultLinkState = 0x5555 << ((8-m_BondMgmt.numberOfLines)<<1);
            m_BondMgmt.asmCell.rxLinkStatus[0] = defaultLinkState;
            m_BondMgmt.asmCell.txLinkStatus[0] = defaultLinkState;
            m_BondMgmt.asmLastRxId = pAsm->asmId;
         }
      }
   } /* if ((m_BondMgmt.knownMapping & (1<<ulLogicalPort)) == 0) */

   {
      /* keep in mind the requested delay */
      m_BondMgmt.mapping [ulLogicalPort].requestedDelay = pAsm->delay;
   }

   {
      int newResetFsmState;

      if ((m_BondMgmt.mapping [ulLogicalPort].localIdToRemoteId != txLinkId)
            || (m_BondMgmt.bondingId != pAsm->groupId)) {
			XtmOsPrintf ("bcmxtmcfg: checkFailed 2 \n") ;
         checkFailed = 1;
		}

      newResetFsmState = UpdateResetFsm (checkFailed, pAsm->msgType) ;

      if (newResetFsmState != BND_RUNNING) {
			MapXtmOsPrintf ("bcmxtmcfg: newResetFsmState is %d \n", newResetFsmState) ;
         return 0; /* no further processing */
		}
   }

   /* do not further handle out of sequence ASM  */
   if ((((UINT32)(pAsm->asmId-m_BondMgmt.asmLastRxId))&0xFF)>128)
   {
		MapXtmOsPrintf ("bcmxtmcfg: outofSequence ASM id %d last asm id %d\n",
				       pAsm->asmId, m_BondMgmt.asmLastRxId) ;
      return 1;
   }

   m_BondMgmt.asmLastRxId = pAsm->asmId ;

   /*
      - for each known line
      - handle remote rx state (check if our local tx state should be adapted)
      - for each active line, change local Tx state

      - handle remote tx state
      - adapt last seq Id
      (remote tx to disable => lastSeqId in resequencing should be ~0)
      */

   {
      int i;
      int linkNumber = m_BondMgmt.numberOfLines;
      int currentShift=(8-linkNumber)<<1;

      UINT32 remoteRxState = pAsm->rxLinkStatus[0];
      UINT32 remoteTxState = pAsm->txLinkStatus[0];
      UINT32 newLocalTx = 0;
      UINT32 newLocalRx = 0;
      UINT32 localRx = 0;
      UINT32 localTx = 0;

      /* keep remote tx state for debugging purposes */
      m_BondMgmt.remoteTxState = remoteTxState;
      m_BondMgmt.remoteRxState = remoteRxState;
      localRx = m_BondMgmt.asmCell.rxLinkStatus[0];
      localTx = m_BondMgmt.asmCell.txLinkStatus[0];

      remoteRxState >>=currentShift;
      remoteTxState >>=currentShift;
      localRx >>= currentShift;
      localTx >>= currentShift;

      for (i=linkNumber;i--;)
      {
         int localId = m_BondMgmt.remoteIdToLocal[i];
         if (localId != -1)
         {
            /* a link is only useable if at least some asm have been received on that link */
            UINT32 ITx, lRx;
            /* compute new local rx/tx state */
				ITx = LineInfo_getUpdateLocalTxState(localId, remoteRxState&LK_ST_MASK);
				newLocalTx |= ITx <<currentShift;
            if ((localTx & LK_ST_MASK) != (remoteRxState & LK_ST_MASK))
            {
               m_BondMgmt.mapping[localId].stateChanges++;
            }

            lRx = LineInfo_getUpdateLocalRxState(localId,remoteTxState&LK_ST_MASK); 
            newLocalRx |= lRx <<currentShift;
            if ((localRx & LK_ST_MASK) != (remoteTxState & LK_ST_MASK))
            {
               m_BondMgmt.mapping[localId].stateChanges++;
            }

            /* if line is not yet active, activate it/deactivate it */
            //UpdateLineState (localId) ;
         }
         else
         {
            newLocalTx |= LK_ST_SHOULD_NOT_USE<<currentShift;
            newLocalRx |= LK_ST_SHOULD_NOT_USE<<currentShift;
         }
         currentShift += 2;
         remoteRxState>>=2;
         remoteTxState>>=2;
         localRx >>= 2;
         localTx >>= 2;
      } /* for i .. LinkNumber */

      m_BondMgmt.asmCell.txLinkStatus[0] = newLocalTx;
      m_BondMgmt.asmCell.rxLinkStatus[0] = newLocalRx;
   }

   return 0 ;
}


/***************************************************************************
 * Function Name: Initialize
 * Description  : Initializes the object.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_ASM_HANDLER::Initialize( XtmBondConfig bondConfig,
		                                     FN_XTMRT_REQ pfnXtmrtReq,
														 XTM_INTERFACE *pInterfaces,
														 XTM_CONNECTION_TABLE *pConnTable)
{
	int k ;
	BCMXTM_STATUS bxStatus = XTMSTS_SUCCESS;
	XTMRT_CELL_HDLR CellHdlr;

	if( !m_ulRegistered )
	{
		m_pfnXtmrtReq = pfnXtmrtReq;
		m_BondCfg.uConfig = bondConfig.uConfig;
		m_BondingFallback = XTM_BONDING_FALLBACK_ENABLED ;
		m_pInterfaces = pInterfaces ;
		m_pConnTable = pConnTable ;
#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
		m_linkup = FALSE;
		m_lastAsmTimestamp = 0;
#endif
		XtmOsPrintf ("bcmxtmcfg: ATM Bonding configured in system. Fallback mode = %s \n",
				       (m_BondingFallback == XTM_BONDING_FALLBACK_ENABLED) ? "Enabled" : "Disabled");

		/* Initialize bondMgmt structure field definitions */
		memset (&m_BondMgmt, 0, sizeof (XtmBondMgmt));

		make_crc_table_rev() ;

		for (k=0;k<MAX_BONDED_LINES;k++) {
			
			/* Set the data tx statuses for the interfaces so no data can be
			 * sent, until both the ends converge through the ASM messages.
			 */
	 		MapXtmOsPrintf ("bcmxtmcfg: tx %d disabled \n", k) ;
			//m_pInterfaces[k].SetLinkDataStatus (DATA_STATUS_DISABLED) ;

			/* reset line context */
			LineInfo_init (&m_BondMgmt.lines[k]) ;

			/* preinit mapping information */
			m_BondMgmt.mapping [k].localIdToRemoteId = -1 ;
			m_BondMgmt.remoteIdToLocal [k] = -1 ;
		}

		m_BondMgmt.nextLineToPoll = MAX_BONDED_LINES ;
		//m_BondMgmt.timeInterval = MAX_TIME_INTERVAL ;
		m_BondMgmt.timeInterval = XTM_BOND_MGMT_TIMEOUT_MS*1000 ;

      m_BondMgmt.asmCell.msgLen = 0x28;
      m_BondMgmt.asmCell.rxAsmStatus[0] = 0xC0 ;

		m_BondMgmt.sidType = ATMBOND_ASM_MESSAGE_TYPE_12BITSID;
		
		/* Register an ASM cell handler with the bcmxtmrt network driver. */
		CellHdlr.ulCellHandlerType = CELL_HDLR_ASM;
		CellHdlr.pfnCellHandler = XTM_ASM_HANDLER::ReceiveAsmCb;
		CellHdlr.pContext = this;
		if( (*m_pfnXtmrtReq) (NULL, XTMRT_CMD_REGISTER_CELL_HANDLER,
					&CellHdlr) == 0 )
		{
			m_ulRegistered = 1;
		}
		else {
			XtmOsPrintf("bcmxtmcfg: Error registering ASM handler\n");
			bxStatus = XTMSTS_ERROR ;
		}

		XtmOsStartTimer((void *) XTM_ASM_HANDLER::MgmtTimerCb, (UINT32) this,
				XTM_BOND_MGMT_TIMEOUT_MS) ;
		bondMgmtValid = 1 ;
	}
	else
		bxStatus = XTMSTS_STATE_ERROR ;

	return( bxStatus );
} /* Initialize */


/***************************************************************************
 * Function Name: Uninitialize
 * Description  : Uninitializes the object.
 * Returns      : XTMSTS_SUCCESS if successful or error status.
 ***************************************************************************/
BCMXTM_STATUS XTM_ASM_HANDLER::Uninitialize( void )
{
	int k = 0;

	if( m_ulRegistered )
	{
		XTMRT_CELL_HDLR CellHdlr;

		for (k=0;k<MAX_BONDED_LINES;k++) {

			/* Set the data tx statuses for the interfaces so no data can be
			 * sent, until both the ends converge through the ASM messages.
			 */

	 		MapXtmOsPrintf ("bcmxtmcfg: tx %d disabled \n", k) ;
			//m_pInterfaces[k].SetLinkDataStatus (DATA_STATUS_DISABLED) ;
		}

		/* Unregister the ASM cell handler with the bcmxtmrt network driver. */
		CellHdlr.ulCellHandlerType = CELL_HDLR_ASM;
		CellHdlr.pfnCellHandler = XTM_ASM_HANDLER::ReceiveAsmCb;
		CellHdlr.pContext = this;
		(*m_pfnXtmrtReq)(NULL, XTMRT_CMD_UNREGISTER_CELL_HANDLER, &CellHdlr);
		m_pConnTable = NULL ;
		m_pInterfaces = NULL ;
		m_pfnXtmrtReq = NULL ;
		m_BondingFallback = XTM_BONDING_FALLBACK_DISABLED ;
		m_ulRegistered = 0;
		bondMgmtValid =  0 ;
#ifdef AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH
		m_linkup = FALSE;
		m_lastAsmTimestamp = 0;
#endif
	}

	return( XTMSTS_SUCCESS );
} /* Uninitialize */

/***************************************************************************
 * Function Name: ReceiveAsmCb
 * Description  : Processes a received ASM cell.
 * Returns      : 0 if successful, non-0 if not
 ***************************************************************************/
int XTM_ASM_HANDLER::ReceiveAsmCb( XTMRT_HANDLE hDev, UINT32 ulCommand,
    void *pParam, void *pContext )
{
	const UINT32 ulAtmHdrSize = 4; /* no HEC */
	XTM_ASM_HANDLER *pThis = (XTM_ASM_HANDLER *) pContext;
	PXTMRT_CELL pCell = (PXTMRT_CELL) pParam;
	UINT32 i, ulLogicalPort;
	int status = 0 ;

	/* Remove ATM header. */
	for( i = 0; i < CELL_PAYLOAD_SIZE; i++ )
		pCell->ucData[i] = pCell->ucData[i + ulAtmHdrSize];

	switch (pCell->ucCircuitType) {
		case CTYPE_ASM_P0		:
			ulLogicalPort = PORT_PHYS_TO_LOG (PHY_PORTID_0) ;
			break ;
		case CTYPE_ASM_P1		:
			ulLogicalPort = PORT_PHYS_TO_LOG (PHY_PORTID_1) ;
			break ;
		case CTYPE_ASM_P2		:
			ulLogicalPort = PORT_PHYS_TO_LOG (PHY_PORTID_2) ;
			break ;
		case CTYPE_ASM_P3		:
			ulLogicalPort = PORT_PHYS_TO_LOG (PHY_PORTID_3) ;
			break ;
		default					:
			XtmOsPrintf ("bcmxtmcfg: Received ASM Unknown %d \n", pCell->ucCircuitType) ;
			status = -1 ;
			break ;
	}

	if (status == 0)
		pThis->ProcessReceivedAsm ((Asm *) pCell->ucData, ulLogicalPort) ;

	return( status );
} /* ReceiveAsmCb */


/* CRC32 routines */
#define CRC32_POLY 0x04C11DB7UL
#define revb(x) ((((x) & 0x80) >> 7) | \
                 (((x) & 0x40) >> 5) | \
                 (((x) & 0x20) >> 3) | \
                 (((x) & 0x10) >> 1) | \
                 (((x) & 0x08) << 1) | \
                 (((x) & 0x04) << 3) | \
                 (((x) & 0x02) << 5) | \
                 (((x) & 0x01) << 7))

#define big_endian_load(p)   (*(p))

UINT32 XTM_ASM_HANDLER::swapWord(UINT32 word)
{
    UINT32 res;
    UINT8 *pres8 = (UINT8 *) &res;
    UINT8 *pword8 = (UINT8 *) &word;

    pres8[0] = pword8[3];
    pres8[1] = pword8[2];
    pres8[2] = pword8[1];
    pres8[3] = pword8[0];
    return res;
}

UINT32 XTM_ASM_HANDLER::AAL5_crc32_common(UINT32 *payload, UINT32 size, UINT32 remainder)
{
    UINT32 k;
    UINT32 word;
    remainder = swapWord(remainder);
    for (k=0; k<size; k++)
    {
        word = big_endian_load(&payload[k]);
        {
            int i;
            remainder ^= word;
            for (i=32; i--; )
            {
                remainder = (remainder << 1) ^ ((remainder & 0x80000000) ? CRC32_POLY : 0);
            }
        }
    }
    remainder = swapWord(remainder);
    return remainder;
}

/* return CRC value (in local endianess).
   Note CRc in AAL5 is big endian oriented 
*/

UINT32 XTM_ASM_HANDLER::AAL5_crc32(UINT32 *payload, UINT32 size_in_UINT32s)
{
    return AAL5_crc32_common(payload, size_in_UINT32s, ~0x0ul);
}

UINT32 crc_table_rev1[256];

void XTM_ASM_HANDLER::make_crc_table_rev ()
{
  UINT32 c;
  int n, k;
  UINT32 poly;            /* polynomial exclusive-or pattern */
  /* terms of polynomial defining this crc (except x^32): */
  static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* make exclusive-or pattern from polynomial (0xedb88320L) */
  poly = 0L;
  for (n = 0; (UINT8) n < sizeof(p)/sizeof(char); n++)
    poly |= 0x80000000L >> (31 - p[n]);

  for (n = 0; n < 256; n++)
  {
    c = ((UINT32)n)<<24;
    for (k = 0; k < 8; k++)
      c = c & (1UL<<31) ? poly ^ (c << 1) : c << 1;
    crc_table_rev1[n] = swapWord(c);
  }
}

UINT32  XTM_ASM_HANDLER::getCrc32_rev32Bit (UINT32 *pdata, UINT32 size, UINT32 crc)
{
    unsigned int word, i ;

    size >>= 2 ;  /* in terms of 32-bit words. Divide by 2. */
    for (i= 0; i<size; i++) {

       word = *(pdata+i) ;

       crc = (crc >> 8) ^ (crc_table_rev1 [(crc ^ (word >> 24)) & 0xff]) ;
       crc = (crc >> 8) ^ (crc_table_rev1 [(crc ^ ((word >> 16) & 0xff)) & 0xff]) ;
       crc = (crc >> 8) ^ (crc_table_rev1 [(crc ^ ((word >> 8) & 0xff)) & 0xff]) ;
       crc = (crc >> 8) ^ (crc_table_rev1 [(crc ^ (word & 0xff)) & 0xff]) ;
    } /* for */
  return (crc);
}

#endif /* if defined(CONFIG_BCM96368) */
