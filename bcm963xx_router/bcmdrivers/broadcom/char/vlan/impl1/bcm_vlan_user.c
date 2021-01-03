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
// File Name  : bcm_vlan_user.c
//
// Description: Broadcom VLAN Interface Driver
//               
//**************************************************************************

#include "bcm_vlan_local.h"
#include "bcm_vlan_dev.h"
#include <board.h>
#include <linux/version.h>
#include <linux/ethtool.h>
#include <net/net_namespace.h>


#define BCM_VLAN_DEVICE_NAME  "bcmvlan"

/* defined in CommEngine/targets/makeDevs */
#define BCM_VLAN_DEVICE_MAJOR 238


/*
 * vlan network devices have devices nesting below it, and are a special
 * "super class" of normal network devices; split their locks off into a
 * separate class since they always nest.
 */
static int vlanMajor = BCM_VLAN_DEVICE_MAJOR;

static char bcmVlan_ifNameSuffix[BCM_VLAN_IF_SUFFIX_SIZE];


/*
 * Local functions
 */

static int unregisterVlanDevice(const char *vlanDevName)
{
    struct net_device *vlanDev;
    struct net_device *realDev;
    int ret = 0;

    vlanDev = dev_get_by_name(&init_net, vlanDevName);
    if (vlanDev != NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Found %s (0x%08lX)", vlanDev->name, (uint32)vlanDev);

        if(vlanDev->priv_flags & IFF_BCM_VLAN)
        {
            rtnl_lock();

            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "0x%08lX -> 0x%08lX -> 0x%08lX -> 0x%08lX -> 0x%08lX",
                          (uint32)vlanDev,
                          (uint32)(netdev_priv(vlanDev)),
                          (uint32)(((bcmVlan_devInfo_t *)(netdev_priv(vlanDev)))->vlanDevCtrl),
                          (uint32)(((bcmVlan_devInfo_t *)(netdev_priv(vlanDev)))->vlanDevCtrl->realDevCtrl),
                          (uint32)(((bcmVlan_devInfo_t *)(netdev_priv(vlanDev)))->vlanDevCtrl->realDevCtrl->realDev));

            realDev = BCM_VLAN_REAL_DEV(vlanDev);

            if(BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl->flags.routed)
            {
                kerSysReleaseMacAddress(vlanDev->dev_addr);
            }

            bcmVlan_freeVlanDevice(vlanDev);

            /* decrement reference counter of the real device, since we
               added a reference in register_vlan_device() when the VLAN
               device was created */
            dev_put(realDev);

            /* unregister_netdevice() also calls dev_put() for the device being unregistered */
            dev_put(vlanDev);

            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "realDev->refcnt %d, vlanDev->refcnt %d",
                          realDev->refcnt.counter, vlanDev->refcnt.counter);

            unregister_netdevice(vlanDev);

            rtnl_unlock();
        }
        else
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN,
                          "Tried to remove the non-VLAN device %s with priv_flags=0x%04X",
                          vlanDev->name, vlanDev->priv_flags);

            ret = -EPERM;
        }
    }
    else
    {
        BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Could not find VLAN Device %s", vlanDevName);

        ret = -EINVAL;
    }

    return ret;
}

#define BCM_VLAN_DRV_VERSION "1.0"

const char bcmVlan_fullname[] = "Broadcom VLAN Interface";
const char bcmVlan_version[] = BCM_VLAN_DRV_VERSION;

static int bcmVlan_ethtoolGetSettings(struct net_device *vlanDev,
                                      struct ethtool_cmd *cmd)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    if (!realDev->ethtool_ops ||
        !realDev->ethtool_ops->get_settings)
        return -EOPNOTSUPP;

    return realDev->ethtool_ops->get_settings(realDev, cmd);
}

static void bcmVlan_ethtoolGetDrvinfo(struct net_device *vlanDev,
                                      struct ethtool_drvinfo *info)
{
    strcpy(info->driver, bcmVlan_fullname);
    strcpy(info->version, bcmVlan_version);
    strcpy(info->fw_version, "N/A");
}

