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
#include "bcm_vlan_dev.h"
#include "pktHdr.h"
#include "skb_defines.h"
#include <linux/version.h>

/*
 * Local macros, type definitions
 */

#define BCM_VLAN_ETHERTYPE_IPv4            0x0800
#define BCM_VLAN_ETHERTYPE_PPPOE_DISCOVERY 0x8863
#define BCM_VLAN_ETHERTYPE_PPPOE_SESSION   0x8864

#define BCM_VLAN_PPPOE_VERSION_TYPE 0x11
#define BCM_VLAN_PPPOE_CODE_SESSION 0
#define BCM_VLAN_PPP_PROTOCOL_IPv4  0x0021

#define BCM_VLAN_IS_DONT_CARE(_x) ( ((_x) == (typeof(_x))(BCM_VLAN_DONT_CARE)) )

#define BCM_VLAN_DSCP_IS_VALID(_dscp) ( ((_dscp) <= 63) )
#define BCM_VLAN_VID_IS_VALID(_vid) ( ((_vid) <= 4094) )
#define BCM_VLAN_CFI_IS_VALID(_cfi) ( ((_cfi) <= 1) )
#define BCM_VLAN_PBITS_IS_VALID(_pbits) ( ((_pbits) <= 7) )
#define BCM_VLAN_FLOWID_IS_VALID(_flowId) ( ((_flowId) <= 255) )
#define BCM_VLAN_PORT_IS_VALID(_port) ( ((_port) <= 255) )
#define BCM_VLAN_QUEUE_IS_VALID(_queue) ( ((_queue) <= 31) )

#define BCM_VLAN_DSCP_FILT_IS_VALID(_dscp) \
    ( BCM_VLAN_IS_DONT_CARE(_dscp) || BCM_VLAN_DSCP_IS_VALID(_dscp) )

#define BCM_VLAN_VID_FILT_IS_VALID(_vid) \
    ( BCM_VLAN_IS_DONT_CARE(_vid) || BCM_VLAN_VID_IS_VALID(_vid) )

#define BCM_VLAN_CFI_FILT_IS_VALID(_cfi) \
    ( BCM_VLAN_IS_DONT_CARE(_cfi) || BCM_VLAN_CFI_IS_VALID(_cfi) )

#define BCM_VLAN_PBITS_FILT_IS_VALID(_pbits) \
    ( BCM_VLAN_IS_DONT_CARE(_pbits) || BCM_VLAN_PBITS_IS_VALID(_pbits) )

#define BCM_VLAN_FLOWID_FILT_IS_VALID(_flowId) \
    ( BCM_VLAN_IS_DONT_CARE(_flowId) || BCM_VLAN_FLOWID_IS_VALID(_flowId) )

#define BCM_VLAN_PORT_FILT_IS_VALID(_port)                              \
  ( BCM_VLAN_IS_DONT_CARE(_port) || BCM_VLAN_PORT_IS_VALID(_port) )

#define BCM_VLAN_FILTER_MATCH(_filter_x, _hdr_x)                        \
    ( (BCM_VLAN_IS_DONT_CARE(_filter_x) || (_filter_x) == (typeof(_filter_x))(_hdr_x)) )

#define BCM_VLAN_TAG_IS_DONT_CARE(_vlanTag)             \
    ( (BCM_VLAN_IS_DONT_CARE((_vlanTag).pbits) &&       \
       BCM_VLAN_IS_DONT_CARE((_vlanTag).cfi) &&         \
       BCM_VLAN_IS_DONT_CARE((_vlanTag).vid) &&         \
       BCM_VLAN_IS_DONT_CARE((_vlanTag).etherType)) )

#define BCM_VLAN_TAG_MATCH(_vlanTag, _vlanHeader)                       \
    ( (((_vlanHeader)->tci & (_vlanTag)->tciMask) == (_vlanTag)->tci) && \
      BCM_VLAN_FILTER_MATCH((_vlanTag)->etherType, (_vlanHeader)->etherType) )

#define BCM_VLAN_PRINT_VAL(_val, _format)                               \
    ( BCM_VLAN_IS_DONT_CARE(_val) ? "-" : printValName((_val), (_format), sizeof(_val)) )

typedef struct {
    struct net_device *txVlanDev;
    bcmVlan_tagRule_t tagRule;
    struct net_device *rxVlanDev;
    UINT32 cmdCount;
    unsigned int hitCount;
    BCM_VLAN_DECLARE_LL_ENTRY();
} bcmVlan_tableEntry_t;

typedef int (* cmdHandlerFunc_t)(bcmVlan_cmdArg_t *arg);

typedef struct {
    char *name;
    cmdHandlerFunc_t func;
} bcmVlan_cmdHandler_t;

typedef struct {
    struct sk_buff *skb;
    unsigned int *tpidTable;
    unsigned int nbrOfTags;
    bcmVlan_ethHeader_t *ethHeader;
    bcmVlan_vlanHeader_t *vlanHeader[BCM_VLAN_MAX_TAGS];
    bcmVlan_ipHeader_t *ipHeader;
    bcmVlan_ruleTableDirection_t tableDir;
    UINT8 *dscpToPbits;
    bcmVlan_localStats_t *localStats;
} bcmVlan_cmdInfo_t;


/*
 * Local variables
 */

static struct kmem_cache *tableEntryCache;

static bcmVlan_cmdInfo_t cmdInfo;


/*
 * Local Functions
 */
static void parseFrameHeader(void)
{
    int i;
    bcmVlan_vlanHeader_t *vlanHeader;

    cmdInfo.nbrOfTags = 0;

    cmdInfo.ethHeader = BCM_VLAN_SKB_ETH_HEADER(cmdInfo.skb);

    vlanHeader = (&cmdInfo.ethHeader->vlanHeader - 1);

    while(BCM_VLAN_TPID_MATCH(cmdInfo.tpidTable, vlanHeader->etherType))
    {
        vlanHeader++;

        if(cmdInfo.nbrOfTags < BCM_VLAN_MAX_TAGS)
        {
            cmdInfo.vlanHeader[cmdInfo.nbrOfTags] = vlanHeader;
        }

        cmdInfo.nbrOfTags++;
    }

    for(i=cmdInfo.nbrOfTags; i<BCM_VLAN_MAX_TAGS; ++i)
    {
        /* initialize remaining VLAN header pointers */
        cmdInfo.vlanHeader[i] = NULL;
    }

    if(vlanHeader->etherType == BCM_VLAN_ETHERTYPE_IPv4)
    {
        cmdInfo.ipHeader = (bcmVlan_ipHeader_t *)(vlanHeader + 1);

        BCM_VLAN_DP_DEBUG("No PPPoE");
    }
    else if(vlanHeader->etherType == BCM_VLAN_ETHERTYPE_PPPOE_SESSION)
    {
        bcmVlan_pppoeSessionHeader_t *pppoeSessionHeader = (bcmVlan_pppoeSessionHeader_t *)(vlanHeader + 1);

        if(pppoeSessionHeader->version_type == BCM_VLAN_PPPOE_VERSION_TYPE &&
           pppoeSessionHeader->code == BCM_VLAN_PPPOE_CODE_SESSION &&
           pppoeSessionHeader->pppHeader == BCM_VLAN_PPP_PROTOCOL_IPv4)
        {
            /* this is a valid IPv4 PPPoE session header */
            cmdInfo.ipHeader = &pppoeSessionHeader->ipHeader;

            BCM_VLAN_DP_DEBUG("PPPoE Session Header @ 0x%08lX", (UINT32)pppoeSessionHeader);
        }
        else
        {
            BCM_VLAN_DP_ERROR("Invalid PPPoE Session Header @ 0x%08lX", (UINT32)pppoeSessionHeader);

            cmdInfo.ipHeader = NULL;
        }
    }
    else
    {
        cmdInfo.ipHeader = NULL;

        BCM_VLAN_DP_DEBUG("Unknown L3 protocol: 0x%04lX", (UINT32)vlanHeader->etherType);
    }

    BCM_VLAN_DP_DEBUG("ETH:0x%08X, Tags=%d: [0]0x%08X, [1]0x%08X, [2]0x%08X, [3]0x%08X, IP:0x%08X",
                      (unsigned int)cmdInfo.ethHeader,
                      cmdInfo.nbrOfTags,
                      (unsigned int)cmdInfo.vlanHeader[0],
                      (unsigned int)cmdInfo.vlanHeader[1],
                      (unsigned int)cmdInfo.vlanHeader[2],
                      (unsigned int)cmdInfo.vlanHeader[3],
                      (unsigned int)cmdInfo.ipHeader);
}

static inline void adjustSkb(struct sk_buff *oldSkb, struct sk_buff *newSkb)
{
    if(newSkb != oldSkb)
    {
        /* save the new skb */
        cmdInfo.skb = newSkb;
        parseFrameHeader();

        BCM_VLAN_DP_DEBUG("************* NEW ***************");
        BCM_VLAN_DP_DEBUG("New skb->mac_header : 0x%08X", (unsigned int)newSkb->mac_header);
        BCM_VLAN_DP_DEBUG("New skb->network_header : 0x%08X", (unsigned int)newSkb->network_header);
    }
}

