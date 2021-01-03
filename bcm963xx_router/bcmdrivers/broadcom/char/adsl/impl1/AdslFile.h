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
/****************************************************************************
 *
 * AdslFile.h
 *
 * Description:
 *		This file contains definitions for Adsl File I/O 
 *
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslFile.h,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslFile.h,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/14 21:11:59  ilyas
 * Inial CVS checkin
 *
 *
 *****************************************************************************/

typedef struct {
	char	imageId[4];
	long	lmemOffset;
	long	lmemSize;
	long	sdramOffset;
	long	sdramSize;
	char	reserved[12];
} adslPhyImageHdr;

int AdslFileLoadImage(char *fname, void *pAdslLMem, void *pAdslSDRAM);
