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
#ifndef _BCM_VLAN_LOCAL_H_
#define _BCM_VLAN_LOCAL_H_

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/init.h>
#include <asm/uaccess.h> /* for copy_from_user */
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <net/datalink.h>
#include <net/p8022.h>
#include <net/arp.h>
#include <linux/version.h>

#include "bcmtypes.h"
#include "bcm_log.h"
#include "bcm_vlan.h"


/*
 * Constants
 */

//#define BCM_VLAN_DATAPATH_DEBUG
//#define BCM_VLAN_IP_CHECK
#define BCM_VLAN_DATAPATH_ERROR_CHECK
//#define BCM_VLAN_PUSH_PADDING
#define BCM_VLAN_USE_DEFAULT_RX_DEV

#define BCM_VLAN_MAX_RULE_TABLES (BCM_VLAN_MAX_TAGS+1)

#define BCM_ETH_HEADER_LEN        14
#define BCM_VLAN_HEADER_LEN       4
#define BCM_ETH_VLAN_HEADER_LEN   (BCM_ETH_HEADER_LEN + BCM_VLAN_HEADER_LEN)
#define BCM_ETH_ADDR_LEN          6

#define BCM_VLAN_MAX_DSCP_VALUES  64

#define BCM_VLAN_FRAME_SIZE_MIN   64


/*
 * Macros
 */

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
extern spinlock_t bcmVlan_lock_g; /* global BCM VLAN lock */
#define BCM_VLAN_LOCK() spin_lock_bh( &bcmVlan_lock_g )
#define BCM_VLAN_UNLOCK() spin_unlock_bh( &bcmVlan_lock_g )
#else
#define BCM_VLAN_LOCK()
#define BCM_VLAN_UNLOCK()
#endif

#ifdef BCM_VLAN_DATAPATH_DEBUG
#define BCM_VLAN_DP_DEBUG(_fmt, _arg...) \
    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, _fmt, ##_arg)
#define BCM_VLAN_DP_ERROR(_fmt, _arg...)                                \
    BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "    **** DATAPATH ERROR **** " _fmt, ##_arg)
#else
#define BCM_VLAN_DP_DEBUG(_fmt, _arg...)
#define BCM_VLAN_DP_ERROR(_fmt, _arg...)
#endif

#define BCM_VLAN_PBITS_MASK  0xE000
#define BCM_VLAN_PBITS_SHIFT 13
#define BCM_VLAN_CFI_MASK    0x1000
#define BCM_VLAN_CFI_SHIFT   12
#define BCM_VLAN_VID_MASK    0x0FFF
#define BCM_VLAN_VID_SHIFT   0

#define BCM_VLAN_GET_TCI_PBITS(_vlanHeader) \
    ( ((_vlanHeader)->tci & BCM_VLAN_PBITS_MASK) >> BCM_VLAN_PBITS_SHIFT )

#define BCM_VLAN_GET_TCI_CFI(_vlanHeader) \
    ( ((_vlanHeader)->tci & BCM_VLAN_CFI_MASK) >> BCM_VLAN_CFI_SHIFT )

#define BCM_VLAN_GET_TCI_VID(_vlanHeader) \
    ( (_vlanHeader)->tci & BCM_VLAN_VID_MASK )

#define BCM_VLAN_SET_SUB_FIELD(_field, _val, _mask, _shift)      \
    do {                                                        \
        (_field) &= ~(_mask);                                   \
        (_field) |= ((typeof((_field)))(_val)) << (_shift);     \
    } while(0)

#define BCM_VLAN_SET_TCI_PBITS(_tci, _pbits)                     \
    BCM_VLAN_SET_SUB_FIELD((_tci), (_pbits), BCM_VLAN_PBITS_MASK, BCM_VLAN_PBITS_SHIFT)

#define BCM_VLAN_SET_TCI_CFI(_tci, _cfi)                                \
    BCM_VLAN_SET_SUB_FIELD((_tci), (_cfi), BCM_VLAN_CFI_MASK, BCM_VLAN_CFI_SHIFT)

#define BCM_VLAN_SET_TCI_VID(_tci, _vid)                         \
    BCM_VLAN_SET_SUB_FIELD((_tci), (_vid), BCM_VLAN_VID_MASK, BCM_VLAN_VID_SHIFT)

