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
// Description: 
//               
//**************************************************************************

#include "bcm_vlan_local.h"

#include <board.h>
#include <linux/version.h>
#include <net/net_namespace.h>
#include <linux/blog.h>

/*
 * Global functions
 */

int bcmVlan_devChangeMtu(struct net_device *dev, int new_mtu)
{
    BCM_LOG_FUNC(BCM_LOG_ID_VLAN);

    if(BCM_VLAN_REAL_DEV(dev)->mtu < new_mtu)
    {
        return -ERANGE;
    }

    dev->mtu = new_mtu;

    return 0;
}

extern const struct net_device_ops bcmVlan_netdev_ops;
extern const struct header_ops bcmVlan_header_ops;

int bcmVlan_devInit(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    netif_carrier_off(vlanDev);

    vlanDev->type = realDev->type;

    /* FIXME: IFF_BROADCAST|IFF_MULTICAST; ??? */
    vlanDev->flags = realDev->flags & ~(IFF_UP | IFF_PROMISC | IFF_ALLMULTI);
    vlanDev->iflink = realDev->ifindex;
    vlanDev->state = ((realDev->state & ((1<<__LINK_STATE_NOCARRIER) |
                                         (1<<__LINK_STATE_DORMANT))) |
                      (1<<__LINK_STATE_PRESENT));

    vlanDev->features |= realDev->features & realDev->vlan_features;
    vlanDev->gso_max_size = realDev->gso_max_size;

    /* ipv6 shared card related stuff */
    vlanDev->dev_id = realDev->dev_id;

    if (is_zero_ether_addr(vlanDev->dev_addr))
        memcpy(vlanDev->dev_addr, realDev->dev_addr, realDev->addr_len);
    if (is_zero_ether_addr(vlanDev->broadcast))
        memcpy(vlanDev->broadcast, realDev->broadcast, realDev->addr_len);

    vlanDev->addr_len = realDev->addr_len;

    vlanDev->header_ops = &bcmVlan_header_ops;
    vlanDev->hard_header_len = (realDev->hard_header_len +
                               (BCM_VLAN_HEADER_LEN * BCM_VLAN_MAX_TAGS));
    vlanDev->netdev_ops = &bcmVlan_netdev_ops;

    netdev_resync_ops(vlanDev);

    return 0;
}

void bcmVlan_devUninit(struct net_device *dev)
{
}

int bcmVlan_devOpen(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);
    int ret;

    BCM_LOG_FUNC(BCM_LOG_ID_VLAN);

    if (!(realDev->flags & IFF_UP))
    {
        return -ENETDOWN;
    }

    if (compare_ether_addr(vlanDev->dev_addr, realDev->dev_addr))
    {
        ret = dev_unicast_add(realDev, vlanDev->dev_addr, ETH_ALEN);
        if (ret < 0)
        {
            goto out;
        }
    }

    if (vlanDev->flags & IFF_ALLMULTI)
    {
        ret = dev_set_allmulti(realDev, 1);
        if (ret < 0)
        {
            goto del_unicast;
        }
    }

    if (vlanDev->flags & IFF_PROMISC)
    {
        ret = dev_set_promiscuity(realDev, 1);
        if (ret < 0)
        {
            goto clear_allmulti;
        }
    }

    memcpy(BCM_VLAN_DEV_INFO(vlanDev)->realDev_addr, realDev->dev_addr, ETH_ALEN);

    netif_carrier_on(vlanDev);

    return 0;

clear_allmulti:
    if (vlanDev->flags & IFF_ALLMULTI)
    {
        dev_set_allmulti(realDev, -1);
    }
del_unicast:
    if (compare_ether_addr(vlanDev->dev_addr, realDev->dev_addr))
    {
        dev_unicast_delete(realDev, vlanDev->dev_addr, ETH_ALEN);
    }
out:
    netif_carrier_off(vlanDev);

    return ret;
}

int bcmVlan_devStop(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    dev_mc_unsync(realDev, vlanDev);

    dev_unicast_unsync(realDev, vlanDev);

    if (vlanDev->flags & IFF_ALLMULTI)
        dev_set_allmulti(realDev, -1);

    if (vlanDev->flags & IFF_PROMISC)
        dev_set_promiscuity(realDev, -1);

    if (compare_ether_addr(vlanDev->dev_addr, realDev->dev_addr))
        dev_unicast_delete(realDev, vlanDev->dev_addr, vlanDev->addr_len);

    netif_carrier_off(vlanDev);

    return 0;
}

