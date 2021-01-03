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
// File Name  : bcm_vlan.c
//
// Description: Broadcom VLAN Interface Driver
//               
//**************************************************************************

#include "bcm_vlan_local.h"
#include "bcm_vlan_dev.h"


#define BCM_VLAN_MODULE_NAME    "Broadcom 802.1Q VLAN Interface"
#define BCM_VLAN_MODULE_VERSION "0.1"


/*
 * Local variables
 */
static int deviceEventHandler(struct notifier_block *unused, unsigned long event, void *ptr);

static struct notifier_block vlanNotifierBlock = {
    .notifier_call = deviceEventHandler,
};


/*
 * Global variables
 */
extern int (*bcm_vlan_handle_frame_hook)(struct sk_buff *, struct net_device *);


/* 
 * Local Functions
 */
static int deviceEventHandler(struct notifier_block *unused, unsigned long event, void *ptr)
{
    struct net_device *dev = ptr;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    /* We run under the RTNL lock here */

    if(dev->priv_flags & IFF_BCM_VLAN)
    {
        /* We should only process non-VLAN devices */
        goto out;
    }

    switch (event) {

	case NETDEV_CHANGE:
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "NETDEV_CHANGE");
            /* Propagate real device state to vlan devices */
            bcmVlan_transferOperstate(dev);
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Done");
            break;

	case NETDEV_DOWN:
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "NETDEV_DOWN");
            /* Put all VLANs for this dev in the down state too.  */
            bcmVlan_updateInterfaceState(dev, NETDEV_DOWN);
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Done");
            break;

	case NETDEV_UP:
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "NETDEV_UP");
            /* Put all VLANs for this dev in the up state too.  */
            bcmVlan_updateInterfaceState(dev, NETDEV_UP);
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Done");
            break;

	case NETDEV_UNREGISTER:
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "NETDEV_UNREGISTER");
            /* Delete all VLAN interfaces of this real device */
            bcmVlan_freeRealDeviceVlans(dev);
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Done");
            break;
    };

out:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return NOTIFY_DONE;
}

static int __init moduleInit(void)
{
    int ret;

    printk(KERN_INFO "%s, v%s\n", BCM_VLAN_MODULE_NAME, BCM_VLAN_MODULE_VERSION);

    ret = bcmVlan_initVlanDevices();
    if(ret)
    {
        return ret;
    }

    ret = bcmVlan_initTagRules();
    if(ret)
    {
        bcmVlan_cleanupVlanDevices();

        return ret;
    }

    ret = bcmVlan_userInit();
    if(ret)
    {
        bcmVlan_cleanupVlanDevices();

        bcmVlan_cleanupTagRules();

        return ret;
    }

    /* Register to receive netdevice events */
    ret = register_netdevice_notifier(&vlanNotifierBlock);
    if (ret < 0)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to register notifier (%d)", ret);

        bcmVlan_cleanupVlanDevices();

        bcmVlan_cleanupTagRules();

        bcmVlan_userCleanup();
    }

    /* register our receive packet handler */
    bcm_vlan_handle_frame_hook = bcmVlan_devReceiveSkb;

    return ret;
}

/*
 *     Module 'remove' entry point.
 *     o delete /proc/net/router directory and static entries.
 */
static void __exit moduleCleanup(void)
{
    bcmVlan_cleanupVlanDevices();

    bcmVlan_cleanupTagRules();

    bcmVlan_userCleanup();

    /* Unregister from receiving netdevice events */
    unregister_netdevice_notifier(&vlanNotifierBlock);

    /* unregister our receive packet handler */
    bcm_vlan_handle_frame_hook = NULL;
}

module_init(moduleInit);
module_exit(moduleCleanup);

MODULE_LICENSE("Proprietary");
MODULE_VERSION(BCM_VLAN_MODULE_VERSION);
