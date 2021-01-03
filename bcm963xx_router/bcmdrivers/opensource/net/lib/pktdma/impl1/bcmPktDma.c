/*
<:copyright-broadcom

 Copyright (c) 2009 Broadcom Corporation
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
/*
 *******************************************************************************
 * File Name  : bcmPktDma.c
 *
 * Description: This file contains the Packet DMA initialization API.
 *
 *******************************************************************************
 */

#include <linux/module.h>
#include <linux/skbuff.h>

#include "bcmtypes.h"
#include "fap_task.h"
#include "bcmPktDmaHooks.h"

bcmPktDma_hostHooks_t bcmPktDma_hostHooks_g;
/* Add code for buffer quick free between enet and xtm - June 2010 */
static RecycleFuncP   bcmPktDma_enet_recycle_hook = NULL;
static RecycleFuncP   bcmPktDma_xtm_recycle_hook = NULL;

static void initHostHooks(void)
{
    memset(&bcmPktDma_hostHooks_g, 0, sizeof(bcmPktDma_hostHooks_t));
}

int bcmPktDma_bind(bcmPktDma_hostHooks_t *hostHooks)
{
    if(hostHooks->xmit2Fap == NULL ||
       /* FAP PSM Memory Allocation added Apr 2010 */
       hostHooks->psmAlloc == NULL ||
       hostHooks->dqmXmitMsgHost == NULL ||
       hostHooks->dqmRecvMsgHost == NULL ||
       hostHooks->dqmEnableHost == NULL)
    {
        return FAP_ERROR;
    }

    bcmPktDma_hostHooks_g = *hostHooks;

    printk("%s: FAP Driver binding successfull\n", __FUNCTION__);

    return FAP_SUCCESS;
}

void bcmPktDma_unbind(void)
{
    initHostHooks();
}

/* Add code for buffer quick free between enet and xtm - June 2010 */
void bcmPktDma_set_enet_recycle(RecycleFuncP enetRecycle)
{
    bcmPktDma_enet_recycle_hook = enetRecycle;
}

RecycleFuncP bcmPktDma_get_enet_recycle(void)
{
    return(bcmPktDma_enet_recycle_hook);
}

void bcmPktDma_set_xtm_recycle(RecycleFuncP xtmRecycle)
{
    bcmPktDma_xtm_recycle_hook = xtmRecycle;
}

RecycleFuncP bcmPktDma_get_xtm_recycle(void)
{
    return(bcmPktDma_xtm_recycle_hook);
}

int __init bcmPktDma_init(void)
{
    printk("%s: Broadcom Packet DMA Library initialized\n", __FUNCTION__);

    initHostHooks();

    return 0;
}

void __exit bcmPktDma_exit(void)
{
    printk("Broadcom Packet DMA Library exited");
}

module_init(bcmPktDma_init);
module_exit(bcmPktDma_exit);

EXPORT_SYMBOL(bcmPktDma_bind);
EXPORT_SYMBOL(bcmPktDma_unbind);
EXPORT_SYMBOL(bcmPktDma_set_enet_recycle);
EXPORT_SYMBOL(bcmPktDma_get_enet_recycle);
EXPORT_SYMBOL(bcmPktDma_set_xtm_recycle);
EXPORT_SYMBOL(bcmPktDma_get_xtm_recycle);