int bcmVlan_devSetMacAddress(struct net_device *vlanDev, void *p)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);
    struct sockaddr *addr = p;
    int err;

    if (!is_valid_ether_addr(addr->sa_data))
        return -EADDRNOTAVAIL;

    if (!(vlanDev->flags & IFF_UP))
        goto out;

    if (compare_ether_addr(addr->sa_data, realDev->dev_addr))
    {
        err = dev_unicast_add(realDev, addr->sa_data, ETH_ALEN);
        if (err < 0)
        {
            return err;
        }
    }

    if (compare_ether_addr(vlanDev->dev_addr, realDev->dev_addr))
        dev_unicast_delete(realDev, vlanDev->dev_addr, ETH_ALEN);

out:
    if(BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl->flags.routed)
    {
        /* free current MAC address */
#ifdef AEI_VDSL_CUSTOMER_NCS
        if (compare_ether_addr(vlanDev->dev_addr, realDev->dev_addr))
            kerSysReleaseMacAddress(vlanDev->dev_addr);
#else
        kerSysReleaseMacAddress(vlanDev->dev_addr);
#endif
    }

    /* apply new MAC address */
    memcpy(vlanDev->dev_addr, addr->sa_data, ETH_ALEN);

    printk("%s MAC address set to %02X:%02X:%02X:%02X:%02X:%02X\n",
           vlanDev->name,
           (unsigned char)addr->sa_data[0], (unsigned char)addr->sa_data[1],
           (unsigned char)addr->sa_data[2], (unsigned char)addr->sa_data[3],
           (unsigned char)addr->sa_data[4], (unsigned char)addr->sa_data[5]);

    return 0;
}

void bcmVlan_devSetRxMode(struct net_device *vlanDev)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    dev_mc_sync(realDev, vlanDev);
    dev_unicast_sync(realDev, vlanDev);
}

void bcmVlan_devChangeRxFlags(struct net_device *vlanDev, int change)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);

    if (change & IFF_ALLMULTI)
        dev_set_allmulti(realDev, vlanDev->flags & IFF_ALLMULTI ? 1 : -1);

    if (change & IFF_PROMISC)
        dev_set_promiscuity(realDev, vlanDev->flags & IFF_PROMISC ? 1 : -1);
}

int bcmVlan_devIoctl(struct net_device *vlanDev, struct ifreq *ifr, int cmd)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);
    const struct net_device_ops *ops = realDev->netdev_ops;
    struct ifreq ifrr;
    int err = -EOPNOTSUPP;

    strncpy(ifrr.ifr_name, realDev->name, IFNAMSIZ);
    ifrr.ifr_ifru = ifr->ifr_ifru;

    switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
            if (netif_device_present(realDev) && ops->ndo_do_ioctl)
                err = ops->ndo_do_ioctl(realDev, &ifrr, cmd);
            break;
    }

    if (!err)
        ifr->ifr_ifru = ifrr.ifr_ifru;

    return err;
}

int bcmVlan_devNeighSetup(struct net_device *vlanDev, struct neigh_parms *pa)
{
    struct net_device *realDev = BCM_VLAN_REAL_DEV(vlanDev);
    const struct net_device_ops *ops = realDev->netdev_ops;
    int err = 0;

    if (netif_device_present(realDev) && ops->ndo_neigh_setup)
        err = ops->ndo_neigh_setup(realDev, pa);

    return err;
}

int bcmVlan_devHardHeader(struct sk_buff *skb, struct net_device *vlanDev,
                          unsigned short type, const void *daddr,
                          const void *saddr, unsigned len)
{
    struct net_device *realDev;
    int ret;

    BCM_LOG_FUNC(BCM_LOG_ID_VLAN);

#if defined(BCM_VLAN_DATAPATH_ERROR_CHECK)
    BCM_ASSERT(skb);
    BCM_ASSERT(vlanDev);
    BCM_ASSERT(daddr);
    BCM_ASSERT(vlanDev->priv_flags & IFF_BCM_VLAN);
#endif

    /* If not specified by the caller, use the VLAN device's MAC SA */
    if (saddr == NULL)
    {
        saddr = vlanDev->dev_addr;
    }

#if defined(BCM_VLAN_DATAPATH_ERROR_CHECK)
    BCM_ASSERT(saddr);
#endif

    /* Now make the underlying real hard header */
    realDev = BCM_VLAN_REAL_DEV(vlanDev);

    ret = dev_hard_header(skb, realDev, type, daddr, saddr, len);

    skb_reset_mac_header(skb);

    return ret;
}

struct net_device_stats *bcmVlan_devGetStats(struct net_device *vlanDev)
{
    return &(vlanDev->stats);
}

