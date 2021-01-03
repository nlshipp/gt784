#include "BcmXdslHmi.h"
#include "bcm_hmiCoreMsg.h"
#include "bcm_hmiLineMsg.h"
#include "softdsl/AdslCoreDefs.h"
#include "AdslCore.h"
#include "linux/string.h"
#include "softdsl/SoftDsl.h"
#include "softdsl/AdslMib.h"

int hmiMsgConstructReply(HmiMsgHdr *pHmiMsgHdr);
static int hmiCoreMsgReply(HmiMsgHdr *pHmiMsgHdr);
static int hmiLineMsgReply(HmiMsgHdr *pHmiMsgHdr);

int hmiMsgConstructReply(HmiMsgHdr *pHmiMsgHdr)
{	
	pHmiMsgHdr->result = HMI_RESULT_SUCCESS;
	
	pHmiMsgHdr->firmwareId = 0x5C39; /* TODO: Need real F/W Id value */
	
	switch(pHmiMsgHdr->applicationId) {
		case CORE_MANAGER_ID:
			pHmiMsgHdr->result = hmiCoreMsgReply(pHmiMsgHdr);
			break;
		case LINE_MANAGER_ID:
			pHmiMsgHdr->result = hmiLineMsgReply(pHmiMsgHdr);
			break;
		default:
			printk("%s: Unknown applicationId: %04X\n", __FUNCTION__, pHmiMsgHdr->applicationId);
			pHmiMsgHdr->result = HMI_RESULT_UNKNOW_APPLICATION_ID;
			break;
	}
	
	return pHmiMsgHdr->result;
}

static int hmiCoreMsgReply(HmiMsgHdr *pHmiMsgHdr)
{	
	switch(pHmiMsgHdr->commandId) {
		case GET_FIRMWARE_VERSION_INFO:
		{
			/* Construct reply */
			adslPhyInfo		*pInfo = AdslCoreGetPhyInfo();
			FirmwareVersionInfo *pVerInfo = (FirmwareVersionInfo*)((uint32)pHmiMsgHdr+HMI_HDR_LEN);
			memset(pVerInfo, 0, sizeof(FirmwareVersionInfo));
			pVerInfo->hwChipId = pInfo->chipType; /* ??? */
			pVerInfo->fwMajorVersion = pInfo->mjVerNum;
			pVerInfo->fwMinorVersion = pInfo->mnVerNum;
			//pVerInfo->fwFixVersion =; /* ??? */
			pVerInfo->hmiMajorVersion = HMI_MAJOR_VERSION;
			pVerInfo->hmiMinorVersion = HMI_MINOR_VERSION;
			strncpy((char *)pVerInfo->info, pInfo->pVerStr, sizeof(pVerInfo->info));
			pVerInfo->nrOfLinesSupportedInFirmwareVersion = 1;
			//pVerInfo->reserved[3];

			/* Client expects reply in Little Endian format */
			bigEndianByteSwap((uint8 *)pVerInfo, GetFirmwareVersionInfoLayout.replyMsgLayout);

			/* Update reply lenght field in msg hdr */
			pHmiMsgHdr->length = sizeof(FirmwareVersionInfo);
		}
			break;
			
		default:
			printk("%s: Unknown commandId: %X\n", __FUNCTION__, pHmiMsgHdr->commandId);
			pHmiMsgHdr->result = HMI_RESULT_UNKNOW_COMMAND_ID;
			break;
	}
	
	return pHmiMsgHdr->result;
}

