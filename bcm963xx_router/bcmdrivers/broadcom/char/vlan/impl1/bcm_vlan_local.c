/*
  <:copyright-broadcom 
 
  Copyright (c) 2007 Broadcom Corporation 
  All Rights Reserved 
  No portions of this material may be reproduced in any form without the 
  written permission of: 
  Broadcom Corporation 
  Irvine, California
  All information contained in this document is Broadcom Corporation 
  company private, proprietary, and trade secret. 
 
  :>
*/
//**************************************************************************
// File Name  : bcm_vlan_local.c
//
// Description: Broadcom VLAN Interface Driver
//               
//**************************************************************************

#include "bcm_vlan_local.h"
#include <linux/version.h>
#include "skb_defines.h"


/*
 * Global variables
 */

/* global BCM VLAN lock */
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
spinlock_t bcmVlan_lock_g = SPIN_LOCK_UNLOCKED;
#endif


/*
 * Local variables
 */

static BCM_VLAN_DECLARE_LL(realDevCtrlLL);

static struct kmem_cache *vlanDevCtrlCache;
static struct kmem_cache *realDevCtrlCache;


/*
 * Local Functions
 */

static inline struct vlanDeviceControl *allocVlanDevCtrl(void)
{
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
    return kmem_cache_alloc(vlanDevCtrlCache, GFP_ATOMIC);
#else
    return kmem_cache_alloc(vlanDevCtrlCache, GFP_KERNEL);
#endif
}

static inline void freeVlanDevCtrl(struct vlanDeviceControl *vlanDevCtrl)
{
    kmem_cache_free(vlanDevCtrlCache, vlanDevCtrl);
}

static inline struct realDeviceControl *allocRealDevCtrl(void)
{
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
    return kmem_cache_alloc(realDevCtrlCache, GFP_ATOMIC);
#else
    return kmem_cache_alloc(realDevCtrlCache, GFP_KERNEL);
#endif
}

static inline void freeRealDevCtrl(struct realDeviceControl *realDevCtrl)
{
    kmem_cache_free(realDevCtrlCache, realDevCtrl);
}

/*
 * Free all VLAN Devices of a given Real Device
 *
 * Entry points: NOTIFIER, CLEANUP
 */
static int freeVlanDevices(struct realDeviceControl *realDevCtrl)
{
    int count = 0;
    struct vlanDeviceControl *vlanDevCtrl, *nextVlanDevCtrl;

    BCM_ASSERT(realDevCtrl);

    /* Make sure the caller has the RTNL lock */
    ASSERT_RTNL();

    vlanDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrl->vlanDevCtrlLL);

    while(vlanDevCtrl)
    {
        BCM_LOG_INFO(BCM_LOG_ID_VLAN, "De-allocated VLAN Device %s", vlanDevCtrl->vlanDev->name);

        count++;

        /* decrement reference counter of the real device, since we
           added a reference in register_vlan_device() when the VLAN
           device was created */
        dev_put(realDevCtrl->realDev);

        /* FIXME: dev_put() for the VLAN device is only called in the
           ioctl() routine, as in the original Linux VLAN code.
           Is this OK??? */
        /* dev_put(vlanDevCtrl->vlanDev); */

        /* FIXME: Do we need to remove both pointers from the Info structure here ? */
        BCM_VLAN_DEV_INFO(vlanDevCtrl->vlanDev)->vlanDevCtrl = NULL;
//        BCM_VLAN_DEV_INFO(vlanDevCtrl->vlanDev)->realDev = NULL;

        unregister_netdevice(vlanDevCtrl->vlanDev);

        nextVlanDevCtrl = vlanDevCtrl;

        nextVlanDevCtrl = BCM_VLAN_LL_GET_NEXT(nextVlanDevCtrl);

        BCM_VLAN_LL_REMOVE(&realDevCtrl->vlanDevCtrlLL, vlanDevCtrl);

        bcmVlan_removeTagRulesByVlanDev(vlanDevCtrl);

        freeVlanDevCtrl(vlanDevCtrl);

        vlanDevCtrl = nextVlanDevCtrl;
    };

    return count;
}

/*
 * Free the VLAN Devices of ALL Real Devices
 *
 * Entry points: CLEANUP
 */