static inline unsigned char *removeSkbHeader(struct sk_buff *skb, unsigned int len)
{
    BCM_ASSERT(len <= skb->len);
    skb->len -= len;
    BCM_ASSERT(skb->len >= skb->data_len);
    skb_postpull_rcsum(skb, skb->data, len);
    return skb->data += len;
}

static inline struct sk_buff *unshareSkb(struct sk_buff *skb)
{
    if(skb_shared(skb) || skb_cloned(skb))
    {
        struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);

        kfree_skb(skb);

        adjustSkb(skb, nskb);

        skb = nskb;
    }

    return skb;
}

static inline struct sk_buff *checkSkbHeadroom(struct sk_buff *skb)
{
    struct sk_buff *sk_tmp = skb;

    if (skb_headroom(skb) < BCM_VLAN_HEADER_LEN)
    {
        /* skb_realloc_headroom() will only be called when no headroom is
           available. Hence, the skb will always be cloned, and then
           modified to point to a new data buffer (copy of the original) with
           enough headroom (done by pskb_expand_head()).
           If we were to use only skb_realloc_headroom(), a copy of the skb
           would be generated by calling pskb_copy() when headroom is available,
           regardless if the skb was cloned or not. By calling skb_unshare() below,
           we get a better performance since the skb will only be copied if
           not cloned */

        skb = skb_realloc_headroom(sk_tmp, BCM_VLAN_HEADER_LEN);

        kfree_skb(sk_tmp);
    }
    else
    {
        /* if skb is cloned, get a new private copy via skb_copy() */
        skb = skb_unshare(skb, GFP_ATOMIC);
    }

    adjustSkb(sk_tmp, skb);

    return skb;
}

static int removeOuterTagOnTransmit(struct sk_buff *skb)
{
    bcmVlan_ethHeader_t *ethHeader;

    /* on transmit direction, skb->data points to the ethernet header */
   
    skb = unshareSkb(skb);
    if(skb == NULL)
    {
        BCM_VLAN_DP_ERROR("Failed to unshare skb");
        return -ENOMEM;
    }

    ethHeader = (bcmVlan_ethHeader_t *)removeSkbHeader(skb, BCM_VLAN_HEADER_LEN);

    memmove(skb->data, skb->data - BCM_VLAN_HEADER_LEN, 2 * BCM_ETH_ADDR_LEN);

    skb->protocol = ethHeader->etherType;

    skb->mac_header += BCM_VLAN_HEADER_LEN;

    return 0;
}

static int removeOuterTagOnReceive(struct sk_buff *skb)
{
    /* On receive direction, skb->data points to the outer tag */

    skb->protocol = ((bcmVlan_vlanHeader_t *)(skb->data))->etherType;

    /* update the skb pointers of all shared copies of the skb */
    removeSkbHeader(skb, BCM_VLAN_HEADER_LEN);

    skb = unshareSkb(skb);
    if(skb == NULL)
    {
        BCM_VLAN_DP_ERROR("Failed to unshare skb");
        return -ENOMEM;
    }

    memmove(skb->data - BCM_ETH_HEADER_LEN, skb->data - BCM_ETH_VLAN_HEADER_LEN, 2 * BCM_ETH_ADDR_LEN);

    skb->mac_header += BCM_VLAN_HEADER_LEN;
    skb->network_header += BCM_VLAN_HEADER_LEN;

    return 0;
}

static int insertOuterTagOnTransmit(bcmVlan_cmdArg_t *arg, struct sk_buff *skb)
{
    bcmVlan_ethHeader_t *ethHeader;

    /* on transmit direction, skb->data points to the ethernet header */

    skb = checkSkbHeadroom(skb);
    if (skb == NULL)
    {
        BCM_VLAN_DP_ERROR("Failed to allocate skb headroom");
        return -ENOMEM;
    }

    ethHeader = (bcmVlan_ethHeader_t *)skb_push(skb, BCM_VLAN_HEADER_LEN);

    /* Move the mac addresses to the beginning of the new header. */
    memmove(skb->data, skb->data + BCM_VLAN_HEADER_LEN, 2 * BCM_ETH_ADDR_LEN);

    /* use the default TPID of the rule table */
    ethHeader->etherType = htons(BCM_VLAN_CMD_GET_VAL(arg));

    /* use the default TCI of the rule table */
    ethHeader->vlanHeader.tci = htons(BCM_VLAN_CMD_GET_VAL2(arg));

//    skb->protocol = __constant_htons(ETH_P_8021Q);
    skb->protocol = htons(BCM_VLAN_CMD_GET_VAL(arg));

    skb->mac_header -= BCM_VLAN_HEADER_LEN;
    skb->network_header -= BCM_VLAN_HEADER_LEN;

    return 0;
}

static int insertOuterTagOnReceive(bcmVlan_cmdArg_t *arg, struct sk_buff *skb)
{
    int ret;
    unsigned char *skbData;

    /* On receive direction, skb->data points to the outer tag, or L3 protocol header */
    /* move skb->data to the MAC header position, so the Linux APIs will work correctly */
    skbData = skb->data;
    skb->data = skb->mac_header;

    ret = insertOuterTagOnTransmit(arg, skb);
    if(!ret)
    {
        /* Make skb->data point to the tag we just added (the outermost tag), which is how the
           frame would have looked if it was received from the driver. The original VLAN code
           removes all tags before passing any tagged frames through, but we will not do this here.
           It will be up to the user to remove all tags in case the frame will be passed up to
           higher protocol layers, since we might want to keep the tags if the frame is bridged */
        skb->data = skbData - BCM_VLAN_HEADER_LEN;
    }

    return ret;
}

static int cmdHandler_popVlanTag(bcmVlan_cmdArg_t *arg)
{
    int ret;
    int i;
    int lastVlanIndex;

    /* make sure we have at least one tag to remove */
    if(cmdInfo.nbrOfTags == 0)
    {
        BCM_VLAN_DP_ERROR("Cannot remove tag from untagged Frame");
        cmdInfo.localStats->error_PopUntagged++;
        ret = 0;
        goto out;
    }

    if(cmdInfo.tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        ret = removeOuterTagOnReceive(cmdInfo.skb);
    }
    else
    {
        ret = removeOuterTagOnTransmit(cmdInfo.skb);
    }

    if(!ret)
    {
#if defined(BCM_VLAN_IP_CHECK)
        if(bcmLog_getLogLevel(BCM_LOG_ID_VLAN) >= BCM_LOG_LEVEL_DEBUG)
        {
            if(cmdInfo.ipHeader && (BCM_VLAN_GET_IP_VERSION(cmdInfo.ipHeader) == 4))
            {
                unsigned int len = ntohs(cmdInfo.ipHeader->totalLength);
                unsigned int ihl = cmdInfo.ipHeader->version_ihl & 0xF;

                if (cmdInfo.skb->len < len || len < 4 * ihl)
                {
                    BCM_VLAN_DP_ERROR("Invalid IP total length: ip %d, skb->len %d, ihl %d",
                                      len, cmdInfo.skb->len, ihl);
                }
            }
        }
#endif
        /* adjust command info */
        cmdInfo.nbrOfTags--;

        lastVlanIndex = BCM_VLAN_MAX_TAGS - 1;

        for(i=0; i<lastVlanIndex; ++i)
        {
            cmdInfo.vlanHeader[i] = cmdInfo.vlanHeader[i+1];
        }

        cmdInfo.vlanHeader[lastVlanIndex] = NULL;

        cmdInfo.ethHeader = BCM_VLAN_SKB_ETH_HEADER(cmdInfo.skb);

        BCM_VLAN_DP_DEBUG("ETH:0x%08X, Tags=%d: [0]0x%08X, [1]0x%08X, [2]0x%08X, [3]0x%08X, IP:0x%08X",
                          (unsigned int)cmdInfo.ethHeader,
                          cmdInfo.nbrOfTags,
                          (unsigned int)cmdInfo.vlanHeader[0],
                          (unsigned int)cmdInfo.vlanHeader[1],
                          (unsigned int)cmdInfo.vlanHeader[2],
                          (unsigned int)cmdInfo.vlanHeader[3],
                          (unsigned int)cmdInfo.ipHeader);
        BCM_VLAN_DP_DEBUG("skb->mac_header 0x%08X, skb->network_header 0x%08X",
                          (unsigned int)cmdInfo.skb->mac_header, (unsigned int)cmdInfo.skb->network_header);
    }
    else
    {
        cmdInfo.localStats->error_PopNoMem++;
    }

out:
    return ret;
}