static int hmiLineMsgReply(HmiMsgHdr *pHmiMsgHdr)
{
	switch(pHmiMsgHdr->commandId) {
		case GET_LINE_COUNTERS:
		{
			adslMibInfo	*dataBuf;
			adslMibInfo mibData;
			int     perfDataLen,res;
			static long secElapsed=0,secElapsedLastRead=0;
			BcmAdslCoreDiagWriteStatusString("Source Ref =%x",pHmiMsgHdr->sourceRef);
			BcmAdslCoreDiagWriteStatusString("Request Ref =%x",pHmiMsgHdr->requestRef);
			BcmAdslCoreDiagWriteStatusString("firmwareId =%d",pHmiMsgHdr->firmwareId);
			BcmAdslCoreDiagWriteStatusString("commandId =%d",pHmiMsgHdr->commandId);
			GetLineCountersMsgRep  *pLineCounters = (GetLineCountersMsgRep *)((uint32)pHmiMsgHdr+HMI_HDR_LEN);
			memset(pLineCounters, 0, sizeof(GetLineCountersMsgRep));
			dataBuf = (void *) AdslCoreGetObjectValue (NULL, 0, NULL, &perfDataLen);
			AdslMibByteMove(sizeof(adslMibInfo),dataBuf,&mibData);
			pLineCounters->derivedCounts[0].FECS=(uint32)mibData.adslPerfData.perfCurr1Day.adslFECs;
			pLineCounters->derivedCounts[0].LOSS=(uint32)mibData.adslPerfData.perfCurr1Day.adslLoss;
			pLineCounters->derivedCounts[0].ES=(uint32)mibData.adslPerfData.perfCurr1Day.adslESs;
			pLineCounters->derivedCounts[0].SES=(uint32)mibData.adslPerfData.perfCurr1Day.adslSES;
			pLineCounters->derivedCounts[0].UAS=(uint32)mibData.adslPerfData.perfCurr1Day.adslUAS;
			pLineCounters->derivedCounts[0].LOFS=(uint32)mibData.adslPerfData.perfCurr1Day.adslLofs;
			pLineCounters->derivedCounts[0].LPRS=(uint32)mibData.adslPerfData.perfCurr1Day.adslLprs;
			pLineCounters->adslAnomalyCounts[0].ne.CV=(uint32)mibData.adslStat.rcvStat.cntSFErr;
			pLineCounters->adslAnomalyCounts[0].ne.FEC=(uint32)mibData.adslStat.rcvStat.cntRSCor;
			pLineCounters->adslAnomalyCounts[0].ne.uncorrectableCodeword=(uint32)mibData.adslStat.rcvStat.cntRSUncor;
			pLineCounters->atmPerfCounts[0].ne.totalCells=(uint32)mibData.atmStat.rcvStat.cntCellTotal;
			pLineCounters->atmPerfCounts[0].ne.totalUserCells=(uint32)mibData.atmStat.rcvStat.cntCellData;
			pLineCounters->atmPerfCounts[0].ne.hecErrors=(uint32)mibData.atmStat.rcvStat.cntHEC;
			pLineCounters->atmPerfCounts[0].ne.overflowCells=(uint32)mibData.atmStat.rcvStat.cntOCD;
			pLineCounters->atmPerfCounts[0].ne.idleCellBitErrors=(uint32)mibData.atmStat.rcvStat.cntBitErrs;
			pLineCounters->failedFullInitCount=(uint16)mibData.adslPerfData.failTotal.adslInitErr;
			pLineCounters->fullInitCount=(uint16)(mibData.adslPerfData.failTotal.adslRetr-mibData.adslPerfData.failTotal.adslInitErr);
			secElapsed=secElapsedInDay;
			if(secElapsed>secElapsedLastRead)
				pLineCounters->elapsedTimeSinceLastRead=(uint16)(secElapsed-secElapsedLastRead);
			else 
				pLineCounters->elapsedTimeSinceLastRead=(uint16)secElapsed;
			secElapsedLastRead=secElapsed;
			BcmAdslCoreDiagWriteStatusString("FECS =%u",pLineCounters->derivedCounts[0].FECS);
			BcmAdslCoreDiagWriteStatusString("LOSS =%u ",pLineCounters->derivedCounts[0].LOSS);
			BcmAdslCoreDiagWriteStatusString("****Sending Response for GetLineCounters");
			bigEndianByteSwap((uint8 *)pLineCounters, GetLineCountersLayout.replyMsgLayout);
			BcmAdslCoreDiagWriteStatusString("Source Ref Resp=%x",pHmiMsgHdr->sourceRef);
			BcmAdslCoreDiagWriteStatusString("Request Ref Resp=%x",pHmiMsgHdr->requestRef);
			BcmAdslCoreDiagWriteStatusString("firmwareId Resp=%d",pHmiMsgHdr->firmwareId);
			BcmAdslCoreDiagWriteStatusString("commandId Resp=%d",pHmiMsgHdr->commandId);
			pHmiMsgHdr->length = sizeof(GetLineCountersMsgRep);
		}
			break;

		case GET_LINE_CPE_VENDOR_INFO:
		{
			/* Construct reply */
			void	*pOemData;
			int		maxLen, *pLen;
			VendorId vendId;
			

			CPEvendorInfo *pCPEvendorInfo = (CPEvendorInfo *)((uint32)pHmiMsgHdr+HMI_HDR_LEN);
			memset(pCPEvendorInfo, 0, sizeof(CPEvendorInfo));
			//pCPEvendorInfo->hsVendorId.countryCode[0]. ;
			pOemData=AdslCoreGetOemParameterData (kDslOemG994VendorId, &pLen, &maxLen);
			AdslMibByteMove(sizeof(vendId.countryCode),pOemData,&(vendId.countryCode));
			AdslMibByteMove(sizeof(vendId.providerCode),pOemData+sizeof(vendId.countryCode),&(vendId.providerCode));
			AdslMibByteMove(sizeof(vendId.vendorInfo),pOemData+sizeof(vendId.countryCode)+sizeof(vendId.providerCode),&(vendId.vendorInfo));
			BcmAdslCoreDiagWriteStatusString("vendId.countryCode=%u",vendId.countryCode);
			BcmAdslCoreDiagWriteStatusString("pOemData Country Code=%x %x %x",(char *)pOemData,(char *)(pOemData+1),(char *)(pOemData+2));
			pCPEvendorInfo->hsVendorId=vendId;
			pCPEvendorInfo->t1_413_vendorInfo=AdslCoreGetOemParameterData (kDslOemT1413VendorId, &pLen, &maxLen);
			BcmAdslCoreDiagWriteStatusString("pCPEvendorInfo->t1_413_vendorInfo=%d",pCPEvendorInfo->t1_413_vendorInfo);
			/* Client expects reply in Little Endian format */
			bigEndianByteSwap((uint8 *)pCPEvendorInfo, GetLineCPEvendorInfoLayout.replyMsgLayout);
			BcmAdslCoreDiagWriteStatusString("****Sending Response for GetCPEvendorInfo");
			/* Update reply lenght field in msg hdr */
			pHmiMsgHdr->length = sizeof(CPEvendorInfo);
			
		}
			break;

		case GET_LINE_STATUS:
		{
			/* Construct reply */
			LineStatus *pLineStatus = (LineStatus *)((uint32)pHmiMsgHdr+HMI_HDR_LEN);
			memset(pLineStatus, 0, sizeof(LineStatus));
			pLineStatus->state = LINE_STATUS_RUN_SHOW_L2;
			pLineStatus->selectedProtocol = LINE_SEL_PROT_992_5_A;
			//pLineStatus->lastRetrainCause = ;
			pLineStatus->loopbackMode = BCM_LOOPBACK_NONE;
			pLineStatus->tpsTcType[0] = LINE_TRAFFIC_ATM;		/* bearer B0 */
			pLineStatus->tpsTcType[1] = LINE_TRAFFIC_INACTIVE;		
			//pLineStatus->reserved[2] =;
			pLineStatus->linkStatus[0].actualLineBitRate = 1024; /* US */
			pLineStatus->linkStatus[0].bearer[0].attainableBitRate = 1024; /* bearer B0 */
			pLineStatus->linkStatus[0].bearer[0].actualBitRate = 720;
			
			pLineStatus->linkStatus[1].actualLineBitRate = 24000;			
			pLineStatus->linkStatus[1].bearer[0].attainableBitRate = 24000; /* bearer B0 */
			pLineStatus->linkStatus[1].bearer[0].actualBitRate = 12000;
			
			/* Client expects reply in Little Endian format */
			bigEndianByteSwap((uint8 *)pLineStatus, GetLineStatusLayout.replyMsgLayout);
			/* Update reply lenght field in msg hdr */
			pHmiMsgHdr->length = sizeof(LineStatus);
		}
			break;
		default:
			printk("%s: Unknown commandId: %X\n", __FUNCTION__, pHmiMsgHdr->commandId);
			pHmiMsgHdr->result = HMI_RESULT_UNKNOW_COMMAND_ID;
			break;
	}
	
	return pHmiMsgHdr->result;

}