static void freeAllVlanDevices(void)
{
    struct realDeviceControl *realDevCtrl, *nextRealDevCtrl;

    rtnl_lock();

    realDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrlLL);

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Cleaning-up Real Devices (0x%08lX)", (uint32)realDevCtrl);

    while(realDevCtrl)
    {
        BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Cleaning-up Real Device %s", realDevCtrl->realDev->name);

        freeVlanDevices(realDevCtrl);

        nextRealDevCtrl = realDevCtrl;

        nextRealDevCtrl = BCM_VLAN_LL_GET_NEXT(nextRealDevCtrl);

        BCM_VLAN_LL_REMOVE(&realDevCtrlLL, realDevCtrl);

        bcmVlan_cleanupRuleTables(realDevCtrl);

        freeRealDevCtrl(realDevCtrl);

        realDevCtrl = nextRealDevCtrl;
    };

    rtnl_unlock();
}


/*
 * Global Functions
 */

int bcmVlan_initVlanDevices(void)
{
    /* create a slab cache for device descriptors */
    vlanDevCtrlCache = kmem_cache_create("bcmvlan_vlanDev",
                                         sizeof(struct vlanDeviceControl),
                                         0, /* align */
                                         0, /* flags */
                                         NULL); /* ctor */
    if(vlanDevCtrlCache == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to create VLAN Devices Cache");

        return -ENOMEM;
    }

    /* create a slab cache for device descriptors */
    realDevCtrlCache = kmem_cache_create("bcmvlan_realDev",
                                         sizeof(struct realDeviceControl),
                                         0, /* align */
                                         0, /* flags */
                                         NULL); /* ctor */
    if(realDevCtrlCache == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to create Real Devices Cache");

        kmem_cache_destroy(vlanDevCtrlCache);

        return -ENOMEM;
    }

    BCM_VLAN_LL_INIT(&realDevCtrlLL);

    return 0;
}

void bcmVlan_cleanupVlanDevices(void)
{
    freeAllVlanDevices();
    kmem_cache_destroy(vlanDevCtrlCache);
    kmem_cache_destroy(realDevCtrlCache);
}

/*
 * IMPORTANT : This function MUST only be called within critical regions
 */
struct realDeviceControl *bcmVlan_getRealDevCtrl(struct net_device *realDev)
{
    struct realDeviceControl *realDevCtrl;

    BCM_ASSERT(realDev);

    realDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrlLL);

    /* find the device control structure associated to the real device */
    while(realDevCtrl)
    {
        if(realDevCtrl->realDev == realDev)
        {
            break;
        }

        realDevCtrl = BCM_VLAN_LL_GET_NEXT(realDevCtrl);
    };

/*     if(realDevCtrl == NULL) */
/*     { */
/*         BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "%s has no VLAN Interfaces", realDev->name); */
/*     } */

    return realDevCtrl;
}

/*
 * Free all VLAN Devices of a specific Real Device
 *
 * Entry points: NOTIFIER
 */
void bcmVlan_freeRealDeviceVlans(struct net_device *realDev)
{
    struct realDeviceControl *realDevCtrl;

    BCM_ASSERT(realDev);

    realDevCtrl = bcmVlan_getRealDevCtrl(realDev);

    if(realDevCtrl != NULL)
    {
        BCM_LOG_INFO(BCM_LOG_ID_VLAN, "De-allocating VLAN Devices of %s", realDev->name);

        freeVlanDevices(realDevCtrl);

        /* there are no more VLAN interfaces associated to the real device */
        BCM_VLAN_LL_REMOVE(&realDevCtrlLL, realDevCtrl);

        bcmVlan_cleanupRuleTables(realDevCtrl);

        freeRealDevCtrl(realDevCtrl);
    }
}

/*
 * Free a specific VLAN Device
 *
 * Entry points: IOCTL
 */
void bcmVlan_freeVlanDevice(struct net_device *vlanDev)
{
    struct vlanDeviceControl *vlanDevCtrl;
    struct realDeviceControl *realDevCtrl;

    BCM_ASSERT(vlanDev);

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    vlanDevCtrl = BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl;
    realDevCtrl = vlanDevCtrl->realDevCtrl;

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "De-allocating %s", vlanDevCtrl->vlanDev->name);

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "head 0x%08lX, tail 0x%08lX",
                  (uint32)(&realDevCtrl->vlanDevCtrlLL)->head,
                  (uint32)realDevCtrl->vlanDevCtrlLL.tail);

    BCM_VLAN_LL_REMOVE(&realDevCtrl->vlanDevCtrlLL, vlanDevCtrl);

    bcmVlan_removeTagRulesByVlanDev(vlanDevCtrl);

    /* FIXME: Do we need to remove both pointers from the Info structure here ? */
    BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl = NULL;