static u32 bcmVlan_ethtoolGetRxCsum(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    if (realDev->ethtool_ops == NULL ||
        realDev->ethtool_ops->get_rx_csum == NULL)
        return 0;

    return realDev->ethtool_ops->get_rx_csum(realDev);
}

static u32 bcmVlan_ethtoolGetFlags(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    if (!(realDev->features & NETIF_F_HW_VLAN_RX) ||
        realDev->ethtool_ops == NULL ||
        realDev->ethtool_ops->get_flags == NULL)
        return 0;

    return realDev->ethtool_ops->get_flags(realDev);
}

static const struct ethtool_ops bcmVlan_ethtool_ops = {
    .get_settings = bcmVlan_ethtoolGetSettings,
    .get_drvinfo  = bcmVlan_ethtoolGetDrvinfo,
    .get_link     = ethtool_op_get_link,
    .get_rx_csum  = bcmVlan_ethtoolGetRxCsum,
    .get_flags    = bcmVlan_ethtoolGetFlags,
};

const struct header_ops bcmVlan_header_ops = {
    .create  = bcmVlan_devHardHeader,
    .rebuild = bcmVlan_devRebuildHeader,
    .parse   = eth_header_parse,
};

const struct net_device_ops bcmVlan_netdev_ops = {
    .ndo_change_mtu         = bcmVlan_devChangeMtu,
    .ndo_init		    = bcmVlan_devInit,
    .ndo_uninit		    = bcmVlan_devUninit,
    .ndo_open		    = bcmVlan_devOpen,
    .ndo_stop		    = bcmVlan_devStop,
    .ndo_start_xmit         = bcmVlan_devHardStartXmit,
    .ndo_validate_addr	    = eth_validate_addr,
    .ndo_set_mac_address    = bcmVlan_devSetMacAddress,
    .ndo_set_rx_mode        = bcmVlan_devSetRxMode,
    .ndo_set_multicast_list = bcmVlan_devSetRxMode,
    .ndo_change_rx_flags    = bcmVlan_devChangeRxFlags,
    .ndo_do_ioctl	    = bcmVlan_devIoctl,
    .ndo_neigh_setup	    = bcmVlan_devNeighSetup,
#ifdef CONFIG_BLOG
    .ndo_get_stats          = bcmVlan_devCollectStats,
#else
    .ndo_get_stats          = bcmVlan_devGetStats,
#endif
};

static void vlanSetup(struct net_device *vlanDev)
{
    ether_setup(vlanDev);

    vlanDev->priv_flags |= IFF_BCM_VLAN;
    vlanDev->tx_queue_len = 0;

    vlanDev->netdev_ops  = &bcmVlan_netdev_ops;
    vlanDev->destructor	 = free_netdev;
    vlanDev->ethtool_ops = &bcmVlan_ethtool_ops;

#ifdef CONFIG_MIPS_BRCM
#ifdef CONFIG_BLOG
    vlanDev->put_stats = bcmVlan_devUpdateStats;
    vlanDev->clr_stats = bcmVlan_devClearStats;
#endif
#endif

    memset(vlanDev->broadcast, 0, ETH_ALEN);
}

static int allocMacAddress(struct net_device *vlanDev)
{
    int ret = 0;
    struct sockaddr addr;
    unsigned long unit = 0, connId = 0, macId = 0;
    char *p;
    int i;
          
    addr.sa_data[0] = 0xFF;

    /* Format the mac id */
    i = strcspn(vlanDev->name, "0123456789");
    if(i > 0)
    {
        unit = simple_strtoul(&(vlanDev->name[i]), NULL, 10);
    }

    p = strstr(vlanDev->name, bcmVlan_ifNameSuffix);
    if(p != NULL)
    {
        connId = simple_strtoul(p + strlen(bcmVlan_ifNameSuffix), NULL, 10);
    }

    macId = kerSysGetMacAddressType(vlanDev->name);

    /* set unit number to bit 20-27, connId to bit 12-19. */
    macId |= ((unit & 0xFF) << 20) | ((connId & 0xFF) << 12);

    kerSysGetMacAddress(addr.sa_data, macId);

    if((addr.sa_data[0] & 0x01) == 0x01)
    {
        printk("Unable to get MAC address from persistent storage\n");

        ret = -EADDRNOTAVAIL;
        goto out;
    }

    ret = bcmVlan_devSetMacAddress(vlanDev, &addr);
    if(ret)
    {
        kerSysReleaseMacAddress(addr.sa_data);
    }

out:
    return ret;
}