#ifdef CONFIG_BLOG
static inline BlogStats_t *bcmVlan_devGetBstats(struct net_device *vlanDev)
{
    return &(BCM_VLAN_DEV_INFO(vlanDev)->bstats);
}

static inline struct net_device_stats *bcmVlan_devGetCstats(struct net_device *vlanDev)
{
    return &(BCM_VLAN_DEV_INFO(vlanDev)->cstats);
}

struct net_device_stats * bcmVlan_devCollectStats(struct net_device * dev_p)
{
    BlogStats_t bStats;
    BlogStats_t * bStats_p;
    struct net_device_stats *dStats_p;
    struct net_device_stats *cStats_p;

    if ( dev_p == (struct net_device *)NULL )
        return (struct net_device_stats *)NULL;

    /* JU: TBD: I have a pretty bad cold when I'm doing this port, and I can't think
       straight, so I'll have to revisit this when I'm a bit more clear.  I need to
       submit it though as it breaks the compile otherwise */
    dStats_p = bcmVlan_devGetStats(dev_p);
    cStats_p = bcmVlan_devGetCstats(dev_p);
    bStats_p = bcmVlan_devGetBstats(dev_p);

    memset(&bStats, 0, sizeof(BlogStats_t));

    blog_notify(FETCH_NETIF_STATS, (void*)dev_p,
                (uint32_t)&bStats, BLOG_PARAM1_NO_CLEAR);

    memcpy( cStats_p, dStats_p, sizeof(struct net_device_stats) );
    cStats_p->rx_packets += ( bStats.rx_packets + bStats_p->rx_packets );
    cStats_p->tx_packets += ( bStats.tx_packets + bStats_p->tx_packets );
    cStats_p->rx_bytes   += ( bStats.rx_bytes   + bStats_p->rx_bytes );
    cStats_p->tx_bytes   += ( bStats.tx_bytes   + bStats_p->tx_bytes );
    cStats_p->multicast  += ( bStats.multicast  + bStats_p->multicast );

    return cStats_p;
}

void bcmVlan_devUpdateStats(struct net_device * dev_p, BlogStats_t *blogStats_p)
{
    BlogStats_t * bStats_p;

    if ( dev_p == (struct net_device *)NULL )
        return;
    bStats_p = bcmVlan_devGetBstats(dev_p);

    bStats_p->rx_packets += blogStats_p->rx_packets;
    bStats_p->tx_packets += blogStats_p->tx_packets;
    bStats_p->rx_bytes   += blogStats_p->rx_bytes;
    bStats_p->tx_bytes   += blogStats_p->tx_bytes;
    bStats_p->multicast  += blogStats_p->multicast;
    return;
}

void bcmVlan_devClearStats(struct net_device * dev_p)
{
    BlogStats_t * bStats_p;
    struct net_device_stats *dStats_p;
    struct net_device_stats *cStats_p;

    if ( dev_p == (struct net_device *)NULL )
        return;

    dStats_p = bcmVlan_devGetStats(dev_p);
    cStats_p = bcmVlan_devGetCstats(dev_p); 
    bStats_p = bcmVlan_devGetBstats(dev_p);

    blog_notify(FETCH_NETIF_STATS, (void*)dev_p, 0, BLOG_PARAM1_DO_CLEAR);

    memset(bStats_p, 0, sizeof(BlogStats_t));
    memset(dStats_p, 0, sizeof(struct net_device_stats));
    memset(cStats_p, 0, sizeof(struct net_device_stats));

    return;
}
#endif /* CONFIG_BLOG */

int bcmVlan_devRebuildHeader(struct sk_buff *skb)
{
    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Function Not Supported");
    return -EPERM;

    /* FIXME: Finish this */
}

static void dumpFrame(char *from, char *to, char *msg,
                      unsigned int *tpidTable, struct sk_buff *skb)
{
#ifdef BCM_VLAN_DATAPATH_DEBUG
    if(bcmLog_getLogLevel(BCM_LOG_ID_VLAN) < BCM_LOG_LEVEL_DEBUG)
    {
        return;
    }

    printk("------------------------------------------------------------\n\n");

    printk("%s -> %s, %s\n\n", from, to, msg);

    bcmVlan_dumpPacket(tpidTable, skb);

    printk("------------------------------------------------------------\n\n");
#endif
}