//    BCM_VLAN_DEV_INFO(vlanDev)->realDev = NULL;

    freeVlanDevCtrl(vlanDevCtrl);

    if(BCM_VLAN_LL_IS_EMPTY(&realDevCtrl->vlanDevCtrlLL))
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device has no more VLANs: De-allocate");

        /* there are no more VLAN interfaces associated to the real device */
        BCM_VLAN_LL_REMOVE(&realDevCtrlLL, realDevCtrl);

        bcmVlan_cleanupRuleTables(realDevCtrl);

        freeRealDevCtrl(realDevCtrl);
    }

    BCM_LOG_INFO(BCM_LOG_ID_VLAN, "De-allocated VLAN Device %s", vlanDev->name);

    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/
}

/*
 * IMPORTANT : This function MUST only be called within critical regions
 *
 * Entry points: IOCTL indirect
 */
struct net_device *bcmVlan_getRealDeviceByName(char *realDevName, struct realDeviceControl **pRealDevCtrl)
{
    struct realDeviceControl *realDevCtrl;
    struct net_device *realDev = NULL;

    /* NULL terminate name, just in case */
    realDevName[IFNAMSIZ-1] = '\0';

    realDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrlLL);

    while(realDevCtrl)
    {
        if(!strcmp(realDevCtrl->realDev->name, realDevName))
        {
            realDev = realDevCtrl->realDev;
            break;
        }

        realDevCtrl = BCM_VLAN_LL_GET_NEXT(realDevCtrl);
    };

    if(pRealDevCtrl != NULL)
    {
        *pRealDevCtrl = realDevCtrl;
    }

    return realDev;
}

/*
 * Entry points: IOCTL indirect
 */
struct net_device *bcmVlan_getVlanDeviceByName(struct realDeviceControl *realDevCtrl, char *vlanDevName)
{
    struct vlanDeviceControl *vlanDevCtrl;
    struct net_device *vlanDev = NULL;

    BCM_ASSERT(realDevCtrl);

    /* NULL terminate name, just in case */
    vlanDevName[IFNAMSIZ-1] = '\0';

    vlanDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrl->vlanDevCtrlLL);

    while(vlanDevCtrl)
    {
        if(!strcmp(vlanDevCtrl->vlanDev->name, vlanDevName))
        {
            vlanDev = vlanDevCtrl->vlanDev;
            break;
        }

        vlanDevCtrl = BCM_VLAN_LL_GET_NEXT(vlanDevCtrl);
    };

    return vlanDev;
}

/*
 * This function must be called after the new vlan device has been registered
 in Linux because we do not check for pre-existing devices here. We rely on
 Linux to do it
 *
 * Entry points: IOCTL indirect
 */
int bcmVlan_createVlanDevice(struct net_device *realDev, struct net_device *vlanDev,
                             bcmVlan_vlanDevFlags_t flags)
{
    int ret = 0;
    struct vlanDeviceControl *vlanDevCtrl;
    struct realDeviceControl *realDevCtrl;

    BCM_ASSERT(realDev);
    BCM_ASSERT(vlanDev);

    /* Make sure the caller has the RTNL lock */
    ASSERT_RTNL();

    vlanDevCtrl = allocVlanDevCtrl();
    if(vlanDevCtrl == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to allocate Device Control memory for %s",
                      vlanDev->name);

        ret = -ENOMEM;
        goto out;
    }

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDevCtrl = bcmVlan_getRealDevCtrl(realDev);
    if(realDevCtrl == NULL)
    {
        /* allocate device control */
        realDevCtrl = allocRealDevCtrl();
        if(realDevCtrl == NULL)
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to allocate Device Control memory for %s",
                          realDev->name);

            freeVlanDevCtrl(vlanDevCtrl);

            ret = -ENOMEM;
            goto out;
        }

        /* initialize real device */
        realDevCtrl->realDev = realDev;

        BCM_VLAN_LL_INIT(&realDevCtrl->vlanDevCtrlLL);

        bcmVlan_initTpidTable(realDevCtrl);

        bcmVlan_initRuleTables(realDevCtrl);

        /* initialize local stats */
        memset(&realDevCtrl->localStats, 0, sizeof(bcmVlan_localStats_t));

        realDevCtrl->mode = BCM_VLAN_MODE_ONT;

        /* insert real device into the main linked list */
        BCM_VLAN_LL_APPEND(&realDevCtrlLL, realDevCtrl);
    }

    /* initialize VLAN Device */
    vlanDevCtrl->vlanDev = vlanDev;
    vlanDevCtrl->realDevCtrl = realDevCtrl;
    vlanDevCtrl->flags = flags;

    /* insert vlan device into the real device's linked list */
    BCM_VLAN_LL_APPEND(&realDevCtrl->vlanDevCtrlLL, vlanDevCtrl);

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "head 0x%08lX, tail 0x%08lX",
                  (uint32)realDevCtrl->vlanDevCtrlLL.head,
                  (uint32)realDevCtrl->vlanDevCtrlLL.tail);

    /* update pointer to the new vlanDevCtrl in the VLAN device structure */
    BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl = vlanDevCtrl;
    BCM_VLAN_DEV_INFO(vlanDev)->realDev = realDev;

    BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Created VLAN Device %s", vlanDev->name);

out:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL, NOTIFIER
 */
void bcmVlan_transferOperstate(struct net_device *realDev)
{
    struct realDeviceControl *realDevCtrl;
    struct vlanDeviceControl *vlanDevCtrl;

    BCM_ASSERT(realDev);

    realDevCtrl = bcmVlan_getRealDevCtrl(realDev);
    if(realDevCtrl == NULL)
    {
        return;
    }

    vlanDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrl->vlanDevCtrlLL);

    while(vlanDevCtrl)
    {
        BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Updating Operation State of %s",
                     vlanDevCtrl->vlanDev->name);

        /* Have to respect userspace enforced dormant state
         * of real device, also must allow supplicant running
         * on VLAN device
         */
        if(realDev->operstate == IF_OPER_DORMANT)
        {
            netif_dormant_on(vlanDevCtrl->vlanDev);
        }
        else
        {
            netif_dormant_off(vlanDevCtrl->vlanDev);
        }

        if(netif_carrier_ok(realDev))
        {
            if(!netif_carrier_ok(vlanDevCtrl->vlanDev))
            {
                netif_carrier_on(vlanDevCtrl->vlanDev);
            }
        }
        else
        {
            if(netif_carrier_ok(vlanDevCtrl->vlanDev))
            {
                netif_carrier_off(vlanDevCtrl->vlanDev);
            }
        }

        vlanDevCtrl = BCM_VLAN_LL_GET_NEXT(vlanDevCtrl);
    };
}

/*
 * Entry points: NOTIFIER
 */
void bcmVlan_updateInterfaceState(struct net_device *realDev, int state)
{
    int flags;
    struct realDeviceControl *realDevCtrl;
    struct vlanDeviceControl *vlanDevCtrl;

    BCM_ASSERT(realDev);

    realDevCtrl = bcmVlan_getRealDevCtrl(realDev);
    if(realDevCtrl == NULL)
    {
        return;
    }

    vlanDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrl->vlanDevCtrlLL);

    while(vlanDevCtrl)
    {
        flags = vlanDevCtrl->vlanDev->flags;

        if(state == NETDEV_UP)
        {
            if(!(flags & IFF_UP))
            {
                BCM_LOG_INFO(BCM_LOG_ID_VLAN, "%s is UP", vlanDevCtrl->vlanDev->name);
                dev_change_flags(vlanDevCtrl->vlanDev, flags & IFF_UP);
            }
        }
        else
        {
            if(flags & IFF_UP)
            {
                BCM_LOG_INFO(BCM_LOG_ID_VLAN, "%s is DOWN", vlanDevCtrl->vlanDev->name);
                dev_change_flags(vlanDevCtrl->vlanDev, flags & ~IFF_UP);
            }
        }

        vlanDevCtrl = BCM_VLAN_LL_GET_NEXT(vlanDevCtrl);
    };
}