static int cmdHandler_pushVlanTag(bcmVlan_cmdArg_t *arg)
{
    int ret;
    int i;

    /* make sure we can still add one tag */
    if(cmdInfo.nbrOfTags >= BCM_VLAN_MAX_TAGS)
    {
        BCM_VLAN_DP_ERROR("Frame already has %d VLAN tags", cmdInfo.nbrOfTags);
        cmdInfo.localStats->error_PushTooManyTags++;
        ret = 0;
        goto out;
    }

    if(cmdInfo.tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        ret = insertOuterTagOnReceive(arg, cmdInfo.skb);
    }
    else
    {
        ret = insertOuterTagOnTransmit(arg, cmdInfo.skb);

#if defined(BCM_VLAN_PUSH_PADDING)
        if(cmdInfo.skb->len < BCM_VLAN_FRAME_SIZE_MIN)
        {
            /* tagged frames generated locally in the modem
               are padded to 64 bytes because some PC NICs
               will strip the tag and pass undersize packets
               up to their drivers, where they will be discarded */
            cmdInfo.skb->len = BCM_VLAN_FRAME_SIZE_MIN;
        }
#endif
    }

    if(!ret)
    {
        /* adjust command info */
        cmdInfo.nbrOfTags++;

        for(i=BCM_VLAN_MAX_TAGS-1; i>0; --i)
        {
            cmdInfo.vlanHeader[i] = cmdInfo.vlanHeader[i-1];
        }

        cmdInfo.ethHeader = BCM_VLAN_SKB_ETH_HEADER(cmdInfo.skb);

        cmdInfo.vlanHeader[0] = &cmdInfo.ethHeader->vlanHeader;

        BCM_VLAN_DP_DEBUG("ETH:0x%08X, Tags=%d: [0]0x%08X, [1]0x%08X, [2]0x%08X, [3]0x%08X, IP:0x%08X",
                          (unsigned int)cmdInfo.ethHeader,
                          cmdInfo.nbrOfTags,
                          (unsigned int)cmdInfo.vlanHeader[0],
                          (unsigned int)cmdInfo.vlanHeader[1],
                          (unsigned int)cmdInfo.vlanHeader[2],
                          (unsigned int)cmdInfo.vlanHeader[3],
                          (unsigned int)cmdInfo.ipHeader);

        BCM_VLAN_DP_DEBUG("skb->mac_header 0x%08X, skb->network_header 0x%08X",
                          (unsigned int)cmdInfo.skb->mac_header, (unsigned int)cmdInfo.skb->network_header);
    }
    else
    {
        cmdInfo.localStats->error_PushNoMem++;
    }

out:
    return ret;
}