int bcmVlan_devHardStartXmit(struct sk_buff *skb, struct net_device *vlanDev)
{
    int ret;
    struct net_device_stats *stats;

#if defined(BCM_VLAN_DATAPATH_ERROR_CHECK)
    BCM_ASSERT(skb);
    BCM_ASSERT(vlanDev);
#endif

//    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "*** TX *** Transmitting skb from %s", vlanDev->name);

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    stats = bcmVlan_devGetStats(vlanDev);

    skb_reset_mac_header(skb);

    /* will send the frame to the Real Device */
    ret = bcmVlan_processFrame(BCM_VLAN_REAL_DEV(vlanDev), vlanDev, &skb, BCM_VLAN_TABLE_DIR_TX);
    if(ret == -ENODEV)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Internal Error: Could not find Real Device %s on Transmission",
                      BCM_VLAN_REAL_DEV(vlanDev)->name);

        kfree_skb(skb);

        stats->tx_errors++;
        goto out;
    }
    if(ret == -EFAULT)
    {
        /* frame was dropped by a command */
        goto out;
    }
    if(ret < 0)
    {
        /* in case of error, bcmVlan_processFrame() frees the skb */
        stats->tx_errors++;
        goto out;
    }

#ifdef CONFIG_BLOG
    blog_link( IF_DEVICE, blog_ptr(skb), (void*)vlanDev, DIR_TX, skb->len );
#endif

    stats->tx_packets++;
    stats->tx_bytes += skb->len;

    /* debugging only */
    dumpFrame(vlanDev->name, skb->dev->name, "*** TX *** (AFTER)",
              BCM_VLAN_DEV_INFO(vlanDev)->vlanDevCtrl->realDevCtrl->tpidTable,
              skb);

    dev_queue_xmit(skb);

out:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    /* should always return 0 regardless of errors because we have no queues
       (see dev_queue_xmit() which calls us through dev_hard_start_xmit()) */
    return 0;
}

int bcmVlan_devReceiveSkb(struct sk_buff **skbp)
{
    int ret;
    struct net_device *realDev;
    struct net_device_stats *stats;
    struct sk_buff *skb;

#if defined(BCM_VLAN_DATAPATH_ERROR_CHECK)
    BCM_ASSERT(skbp);
    BCM_ASSERT(*skbp);
#endif

//    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "*** RX *** Receiving skb from %s", realDev->name);

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = (*skbp)->dev;

    ret = bcmVlan_processFrame(realDev, NULL, skbp, BCM_VLAN_TABLE_DIR_RX);

    skb = *skbp;

    if(ret == -ENODEV)
    {
        /* this frame is not for us */
        ret = 0;
        goto out;
    }
    if(ret == -EFAULT)
    {
        /* frame was dropped by a command */
        goto out;
    }
    if(ret < 0)
    {
        /* in case of error, bcmVlan_processFrame() frees the skb */
        stats = realDev->get_stats(realDev);
        stats->rx_errors++;

        goto out;
    }
    
    /* frame was processed successfully */

#ifdef BCM_VLAN_DATAPATH_DEBUG
    {
        unsigned int *tpidTable = NULL;

        if(skb->dev == realDev)
        {
            struct realDeviceControl *realDevCtrl = bcmVlan_getRealDevCtrl(realDev);
            if(realDevCtrl)
            {
                tpidTable = realDevCtrl->tpidTable;
            }
        }
        else
        {
            tpidTable = BCM_VLAN_DEV_INFO(skb->dev)->vlanDevCtrl->realDevCtrl->tpidTable;
        }

        if(tpidTable)
        {
            dumpFrame(realDev->name, skb->dev->name, "*** RX *** (AFTER)", tpidTable, skb);
        }
    }
#endif

    skb->dev->last_rx = jiffies;

#ifdef CONFIG_BLOG
    blog_link( IF_DEVICE, blog_ptr(skb), (void*)skb->dev, DIR_RX, skb->len );
#endif

    /* Bump the rx counters for the VLAN device. */
    stats = bcmVlan_devGetStats(skb->dev);
    stats->rx_packets++;
    stats->rx_bytes += skb->len;

    /* The ethernet driver already did the pkt_type calculations */
    switch (skb->pkt_type) {
	case PACKET_BROADCAST:
            // stats->broadcast ++; // no such counter
            break;

	case PACKET_MULTICAST:
            stats->multicast++;
            break;

	case PACKET_OTHERHOST:
            /* Our lower layer thinks this is not local, let's make sure.
             * This allows the VLAN to have a different MAC than the underlying
             * device, and still route correctly.
             */
            if (!compare_ether_addr(eth_hdr(skb)->h_dest, skb->dev->dev_addr)) {
                /* It is for our (changed) MAC-address! */
                skb->pkt_type = PACKET_HOST;
            }
            break;
	default:
            break;
    };

out:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}