#define BCM_VLAN_COPY_TCI_SUBFIELD(_toVlanHeader, _fromVlanHeader, _mask) \
    do {                                                                \
        (_toVlanHeader)->tci &= ~(_mask);                               \
        (_toVlanHeader)->tci |= ((_fromVlanHeader)->tci & (_mask));     \
    } while(0)

#define BCM_VLAN_COPY_TCI_PBITS(_toVlanHeader, _fromVlanHeader)         \
    BCM_VLAN_COPY_TCI_SUBFIELD(_toVlanHeader, _fromVlanHeader, BCM_VLAN_PBITS_MASK)

#define BCM_VLAN_COPY_TCI_CFI(_toVlanHeader, _fromVlanHeader)         \
    BCM_VLAN_COPY_TCI_SUBFIELD(_toVlanHeader, _fromVlanHeader, BCM_VLAN_CFI_MASK)

#define BCM_VLAN_COPY_TCI_VID(_toVlanHeader, _fromVlanHeader)         \
    BCM_VLAN_COPY_TCI_SUBFIELD(_toVlanHeader, _fromVlanHeader, BCM_VLAN_VID_MASK)

#define BCM_VLAN_CREATE_TCI(_pbits, _cfi, _vid) \
  ( (((UINT16)(_pbits))<<BCM_VLAN_PBITS_SHIFT) | (((UINT16)(_cfi))<<BCM_VLAN_CFI_SHIFT) | ((UINT16)(_vid)) )

#define BCM_VLAN_GET_IP_DSCP(_ipHeader) ( ((_ipHeader)->tos & 0xFC) >> 2 )
#define BCM_VLAN_SET_IP_DSCP(_ipHeader, _dscp) BCM_VLAN_SET_SUB_FIELD((_ipHeader)->tos, _dscp, 0xFC, 2)
#define BCM_VLAN_GET_IP_VERSION(_ipHeader) ( ((_ipHeader)->version_ihl >> 4) )

#define BCM_VLAN_SKB_ETH_HEADER(_skb) ( (bcmVlan_ethHeader_t *)(skb_mac_header(_skb) ) )

#define BCM_VLAN_DEV_INFO(_vlanDev) ((bcmVlan_devInfo_t *)netdev_priv(_vlanDev))

#define BCM_VLAN_REAL_DEV(_vlanDev) BCM_VLAN_DEV_INFO(_vlanDev)->realDev

#define BCM_VLAN_GET_NEXT_VLAN_HEADER(_tpidTable, _vlanHeader)                      \
    ( BCM_VLAN_TPID_MATCH((_tpidTable), (_vlanHeader)->etherType) ? ((_vlanHeader)+1) : NULL )

#define BCM_VLAN_TPID_MATCH(_tpidTable, _tpid) bcmVlan_tpidMatch((_tpidTable), (_tpid))

/* Linked List API */

#define BCM_VLAN_DECLARE_LL(_name) bcmVlan_linkedList_t _name

#define BCM_VLAN_DECLARE_LL_ENTRY() bcmVlan_linkedListEntry_t llEntry

#define BCM_VLAN_LL_INIT(_linkedList)                           \
    do {                                                        \
        (_linkedList)->head = NULL;                             \
        (_linkedList)->tail = NULL;                             \
    } while(0)

/* #define BCM_VLAN_LL_IS_EMPTY(_linkedList)                               \ */
/*     ( ((_linkedList)->head == NULL) && ((_linkedList)->tail == NULL) ) */

#define BCM_VLAN_LL_IS_EMPTY(_linkedList) ( (_linkedList)->head == NULL )