static int cmdHandler_setEthertype(bcmVlan_cmdArg_t *arg)
{
    if(cmdInfo.nbrOfTags == 0 || !BCM_VLAN_TPID_MATCH(cmdInfo.tpidTable, BCM_VLAN_CMD_GET_VAL(arg)))
    {
        BCM_VLAN_DP_ERROR("Cannot set Ethertype: nbrOfTags %d, etherType 0x%04X",
                          cmdInfo.nbrOfTags, BCM_VLAN_CMD_GET_VAL(arg));
        cmdInfo.localStats->error_SetEtherType++;
        return 0;
    }

    cmdInfo.ethHeader->etherType = __constant_htons(BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setPbits(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *vlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(vlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Vlan Tag %d does not exist", BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_SET_TCI_PBITS(vlanHeader->tci, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setCfi(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *vlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(vlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Vlan Tag %d does not exist", BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_SET_TCI_CFI(vlanHeader->tci, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setVid(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *vlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(vlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Vlan Tag %d does not exist", BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_SET_TCI_VID(vlanHeader->tci, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setTagEthertype(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *vlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(vlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Vlan Tag %d does not exist", BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    if((BCM_VLAN_CMD_GET_TARGET_TAG(arg) == cmdInfo.nbrOfTags-1) ||
       !BCM_VLAN_TPID_MATCH(cmdInfo.tpidTable, BCM_VLAN_CMD_GET_VAL(arg)))
    {
        BCM_VLAN_DP_ERROR("Cannot set Tag Ethertype: nbrOfTags %d, Target %d, etherType 0x%04X",
                          cmdInfo.nbrOfTags, BCM_VLAN_CMD_GET_TARGET_TAG(arg), BCM_VLAN_CMD_GET_VAL(arg));
        cmdInfo.localStats->error_SetTagEtherType++;
        return 0;
    }

    vlanHeader->etherType = __constant_htons(BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_copyPbits(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *sourceVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_SOURCE_TAG(arg)];
    bcmVlan_vlanHeader_t *targetVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(sourceVlanHeader == NULL || targetVlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Invalid Source/Target Vlan Tags (s:0x%08X, t:0x%08X)",
                          (unsigned int)sourceVlanHeader, (unsigned int)targetVlanHeader);
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_COPY_TCI_PBITS(targetVlanHeader, sourceVlanHeader);

    return 0;
}

static int cmdHandler_copyCfi(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *sourceVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_SOURCE_TAG(arg)];
    bcmVlan_vlanHeader_t *targetVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(sourceVlanHeader == NULL || targetVlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Invalid Source/Target Vlan Tags (s:0x%08X, t:0x%08X)",
                          (unsigned int)sourceVlanHeader, (unsigned int)targetVlanHeader);
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_COPY_TCI_CFI(targetVlanHeader, sourceVlanHeader);

    return 0;
}

static int cmdHandler_copyVid(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *sourceVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_SOURCE_TAG(arg)];
    bcmVlan_vlanHeader_t *targetVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(sourceVlanHeader == NULL || targetVlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Invalid Source/Target Vlan Tags (s:0x%08X, t:0x%08X)",
                          (unsigned int)sourceVlanHeader, (unsigned int)targetVlanHeader);
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_COPY_TCI_VID(targetVlanHeader, sourceVlanHeader);

    return 0;
}

static int cmdHandler_copyTagEthertype(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *sourceVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_SOURCE_TAG(arg)];
    bcmVlan_vlanHeader_t *targetVlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];

    if(sourceVlanHeader == NULL || targetVlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Invalid Source/Target Vlan Tags (s:0x%08X, t:0x%08X)",
                          (unsigned int)sourceVlanHeader, (unsigned int)targetVlanHeader);
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    if(BCM_VLAN_CMD_GET_TARGET_TAG(arg) == cmdInfo.nbrOfTags-1)
    {
        BCM_VLAN_DP_ERROR("Cannot Copy Tag Ethertype: nbrOfTags %d, Target %d",
                          cmdInfo.nbrOfTags, BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_SetTagEtherType++;
        return 0;
    }

    targetVlanHeader->etherType = sourceVlanHeader->etherType;

    return 0;
}

static int cmdHandler_dscp2pbits(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_vlanHeader_t *vlanHeader = cmdInfo.vlanHeader[BCM_VLAN_CMD_GET_TARGET_TAG(arg)];
    bcmVlan_ipHeader_t *ipHeader = cmdInfo.ipHeader;

    if(ipHeader == NULL || BCM_VLAN_GET_IP_VERSION(ipHeader) != 4)
    {
        BCM_VLAN_DP_ERROR("Unknown IP Header");
        cmdInfo.localStats->error_UnknownL3Header++;
        return 0;
    }

    if(vlanHeader == NULL)
    {
        BCM_VLAN_DP_ERROR("Vlan Tag %d does not exist", BCM_VLAN_CMD_GET_TARGET_TAG(arg));
        cmdInfo.localStats->error_InvalidTagNbr++;
        return 0;
    }

    BCM_VLAN_DP_DEBUG("dscp = %d -> pbits = %d",
                      BCM_VLAN_GET_IP_DSCP(ipHeader),
                      cmdInfo.dscpToPbits[BCM_VLAN_GET_IP_DSCP(ipHeader)]);

    BCM_VLAN_SET_TCI_PBITS(vlanHeader->tci, cmdInfo.dscpToPbits[BCM_VLAN_GET_IP_DSCP(ipHeader)]);

    return 0;
}

static int cmdHandler_setDscp(bcmVlan_cmdArg_t *arg)
{
    bcmVlan_ipHeader_t *ipHeader = cmdInfo.ipHeader;
    UINT16 oldTos;
    UINT16 newTos;
    UINT16 dscp;
    UINT16 icsum16;

    if(ipHeader == NULL || BCM_VLAN_GET_IP_VERSION(ipHeader) != 4)
    {
        BCM_VLAN_DP_ERROR("Unknown IP Header");
        cmdInfo.localStats->error_UnknownL3Header++;
        return 0;
    }

    oldTos = *((UINT16 *)(ipHeader));
    dscp = __constant_htons(BCM_VLAN_CMD_GET_VAL(arg));

    /* update incremental checksum of the IP header */
    newTos = oldTos & ~0x00FC;
    newTos |= dscp << 2;

    icsum16 = _compute_icsum16(0, oldTos, newTos);

    ipHeader->headerChecksum = _apply_icsum(ipHeader->headerChecksum, icsum16);

    /* set the new dscp value */
    *((UINT16 *)(ipHeader)) = newTos;

    return 0;
}

static int cmdHandler_dropFrame(bcmVlan_cmdArg_t *arg)
{
    struct net_device_stats *stats = cmdInfo.skb->dev->get_stats(cmdInfo.skb->dev);

    if(cmdInfo.tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        stats->rx_dropped++;
    }
    else
    {
        stats->tx_dropped++;
    }

    kfree_skb(cmdInfo.skb);

    return -EFAULT;
}

static int cmdHandler_setSkbPriority(bcmVlan_cmdArg_t *arg)
{
    cmdInfo.skb->priority = BCM_VLAN_CMD_GET_VAL(arg);

    return 0;
}

static int cmdHandler_setSkbMarkPort(bcmVlan_cmdArg_t *arg)
{
    cmdInfo.skb->mark = SKBMARK_SET_PORT(cmdInfo.skb->mark, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setSkbMarkQueue(bcmVlan_cmdArg_t *arg)
{
    cmdInfo.skb->mark = SKBMARK_SET_Q(cmdInfo.skb->mark, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_setSkbMarkFlowId(bcmVlan_cmdArg_t *arg)
{
    cmdInfo.skb->mark = SKBMARK_SET_FLOW_ID(cmdInfo.skb->mark, BCM_VLAN_CMD_GET_VAL(arg));

    return 0;
}

static int cmdHandler_continue(bcmVlan_cmdArg_t *arg)
{
    return 0;
}

static bcmVlan_cmdHandler_t cmdHandler[BCM_VLAN_OPCODE_MAX] =
    { [BCM_VLAN_OPCODE_NOP] = {"NOP", NULL},
      [BCM_VLAN_OPCODE_POP_TAG] = {"POP_TAG", cmdHandler_popVlanTag},
      [BCM_VLAN_OPCODE_PUSH_TAG] = {"PUSH_TAG", cmdHandler_pushVlanTag},
      [BCM_VLAN_OPCODE_SET_ETHERTYPE] = {"SET_ETHERTYPE", cmdHandler_setEthertype},
      [BCM_VLAN_OPCODE_SET_PBITS] = {"SET_PBITS", cmdHandler_setPbits},
      [BCM_VLAN_OPCODE_SET_CFI] = {"SET_CFI", cmdHandler_setCfi},
      [BCM_VLAN_OPCODE_SET_VID] = {"SET_VID", cmdHandler_setVid},
      [BCM_VLAN_OPCODE_SET_TAG_ETHERTYPE] = {"SET_TAG_ETHERTYPE", cmdHandler_setTagEthertype},
      [BCM_VLAN_OPCODE_COPY_PBITS] = {"COPY_PBITS", cmdHandler_copyPbits},
      [BCM_VLAN_OPCODE_COPY_CFI] = {"COPY_CFI", cmdHandler_copyCfi},
      [BCM_VLAN_OPCODE_COPY_VID] = {"COPY_VID", cmdHandler_copyVid},
      [BCM_VLAN_OPCODE_COPY_TAG_ETHERTYPE] = {"COPY_TAG_ETHERTYPE", cmdHandler_copyTagEthertype},
      [BCM_VLAN_OPCODE_DSCP2PBITS] = {"DSCP2PBITS", cmdHandler_dscp2pbits},
      [BCM_VLAN_OPCODE_SET_DSCP] = {"SET_DSCP", cmdHandler_setDscp},
      [BCM_VLAN_OPCODE_DROP_FRAME] = {"DROP_FRAME", cmdHandler_dropFrame},
      [BCM_VLAN_OPCODE_SET_SKB_PRIO] = {"SET_SKB_PRIO", cmdHandler_setSkbPriority},
      [BCM_VLAN_OPCODE_SET_SKB_MARK_PORT] = {"SET_SKB_MARK_PORT", cmdHandler_setSkbMarkPort},
      [BCM_VLAN_OPCODE_SET_SKB_MARK_QUEUE] = {"SET_SKB_MARK_QUEUE", cmdHandler_setSkbMarkQueue},
      [BCM_VLAN_OPCODE_SET_SKB_MARK_FLOWID] = {"SET_SKB_MARK_FLOWID", cmdHandler_setSkbMarkFlowId},
      [BCM_VLAN_OPCODE_CONTINUE] = {"CONTINUE", cmdHandler_continue} };

static inline bcmVlan_tableEntry_t *allocTableEntry(void)
{
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
    return kmem_cache_alloc(tableEntryCache, GFP_ATOMIC);
#else
    return kmem_cache_alloc(tableEntryCache, GFP_KERNEL);
#endif
}

static inline void freeTableEntry(bcmVlan_tableEntry_t *tableEntry)
{
    kmem_cache_free(tableEntryCache, tableEntry);
}

static int tagRulePreProcessor(UINT8 nbrOfTags, bcmVlan_tagRule_t *tagRule,
                               bcmVlan_ruleTable_t *ruleTable, UINT32 *cmdCount)
{
    int i;
    bcmVlan_vlanTag_t *vlanTag;

    /* NULL terminate names, just in case */
    tagRule->rxVlanDevName[IFNAMSIZ-1] = '\0';
    tagRule->filter.txVlanDevName[IFNAMSIZ-1] = '\0';

    if(!BCM_VLAN_DSCP_FILT_IS_VALID(tagRule->filter.dscp))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid filter: DSCP = %d",
                      tagRule->filter.dscp);
        goto out_error;
    }

    if(!BCM_VLAN_FLOWID_FILT_IS_VALID(tagRule->filter.skbMarkFlowId))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid filter: SKB Mark FlowID = %d",
                      tagRule->filter.skbMarkFlowId);
        goto out_error;
    }

    if(!BCM_VLAN_PORT_FILT_IS_VALID(tagRule->filter.skbMarkPort))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid filter: SKB Mark Port = %d",
                      tagRule->filter.skbMarkPort);
        goto out_error;
    }

    /* process the VLAN tag filters */
    for(i=0; i<nbrOfTags; ++i)
    {
        vlanTag = &tagRule->filter.vlanTag[i];

        if(!BCM_VLAN_PBITS_FILT_IS_VALID(vlanTag->pbits) ||
           !BCM_VLAN_CFI_FILT_IS_VALID(vlanTag->cfi) ||
           !BCM_VLAN_VID_FILT_IS_VALID(vlanTag->vid))
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid filter: VLAN Tag #%d", i);
            goto out_error;
        }

        vlanTag->tciMask = 0;
        vlanTag->tci = 0;

        if(!BCM_VLAN_IS_DONT_CARE(vlanTag->pbits))
        {
            vlanTag->tciMask |= BCM_VLAN_PBITS_MASK;
            BCM_VLAN_SET_TCI_PBITS(vlanTag->tci, vlanTag->pbits);
        }

        if(!BCM_VLAN_IS_DONT_CARE(vlanTag->cfi))
        {
            vlanTag->tciMask |= BCM_VLAN_CFI_MASK;
            BCM_VLAN_SET_TCI_CFI(vlanTag->tci, vlanTag->cfi);
        }

        if(!BCM_VLAN_IS_DONT_CARE(vlanTag->vid))
        {
            vlanTag->tciMask |= BCM_VLAN_VID_MASK;
            BCM_VLAN_SET_TCI_VID(vlanTag->tci, vlanTag->vid);
        }
    }

    /* initialize the remaining filters so we can match when
       additional tags are pushed */
    for(i=nbrOfTags; i<BCM_VLAN_MAX_TAGS; ++i)
    {
        vlanTag = &tagRule->filter.vlanTag[i];

        vlanTag->tciMask = 0;
        vlanTag->tci = 0;
    }

    *cmdCount = 0;

    for(i=0; i<BCM_VLAN_MAX_RULE_COMMANDS; ++i)
    {
        switch(tagRule->cmd[i].opCode)
        {
            case BCM_VLAN_OPCODE_NOP:
            case BCM_VLAN_OPCODE_SET_ETHERTYPE:
                break;

            case BCM_VLAN_OPCODE_PUSH_TAG:
                if(nbrOfTags >= BCM_VLAN_MAX_TAGS)
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Command: #%d: Max number of tags exceeded", i);
                    goto out_error;
                }
                /* save the default tpid and default tci in the command arguments */
                BCM_VLAN_CMD_SET_VAL(tagRule->cmd[i].arg, ruleTable->defaultTpid);
                BCM_VLAN_CMD_SET_VAL2(tagRule->cmd[i].arg, BCM_VLAN_CREATE_TCI(ruleTable->defaultPbits,
                                                                               ruleTable->defaultCfi,
                                                                               ruleTable->defaultVid));
                break;

            case BCM_VLAN_OPCODE_POP_TAG:
                if(nbrOfTags == 0)
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Command: #%d: Cannot remove tag from untagged frame", i);
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_TAG_ETHERTYPE:
            case BCM_VLAN_OPCODE_DSCP2PBITS:
                if(BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS)
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument: Target %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_PBITS:
                if((BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS) ||
                   !BCM_VLAN_PBITS_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d, Target %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg),
                                  BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_CFI:
                if((BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS) ||
                   !BCM_VLAN_CFI_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d, Target %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg),
                                  BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_VID:
                if((BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS) ||
                   !BCM_VLAN_VID_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d, Target %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg),
                                  BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_COPY_PBITS:
            case BCM_VLAN_OPCODE_COPY_CFI:
            case BCM_VLAN_OPCODE_COPY_VID:
            case BCM_VLAN_OPCODE_COPY_TAG_ETHERTYPE:
                if((BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS) ||
                   (BCM_VLAN_CMD_GET_SOURCE_TAG(tagRule->cmd[i].arg) >= BCM_VLAN_MAX_TAGS))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Source %d, Target %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_SOURCE_TAG(tagRule->cmd[i].arg),
                                  BCM_VLAN_CMD_GET_TARGET_TAG(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_DSCP:
                if(!BCM_VLAN_DSCP_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_DROP_FRAME:
                break;

            case BCM_VLAN_OPCODE_SET_SKB_PRIO:
                break;

            case BCM_VLAN_OPCODE_SET_SKB_MARK_PORT:
                if(!BCM_VLAN_PORT_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_SKB_MARK_QUEUE:
                if(!BCM_VLAN_QUEUE_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_SET_SKB_MARK_FLOWID:
                if(!BCM_VLAN_FLOWID_IS_VALID(BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg)))
                {
                    BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d: %s: Invalid argument(s): Value %d",
                                  i, cmdHandler[tagRule->cmd[i].opCode].name,
                                  BCM_VLAN_CMD_GET_VAL(tagRule->cmd[i].arg));
                    goto out_error;
                }
                break;

            case BCM_VLAN_OPCODE_CONTINUE:
                break;

            default:
                BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Command #%d': Invalid opCode %d",
                              i, tagRule->cmd[i].opCode);
                goto out_error;
        }

        if(tagRule->cmd[i].opCode != BCM_VLAN_OPCODE_NOP)
        {
            (*cmdCount)++;
        }
        else
        {
            break;
        }
    }

    return 0;

out_error:
    return -EINVAL;
}

static int removeTableEntryById(bcmVlan_ruleTable_t *ruleTable, bcmVlan_tagRuleIndex_t tagRuleId)
{
    int ret = 0;
    bcmVlan_tableEntry_t *tableEntry;

    BCM_ASSERT(ruleTable);

    tableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);

    while(tableEntry)
    {
        if(tableEntry->tagRule.id == tagRuleId)
        {
            BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Freeing Entry Id: %d", (int)tableEntry->tagRule.id);

            BCM_VLAN_LL_REMOVE(&ruleTable->tableEntryLL, tableEntry);

            freeTableEntry(tableEntry);

            break;
        }

        tableEntry = BCM_VLAN_LL_GET_NEXT(tableEntry);
    };

    if(tableEntry == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Could not find Tag Rule Id %d", (int)tagRuleId);

        ret = -EINVAL;
    }
    else if(BCM_VLAN_LL_IS_EMPTY(&ruleTable->tableEntryLL))
    {
        ruleTable->tagRuleIdCount = 0;
    }

    return ret;
}