static struct net_device *registerVlanDevice(const char *realDevName, int vlanDevId,
                                             int isRouted, int isMulticast)
{
    struct net_device *newDev;
    struct net_device *realDev; /* the ethernet device */
    struct net *net;
    char name[IFNAMSIZ];
    bcmVlan_vlanDevFlags_t vlanDevFlags;
    int ret;

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Creating VLAN Interface on %s", realDevName);

    realDev = dev_get_by_name(&init_net, realDevName);
    if(!realDev)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Could not find Real Device %s", realDevName);
        goto out_ret_null;
    }

    if(realDev->features & NETIF_F_VLAN_CHALLENGED)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "VLANs not supported by %s", realDev->name);
        goto out_put_dev;
    }

    if(realDev->features & (NETIF_F_HW_VLAN_RX |
                            NETIF_F_HW_VLAN_TX |
                            NETIF_F_HW_VLAN_FILTER))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "HW Accelerator is not supported (%s)", realDev->name);
        goto out_put_dev;
    }

    /* We will make changes to a net_device structure, so get the
       routing netlink semaphore */
    rtnl_lock();

    /* The real device must be up and operating in order to
       assosciate a VLAN device with it */
    if (!(realDev->flags & IFF_UP))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Device %s is not UP", realDev->name);
        goto out_unlock;
    }

    /* allocate net_device structure for the new vlan interface */
    snprintf(name, IFNAMSIZ, "%s%s%d", realDev->name, bcmVlan_ifNameSuffix, vlanDevId);

    newDev = alloc_netdev(sizeof(bcmVlan_devInfo_t), name, vlanSetup);

    if (newDev == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to allocate %s for %s", name, realDev->name);
        goto out_unlock;
    }

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Allocated new VLAN Interface: %s", newDev->name);

    newDev->priv_flags |= realDev->priv_flags;

    net = dev_net(realDev);
    dev_net_set(newDev, net);

    /* FIXME: need N*4 bytes for extra VLAN header info, hope the underlying
       device can handle it */
    newDev->mtu = realDev->mtu;

    /* Initialize some VLAN dev info members */
    BCM_VLAN_DEV_INFO(newDev)->realDev = realDev;

    /* FIXME: Do we need to support Netlink ops??? */
//    newDev->rtnl_link_ops = ;

    if (register_netdevice(newDev))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to REGISTER VLAN Device %s", newDev->name);
        goto out_free_newdev;
    }

/*     BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "After register_netdevice(): realDev->refcnt %d, vlanDev->refcnt %d", */
/*                   realDev->refcnt.counter, vlanDev->refcnt.counter); */

//    dev_hold(realDev);  --> dev_get_by_name() already calls dev_hold() for the real device

    /* register_netdevice() checks for uniqueness of device name,
       so we don't have to worry about checking if the new device
       already exists */

    vlanDevFlags.u32 = 0;
    vlanDevFlags.routed = (isRouted) ? 1 : 0;
    vlanDevFlags.multicast = (isMulticast) ? 1 : 0;

    ret = bcmVlan_createVlanDevice(realDev, newDev, vlanDevFlags);
    if(ret)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to ADD VLAN Device %s", newDev->name);

        unregister_netdevice(newDev);
        goto out_free_newdev;
    }

