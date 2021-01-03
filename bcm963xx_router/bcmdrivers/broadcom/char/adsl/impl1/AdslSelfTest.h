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
 * AdslSelfTest.h -- Definitions for ADSL self test module
 *
 * Description:
 *	Definitions for ADSL self test module
 *
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslSelfTest.h,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslSelfTest.h,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

#ifndef _ADSL_SELF_TEST_H
#define _ADSL_SELF_TEST_H

int	AdslSelfTestRun(int stMode);
int AdslSelfTestGetResults(void);

#endif	/* _ADSL_SELF_TEST_H */