static int removeTableEntryByFilter(bcmVlan_ruleTable_t *ruleTable,
                                    bcmVlan_tagRuleFilter_t *tagRuleFilter)
{
    int ret = 0;
    bcmVlan_tableEntry_t *tableEntry;

    BCM_ASSERT(ruleTable);
    BCM_ASSERT(tagRuleFilter);

    tableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);

    while(tableEntry)
    {
        if(!memcmp(&tableEntry->tagRule.filter, tagRuleFilter, sizeof(bcmVlan_tagRuleFilter_t)))
        {
            BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Freeing Entry Id: %d", (int)tableEntry->tagRule.id);

            BCM_VLAN_LL_REMOVE(&ruleTable->tableEntryLL, tableEntry);

            freeTableEntry(tableEntry);

            break;
        }

        tableEntry = BCM_VLAN_LL_GET_NEXT(tableEntry);
    };

    if(tableEntry == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Could not find matching Tag Rule");

        ret = -EINVAL;
    }
    else if(BCM_VLAN_LL_IS_EMPTY(&ruleTable->tableEntryLL))
    {
        ruleTable->tagRuleIdCount = 0;
    }

    return ret;
}

static int removeTableEntries(bcmVlan_ruleTable_t *ruleTable, struct net_device *vlanDev)
{
    int count = 0;
    bcmVlan_tableEntry_t *tableEntry, *nextTableEntry;

    BCM_ASSERT(ruleTable);

    tableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);

    while(tableEntry)
    {
        nextTableEntry = tableEntry;

        nextTableEntry = BCM_VLAN_LL_GET_NEXT(nextTableEntry);

/*         BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "vlanDev 0x%08X, rxVlanDev 0x%08X, txVlanDev 0x%08X", */
/*                       (unsigned int)vlanDev, */
/*                       (unsigned int)tableEntry->rxVlanDev, */
/*                       (unsigned int)tableEntry->txVlanDev); */

        if(vlanDev == NULL || tableEntry->rxVlanDev == vlanDev || tableEntry->txVlanDev == vlanDev)
        {
            BCM_LOG_INFO(BCM_LOG_ID_VLAN, "Freeing Tag Rule Id %d", (int)tableEntry->tagRule.id);

            count++;

            BCM_VLAN_LL_REMOVE(&ruleTable->tableEntryLL, tableEntry);

            freeTableEntry(tableEntry);
        }

        tableEntry = nextTableEntry;
    };

    if(BCM_VLAN_LL_IS_EMPTY(&ruleTable->tableEntryLL))
    {
        ruleTable->tagRuleIdCount = 0;
    }

    return count;
}

static bcmVlan_tableEntry_t *getTableEntryById(bcmVlan_ruleTable_t *ruleTable, bcmVlan_tagRuleIndex_t tagRuleId)
{
    bcmVlan_tableEntry_t *tableEntry;

    BCM_ASSERT(ruleTable);

    tableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);

    while(tableEntry)
    {
        if(tableEntry->tagRule.id == tagRuleId)
        {
            break;
        }

        tableEntry = BCM_VLAN_LL_GET_NEXT(tableEntry);
    };

    return tableEntry;
}

static char *printValName(unsigned int val, int format, int size)
{
    static char name[IFNAMSIZ];

    if(format == 0)
    {
        sprintf(name, "%d", val);
    }
    else
    {
        switch(size)
        {
            case sizeof(UINT8):
                sprintf(name, "0x%02X", val);
                break;

            case sizeof(UINT16):
                sprintf(name, "0x%04X", val);
                break;

            case sizeof(UINT32):
                sprintf(name, "0x%08X", val);
                break;

            default:
                sprintf(name, "ERROR");
        }
    }

    return name;
}