/*     BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "After bcmVlan_createVlanDevice(): realDev->refcnt %d, vlanDev->refcnt %d", */
/*                   realDev->refcnt.counter, vlanDev->refcnt.counter); */

    bcmVlan_transferOperstate(realDev);

    /* FIXME: What is this ??? Increment vlanDev refcounter! */
    linkwatch_fire_event(newDev); /* _MUST_ call rfc2863_policy() */

    if(isRouted)
    {
        ret = allocMacAddress(newDev);
        if(ret)
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to allocate %s MAC Address", newDev->name);

            unregister_netdevice(newDev);
            goto out_free_newdev;
        }
    }

    rtnl_unlock();

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Registered new VLAN Interface: %s", newDev->name);

    return newDev;

out_free_newdev:
    free_netdev(newDev);

out_unlock:
    rtnl_unlock();

out_put_dev:
    dev_put(realDev);

out_ret_null:
    return NULL;
}

static int vlanOpen(struct inode *ip, struct file *fp)
{
//    BCM_LOG_FUNC(BCM_LOG_ID_VLAN);

    return 0;
}

static int vlanClose(struct inode *ip, struct file *fp)
{
//    BCM_LOG_FUNC(BCM_LOG_ID_VLAN);

    return 0;
}

static int dumpLocalStats(char *realDevName)
{
    int ret = 0;
    struct net_device *realDev = dev_get_by_name(&init_net, realDevName);

    if(realDev != NULL)
    {
        struct realDeviceControl *realDevCtrl;

        /******** CRITICAL REGION BEGIN ********/
        BCM_VLAN_LOCK();

        realDevCtrl = bcmVlan_getRealDevCtrl(realDev);

        if(realDevCtrl != NULL)
        {
            bcmVlan_localStats_t *localStats = &realDevCtrl->localStats;

            printk("*** %s Local Stats ***\n", realDev->name);

            printk("rx_Misses             : %ld\n", localStats->rx_Misses);
            printk("tx_Misses             : %ld\n", localStats->tx_Misses);
            printk("error_PopUntagged     : %ld\n", localStats->error_PopUntagged);
            printk("error_PopNoMem        : %ld\n", localStats->error_PopNoMem);
            printk("error_PushTooManyTags : %ld\n", localStats->error_PushTooManyTags);
            printk("error_PushNoMem       : %ld\n", localStats->error_PushNoMem);
            printk("error_SetEtherType    : %ld\n", localStats->error_SetEtherType);
            printk("error_SetTagEtherType : %ld\n", localStats->error_SetTagEtherType);
            printk("error_InvalidTagNbr   : %ld\n", localStats->error_InvalidTagNbr);
            printk("error_UnknownL3Header : %ld\n", localStats->error_UnknownL3Header);
        }
        else
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "%s has no BCM VLAN Interfaces", realDev->name);

            ret = -EINVAL;
        }

        BCM_VLAN_UNLOCK();
        /******** CRITICAL REGION END ********/

        dev_put(realDev);
    }

    return ret;
}

