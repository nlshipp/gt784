#ifndef BCM_XDSL_HMI_HDR_H
#define BCM_XDSL_HMI_HDR_H

#include "HmiDef.h"
#include "bcm_hmiMsgHdr.h"

#define BYTESWAP16(a) ((uint16)((((uint16)(a)<<8) | ((uint16)(a)>>8)) & 0xffff))
#define BYTESWAP32(a) ((uint32)((a)<<24) | (((uint32)(a)<<8)&0xff0000) | (((uint32)(a)>>8)&0xff00) | ((uint32)(a)>>24))

typedef struct _udpFrameHdr UdpFrameHdr;

#define UDP_FRAME_HDR_LEN		sizeof(UdpFrameHdr)
#define HMI_HDR_LEN				sizeof(HmiMsgHdr)
#define HMI_HDR_OFFSET(x)		((x)->data+UDP_FRAME_HDR_LEN)
#define HMI_PYLD_OFFSET(x)		((x)->data+UDP_FRAME_HDR_LEN+HMI_HDR_LEN)

int BcmAdslCoreHmiInit(void);
void BcmAdslHmiMsgProcess(void *pArg);
void BcmAdslHmiAnswerProcess(void);

#endif