#define BCM_VLAN_LL_INSERT(_linkedList, _newObj, _pos, _currObj)        \
    do {                                                                \
        if(BCM_VLAN_LL_IS_EMPTY(_linkedList))                           \
        {                                                               \
            (_linkedList)->head = (void *)(_newObj);                    \
            (_linkedList)->tail = (void *)(_newObj);                    \
            (_newObj)->llEntry.prev = NULL;                             \
            (_newObj)->llEntry.next = NULL;                             \
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Inserted FIRST object in %s", #_linkedList); \
        }                                                               \
        else                                                            \
        {                                                               \
            if((_pos) == BCM_VLAN_POSITION_APPEND)                      \
            {                                                           \
                typeof(*(_newObj)) *_tailObj = (_linkedList)->tail;     \
                _tailObj->llEntry.next = (_newObj);                     \
                (_newObj)->llEntry.prev = _tailObj;                     \
                (_newObj)->llEntry.next = NULL;                         \
                (_linkedList)->tail = (_newObj);                        \
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "APPENDED object in %s", #_linkedList); \
            }                                                           \
            else                                                        \
            {                                                           \
                if((_pos) == BCM_VLAN_POSITION_BEFORE)                  \
                {                                                       \
                    typeof(*(_newObj)) *_prevObj = (_currObj)->llEntry.prev; \
                    (_currObj)->llEntry.prev = (_newObj);               \
                    (_newObj)->llEntry.prev = _prevObj;                 \
                    (_newObj)->llEntry.next = (_currObj);               \
                    if(_prevObj != NULL)                                \
                    {                                                   \
                        _prevObj->llEntry.next = (_newObj);             \
                        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Inserted INNER object (BEFORE) in %s", #_linkedList); \
                    }                                                   \
                    if((_linkedList)->head == (_currObj))               \
                    {                                                   \
                        (_linkedList)->head = (_newObj);                \
                        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Inserted HEAD object in %s", #_linkedList); \
                    }                                                   \
                }                                                       \
                else                                                    \
                {                                                       \
                    typeof(*(_newObj)) *_nextObj = (_currObj)->llEntry.next; \
                    (_currObj)->llEntry.next = (_newObj);               \
                    (_newObj)->llEntry.prev = (_currObj);               \
                    (_newObj)->llEntry.next = _nextObj;                 \
                    if(_nextObj != NULL)                                \
                    {                                                   \
                        _nextObj->llEntry.prev = (_newObj);             \
                        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Inserted INNER object (AFTER) in %s", #_linkedList); \
                    }                                                   \
                    if((_linkedList)->tail == (_currObj))               \
                    {                                                   \
                        (_linkedList)->tail = (_newObj);                \
                        BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Inserted TAIL object in %s", #_linkedList); \
                    }                                                   \
                }                                                       \
            }                                                           \
        }                                                               \
    } while(0)

#define BCM_VLAN_LL_APPEND(_linkedList, _obj)                           \
    BCM_VLAN_LL_INSERT(_linkedList, _obj, BCM_VLAN_POSITION_APPEND, _obj)

#define BCM_VLAN_LL_REMOVE(_linkedList, _obj)                           \
    do {                                                                \
        BCM_ASSERT(!BCM_VLAN_LL_IS_EMPTY(_linkedList));                 \
        if((_linkedList)->head == (_obj) && (_linkedList)->tail == (_obj)) \
        {                                                               \
            (_linkedList)->head = NULL;                                 \
            (_linkedList)->tail = NULL;                                 \
            BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Removed LAST object from %s", #_linkedList); \
        }                                                               \
        else                                                            \
        {                                                               \
            if((_linkedList)->head == (_obj))                           \
            {                                                           \
                typeof(*(_obj)) *_nextObj = (_obj)->llEntry.next;       \
                (_linkedList)->head = _nextObj;                         \
                _nextObj->llEntry.prev = NULL;                          \
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Removed HEAD object from %s", #_linkedList); \
            }                                                           \
            else if((_linkedList)->tail == (_obj))                      \
            {                                                           \
                typeof(*(_obj)) *_prevObj = (_obj)->llEntry.prev;       \
                (_linkedList)->tail = _prevObj;                         \
                _prevObj->llEntry.next = NULL;                          \
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Removed TAIL object from %s", #_linkedList); \
            }                                                           \
            else                                                        \
            {                                                           \
                typeof(*(_obj)) *_prevObj = (_obj)->llEntry.prev;       \
                typeof(*(_obj)) *_nextObj = (_obj)->llEntry.next;       \
                _prevObj->llEntry.next = (_obj)->llEntry.next;          \
                _nextObj->llEntry.prev = (_obj)->llEntry.prev;          \
                BCM_LOG_DEBUG(BCM_LOG_ID_VLAN, "Removed INNER object from %s", #_linkedList); \
            }                                                           \
        }                                                               \
    } while(0)

#define BCM_VLAN_LL_GET_HEAD(_linkedList) (_linkedList).head

#define BCM_VLAN_LL_GET_NEXT(_obj) (_obj)->llEntry.next


/*
 * Type definitions
 */

typedef struct {
    void* head;
    void* tail;
} bcmVlan_linkedList_t;

typedef struct {
    void* prev;
    void* next;
} bcmVlan_linkedListEntry_t;

typedef struct {
    bcmVlan_tagRuleIndex_t tagRuleIdCount;
    UINT16 defaultTpid;
    UINT8 defaultPbits;
    UINT8 defaultCfi;
    UINT16 defaultVid;
    bcmVlan_defaultAction_t defaultAction;
    BCM_VLAN_DECLARE_LL(tableEntryLL);
} bcmVlan_ruleTable_t;

typedef struct {
    UINT32 rx_Misses;
    UINT32 tx_Misses;
    UINT32 error_PopUntagged;
    UINT32 error_PopNoMem;
    UINT32 error_PushTooManyTags;
    UINT32 error_PushNoMem;
    UINT32 error_SetEtherType;
    UINT32 error_SetTagEtherType;
    UINT32 error_InvalidTagNbr;
    UINT32 error_UnknownL3Header;
} bcmVlan_localStats_t;

struct realDeviceControl {
    BCM_VLAN_DECLARE_LL_ENTRY();
    struct net_device *realDev;
    bcmVlan_ruleTable_t ruleTable[BCM_VLAN_TABLE_DIR_MAX][BCM_VLAN_MAX_RULE_TABLES];
    UINT8 dscpToPbits[BCM_VLAN_MAX_DSCP_VALUES];
    unsigned int tpidTable[BCM_VLAN_MAX_TPID_VALUES];
    bcmVlan_localStats_t localStats;
    bcmVlan_realDevMode_t mode;
    BCM_VLAN_DECLARE_LL(vlanDevCtrlLL);
};

typedef union {
    struct {
        uint32 multicast : 1;
        uint32 routed    : 1;
        uint32 reserved  : 30;
    };
    uint32 u32;
} bcmVlan_vlanDevFlags_t;

struct vlanDeviceControl {
    BCM_VLAN_DECLARE_LL_ENTRY();
    struct net_device *vlanDev;
    struct realDeviceControl *realDevCtrl;
    bcmVlan_vlanDevFlags_t flags;
};

typedef struct {
    struct dev_mc_list *old_mc_list;  /* old multi-cast list for the VLAN interface..
                                       * we save this so we can tell what changes were
                                       * made, in order to feed the right changes down
                                       * to the real hardware...
                                       */
    int old_allmulti;               /* similar to above. */
    int old_promiscuity;            /* similar to above. */
    struct vlanDeviceControl *vlanDevCtrl;
    struct net_device *realDev; /* this pointer is needed because Linux still calls
                                   the handlers of the VLAN interface after it has
                                   been unregistered. vlanDevCtrl is freed *before*
                                   unregistration takes place, so it cannot be used */
    unsigned char realDev_addr[ETH_ALEN];
#ifdef CONFIG_BLOG
    BlogStats_t bstats; /* stats when the blog promiscuous layer has consumed packets */
    struct net_device_stats cstats; /* Cummulative Device stats (rx-bytes, tx-pkts, etc...) */
#endif
} bcmVlan_devInfo_t;

typedef struct {
    __be16 tci;
    __be16 etherType;
} __attribute__((packed)) bcmVlan_vlanHeader_t;

typedef struct {
    unsigned char macDest[ETH_ALEN];
    unsigned char macSrc[ETH_ALEN];
    __be16 etherType;
    bcmVlan_vlanHeader_t vlanHeader;
} __attribute__((packed)) bcmVlan_ethHeader_t;

typedef struct {
    __u8 version_ihl;
    __u8 tos;
    __be16 totalLength;
    __be16 id;
    __be16 flags_fragOffset;
    __u8 ttl;
    __u8 protocol;
    __be16 headerChecksum;
    __be32 ipSrc;
    __be32 ipDest;
    /* options... */
} __attribute__((packed)) bcmVlan_ipHeader_t;

typedef struct {
    UINT8 version_type;
    UINT8 code;
    UINT16 sessionId;
    uint16 length;
    UINT16 pppHeader;
    bcmVlan_ipHeader_t ipHeader;
} __attribute__((packed)) bcmVlan_pppoeSessionHeader_t;

static inline void dumpHexData(void *head, int len)
{
    int i;
    unsigned char *curPtr = (unsigned char*)head;

    if(bcmLog_getLogLevel(BCM_LOG_ID_VLAN) < BCM_LOG_LEVEL_DEBUG)
    {
        return;
    }

    printk("Address : 0x%08X", (unsigned int)curPtr);

    for (i = 0; i < len; ++i) {
        if (i % 4 == 0)
            printk(" ");       
        if (i % 16 == 0)
            printk("\n0x%04X:  ", i);
        printk("%02X ", *curPtr++);
    }

    printk("\n");
}

static inline struct net_device *bcmVlan_getDefaultVlanDevice(struct realDeviceControl *realDevCtrl)
{
    /* get head VLAN Device */
    return ((struct vlanDeviceControl *)(realDevCtrl->vlanDevCtrlLL.head))->vlanDev;
}

static inline int bcmVlan_tpidMatch(unsigned int *tpidTable, unsigned int tpid)
{
    int i;

    for(i=0; i<BCM_VLAN_MAX_TPID_VALUES; ++i)
    {
        if(tpidTable[i] == tpid)
        {
            return 1;
        }
    }

    return 0;
}

/*
 * Function Prototypes
 */

/* bcm_vlan_local.c */
int bcmVlan_initVlanDevices(void);
void bcmVlan_cleanupVlanDevices(void);
struct realDeviceControl *bcmVlan_getRealDevCtrl(struct net_device *realDev);
struct net_device *bcmVlan_getVlanDeviceByName(struct realDeviceControl *realDevCtrl, char *vlanDevName);
struct net_device *bcmVlan_getRealDeviceByName(char *realDevName, struct realDeviceControl **pRealDevCtrl);
void bcmVlan_freeRealDeviceVlans(struct net_device *realDev);
void bcmVlan_freeVlanDevice(struct net_device *vlanDev);
int bcmVlan_createVlanDevice(struct net_device *realDev, struct net_device *vlanDev,
                             bcmVlan_vlanDevFlags_t flags);
void bcmVlan_transferOperstate(struct net_device *realDev);
void bcmVlan_updateInterfaceState(struct net_device *realDev, int up);
void bcmVlan_dumpPacket(unsigned int *tpidTable, struct sk_buff *skb);

/* bcm_vlan_user.c */
int bcmVlan_userInit(void);
void bcmVlan_userCleanup(void);

/* bcm_vlan_rule.c */
int bcmVlan_initTagRules(void);
void bcmVlan_cleanupTagRules(void);
void bcmVlan_initTpidTable(struct realDeviceControl *realDevCtrl);
int bcmVlan_setTpidTable(char *realDevName, unsigned int *tpidTable);
int bcmVlan_dumpTpidTable(char *realDevName);
void bcmVlan_initRuleTables(struct realDeviceControl *realDevCtrl);
int bcmVlan_setDefaultAction(char *realDevName, UINT8 nbrOfTags,
                             bcmVlan_ruleTableDirection_t tableDir,
                             bcmVlan_defaultAction_t defaultAction);
int bcmVlan_setRealDevMode(char *realDevName, bcmVlan_realDevMode_t mode);
void bcmVlan_cleanupRuleTables(struct realDeviceControl *realDevCtrl);
int bcmVlan_insertTagRule(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir,
                          bcmVlan_tagRule_t *tagRule, bcmVlan_ruleInsertPosition_t position,
                          bcmVlan_tagRuleIndex_t posTagRuleId);
int bcmVlan_removeTagRuleById(char *realDevName, UINT8 nbrOfTags,
                              bcmVlan_ruleTableDirection_t tableDir, bcmVlan_tagRuleIndex_t tagRuleId);
int bcmVlan_removeTagRuleByFilter(char *realDevName, UINT8 nbrOfTags,
                                  bcmVlan_ruleTableDirection_t tableDir,
                                  bcmVlan_tagRule_t *tagRule);
int bcmVlan_removeTagRulesByVlanDev(struct vlanDeviceControl *vlanDevCtrl);
int bcmVlan_dumpTagRulesByTable(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir);
int bcmVlan_setDefaultVlanTag(char *realDevName, UINT8 nbrOfTags, bcmVlan_ruleTableDirection_t tableDir,
                              UINT16 defaultTpid, UINT8 defaultPbits, UINT8 defaultCfi, UINT16 defaultVid);
int bcmVlan_setDscpToPbitsTable(char *realDevName, UINT8 dscp, UINT8 pbits);
int bcmVlan_dumpDscpToPbitsTable(char *realDevName, UINT8 dscp);
int bcmVlan_processFrame(struct net_device *realDev, struct net_device *vlanDev,
                         struct sk_buff **skbp, bcmVlan_ruleTableDirection_t tableDir);

#endif /* _BCM_VLAN_LOCAL_H_ */