static int vlanIoctl(struct inode *ip, struct file *fp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Command = %d", cmd);

    switch((bcmVlan_ioctlCmd_t)(cmd))
    {
        case BCM_VLAN_IOC_CREATE_VLAN:
        {
            struct net_device *vlanDev;
            bcmVlan_iocCreateVlan_t iocCreateVlan;

            copy_from_user(&iocCreateVlan, (bcmVlan_iocCreateVlan_t *)arg,
                           sizeof(bcmVlan_iocCreateVlan_t));

            vlanDev = registerVlanDevice(iocCreateVlan.realDevName, iocCreateVlan.vlanDevId,
                                         iocCreateVlan.isRouted, iocCreateVlan.isMulticast);
            if(vlanDev == NULL)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to create VLAN device (%s, %d)",
                              iocCreateVlan.realDevName, iocCreateVlan.vlanDevId);
                ret = -ENODEV;
            }

            break;
        }

        case BCM_VLAN_IOC_DELETE_VLAN:
        {
            bcmVlan_iocDeleteVlan_t iocDeleteVlan;

            copy_from_user(&iocDeleteVlan, (bcmVlan_iocDeleteVlan_t *)arg,
                           sizeof(bcmVlan_iocDeleteVlan_t));

            ret = unregisterVlanDevice(iocDeleteVlan.vlanDevName);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to delete VLAN device %s",
                              iocDeleteVlan.vlanDevName);
            }

            break;
        }

        case BCM_VLAN_IOC_INSERT_TAG_RULE:
        {
            bcmVlan_iocInsertTagRule_t iocInsertTagRule;

            copy_from_user(&iocInsertTagRule, (bcmVlan_iocInsertTagRule_t *)arg,
                           sizeof(bcmVlan_iocInsertTagRule_t));

            ret = bcmVlan_insertTagRule(iocInsertTagRule.ruleTableId.realDevName,
                                        iocInsertTagRule.ruleTableId.nbrOfTags,
                                        iocInsertTagRule.ruleTableId.tableDir,
                                        &iocInsertTagRule.tagRule,
                                        iocInsertTagRule.position,
                                        iocInsertTagRule.posTagRuleId);
            if(ret)
            {
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Failed to Insert Tag Rule in %s (tags=%d, dir=%d)",
                              iocInsertTagRule.ruleTableId.realDevName, iocInsertTagRule.ruleTableId.nbrOfTags,
                              (int)iocInsertTagRule.ruleTableId.tableDir);
            }
            else
            {
                BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Inserted Tag Rule in %s (tags=%d, dir=%d, id=%d)",
                             iocInsertTagRule.ruleTableId.realDevName, iocInsertTagRule.ruleTableId.nbrOfTags,
                             (int)iocInsertTagRule.ruleTableId.tableDir, (int)iocInsertTagRule.tagRule.id);

                /* send argument back, containing the assigned rule tag Id */
                copy_to_user((bcmVlan_iocInsertTagRule_t *)arg, &iocInsertTagRule,
                             sizeof(bcmVlan_iocInsertTagRule_t));
            }

            break;
        }

        case BCM_VLAN_IOC_REMOVE_TAG_RULE:
        {
            bcmVlan_iocRemoveTagRule_t iocRemoveTagRule;

            copy_from_user(&iocRemoveTagRule, (bcmVlan_iocRemoveTagRule_t *)arg,
                           sizeof(bcmVlan_iocRemoveTagRule_t));

            if(iocRemoveTagRule.tagRuleId == BCM_VLAN_DONT_CARE)
            {
                ret = bcmVlan_removeTagRuleByFilter(iocRemoveTagRule.ruleTableId.realDevName,
                                                    iocRemoveTagRule.ruleTableId.nbrOfTags,
                                                    iocRemoveTagRule.ruleTableId.tableDir,
                                                    &iocRemoveTagRule.tagRule);
            }
            else
            {
                ret = bcmVlan_removeTagRuleById(iocRemoveTagRule.ruleTableId.realDevName,
                                                iocRemoveTagRule.ruleTableId.nbrOfTags,
                                                iocRemoveTagRule.ruleTableId.tableDir,
                                                iocRemoveTagRule.tagRuleId);
            }

            if(ret)
            {
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Failed to removed Tag Rule from %s (tags=%d, dir=%d, id=%d)",
                              iocRemoveTagRule.ruleTableId.realDevName, iocRemoveTagRule.ruleTableId.nbrOfTags,
                              (int)iocRemoveTagRule.ruleTableId.tableDir, (int)iocRemoveTagRule.tagRuleId);
            }
            else
            {
                BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Removed Tag Rule from %s (tags=%d, dir=%d, id=%d)",
                             iocRemoveTagRule.ruleTableId.realDevName, iocRemoveTagRule.ruleTableId.nbrOfTags,
                             (int)iocRemoveTagRule.ruleTableId.tableDir, (int)iocRemoveTagRule.tagRuleId);
            }

            break;
        }

        case BCM_VLAN_IOC_DUMP_RULE_TABLE:
        {
            bcmVlan_iocDumpRuleTable_t iocDumpRuleTable;

            copy_from_user(&iocDumpRuleTable, (bcmVlan_iocDumpRuleTable_t *)arg,
                           sizeof(bcmVlan_iocDumpRuleTable_t));

            ret = bcmVlan_dumpTagRulesByTable(iocDumpRuleTable.ruleTableId.realDevName,
                                              iocDumpRuleTable.ruleTableId.nbrOfTags,
                                              iocDumpRuleTable.ruleTableId.tableDir);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to dump Rule Table from %s (tags=%d, dir=%d)",
                              iocDumpRuleTable.ruleTableId.realDevName, iocDumpRuleTable.ruleTableId.nbrOfTags,
                              (int)iocDumpRuleTable.ruleTableId.tableDir);
            }

            break;
        }

        case BCM_VLAN_IOC_SET_DEFAULT_TAG:
        {
            bcmVlan_iocSetDefaultVlanTag_t iocSetDefaultVlanTag;

            copy_from_user(&iocSetDefaultVlanTag, (bcmVlan_iocSetDefaultVlanTag_t *)arg,
                           sizeof(bcmVlan_iocSetDefaultVlanTag_t));

            ret = bcmVlan_setDefaultVlanTag(iocSetDefaultVlanTag.ruleTableId.realDevName,
                                            iocSetDefaultVlanTag.ruleTableId.nbrOfTags,
                                            iocSetDefaultVlanTag.ruleTableId.tableDir,
                                            iocSetDefaultVlanTag.defaultTpid,
                                            iocSetDefaultVlanTag.defaultPbits,
                                            iocSetDefaultVlanTag.defaultCfi,
                                            iocSetDefaultVlanTag.defaultVid);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to set default VLAN Tag of %s (tags=%d, dir=%d)",
                              iocSetDefaultVlanTag.ruleTableId.realDevName, iocSetDefaultVlanTag.ruleTableId.nbrOfTags,
                              (int)iocSetDefaultVlanTag.ruleTableId.tableDir);
            }

            break;
        }

        case BCM_VLAN_IOC_SET_DSCP_TO_PBITS:
        {
            bcmVlan_iocDscpToPbits_t iocDscpToPbits;

            copy_from_user(&iocDscpToPbits, (bcmVlan_iocDscpToPbits_t *)arg,
                           sizeof(bcmVlan_iocDscpToPbits_t));

            ret = bcmVlan_setDscpToPbitsTable(iocDscpToPbits.realDevName,
                                              iocDscpToPbits.dscp,
                                              iocDscpToPbits.pbits);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to set DSCP to PBITS table entry: %s, dscp %d, pbits %d",
                              iocDscpToPbits.realDevName, iocDscpToPbits.dscp, iocDscpToPbits.pbits);
            }

            break;
        }

        case BCM_VLAN_IOC_DUMP_DSCP_TO_PBITS:
        {
            bcmVlan_iocDscpToPbits_t iocDscpToPbits;

            copy_from_user(&iocDscpToPbits, (bcmVlan_iocDscpToPbits_t *)arg,
                           sizeof(bcmVlan_iocDscpToPbits_t));

            ret = bcmVlan_dumpDscpToPbitsTable(iocDscpToPbits.realDevName, iocDscpToPbits.dscp);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to dump DSCP to PBITS table: %s, dscp %d",
                              iocDscpToPbits.realDevName, iocDscpToPbits.dscp);
            }

            break;
        }

        case BCM_VLAN_IOC_DUMP_LOCAL_STATS:
        {
            bcmVlan_iocDumpLocalStats_t iocDumpLocalStats;

            copy_from_user(&iocDumpLocalStats, (bcmVlan_iocDumpLocalStats_t *)arg,
                           sizeof(bcmVlan_iocDumpLocalStats_t));

            ret = dumpLocalStats(iocDumpLocalStats.vlanDevName);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to dump Local Stats of %s", iocDumpLocalStats.vlanDevName);
            }

            break;
        }

        case BCM_VLAN_IOC_SET_TPID_TABLE:
        {
            bcmVlan_iocSetTpidTable_t iocSetTpidTable;

            copy_from_user(&iocSetTpidTable, (bcmVlan_iocSetTpidTable_t *)arg,
                           sizeof(bcmVlan_iocSetTpidTable_t));

            ret = bcmVlan_setTpidTable(iocSetTpidTable.realDevName, iocSetTpidTable.tpidTable);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to Set TPID Table in %s",
                              iocSetTpidTable.realDevName);
            }

            break;
        }

        case BCM_VLAN_IOC_DUMP_TPID_TABLE:
        {
            bcmVlan_iocDumpTpidTable_t iocDumpTpidTable;

            copy_from_user(&iocDumpTpidTable, (bcmVlan_iocDumpTpidTable_t *)arg,
                           sizeof(bcmVlan_iocDumpTpidTable_t));

            ret = bcmVlan_dumpTpidTable(iocDumpTpidTable.realDevName);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to Dump TPID Table for %s",
                              iocDumpTpidTable.realDevName);
            }

            break;
        }

        case BCM_VLAN_IOC_SET_IF_SUFFIX:
        {
            bcmVlan_iocSetIfSuffix_t iocSetIfSuffix;

            copy_from_user(&iocSetIfSuffix, (bcmVlan_iocSetIfSuffix_t *)arg,
                           sizeof(bcmVlan_iocSetIfSuffix_t));

            snprintf(bcmVlan_ifNameSuffix, BCM_VLAN_IF_SUFFIX_SIZE,
                     "%s", iocSetIfSuffix.suffix);

            break;
        }

        case BCM_VLAN_IOC_SET_DEFAULT_ACTION:
        {
            bcmVlan_iocSetDefaultAction_t iocSetDefaultAction;

            copy_from_user(&iocSetDefaultAction, (bcmVlan_iocSetDefaultAction_t *)arg,
                           sizeof(bcmVlan_iocSetDefaultAction_t));

            ret = bcmVlan_setDefaultAction(iocSetDefaultAction.ruleTableId.realDevName,
                                           iocSetDefaultAction.ruleTableId.nbrOfTags,
                                           iocSetDefaultAction.ruleTableId.tableDir,
                                           iocSetDefaultAction.defaultAction);
            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to set default action: %s (tags=%d, dir=%d)",
                              iocSetDefaultAction.ruleTableId.realDevName,
                              iocSetDefaultAction.ruleTableId.nbrOfTags,
                              (int)iocSetDefaultAction.ruleTableId.tableDir);
            }

            break;
        }

        case BCM_VLAN_IOC_SET_REAL_DEV_MODE:
        {
            bcmVlan_iocSetRealDevMode_t iocSetRealDevMode;

            copy_from_user(&iocSetRealDevMode, (bcmVlan_iocSetRealDevMode_t *)arg,
                           sizeof(bcmVlan_iocSetRealDevMode_t));

            ret = bcmVlan_setRealDevMode(iocSetRealDevMode.realDevName,
                                         iocSetRealDevMode.mode);

            if(ret)
            {
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Failed to set Real Device mode: %s (mode=%d)",
                              iocSetRealDevMode.realDevName,
                              iocSetRealDevMode.mode);
            }

            break;
        }

        default:
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid command, %d", cmd);
            ret = -EINVAL;
    }

    return ret;
}

static struct file_operations vlanFops = {
    .ioctl   = vlanIoctl,
    .open    = vlanOpen,
    .release = vlanClose,
    .owner   = THIS_MODULE
};

int bcmVlan_userInit(void)
{
    int ret;

    /* Register the driver and link it to our fops */
    ret = register_chrdev(vlanMajor, BCM_VLAN_DEVICE_NAME, &vlanFops);
    if(ret < 0)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN,
                      "Unable to register %s char device (major=%d), error:%d\n",
                      BCM_VLAN_DEVICE_NAME, vlanMajor, ret);

        return ret;
    }

    if(vlanMajor == 0)
    {
        vlanMajor = ret;
    }

    snprintf(bcmVlan_ifNameSuffix, BCM_VLAN_IF_SUFFIX_SIZE,
             "%s", BCM_VLAN_IF_SUFFIX_DEFAULT);

    return 0;
}

void bcmVlan_userCleanup(void)
{
    unregister_chrdev(vlanMajor, BCM_VLAN_DEVICE_NAME);

    if(vlanMajor != BCM_VLAN_DEVICE_MAJOR)
    {
        vlanMajor = 0;
    }
}