static void dumpTableEntry(struct realDeviceControl *realDevCtrl, UINT8 nbrOfTags,
                           bcmVlan_ruleTableDirection_t tableDir, bcmVlan_tableEntry_t *tableEntry)
{
    int i, j;

    printk("\n--------------------------------------------------------------------------------\n");

    printk("===> %s (%s) : %s, %d tag(s)\n",
           realDevCtrl->realDev->name,
           realDevCtrl->mode == BCM_VLAN_MODE_ONT ? "ONT" : "RG",
           (tableDir==BCM_VLAN_TABLE_DIR_RX) ? "RX" : "TX", nbrOfTags);

    printk("Tag Rule ID : %d\n", (int)tableEntry->tagRule.id);

    printk("Rx VLAN Device : %s\n", tableEntry->tagRule.rxVlanDevName);

    printk("\nFilters\n");
    printk("\tTx VLANIF       : %s\n", tableEntry->tagRule.filter.txVlanDevName);
    printk("\tVlanDev MacAddr : %s\n", tableEntry->tagRule.filter.vlanDevMacAddr ? "Yes" : "No");
    printk("\tSKB Prio        : %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.skbPrio, 0));
    printk("\tSKB Mark FlowId : %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.skbMarkFlowId, 0));
    printk("\tSKB Mark Port   : %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.skbMarkPort, 0));
    printk("\tEtherType       : %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.etherType, 1));
    printk("\tIPv4 DSCP       : %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.dscp, 0));
    for(i=0; i<BCM_VLAN_MAX_TAGS; ++i)
    {
        printk("\tVLAN Tag %d      : ", i);
        printk("pbits %s, ", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.vlanTag[i].pbits, 0));
        printk("cfi %s, ", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.vlanTag[i].cfi, 0));
        printk("vid %s, ", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.vlanTag[i].vid, 0));
        printk("(tci 0x%04X/0x%04X), ", tableEntry->tagRule.filter.vlanTag[i].tciMask,
               tableEntry->tagRule.filter.vlanTag[i].tci);
        printk("ether %s\n", BCM_VLAN_PRINT_VAL(tableEntry->tagRule.filter.vlanTag[i].etherType, 1));
    }

    printk("\nCommands");
    for(i=0; i<BCM_VLAN_MAX_RULE_COMMANDS; ++i)
    {
        if(i > 0 && tableEntry->tagRule.cmd[i].opCode == BCM_VLAN_OPCODE_NOP)
        {
            break;
        }
        printk("\n\t");
        printk("%02d:[%s", i, cmdHandler[tableEntry->tagRule.cmd[i].opCode].name);
        for(j=0; j<BCM_VLAN_MAX_CMD_ARGS; ++j)
        {
            printk(", 0x%08X", (int)tableEntry->tagRule.cmd[i].arg[j]);
        }
        printk("] ");
    }

    printk("\n\nHit Count : %d", tableEntry->hitCount);
}


/*
 * Global functions
 */

int bcmVlan_initTagRules(void)
{
    int ret = 0;

    /* create a slab cache for device descriptors */
    tableEntryCache = kmem_cache_create("bcmvlan_tableEntry",
                                        sizeof(bcmVlan_tableEntry_t),
                                        0, /* align */
                                        SLAB_HWCACHE_ALIGN, /* flags */
                                        NULL); /* ctor */
    if(tableEntryCache == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Unable to create Table Entry cache");

        ret = -ENOMEM;
        goto out;
    }

out:
    return ret;
}

void bcmVlan_cleanupTagRules(void)
{
    kmem_cache_destroy(tableEntryCache);
}

/*
 * Entry points: IOCTL, indirect
 */
void bcmVlan_initTpidTable(struct realDeviceControl *realDevCtrl)
{
    int i;

    for(i=0; i<BCM_VLAN_MAX_TPID_VALUES; ++i)
    {
        realDevCtrl->tpidTable[i] = __constant_htons(ETH_P_8021Q);
    }
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_setTpidTable(char *realDevName, unsigned int *tpidTable)
{
    int ret = 0;
    int i;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    for(i=0; i<BCM_VLAN_MAX_TPID_VALUES; ++i)
    {
        realDevCtrl->tpidTable[i] = tpidTable[i];
    }

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_dumpTpidTable(char *realDevName)
{
    int ret = 0;
    int i;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    printk("%s: TPID Table : ", realDev->name);

    for(i=0; i<BCM_VLAN_MAX_TPID_VALUES; ++i)
    {
        printk("0x%04X ", realDevCtrl->tpidTable[i]);
    }

    printk("\n");

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL, indirect
 */
void bcmVlan_initRuleTables(struct realDeviceControl *realDevCtrl)
{
    int i, j;

    for(i=0; i<BCM_VLAN_TABLE_DIR_MAX; ++i)
    {
        for(j=0; j<BCM_VLAN_MAX_RULE_TABLES; ++j)
        {
            realDevCtrl->ruleTable[i][j].tagRuleIdCount = 0;

            realDevCtrl->ruleTable[i][j].defaultTpid = BCM_VLAN_DEFAULT_TAG_TPID;
            realDevCtrl->ruleTable[i][j].defaultPbits = BCM_VLAN_DEFAULT_TAG_PBITS;
            realDevCtrl->ruleTable[i][j].defaultCfi = BCM_VLAN_DEFAULT_TAG_CFI;
            realDevCtrl->ruleTable[i][j].defaultVid = BCM_VLAN_DEFAULT_TAG_VID;

            realDevCtrl->ruleTable[i][j].defaultAction = (i == BCM_VLAN_TABLE_DIR_RX) ?
                BCM_VLAN_DEFAULT_RX_ACTION : BCM_VLAN_DEFAULT_TX_ACTION;

            BCM_VLAN_LL_INIT(&realDevCtrl->ruleTable[i][j].tableEntryLL);
        }
    }

    for(i=0; i<BCM_VLAN_MAX_DSCP_VALUES; ++i)
    {
        realDevCtrl->dscpToPbits[i] = (UINT8)(i & 0x7);
    }
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_setDefaultAction(char *realDevName, UINT8 nbrOfTags,
                             bcmVlan_ruleTableDirection_t tableDir,
                             bcmVlan_defaultAction_t defaultAction)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    if(defaultAction >= BCM_VLAN_ACTION_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Default Action: %d", defaultAction);

        ret = -EINVAL;
        goto out_error;
    }

    realDevCtrl->ruleTable[tableDir][nbrOfTags].defaultAction = defaultAction;

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_setRealDevMode(char *realDevName, bcmVlan_realDevMode_t mode)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(mode >= BCM_VLAN_MODE_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Real Device Mode: %d (max %d)",
                      mode, BCM_VLAN_MODE_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    realDevCtrl->mode = mode;

    printk("BCMVLAN : %s mode was set to %s\n", realDevCtrl->realDev->name,
           realDevCtrl->mode == BCM_VLAN_MODE_ONT ? "ONT" : "RG");

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL indirect, NOTIFIER, CLEANUP
 */
void bcmVlan_cleanupRuleTables(struct realDeviceControl *realDevCtrl)
{
    int i, j;

    for(i=0; i<BCM_VLAN_TABLE_DIR_MAX; ++i)
    {
        for(j=0; j<BCM_VLAN_MAX_RULE_TABLES; ++j)
        {
            removeTableEntries(&realDevCtrl->ruleTable[i][j], NULL);

            realDevCtrl->ruleTable[i][j].tagRuleIdCount = 0;

            BCM_VLAN_LL_INIT(&realDevCtrl->ruleTable[i][j].tableEntryLL);
        }
    }
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_insertTagRule(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir,
                          bcmVlan_tagRule_t *tagRule, bcmVlan_ruleInsertPosition_t position,
                          bcmVlan_tagRuleIndex_t posTagRuleId)
{
    int ret = 0;
    struct net_device *realDev;
    struct net_device *rxVlanDev;
    struct net_device *txVlanDev;
    struct realDeviceControl *realDevCtrl = NULL;
    bcmVlan_tableEntry_t *tableEntry, *posTableEntry;
    bcmVlan_ruleTable_t *ruleTable;
    UINT32 cmdCount;

    BCM_ASSERT(tagRule);

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out;
    }

    ruleTable = &realDevCtrl->ruleTable[tableDir][nbrOfTags];

    ret = tagRulePreProcessor(nbrOfTags, tagRule, ruleTable, &cmdCount);
    if(ret)
    {
        goto out;
    }

    if(position >= BCM_VLAN_POSITION_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid rule insertion position: %d (max %d)",
                      position, BCM_VLAN_POSITION_MAX);

        ret = -EINVAL;
        goto out;
    }

    if(tableDir == BCM_VLAN_TABLE_DIR_TX || !strcmp(tagRule->rxVlanDevName, BCM_VLAN_DEFAULT_DEV_NAME))
    {
        rxVlanDev = NULL;
    }
    else
    {
        rxVlanDev = bcmVlan_getVlanDeviceByName(realDevCtrl, tagRule->rxVlanDevName);
        if(rxVlanDev == NULL)
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "VLAN Device %s does not exist", tagRule->rxVlanDevName);

            ret = -EINVAL;
            goto out;
        }
    }

    if(tableDir == BCM_VLAN_TABLE_DIR_RX || !strcmp(tagRule->filter.txVlanDevName, BCM_VLAN_DEFAULT_DEV_NAME))
    {
        txVlanDev = NULL;
    }
    else
    {
        txVlanDev = bcmVlan_getVlanDeviceByName(realDevCtrl, tagRule->filter.txVlanDevName);
        if(txVlanDev == NULL)
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "VLAN Device %s does not exist", tagRule->filter.txVlanDevName);

            ret = -EINVAL;
            goto out;
        }
    }

    /* allocate a new table entry */
    tableEntry = allocTableEntry();
    if(tableEntry == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Could not allocate table entry memory");

        ret = -ENOMEM;
        goto out;
    }

    /* allocate a new Tag Rule ID */
    tagRule->id = ruleTable->tagRuleIdCount++;

    /* initialize table entry */
    tableEntry->cmdCount = cmdCount;
    tableEntry->hitCount = 0;

    tableEntry->tagRule = *tagRule;
    tableEntry->rxVlanDev = rxVlanDev;

    tableEntry->txVlanDev = txVlanDev;

    if((position == BCM_VLAN_POSITION_APPEND) ||
       ((position == BCM_VLAN_POSITION_AFTER) && BCM_VLAN_IS_DONT_CARE(posTagRuleId)))
    {
        /* add tag rule to the tail */
        BCM_VLAN_LL_APPEND(&ruleTable->tableEntryLL, tableEntry);
    }
    else if((position == BCM_VLAN_POSITION_BEFORE) && BCM_VLAN_IS_DONT_CARE(posTagRuleId))
    {
        /* add tag rule to the head */
        posTableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);

        BCM_VLAN_LL_INSERT(&ruleTable->tableEntryLL, tableEntry, position, posTableEntry);
    }
    else
    {
        posTableEntry = getTableEntryById(ruleTable, posTagRuleId);
        if(posTableEntry == NULL)
        {
            BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Could not find insertion point: Tag Rule ID %d", (int)posTagRuleId);

            freeTableEntry(tableEntry);

            ret = -EINVAL;
            goto out;
        }

        BCM_VLAN_LL_INSERT(&ruleTable->tableEntryLL, tableEntry, position, posTableEntry);
    }

out:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_removeTagRuleById(char *realDevName, UINT8 nbrOfTags,
                              bcmVlan_ruleTableDirection_t tableDir, bcmVlan_tagRuleIndex_t tagRuleId)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    ret = removeTableEntryById(&realDevCtrl->ruleTable[tableDir][nbrOfTags], tagRuleId);

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_removeTagRuleByFilter(char *realDevName, UINT8 nbrOfTags,
                                  bcmVlan_ruleTableDirection_t tableDir,
                                  bcmVlan_tagRule_t *tagRule)
{
    int ret;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;
    bcmVlan_ruleTable_t *ruleTable;
    UINT32 cmdCount;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    BCM_ASSERT(tagRule);

    ruleTable = &realDevCtrl->ruleTable[tableDir][nbrOfTags];

    /* need to initialize tag rule to get a full match */
    ret = tagRulePreProcessor(nbrOfTags, tagRule, ruleTable, &cmdCount);
    if(ret)
    {
        goto out_error;
    }

    ret = removeTableEntryByFilter(ruleTable, &tagRule->filter);

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL, NOTIFIER
 */
int bcmVlan_removeTagRulesByVlanDev(struct vlanDeviceControl *vlanDevCtrl)
{
    int ret = 0;
    int i, j;

    for(i=0; i<BCM_VLAN_TABLE_DIR_MAX; ++i)
    {
        for(j=0; j<BCM_VLAN_MAX_RULE_TABLES; ++j)
        {
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Direction %s, Tags %u",
                          (i==BCM_VLAN_TABLE_DIR_RX) ? "RX" : "TX", j);

            removeTableEntries(&vlanDevCtrl->realDevCtrl->ruleTable[i][j],
                               vlanDevCtrl->vlanDev);
        }
    }

    return ret;
}

#if 0
int bcmVlan_getTagRuleById(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir,
                           bcmVlan_tagRuleIndex_t tagRuleId, bcmVlan_tagRule_t **pTagRule)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;
    bcmVlan_tableEntry_t *tableEntry;

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    tableEntry = getTableEntryById(&realDevCtrl->ruleTable[tableDir][nbrOfTags], tagRuleId);
    if(tableEntry == NULL)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Tag Rule ID: %d", (int)tagRuleId);

        ret = -EINVAL;
        *pTagRule = NULL;
    }
    else
    {
        dumpTableEntry(realDevCtrl, nbrOfTags, tableDir, tableEntry);

        printk("\n--------------------------------------------------------------------------------\n");

        *pTagRule = &tableEntry->tagRule;
    }

out_error:
    return ret;
}
#endif