void bcmVlan_dumpPacket(unsigned int *tpidTable, struct sk_buff *skb)
{
#ifdef BCM_VLAN_DATAPATH_DEBUG
    int i;
    bcmVlan_ethHeader_t *ethHeader = BCM_VLAN_SKB_ETH_HEADER(skb);
    bcmVlan_vlanHeader_t *vlanHeader = &ethHeader->vlanHeader;
    bcmVlan_vlanHeader_t *prevVlanHeader = vlanHeader - 1;
    bcmVlan_ipHeader_t *ipHeader;

    if(bcmLog_getLogLevel(BCM_LOG_ID_VLAN) != BCM_LOG_LEVEL_DEBUG)
    {
        return;
    }

    printk("-> SKB (0x%08X)\n", (unsigned int)(skb));
    printk("Priority    : %d\n", skb->priority);
    printk("Mark.FlowId : %d\n", SKBMARK_GET_FLOW_ID(skb->mark));
    printk("Mark.Port   : %d\n", SKBMARK_GET_PORT(skb->mark));
    printk("Mark.Queue  : %d\n", SKBMARK_GET_Q(skb->mark));
    printk("\n");

    printk("-> L2 Header (0x%08X)\n", (unsigned int)(ethHeader));

    printk("DA    : ");
    for(i=0; i<ETH_ALEN; ++i)
    {
        printk("%02X ", ethHeader->macDest[i]);
    }
    printk("\n");

    printk("SA    : ");
    for(i=0; i<ETH_ALEN; ++i)
    {
        printk("%02X ", ethHeader->macSrc[i]);
    }
    printk("\n");

    printk("ETHER : %04X\n", ethHeader->etherType);

    if(BCM_VLAN_TPID_MATCH(tpidTable, ethHeader->etherType))
    {
        printk("\n");

        do {
            printk("-> VLAN Header (0x%08X)\n", (unsigned int)(vlanHeader));
            printk("PBITS : %d\n", BCM_VLAN_GET_TCI_PBITS(vlanHeader));
            printk("CFI   : %d\n", BCM_VLAN_GET_TCI_CFI(vlanHeader));
            printk("VID   : %d\n", BCM_VLAN_GET_TCI_VID(vlanHeader));
            printk("ETHER : %04X\n", vlanHeader->etherType);
            printk("\n");
            prevVlanHeader = vlanHeader;
        } while((vlanHeader = BCM_VLAN_GET_NEXT_VLAN_HEADER(tpidTable, vlanHeader)) != NULL);
    }

    if(prevVlanHeader->etherType == 0x0800)
    {
        ipHeader = (bcmVlan_ipHeader_t *)(prevVlanHeader + 1);
    }
    else if(prevVlanHeader->etherType == 0x8864)
    {
        bcmVlan_pppoeSessionHeader_t *pppoeSessionHeader = (bcmVlan_pppoeSessionHeader_t *)(prevVlanHeader + 1);

        printk("-> PPPoE Header (0x%08lX)\n", (UINT32)pppoeSessionHeader);

        printk("version_type : 0x%02X\n", pppoeSessionHeader->version_type);
        printk("code         : 0x%02X\n", pppoeSessionHeader->code);
        printk("sessionId    : 0x%04X\n", pppoeSessionHeader->sessionId);
        printk("pppHeader    : 0x%04X\n", pppoeSessionHeader->pppHeader);

        ipHeader = &pppoeSessionHeader->ipHeader;
    }
    else
    {
        ipHeader = NULL;

        printk("Unknown L3 Header: 0x%04X\n", prevVlanHeader->etherType);
    }

    if(ipHeader)
    {
        printk("\n");

        printk("-> IP Header (0x%08X)\n", (unsigned int)(ipHeader));

        if(BCM_VLAN_GET_IP_VERSION(ipHeader) == 4)
        {
#if 0
            UINT32 *p = (UINT32*)ipHeader;
            for(i=0; i<5; ++i)
            {
                printk("0x%08lX\n", p[i]);
            }
#endif
            printk("Type   : IPv4\n");
            printk("DSCP   : %d\n", BCM_VLAN_GET_IP_DSCP(ipHeader));
            printk("Proto  : %d\n", ipHeader->protocol);
            printk("IP Src : %d.%d.%d.%d\n",
                   (ipHeader->ipSrc & 0xFF000000) >> 24,
                   (ipHeader->ipSrc & 0x00FF0000) >> 16,
                   (ipHeader->ipSrc & 0x0000FF00) >> 8,
                   (ipHeader->ipSrc & 0x000000FF));
            printk("IP Dst : %d.%d.%d.%d\n",
                   (ipHeader->ipDest & 0xFF000000) >> 24,
                   (ipHeader->ipDest & 0x00FF0000) >> 16,
                   (ipHeader->ipDest & 0x0000FF00) >> 8,
                   (ipHeader->ipDest & 0x000000FF));
        }
        else
        {
            printk("Not IPv4\n");
        }
    }

    printk("\n");
#endif
}