/*
 * Entry points: IOCTL
 */
int bcmVlan_dumpTagRulesByTable(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;
    bcmVlan_tableEntry_t *tableEntry;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    tableEntry = BCM_VLAN_LL_GET_HEAD(realDevCtrl->ruleTable[tableDir][nbrOfTags].tableEntryLL);

    if(tableEntry == NULL)
    {
        printk("No entries found\n");
    }

    while(tableEntry)
    {
        dumpTableEntry(realDevCtrl, nbrOfTags, tableDir, tableEntry);

        tableEntry = BCM_VLAN_LL_GET_NEXT(tableEntry);
    };

    printk("\n--------------------------------------------------------------------------------\n");

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_setDefaultVlanTag(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir,
                              UINT16 defaultTpid, UINT8 defaultPbits, UINT8 defaultCfi, UINT16 defaultVid)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;
    bcmVlan_ruleTable_t *ruleTable;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    if(nbrOfTags >= BCM_VLAN_MAX_RULE_TABLES)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Number of Tags: %d (max %d)",
                      nbrOfTags, BCM_VLAN_MAX_RULE_TABLES);

        ret = -EINVAL;
        goto out_error;
    }

    if(tableDir >= BCM_VLAN_TABLE_DIR_MAX)
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid Direction: %d (max %d)",
                      tableDir, BCM_VLAN_TABLE_DIR_MAX);

        ret = -EINVAL;
        goto out_error;
    }

    if(!BCM_VLAN_PBITS_IS_VALID(defaultPbits) ||
       !BCM_VLAN_CFI_IS_VALID(defaultCfi) ||
       !BCM_VLAN_VID_IS_VALID(defaultVid))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid default VLAN TCI");
        goto out_error;
    }

    ruleTable = &realDevCtrl->ruleTable[tableDir][nbrOfTags];

    if(!BCM_VLAN_IS_DONT_CARE(defaultTpid))
    {
        ruleTable->defaultTpid = defaultTpid;
    }

    if(!BCM_VLAN_IS_DONT_CARE(defaultPbits))
    {
        ruleTable->defaultPbits = defaultPbits;
    }

    if(!BCM_VLAN_IS_DONT_CARE(defaultCfi))
    {
        ruleTable->defaultCfi = defaultCfi;
    }

    if(!BCM_VLAN_IS_DONT_CARE(defaultVid))
    {
        ruleTable->defaultVid = defaultVid;
    }

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_setDscpToPbitsTable(char *realDevName, UINT8 dscp, UINT8 pbits)
{
    int ret = 0;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    if(!BCM_VLAN_DSCP_IS_VALID(dscp) || !BCM_VLAN_PBITS_IS_VALID(pbits))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid arguments: DSCP %d, PBITS %d", dscp, pbits);

        ret = -EINVAL;
        goto out_error;
    }

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    realDevCtrl->dscpToPbits[dscp] = pbits;

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

/*
 * Entry points: IOCTL
 */
int bcmVlan_dumpDscpToPbitsTable(char *realDevName, UINT8 dscp)
{
    int ret = 0;
    int i;
    struct net_device *realDev;
    struct realDeviceControl *realDevCtrl = NULL;

    /******** CRITICAL REGION BEGIN ********/
    BCM_VLAN_LOCK();

    if(!BCM_VLAN_IS_DONT_CARE(dscp) && !BCM_VLAN_DSCP_IS_VALID(dscp))
    {
        BCM_LOG_ERROR(BCM_LOG_ID_VLAN, "Invalid DSCP value: %d", dscp);

        ret = -EINVAL;
        goto out_error;
    }

    realDev = bcmVlan_getRealDeviceByName(realDevName, &realDevCtrl);
    if(realDev == NULL || realDevCtrl == NULL)
    {
        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Real Device %s has no VLAN Interfaces", realDevName);

        ret = -EINVAL;
        goto out_error;
    }

    printk("%s: DSCP to PBITS Table\n", realDev->name);

    if(BCM_VLAN_IS_DONT_CARE(dscp))
    {
        for(i=0; i<BCM_VLAN_MAX_DSCP_VALUES; ++i)
        {
            printk("%d : %d\n", i, realDevCtrl->dscpToPbits[i]);
        }
    }
    else
    {
        printk("%d : %d\n", dscp, realDevCtrl->dscpToPbits[dscp]);
    }

out_error:
    BCM_VLAN_UNLOCK();
    /******** CRITICAL REGION END ********/

    return ret;
}

static void forwardMulticast(struct realDeviceControl *realDevCtrl,
                             struct sk_buff *skb, int broadcast)
{
    struct vlanDeviceControl *vlanDevCtrl;
    struct net_device *firstVlanDev = NULL;

    vlanDevCtrl = BCM_VLAN_LL_GET_HEAD(realDevCtrl->vlanDevCtrlLL);

    while(vlanDevCtrl)
    {
        if(vlanDevCtrl->flags.multicast)
        {
            if(firstVlanDev == NULL)
            {
                firstVlanDev = vlanDevCtrl->vlanDev;
            }
            else
            {
                struct sk_buff *skb2;
                struct net_device_stats *stats;

                skb2 = skb_copy(skb, GFP_ATOMIC);
                skb2->dev = vlanDevCtrl->vlanDev;
                skb2->pkt_type = broadcast? PACKET_BROADCAST : PACKET_MULTICAST;

                stats = bcmVlan_devGetStats(skb2->dev);
                stats->rx_packets++;
                stats->rx_bytes += skb2->len;

                netif_rx(skb2);

                BCM_VLAN_DP_DEBUG("Multicast: Forward to %s", skb2->dev->name);
            }
        }

        vlanDevCtrl = BCM_VLAN_LL_GET_NEXT(vlanDevCtrl);
    }

    if(firstVlanDev != NULL)
    {
        skb->dev = firstVlanDev;
        skb->pkt_type = broadcast? PACKET_BROADCAST : PACKET_MULTICAST;

        BCM_VLAN_DP_DEBUG("Multicast: Forward to %s", skb->dev->name);
    }
}

#if 0
#define BCM_VLAN_MATCH_DEBUG(_fmt, _arg...) BCM_VLAN_DP_DEBUG(_fmt, ##_arg)
#else
#define BCM_VLAN_MATCH_DEBUG(_fmt, _arg...)
#endif

static int tagRuleMatch(bcmVlan_tableEntry_t *tableEntry, struct net_device *vlanDev)
{

    BCM_VLAN_MATCH_DEBUG("@@@ Tag Rule ID %d @@@", tableEntry->tagRule.id);

    /* match on Tx VLAN Device */
    if(tableEntry->txVlanDev == NULL || tableEntry->txVlanDev == vlanDev)
    {
        bcmVlan_tagRuleFilter_t *tagRuleFilter = &tableEntry->tagRule.filter;

        /* match on rx vlan interface */
        if(tagRuleFilter->vlanDevMacAddr && tableEntry->rxVlanDev != NULL)
        {
           bcmVlan_ethHeader_t *ethHeader = BCM_VLAN_SKB_ETH_HEADER(cmdInfo.skb);
           
           if(memcmp(tableEntry->rxVlanDev->dev_addr, ethHeader->macDest, ETH_ALEN))
           {
              return 0;
           } 
        }

        /* match on SKB Priority */
        if(BCM_VLAN_FILTER_MATCH(tagRuleFilter->skbPrio, cmdInfo.skb->priority))
        {
            BCM_VLAN_MATCH_DEBUG("MATCH: SKB PRIORITY <%d>", tagRuleFilter->skbPrio);

            /* match on SKB Mark FlowId */
            if(BCM_VLAN_FILTER_MATCH(tagRuleFilter->skbMarkFlowId, SKBMARK_GET_FLOW_ID(cmdInfo.skb->mark)))
            {
                BCM_VLAN_MATCH_DEBUG("MATCH: FLOWID <%d>", tagRuleFilter->skbMarkFlowId);

                /* match on SKB Mark Port */
                if(BCM_VLAN_FILTER_MATCH(tagRuleFilter->skbMarkPort, SKBMARK_GET_PORT(cmdInfo.skb->mark)))
                {
                    BCM_VLAN_MATCH_DEBUG("MATCH: PORT <%d>", tagRuleFilter->skbMarkPort);

                    /* match on Ethertype */
                    if(BCM_VLAN_FILTER_MATCH(tagRuleFilter->etherType, cmdInfo.ethHeader->etherType))
                    {
                        BCM_VLAN_MATCH_DEBUG("MATCH: ETHERTYPE <0x%04X>", tagRuleFilter->etherType);

                        /* match on DSCP */
                        if(BCM_VLAN_IS_DONT_CARE(tagRuleFilter->dscp) ||
                           (cmdInfo.ipHeader && tagRuleFilter->dscp == BCM_VLAN_GET_IP_DSCP(cmdInfo.ipHeader)))
                        {
                            /* match on VLAN tags */
                            int i;
                            bcmVlan_vlanHeader_t *vlanHeader = &cmdInfo.ethHeader->vlanHeader;

                            BCM_VLAN_MATCH_DEBUG("MATCH: DSCP <%d>", tagRuleFilter->dscp);

                            for(i=0; i<cmdInfo.nbrOfTags; ++i)
                            {
                                if(!BCM_VLAN_TAG_MATCH(&tagRuleFilter->vlanTag[i], vlanHeader))
                                {
                                    return 0;
                                }

                                BCM_VLAN_MATCH_DEBUG("MATCH: VLAN Tag %d", i);

                                vlanHeader++;
                            }

                            /* if we get here, there was a match */
                            return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

/*
 * Entry points: RECV, XMIT
 */
int bcmVlan_processFrame(struct net_device *realDev, struct net_device *vlanDev,
                         struct sk_buff **skbp, bcmVlan_ruleTableDirection_t tableDir)
{
    int ret = 0;
    int i;
    struct realDeviceControl *realDevCtrl;
    bcmVlan_ruleTable_t *ruleTable = NULL;
    bcmVlan_tableEntry_t *tableEntry;
    bcmVlan_tagRuleCommand_t *cmd;
    struct net_device *rxVlanDevRule = NULL;
    int match;
    unsigned int rxNbrOfTags = 0;

    realDevCtrl = bcmVlan_getRealDevCtrl(realDev);
    if(realDevCtrl == NULL)
    {
        /* this frame is not for us */
//        BCM_VLAN_DP_DEBUG("%s has no VLAN Interfaces", realDev->name);

        ret = -ENODEV;
        goto out;
    }

    cmdInfo.skb = *skbp;
    cmdInfo.tpidTable = realDevCtrl->tpidTable;
    cmdInfo.localStats = &realDevCtrl->localStats;
    cmdInfo.tableDir = tableDir;

#ifdef BCM_VLAN_DATAPATH_DEBUG
    if(tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        BCM_VLAN_DP_DEBUG("************ RX Frame ************");
        bcmVlan_dumpPacket(cmdInfo.tpidTable, cmdInfo.skb);
    }
#endif

    parseFrameHeader();
    if(cmdInfo.nbrOfTags > BCM_VLAN_MAX_TAGS)
    {
        BCM_VLAN_DP_ERROR("Too many VLAN Tags (%d, max %d)", cmdInfo.nbrOfTags, BCM_VLAN_MAX_TAGS);

        goto miss;
    }

#if defined(BCM_VLAN_IP_CHECK)
    if(bcmLog_getLogLevel(BCM_LOG_ID_VLAN) >= BCM_LOG_LEVEL_DEBUG)
    {
        if(cmdInfo.ipHeader && BCM_VLAN_GET_IP_VERSION(cmdInfo.ipHeader) == 4)
        {
            unsigned int ihl = cmdInfo.ipHeader->version_ihl & 0xF;

            if (ip_fast_csum((UINT8 *)cmdInfo.ipHeader, ihl) != 0)
            {
                BCM_VLAN_DP_ERROR("Invalid IP checksum: version %d, ihl %d",
                                  cmdInfo.ipHeader->version_ihl >> 4, ihl);
            }
        }
    }
#endif

#if defined(BCM_VLAN_DATAPATH_ERROR_CHECK)
    BCM_ASSERT(tableDir < BCM_VLAN_TABLE_DIR_MAX);
#endif

    /* save the original number of tags in the frame */
    rxNbrOfTags = cmdInfo.nbrOfTags;

    /* get rule table */
    ruleTable = &realDevCtrl->ruleTable[tableDir][cmdInfo.nbrOfTags];

    /* try to find a matching rule, if any */
    match = 0;
    tableEntry = BCM_VLAN_LL_GET_HEAD(ruleTable->tableEntryLL);
    while(tableEntry)
    {
        if(tagRuleMatch(tableEntry, vlanDev))
        {
            BCM_VLAN_DP_DEBUG("*** HIT *** %s, %s, Tags %d, RuleId %d",
                              (tableDir == BCM_VLAN_TABLE_DIR_RX) ? "RX" : "TX",
                              cmdInfo.skb->dev->name,
                              cmdInfo.nbrOfTags,
                              (int)tableEntry->tagRule.id);

            match = 1;
            tableEntry->hitCount++;

            /* save target VLAN device to the VLAN device specified in the rule */
            rxVlanDevRule = tableEntry->rxVlanDev;

            /* initialize command info */
            cmdInfo.dscpToPbits = realDevCtrl->dscpToPbits;

            /* execute commands */
            for(i=0; i<tableEntry->cmdCount && !ret; ++i)
            {
                cmd = &tableEntry->tagRule.cmd[i];

                BCM_VLAN_DP_DEBUG("[%s, 0x%08X, 0x%08X]",
                                  cmdHandler[cmd->opCode].name, cmd->arg[0], cmd->arg[1]);

                if(cmd->opCode == BCM_VLAN_OPCODE_CONTINUE)
                {
                    goto next_table_entry;
                }

                ret = cmdHandler[cmd->opCode].func(cmd->arg);
            }

            goto set_skb_dev;
        }

    next_table_entry:
        tableEntry = BCM_VLAN_LL_GET_NEXT(tableEntry);
    }

    /* this check is needed because of the CONTINUE command */
    if(match)
    {
        goto set_skb_dev;
    }

miss:
    BCM_VLAN_DP_DEBUG("*** MISS *** %s, %s, Tags %d",
                      (tableDir == BCM_VLAN_TABLE_DIR_RX) ? "RX" : "TX",
                      cmdInfo.skb->dev->name,
                      cmdInfo.nbrOfTags);

    /* update local stats on misses */
    if(tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        cmdInfo.localStats->rx_Misses++;
    }
    else
    {
        cmdInfo.localStats->tx_Misses++;
    }

    /* drop packet, if this is the default action */
    if(ruleTable && ruleTable->defaultAction == BCM_VLAN_ACTION_DROP)
    {
        ret = cmdHandler_dropFrame(NULL);
        goto out;
    }

set_skb_dev:
    /* set the skb device */
    if(tableDir == BCM_VLAN_TABLE_DIR_RX)
    {
        if(realDevCtrl->mode == BCM_VLAN_MODE_RG)
        {
           bcmVlan_ethHeader_t *ethHeader = BCM_VLAN_SKB_ETH_HEADER(cmdInfo.skb);

           if(rxNbrOfTags == 0)
           {
              /* This is an untagged frame. */
              if(is_multicast_ether_addr(ethHeader->macDest))
              {
                 /* Regardless hit or miss, admit the multicast frame to all multicast
                  * enabled interfaces.
                  * Note that all the untagged virtual interfaces shall be multicast
                  * enabled. Therefore, if this is a hit, the rule rx interface
                  * is also in the list of multicast enabled interfaces.
                  */
                 forwardMulticast(realDevCtrl, cmdInfo.skb, is_broadcast_ether_addr(ethHeader->macDest));
              }
              else if(rxVlanDevRule != NULL)
              {
                 /* It's a hit. Admit the unicast frame to the rule rx interface. */
                 cmdInfo.skb->dev = rxVlanDevRule;
              }
              /* else, it's a miss. Forward the unicast frame to the real device */
           }
           else
           {
              /* This is a tagged frame. */
              if(rxVlanDevRule != NULL)
              {
                 /* It's a hit. */
                 /* The frame will be admitted to the rule rx interface,
                  *   if the frame is multicast,
                  *   or if the rule interface is bridged,
                  *   or if the frame destination MAC matches the rule rx interface MAC.
                  * Otherwise, the frame will be dropped. 
                  */
                 if(is_multicast_ether_addr(ethHeader->macDest) ||
                    !BCM_VLAN_DEV_INFO(rxVlanDevRule)->vlanDevCtrl->flags.routed ||
                    !memcmp(rxVlanDevRule->dev_addr, ethHeader->macDest, ETH_ALEN))
                 {
                    cmdInfo.skb->dev = rxVlanDevRule;
                 }
                 else
                 {
                    ret = cmdHandler_dropFrame(NULL);
                 }
              }
              /* else, it's a miss. Forward the frame to the real device */ 
           }
        }
        else   /* ONT */
        {
           if(rxVlanDevRule != NULL)
           {
              /* Rx vlan device was specified. */
              cmdInfo.skb->dev = rxVlanDevRule;
           }
#if defined(BCM_VLAN_USE_DEFAULT_RX_DEV)
           else
           {
               /* get default VLAN Device */
               cmdInfo.skb->dev = bcmVlan_getDefaultVlanDevice(realDevCtrl);
           }
#endif
           /* else, Forward the frame to the real device */ 
        }
    }
    else
    {
        cmdInfo.skb->dev = realDev;
    }

    *skbp = cmdInfo.skb;

out:
    return ret;
}

