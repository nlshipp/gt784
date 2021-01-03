/*
<:copyright-gpl

 Copyright 2003 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.

 :>
*/

/*
 *******************************************************************************
 * File Name  : blog.c
 * Description: Implements the tracing of L2 and L3 modifications to a packet
 * 		buffer while it traverses the Linux networking stack.
 *******************************************************************************
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blog.h>
#include <linux/blog_net.h>
#include <linux/nbuff.h>
#include <linux/skbuff.h>

#if defined(CONFIG_BLOG)

#include <linux/netdevice.h>
#include <linux/slab.h>
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   
#define BLOG_NF_CONNTRACK
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#endif /* defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE) */

#include "../bridge/br_private.h"

#ifdef CC_BLOG_SUPPORT_COLOR
#define COLOR(clr_code)     clr_code
#else
#define COLOR(clr_code)
#endif
#define CLRb                COLOR("\e[0;34m")       /* blue */
#define CLRc                COLOR("\e[0;36m")       /* cyan */
#define CLRn                COLOR("\e[0m")          /* normal */
#define CLRerr              COLOR("\e[0;33;41m")    /* yellow on red */
#define CLRN                CLRn"\n"                /* normal newline */

/*--- globals ---*/

/* RFC4008 */
#ifdef SUPPORT_GPL
uint32_t blog_nat_tcp_def_idle_timeout = 21600 *HZ;  
#else
uint32_t blog_nat_tcp_def_idle_timeout = 86400 *HZ;  /* 5 DAYS */
#endif
uint32_t blog_nat_udp_def_idle_timeout = 300 *HZ;    /* 300 seconds */

EXPORT_SYMBOL(blog_nat_tcp_def_idle_timeout);
EXPORT_SYMBOL(blog_nat_udp_def_idle_timeout);

/* Debug macros */
int blog_dbg = 0;


#if defined(CC_BLOG_SUPPORT_DEBUG)
#define blog_print(fmt, arg...)                                         \
    if ( blog_dbg )                                                     \
    printk( CLRc "BLOG %s :" fmt CLRN, __FUNCTION__, ##arg )
#define blog_assertv(cond)                                              \
    if ( !cond ) {                                                      \
        printk( CLRerr "BLOG ASSERT %s : " #cond CLRN, __FUNCTION__ );  \
        return;                                                         \
    }
#define blog_assertr(cond, rtn)                                         \
    if ( !cond ) {                                                      \
        printk( CLRerr "BLOG ASSERT %s : " #cond CLRN, __FUNCTION__ );  \
        return rtn;                                                     \
    }
#define BLOG_DBG(debug_code)    do { debug_code } while(0)
#else
#define blog_print(fmt, arg...) NULL_STMT
#define blog_assertv(cond)      NULL_STMT
#define blog_assertr(cond, rtn) NULL_STMT
#define BLOG_DBG(debug_code)    NULL_STMT
#endif

#define blog_error(fmt, arg...)                                         \
    printk( CLRerr "BLOG ERROR %s :" fmt CLRN, __FUNCTION__, ##arg)

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
#define BLOG_LOCK()             spin_lock_bh( &blog_lock_g )
#define BLOG_UNLOCK()           spin_unlock_bh( &blog_lock_g )
#define BLOG_LOCK_BH()          spin_lock_bh( &blog_lock_g )
#define BLOG_UNLOCK_BH()        spin_unlock_bh( &blog_lock_g )
#define BLOG_LOCK_POOL()        spin_lock_irq( &blog_pool_lock_g )
#define BLOG_UNLOCK_POOL()      spin_unlock_bh( &blog_pool_lock_g )
#else
#define BLOG_LOCK()             local_irq_disable()
#define BLOG_UNLOCK()           local_irq_enable()
#define BLOG_LOCK_BH()          NULL_STMT
#define BLOG_UNLOCK_BH()        NULL_STMT
#define BLOG_LOCK_POOL()        local_irq_disable()
#define BLOG_UNLOCK_POOL()      local_irq_enable()
#endif

#undef  BLOG_DECL
#define BLOG_DECL(x)        #x,         /* string declaration */

/*--- globals ---*/

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)   
spinlock_t blog_lock_g = SPIN_LOCK_UNLOCKED;      /* blogged packet flow */
spinlock_t blog_pool_lock_g = SPIN_LOCK_UNLOCKED; /* blog pool only      */
#endif

/*
 * blog_support_mcast_g inherits the default value from CC_BLOG_SUPPORT_MCAST
 * Exported blog_support_mcast() may be used to set blog_support_mcast_g.
 */
int blog_support_mcast_g = CC_BLOG_SUPPORT_MCAST;
void blog_support_mcast(int config) { blog_support_mcast_g = config; }

/*
 * Traffic flow generator, keep conntrack alive during idle traffic periods
 * by refreshing the conntrack. Dummy sk_buff passed to nf_conn.
 * Netfilter may not be statically loaded.
 */
blog_refresh_t blog_refresh_fn = (blog_refresh_t) NULL;
struct sk_buff * nfskb_p = (struct sk_buff *) NULL;

/*----- Constant string representation of enums for print -----*/
const char * strBlogAction[BLOG_ACTION_MAX] =
{
    BLOG_DECL(PKT_DONE)
    BLOG_DECL(PKT_NORM)
    BLOG_DECL(PKT_BLOG)
};

const char * strBlogDir[BLOG_DIR_MAX] =
{
    BLOG_DECL(DIR_RX)
    BLOG_DECL(DIR_TX)
};

const char * strBlogNetEntity[BLOG_NET_ENTITY_MAX] =
{
    BLOG_DECL(FLOWTRACK)
    BLOG_DECL(BRIDGEFDB)
    BLOG_DECL(MCAST_FDB)
    BLOG_DECL(IF_DEVICE)
};

const char * strBlogNotify[BLOG_NOTIFY_MAX] =
{
    BLOG_DECL(DESTROY_FLOWTRACK)
    BLOG_DECL(DESTROY_BRIDGEFDB)
    BLOG_DECL(MCAST_CONTROL_EVT)
    BLOG_DECL(DESTROY_NETDEVICE)
    BLOG_DECL(LINK_STATE_CHANGE)
    BLOG_DECL(FETCH_NETIF_STATS)
    BLOG_DECL(DYNAMIC_TOS_EVENT)
};

const char * strBlogRequest[BLOG_REQUEST_MAX] =
{
    BLOG_DECL(FLOWTRACK_KEY_SET)
    BLOG_DECL(FLOWTRACK_KEY_GET)
    BLOG_DECL(FLOWTRACK_DSCP_GET)
    BLOG_DECL(FLOW_CONFIRMED)
    BLOG_DECL(FLOW_ASSURED)
    BLOG_DECL(FLOW_ALG_HELPER)
    BLOG_DECL(FLOW_EXCLUDE)
    BLOG_DECL(FLOW_REFRESH)
    BLOG_DECL(BRIDGE_REFRESH)
    BLOG_DECL(NETIF_PUT_STATS)
    BLOG_DECL(LINK_XMIT_FN)
    BLOG_DECL(LINK_NOCARRIER)
};

const char * strBlogEncap[PROTO_MAX] =
{
    BLOG_DECL(BCM_XPHY)
    BLOG_DECL(BCM_SWC)
    BLOG_DECL(ETH_802x)
    BLOG_DECL(VLAN_8021Q)
    BLOG_DECL(PPPoE_2516)
    BLOG_DECL(PPP_1661)
    BLOG_DECL(L3_IPv4)
    BLOG_DECL(L3_IPv6)
};

/*
 *------------------------------------------------------------------------------
 * Support for RFC 2684 headers logging.
 *------------------------------------------------------------------------------
 */
const char * strRfc2684[RFC2684_MAX] =
{
    BLOG_DECL(RFC2684_NONE)         /*                               */
    BLOG_DECL(LLC_SNAP_ETHERNET)    /* AA AA 03 00 80 C2 00 07 00 00 */
    BLOG_DECL(LLC_SNAP_ROUTE_IP)    /* AA AA 03 00 00 00 08 00       */
    BLOG_DECL(LLC_ENCAPS_PPP)       /* FE FE 03 CF                   */
    BLOG_DECL(VC_MUX_ETHERNET)      /* 00 00                         */
    BLOG_DECL(VC_MUX_IPOA)          /*                               */
    BLOG_DECL(VC_MUX_PPPOA)         /*                               */
    BLOG_DECL(PTM)                  /*                               */
};

const uint8_t rfc2684HdrLength[BLOG_MAXPHY] =
{
     0, /* header was already stripped. :                               */
    10, /* LLC_SNAP_ETHERNET            : AA AA 03 00 80 C2 00 07 00 00 */
     8, /* LLC_SNAP_ROUTE_IP            : AA AA 03 00 00 00 08 00       */
     4, /* LLC_ENCAPS_PPP               : FE FE 03 CF                   */
     2, /* VC_MUX_ETHERNET              : 00 00                         */
     0, /* VC_MUX_IPOA                  :                               */
     0, /* VC_MUX_PPPOA                 :                               */
     0, /* PTM                          :                               */
     0, /* ENET                                                         */
     0, /* GPON                                                         */
     0, /* USB                                                          */
     0, /* WLAN                                                         */
};

const uint8_t rfc2684HdrData[RFC2684_MAX][16] =
{
    {},
    { 0xAA, 0xAA, 0x03, 0x00, 0x80, 0xC2, 0x00, 0x07, 0x00, 0x00 },
    { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00 },
    { 0xFE, 0xFE, 0x03, 0xCF },
    { 0x00, 0x00 },
    {},
    {},
    {}
};

const char * strBlogPhy[BLOG_MAXPHY] =
{
    BLOG_DECL(BLOG_PHY_NONE)
    BLOG_DECL(BLOG_XTMPHY_LLC_SNAP_ETHERNET)
    BLOG_DECL(BLOG_XTMPHY_LLC_SNAP_ROUTE_IP)
    BLOG_DECL(BLOG_XTMPHY_LLC_ENCAPS_PPP)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_ETHERNET)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_IPOA)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_PPPOA)
    BLOG_DECL(BLOG_XTMPHY_PTM)
    BLOG_DECL(BLOG_ENETPHY)
    BLOG_DECL(BLOG_GPONPHY)
    BLOG_DECL(BLOG_USBPHY)
    BLOG_DECL(BLOG_WLANPHY)
};

const char * strIpctDir[] = {   /* in reference to enum ip_conntrack_dir */
    BLOG_DECL(DIR_ORIG)
    BLOG_DECL(DIR_RPLY)
    BLOG_DECL(DIR_UNKN)
};

const char * strIpctStatus[] =  /* in reference to enum ip_conntrack_status */
{
    BLOG_DECL(EXPECTED)
    BLOG_DECL(SEEN_REPLY)
    BLOG_DECL(ASSURED)
    BLOG_DECL(CONFIRMED)
    BLOG_DECL(SRC_NAT)
    BLOG_DECL(DST_NAT)
    BLOG_DECL(SEQ_ADJUST)
    BLOG_DECL(SRC_NAT_DONE)
    BLOG_DECL(DST_NAT_DONE)
    BLOG_DECL(DYING)
    BLOG_DECL(FIXED_TIMEOUT)
    BLOG_DECL(BLOG)
};

/*
 *------------------------------------------------------------------------------
 * Default Rx and Tx hooks.
 *------------------------------------------------------------------------------
 */
static BlogDevHook_t blog_rx_hook_g = (BlogDevHook_t)NULL;
static BlogDevHook_t blog_tx_hook_g = (BlogDevHook_t)NULL;
static BlogNotifyHook_t blog_xx_hook_g = (BlogNotifyHook_t)NULL;


/*
 *------------------------------------------------------------------------------
 * Blog_t Free Pool Management.
 * The free pool of Blog_t is self growing (extends upto an engineered
 * value). Could have used a kernel slab cache. 
 *------------------------------------------------------------------------------
 */

/* Global pointer to the free pool of Blog_t */
static Blog_t * blog_list_gp = BLOG_NULL;

static int blog_extends = 0;        /* Extension of Pool on depletion */
#if defined(CC_BLOG_SUPPORT_DEBUG)
static int blog_cnt_free = 0;       /* Number of Blog_t free */
static int blog_cnt_used = 0;       /* Number of in use Blog_t */
static int blog_cnt_hwm  = 0;       /* In use high water mark for engineering */
static int blog_cnt_fails = 0;
#endif

/*
 *------------------------------------------------------------------------------
 * Function   : blog_extend
 * Description: Create a pool of Blog_t objects. When a pool is exhausted
 *              this function may be invoked to extend the pool. The pool is
 *              identified by a global pointer, blog_list_gp. All objects in
 *              the pool chained together in a single linked list.
 * Parameters :
 *   num      : Number of Blog_t objects to be allocated.
 * Returns    : Number of Blog_t objects allocated in pool.
 *------------------------------------------------------------------------------
 */
uint32_t blog_extend( uint32_t num )
{
    register int i;
    register Blog_t * list_p;

    blog_print( "%u", num );

    list_p = (Blog_t *) kmalloc( num * sizeof(Blog_t), GFP_ATOMIC);
    if ( list_p == BLOG_NULL )
    {
#if defined(CC_BLOG_SUPPORT_DEBUG)
        blog_cnt_fails++;
#endif
        blog_print( "WARNING: Failure to initialize %d Blog_t", num );
        return 0;
    }
    blog_extends++;

    /* memset( (void *)list_p, 0, (sizeof(Blog_t) * num ); */
    for ( i = 0; i < num; i++ )
        list_p[i].blog_p = &list_p[i+1];

    BLOG_LOCK_POOL();

    BLOG_DBG( blog_cnt_free += num; );
    list_p[num-1].blog_p = blog_list_gp; /* chain last Blog_t object */
    blog_list_gp = list_p;  /* Head of list */

    BLOG_UNLOCK_POOL();

    return num;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_clr
 * Description  : Clear the data of a Blog_t
 *------------------------------------------------------------------------------
 */
static inline void blog_clr( Blog_t * blog_p )
{
    blog_assertv( ((blog_p != BLOG_NULL) && (_IS_BPTR_(blog_p))) );
    BLOG_DBG( memset( (void*)blog_p, 0, sizeof(Blog_t) ); );

    /* clear phyHdr, count, bmap, and channel */
    blog_p->rx.word = 0;
    blog_p->tx.word = 0;
    blog_p->key.match = 0;    /* clears hash, protocol, l1_tuple */
    blog_p->rx.ct_p = (void *)NULL;
    blog_p->tx.dev_p = (void *)NULL;
    blog_p->fdb[0] = (void *)NULL;
    blog_p->fdb[1] = (void *)NULL;
    blog_p->minMtu = BLOG_ETH_MTU_LEN;
    blog_p->tupleV6.t4in6offset = 0;
    memset( (void*)blog_p->virt_dev_p, 0, sizeof(void*) * MAX_VIRT_DEV);

    blog_print( "blog<0x%08x>", (int)blog_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_get
 * Description  : Allocate a Blog_t from the free list
 * Returns      : Pointer to an Blog_t or NULL, on depletion.
 *------------------------------------------------------------------------------
 */
Blog_t * blog_get( void )
{
    register Blog_t * blog_p;

    if ( blog_list_gp == BLOG_NULL )
    {
#ifdef CC_BLOG_SUPPORT_EXTEND
        if ( (blog_extends >= BLOG_EXTEND_MAX_ENGG)/* Try extending free pool */
          || (blog_extend( BLOG_EXTEND_SIZE_ENGG ) != BLOG_EXTEND_SIZE_ENGG))
        {
            blog_print( "WARNING: free list exhausted" );
            return BLOG_NULL;
        }
#else
        if ( blog_extend( BLOG_EXTEND_SIZE_ENGG ) == 0 )
        {
            blog_print( "WARNING: out of memory" );
            return BLOG_NULL;
        }
#endif
    }

    BLOG_LOCK_POOL();

    BLOG_DBG(
        blog_cnt_free--;
        if ( ++blog_cnt_used > blog_cnt_hwm )
            blog_cnt_hwm = blog_cnt_used;
        );
    blog_p = blog_list_gp;
    blog_list_gp = blog_list_gp->blog_p;

    blog_clr( blog_p );     /* quickly clear the contents */

    BLOG_UNLOCK_POOL();

    blog_print( "blog<0x%08x>", (int)blog_p );

    return blog_p;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_put
 * Description  : Release a Blog_t back into the free pool
 * Parameters   :
 *  blog_p      : Pointer to a non-null Blog_t to be freed.
 *------------------------------------------------------------------------------
 */
void blog_put( Blog_t * blog_p )
{
    blog_assertv( ((blog_p != BLOG_NULL) && (_IS_BPTR_(blog_p))) );

    BLOG_LOCK_POOL();

    blog_clr( blog_p );

    BLOG_DBG( blog_cnt_used--; blog_cnt_free++; );
    blog_p->blog_p = blog_list_gp;  /* clear pointer to skb_p */
    blog_list_gp = blog_p;          /* link into free pool */

    BLOG_UNLOCK_POOL();

    blog_print( "blog<0x%08x>", (int)blog_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_skb
 * Description  : Allocate and associate a Blog_t with an sk_buff.
 * Parameters   :
 *  skb_p       : pointer to a non-null sk_buff
 * Returns      : A Blog_t object or NULL,
 *------------------------------------------------------------------------------
 */
Blog_t * blog_skb( struct sk_buff * skb_p )
{
    blog_assertr( (skb_p != (struct sk_buff *)NULL), BLOG_NULL );
    blog_assertr( (!_IS_BPTR_(skb_p->blog_p)), BLOG_NULL ); /* avoid leak */

    skb_p->blog_p = blog_get(); /* Allocate and associate with sk_buff */

    blog_print( "skb<0x%08x> blog<0x%08x>", (int)skb_p, (int)skb_p->blog_p );

    /* CAUTION: blog_p does not point back to the skb, do it explicitly */
    return skb_p->blog_p;       /* May be null */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_fkb
 * Description  : Allocate and associate a Blog_t with an fkb.
 * Parameters   :
 *  fkb_p       : pointer to a non-null FkBuff_t
 * Returns      : A Blog_t object or NULL,
 *------------------------------------------------------------------------------
 */
Blog_t * blog_fkb( struct fkbuff * fkb_p )
{
    uint32_t in_skb_tag;
    blog_assertr( (fkb_p != (FkBuff_t *)NULL), BLOG_NULL );
    blog_assertr( (!_IS_BPTR_(fkb_p->blog_p)), BLOG_NULL ); /* avoid leak */

    in_skb_tag = _is_in_skb_tag_( fkb_p->flags );

    fkb_p->blog_p = blog_get(); /* Allocate and associate with fkb */

    if ( fkb_p->blog_p != BLOG_NULL )   /* Move in_skb_tag to blog rx info */
        fkb_p->blog_p->rx.info.fkbInSkb = in_skb_tag;

    blog_print( "fkb<0x%08x> blog<0x%08x> in_skb_tag<%u>",
                (int)fkb_p, (int)fkb_p->blog_p, in_skb_tag );
    return fkb_p->blog_p;       /* May be null */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_snull
 * Description  : Dis-associate a sk_buff with any Blog_t
 * Parameters   :
 *  skb_p       : Pointer to a non-null sk_buff
 * Returns      : Previous Blog_t associated with sk_buff
 *------------------------------------------------------------------------------
 */
inline Blog_t * _blog_snull( struct sk_buff * skb_p )
{
    register Blog_t * blog_p;
    blog_p = skb_p->blog_p;
    skb_p->blog_p = BLOG_NULL;
    return blog_p;
}

Blog_t * blog_snull( struct sk_buff * skb_p )
{
    blog_assertr( (skb_p != (struct sk_buff *)NULL), BLOG_NULL );
    blog_print( "skb<0x%08x> blog<0x%08x>", (int)skb_p, (int)skb_p->blog_p );
    return _blog_snull( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_fnull
 * Description  : Dis-associate a fkbuff with any Blog_t
 * Parameters   :
 *  fkb_p       : Pointer to a non-null fkbuff
 * Returns      : Previous Blog_t associated with fkbuff
 *------------------------------------------------------------------------------
 */
inline Blog_t * _blog_fnull( struct fkbuff * fkb_p )
{
    register Blog_t * blog_p;
    blog_p = fkb_p->blog_p;
    fkb_p->blog_p = BLOG_NULL;
    return blog_p;
}

Blog_t * blog_fnull( struct fkbuff * fkb_p )
{
    blog_assertr( (fkb_p != (struct fkbuff *)NULL), BLOG_NULL );
    blog_print( "fkb<0x%08x> blogp<0x%08x>", (int)fkb_p, (int)fkb_p->blog_p );
    return _blog_fnull( fkb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_free
 * Description  : Free any Blog_t associated with a sk_buff
 * Parameters   :
 *  skb_p       : Pointer to a non-null sk_buff
 *------------------------------------------------------------------------------
 */
inline void _blog_free( struct sk_buff * skb_p )
{
    register Blog_t * blog_p;
    blog_p = _blog_snull( skb_p );   /* Dis-associate Blog_t from skb_p */
    if ( likely(blog_p != BLOG_NULL) )
        blog_put( blog_p );         /* Recycle blog_p into free list */
}

void blog_free( struct sk_buff * skb_p )
{
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );
    BLOG_DBG(
        if ( skb_p->blog_p != BLOG_NULL )
            blog_print( "skb<0x%08x> blog<0x%08x> [<%08x>]",
                        (int)skb_p, (int)skb_p->blog_p,
                        (int)__builtin_return_address(0) ); );
    _blog_free( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_skip
 * Description  : Disable further tracing of sk_buff by freeing associated
 *                Blog_t (if any)
 * Parameters   :
 *  skb_p       : Pointer to a sk_buff
 *------------------------------------------------------------------------------
 */
void blog_skip( struct sk_buff * skb_p )
{
    blog_print( "skb<0x%08x> [<%08x>]",
                (int)skb_p, (int)__builtin_return_address(0) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );
    _blog_free( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_xfer
 * Description  : Transfer ownership of a Blog_t between two sk_buff(s)
 * Parameters   :
 *  skb_p       : New owner of Blog_t object 
 *  prev_p      : Former owner of Blog_t object
 *------------------------------------------------------------------------------
 */
void blog_xfer( struct sk_buff * skb_p, const struct sk_buff * prev_p )
{
    Blog_t * blog_p;
    struct sk_buff * mod_prev_p;
    blog_assertv( (prev_p != (struct sk_buff *)NULL) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    mod_prev_p = (struct sk_buff *) prev_p; /* const removal without warning */
    blog_p = _blog_snull( mod_prev_p );
    skb_p->blog_p = blog_p;

    if ( likely(blog_p != BLOG_NULL) )
    {
        blog_print( "skb<0x%08x> to new<0x%08x> blog<0x%08x> [<%08x>]",
                    (int)prev_p, (int)skb_p, (int)blog_p,
                    (int)__builtin_return_address(0) );
        blog_assertv( (_IS_BPTR_(blog_p)) );
        blog_p->skb_p = skb_p;
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_clone
 * Description  : Duplicate a Blog_t for another sk_buff
 * Parameters   :
 *  skb_p       : New owner of cloned Blog_t object 
 *  prev_p      : Blog_t object to be cloned
 *------------------------------------------------------------------------------
 */
void blog_clone( struct sk_buff * skb_p, const struct blog_t * prev_p )
{
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    if ( likely(prev_p != BLOG_NULL) )
    {
        Blog_t * blog_p;

        blog_assertv( (_IS_BPTR_(prev_p)) );
        
        skb_p->blog_p = blog_get(); /* Allocate and associate with skb */
        blog_p = skb_p->blog_p;

        blog_print( "orig blog<0x%08x> new skb<0x%08x> blog<0x%08x> [<%08x>]",
                    (int)prev_p, (int)skb_p, (int)blog_p,
                    (int)__builtin_return_address(0) );

        if ( likely(blog_p != BLOG_NULL) )
        {
            blog_p->skb_p = skb_p;
#define CPY(x) blog_p->x = prev_p->x
            CPY(key.match);
            CPY(mark);
            CPY(priority);
            CPY(rx);
            blog_p->tx.word = 0;
        }
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_link
 * Description  : Associate a network entity with an skb's blog object
 * Parameters   :
 *  entity_type : Network entity type
 *  blog_p      : Pointer to a Blog_t
 *  net_p       : Pointer to a network stack entity 
 *  param1      : optional parameter 1
 *  param2      : optional parameter 2
 *------------------------------------------------------------------------------
 */
void blog_link( BlogNetEntity_t entity_type, Blog_t * blog_p,
                void * net_p, uint32_t param1, uint32_t param2 )
{
    blog_assertv( (entity_type < BLOG_NET_ENTITY_MAX) );
    blog_assertv( (net_p != (void *)NULL) );

    if ( unlikely(blog_p == BLOG_NULL) )
        return;

    blog_assertv( (_IS_BPTR_(blog_p)) );

    blog_print( "link<%s> skb<0x%08x> blog<0x%08x> net<0x%08x> %u %u [<%08x>]",
                strBlogNetEntity[entity_type], (int)blog_p->skb_p, (int)blog_p,
                (int)net_p, param1, param2, (int)__builtin_return_address(0) );

    switch ( entity_type )
    {
        case FLOWTRACK:
        {
#if defined(BLOG_NF_CONNTRACK)
            blog_assertv( ((param1 == BLOG_PARAM1_DIR_ORIG) ||
                           (param1 == BLOG_PARAM1_DIR_REPLY)) );

            if ( unlikely(blog_p->rx.info.multicast) )
                return;

            BLOG_LOCK_BH();

            blog_p->rx.ct_p = net_p; /* Pointer to conntrack */
            /* Save flow direction */
            blog_p->rx.nf_dir = param1;

            BLOG_UNLOCK_BH();
#endif
            break;
        }

        case BRIDGEFDB:
        {
            blog_assertv( ((param1 == BLOG_PARAM1_SRCFDB) ||
                           (param1 == BLOG_PARAM1_DSTFDB)) );

            BLOG_LOCK_BH();

            blog_p->fdb[param1] = net_p;

            BLOG_UNLOCK_BH();

            break;
        }

        case MCAST_FDB:
        {
            blog_assertv( (entity_type != MCAST_FDB) );
            break;
        }

        case IF_DEVICE: /* link virtual interfaces traversed by flow */
        {
            int i;

            blog_assertv( (param1 < BLOG_DIR_MAX) );

            BLOG_LOCK_BH();

            for (i=0; i<MAX_VIRT_DEV; i++)
            {
                /* A flow should not rx and tx with the same device!!  */
                blog_assertv((net_p != DEVP_DETACH_DIR(blog_p->virt_dev_p[i])));

                if ( blog_p->virt_dev_p[i] == NULL )
                {
                    blog_p->virt_dev_p[i] = DEVP_APPEND_DIR(net_p, param1);
                    blog_p->delta[i] = param2 - blog_p->tx.pktlen;
                    break;
                }
            }

            BLOG_UNLOCK_BH();

            blog_assertv( (i != MAX_VIRT_DEV) );
            break;
        }

        default:
            break;
    }
    return;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_notify
 * Description  : Notify a Blog client (xx_hook) of an event.
 * Parameters   :
 *  event       : notification
 *  net_p       : Pointer to a network stack entity
 *  param1      : optional parameter 1
 *  param2      : optional parameter 2
 *------------------------------------------------------------------------------
 */
void blog_notify( BlogNotify_t event, void * net_p,
                  uint32_t param1, uint32_t param2 )
{
    blog_assertv( (event < BLOG_NOTIFY_MAX) );
    blog_assertv( (net_p != (void *)NULL) );

    if ( unlikely(blog_xx_hook_g == (BlogNotifyHook_t)NULL) )
        return;

    blog_print( "notify<%s> net_p<0x%08x>"
                " param1<%u:0x%08x> param2<%u:0x%08x> [<%08x>]",
                strBlogNotify[event], (int)net_p,
                param1, (int)param1, param2, (int)param2,
                (int)__builtin_return_address(0) );

    BLOG_LOCK();

    blog_xx_hook_g( event, net_p, param1, param2 );

    BLOG_UNLOCK();

    return;
}


/*
 *------------------------------------------------------------------------------
 * Function     : blog_request
 * Description  : Blog client requests an operation to be performed on a network
 *                stack entity.
 * Parameters   :
 *  request     : request type
 *  net_p       : Pointer to a network stack entity
 *  param1      : optional parameter 1
 *  param2      : optional parameter 2
 *------------------------------------------------------------------------------
 */
extern void br_fdb_refresh( struct net_bridge_fdb_entry *fdb );

uint32_t blog_request( BlogRequest_t request, void * net_p,
                       uint32_t param1, uint32_t param2 )
{
    uint32_t ret;

    blog_assertr( (request < BLOG_REQUEST_MAX), 0 );
    blog_assertr( (net_p != (void *)NULL), 0 );

#if defined(CC_BLOG_SUPPORT_DEBUG)
    if ( (request!=FLOWTRACK_REFRESH) && (request!=BRIDGE_REFRESH) )
#endif
        blog_print( "request<%s> net_p<0x%08x>"
                    " param1<%u:0x%08x> param2<%u:0x%08x>",
                    strBlogRequest[request], (int)net_p,
                    param1, (int)param1, param2, (int)param2);

    switch ( request )
    {
#if defined(BLOG_NF_CONNTRACK)
        case FLOWTRACK_KEY_SET:
            blog_assertr( ((param1 == BLOG_PARAM1_DIR_ORIG) ||
                           (param1 == BLOG_PARAM1_DIR_REPLY)), 0 );
            ((struct nf_conn *)net_p)->blog_key[param1] = param2;
            return 0;

        case FLOWTRACK_KEY_GET:
            blog_assertr( ((param1 == BLOG_PARAM1_DIR_ORIG) ||
                           (param1 == BLOG_PARAM1_DIR_REPLY)), 0 );
            ret = ((struct nf_conn *)net_p)->blog_key[param1];
            break;

#if defined(CONFIG_NF_DYNTOS) || defined(CONFIG_NF_DYNTOS_MODULE)
        case FLOWTRACK_DSCP:
            blog_assertr( ((param1 == BLOG_PARAM1_DIR_ORIG) ||
                           (param1 == BLOG_PARAM1_DIR_REPLY)), 0 );
            ret = ((struct nf_conn *)net_p)->dyntos.dscp[param1];
            break;
#endif

        case FLOWTRACK_CONFIRMED:    /* E.g. UDP connection confirmed */
            ret = test_bit( IPS_CONFIRMED_BIT,
                            &((struct nf_conn *)net_p)->status );
            break;

        case FLOWTRACK_ASSURED:      /* E.g. TCP connection confirmed */
            ret = test_bit( IPS_ASSURED_BIT,
                            &((struct nf_conn *)net_p)->status );
            break;

        case FLOWTRACK_ALG_HELPER:
        {
            struct nf_conn * nfct_p;
            struct nf_conn_help * help;

            nfct_p = (struct nf_conn *)net_p;
            help = nfct_help(nfct_p);

            if ( (help != (struct nf_conn_help *)NULL )
                && (help->helper != (struct nf_conntrack_helper *)NULL) )
            {
                blog_print( "HELPER ct<0x%08x> helper<%s>",
                            (int)net_p, help->helper->name );
                return 1;
            }
            return 0;
        }

        case FLOWTRACK_EXCLUDE:  /* caution: modifies net_p */
            clear_bit(IPS_BLOG_BIT, &((struct nf_conn *)net_p)->status);
            return 0;

        case FLOWTRACK_REFRESH:
        {
            if ( blog_refresh_fn  )
            {
                uint32_t jiffies;

                if ( param1 == IPPROTO_TCP )
                    jiffies = blog_nat_tcp_def_idle_timeout;
                else if ( param1 == IPPROTO_UDP )
                    jiffies = blog_nat_udp_def_idle_timeout;
                else
                    jiffies = 60;   /* default: non-TCP|UDP timer refresh */

                nfskb_p->nfct = (struct nf_conntrack *)net_p;
                (*blog_refresh_fn)( net_p, 0, nfskb_p, jiffies, 0 );
            }
            return 0;
        }

#endif /* defined(BLOG_NF_CONNTRACK) */

        case BRIDGE_REFRESH:
        {
            br_fdb_refresh( (struct net_bridge_fdb_entry *)net_p );
            return 0;
        }

        case NETIF_PUT_STATS:
        {
            struct net_device * dev_p = (struct net_device *)net_p;
            BlogStats_t * bstats_p = (BlogStats_t *) param1;
            blog_assertr( (bstats_p != (BlogStats_t *)NULL), 0 );

            blog_print("dev_p<0x%08x> rx_pkt<%lu> rx_byte<%lu> tx_pkt<%lu>"
                       " tx_byte<%lu> multicast<%lu>", (int)dev_p,
                        bstats_p->rx_packets, bstats_p->rx_bytes,
                        bstats_p->tx_packets, bstats_p->tx_bytes,
                        bstats_p->multicast);

            if ( dev_p->put_stats )
                dev_p->put_stats( dev_p, bstats_p );
            return 0;
        }
        
        case LINK_XMIT_FN:
        {
            struct net_device * dev_p = (struct net_device *)net_p;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            ret = (uint32_t)(dev_p->netdev_ops->ndo_start_xmit);
#else
            ret = (uint32_t)(dev_p->hard_start_xmit);
#endif
            break;
        }

        case LINK_NOCARRIER:
            ret = test_bit( __LINK_STATE_NOCARRIER,
                            &((struct net_device *)net_p)->state );
            break;

        default:
            return 0;
    }

    blog_print("ret<%u:0x%08x>", ret, (int)ret);

    return ret;
}


/*
 *------------------------------------------------------------------------------
 * Function     : blog_filter
 * Description  : Filter packets that need blogging.
 *                E.g. To skip logging of control versus data type packet.
 *   blog_p     : Received packet parsed and logged into a blog
 * Returns      :
 *   PKT_NORM   : If normal stack processing without logging
 *   PKT_BLOG   : If stack processing with logging
 *------------------------------------------------------------------------------
 */
BlogAction_t blog_filter( Blog_t * blog_p )
{
#if defined(CC_BLOG_SUPPORT_USER_FILTER)
    blog_assertr( ((blog_p != BLOG_NULL) && (_IS_BPTR_(blog_p))), PKT_NORM );
    blog_assertr( (blog_p->rx.info.hdrs != 0), PKT_NORM );

    /*
     * E.g. IGRS/UPnP using Simple Service Discovery Protocol SSDP over HTTPMU
     *      HTTP Multicast over UDP 239.255.255.250:1900,
     *
     *  if ( ! RX_IPinIP(blog_p) && RX_IPV4(blog_p)
     *      && (blog_p->rx.tuple.daddr == htonl(0xEFFFFFFA))
     *      && (blog_p->rx.tuple.port.dest == 1900)
     *      && (blog_p->key.protocol == IPPROTO_UDP) )
     *          return PKT_NORM;
     *
     *  E.g. To filter IPv4 Local Network Control Block 224.0.0/24
     *             and IPv4 Internetwork Control Block  224.0.1/24
     *
     *  if ( ! RX_IPinIP(blog_p) && RX_IPV4(blog_p)
     *      && ( (blog_p->rx.tuple.daddr & htonl(0xFFFFFE00))
     *           == htonl(0xE0000000) )
     *          return PKT_NORM;
     *  
     */

#endif
    return PKT_BLOG;    /* continue in stack with logging */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_finit, blog_sinit
 * Description  : This function may be inserted in a physical network device's
 *                packet receive handler. A receive handler typically extracts
 *                the packet data from the rx DMA buffer ring, allocates and
 *                sets up a sk_buff, decodes the l2 headers and passes the
 *                sk_buff into the network stack via netif_receive_skb/netif_rx.
 *
 *                Prior to constructing a sk_buff, blog_finit() may be invoked
 *                using a fast kernel buffer to carry the received buffer's
 *                context <data,len>, and the receive net_device and l1 info.
 *
 *                This function invokes the bound receive blog hook.
 *
 * Parameters   :
 *  blog_finit() fkb_p: Pointer to a fast kernel buffer<data,len>
 *  blog_sinit() skb_p: Pointer to a Linux kernel skbuff
 *  dev_p       : Pointer to the net_device on which the packet arrived.
 *  encap       : First encapsulation type
 *  channel     : Channel/Port number on which the packet arrived.
 *  phyHdr      : e.g. XTM device RFC2684 header type
 *
 * Returns      :
 *  PKT_DONE    : The fkb|skb is consumed and device should not process fkb|skb.
 *
 *  PKT_NORM    : Device may invoke netif_receive_skb for normal processing.
 *                No Blog is associated and fkb reference count = 0.
 *                [invoking fkb_release() has no effect]
 *
 *  PKT_BLOG    : PKT_NORM behaviour + Blogging enabled.
 *                Must call fkb_release() to free associated Blog
 *
 *------------------------------------------------------------------------------
 */
inline
BlogAction_t blog_finit( struct fkbuff * fkb_p, void * dev_p,
                         uint32_t encap, uint32_t channel, uint32_t phyHdr )
{
    BlogHash_t blogHash;
    BlogAction_t action = PKT_NORM;

    blogHash.match = 0U;     /* also clears hash, protocol = 0 */

    if ( unlikely(blog_rx_hook_g == (BlogDevHook_t)NULL) )
        goto bypass;

    blogHash.l1_channel = (uint8_t)channel;
    blogHash.l1_phy     = phyHdr;

    blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM);
    blog_print( "fkb<0x%08x:%x> pData<0x%08x> length<%d> dev<0x%08x>"
                " chnl<%u> %s PhyHdrLen<%u> key<0x%08x>",
                (int)fkb_p, _is_in_skb_tag_(fkb_p->flags),
                (int)fkb_p->data, fkb_p->len, (int)dev_p,
                channel, strBlogPhy[phyHdr],
                (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0,
                blogHash.match );

    BLOG_LOCK_BH();

    action = blog_rx_hook_g( fkb_p, (void *)dev_p, encap, blogHash.match );

#if defined(CC_BLOG_SUPPORT_USER_FILTER)
    if ( action == PKT_BLOG )
        action = blog_filter(fkb_p->blog_p);
#endif

    if ( unlikely(action == PKT_NORM) )
        fkb_release( fkb_p );

    BLOG_UNLOCK_BH();

bypass:
    return action;
}

/*
 * blog_sinit serves as a wrapper to blog_finit() by overlaying an fkb into a
 * skb and invoking blog_finit().
 */
BlogAction_t blog_sinit( struct sk_buff * skb_p, void * dev_p,
                         uint32_t encap, uint32_t channel, uint32_t phyHdr )
{
    struct fkbuff * fkb_p;
    BlogAction_t action = PKT_NORM;

    if ( unlikely(blog_rx_hook_g == (BlogDevHook_t)NULL) )
        goto bypass;

    blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM );
    blog_print( "skb<0x%08x> pData<0x%08x> length<%d> dev<0x%08x>"
                " chnl<%u> %s PhyHdrLen<%u>",
                (int)skb_p, (int)skb_p->data, skb_p->len, (int)dev_p,
                channel, strBlogPhy[phyHdr],
                (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0 );

    /* CAUTION: Tag that the fkbuff is from sk_buff */
    fkb_p = (FkBuff_t *) &skb_p->fkbInSkb;
    fkb_p->flags = _set_in_skb_tag_(0); /* clear and set in_skb tag */

    action = blog_finit( fkb_p, dev_p, encap, channel, phyHdr );

    if ( action == PKT_BLOG )
    {
         blog_assertr( (fkb_p->blog_p != BLOG_NULL), PKT_NORM );
         fkb_p->blog_p->skb_p = skb_p;
    }
    else
        fkb_p->flags = 0;

bypass:
    return action;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_emit
 * Description  : This function may be inserted in a physical network device's
 *                hard_start_xmit function just before the packet data is
 *                extracted from the sk_buff and enqueued for DMA transfer.
 *
 *                This function invokes the transmit blog hook.
 * Parameters   :
 *  nbuff_p     : Pointer to a NBuff
 *  dev_p       : Pointer to the net_device on which the packet is transmited.
 *  encap       : First encapsulation type
 *  channel     : Channel/Port number on which the packet is transmited.
 *  phyHdr      : e.g. XTM device RFC2684 header type
 *
 * Returns      :
 *  PKT_DONE    : The skb_p is consumed and device should not process skb_p.
 *  PKT_NORM    : Device may use skb_p and proceed with hard xmit 
 *                Blog object is disassociated and freed.
 *------------------------------------------------------------------------------
 */
BlogAction_t blog_emit( void * nbuff_p, void * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr )
{
    BlogHash_t blogHash;
    struct sk_buff * skb_p;
    Blog_t * blog_p;
    BlogAction_t action = PKT_NORM;

    blog_assertr( (nbuff_p != (void *)NULL), PKT_NORM );

    if ( !IS_SKBUFF_PTR(nbuff_p) )
        goto bypass;
    skb_p = PNBUFF_2_SKBUFF(nbuff_p);   /* same as nbuff_p */

    blog_p = skb_p->blog_p;
    if ( blog_p == BLOG_NULL )
        goto bypass;

    blog_assertr( (_IS_BPTR_(blog_p)), PKT_NORM );

    blogHash.match = 0U;

    if ( likely(blog_tx_hook_g != (BlogDevHook_t)NULL) )
    {
        BLOG_LOCK_BH();

        blog_p->tx.dev_p = (void *)dev_p;           /* Log device info */

        blogHash.l1_channel = (uint8_t)channel;
        blogHash.l1_phy     = phyHdr;

        blog_p->priority = skb_p->priority;         /* Log skb info */
        blog_p->mark = skb_p->mark;

        blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM);
        blog_print( "skb<0x%08x> blog<0x%08x> pData<0x%08x> length<%d>"
                    " dev<0x%08x> chnl<%u> %s PhyHdrLen<%u> key<0x%08x>",
            (int)skb_p, (int)blog_p, (int)skb_p->data, skb_p->len,
            (int)dev_p, channel, strBlogPhy[phyHdr],
            (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0,
            blogHash.match );

        action = blog_tx_hook_g( skb_p, (void*)skb_p->dev,
                                 encap, blogHash.match );

        BLOG_UNLOCK_BH();
    }
    blog_free( skb_p );                             /* Dis-associate w/ skb */

bypass:
    return action;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_bind
 * Description  : Override default rx and tx hooks.
 *  blog_rx     : Function pointer to be invoked in blog_finit(), blog_sinit()
 *  blog_tx     : Function pointer to be invoked in blog_emit()
 *  blog_xx     : Function pointer to be invoked in blog_notify()
 *------------------------------------------------------------------------------
 */
void blog_bind( BlogDevHook_t blog_rx, BlogDevHook_t blog_tx,
                BlogNotifyHook_t blog_xx )
{
    blog_print( "Bind Rx[<%08x>] Tx[<%08x>] Notify[<%08x>]",
                (int)blog_rx, (int)blog_tx, (int)blog_xx );

    blog_rx_hook_g = blog_rx;   /* Receive  hook */
    blog_tx_hook_g = blog_tx;   /* Transmit hook */
    blog_xx_hook_g = blog_xx;   /* Notify hook */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog
 * Description  : Log the L2 or L3+4 tuple information
 * Parameters   :
 *  skb_p       : Pointer to the sk_buff
 *  dir         : rx or tx path
 *  encap       : Encapsulation type
 *  len         : Length of header
 *  data_p      : Pointer to encapsulation header data.
 *------------------------------------------------------------------------------
 */
void blog( struct sk_buff * skb_p, BlogDir_t dir, BlogEncap_t encap,
           size_t len, void * data_p )
{
    BlogHeader_t * bHdr_p;
    Blog_t * blog_p;

    blog_assertv( (skb_p != (struct sk_buff *)NULL ) );
    blog_assertv( (skb_p->blog_p != BLOG_NULL) );
    blog_assertv( (_IS_BPTR_(skb_p->blog_p)) );
    blog_assertv( (data_p != (void *)NULL ) );
    blog_assertv( (len <= BLOG_HDRSZ_MAX) );
    blog_assertv( (encap < PROTO_MAX) );

    blog_p = skb_p->blog_p;
    blog_assertv( (blog_p->skb_p == skb_p) );

    bHdr_p = &blog_p->rx + dir;

    if ( encap == L3_IPv4 )    /* Log the IP Tuple */
    {
        BlogTuple_t * bTuple_p = &bHdr_p->tuple;
        BlogIpv4Hdr_t * ip_p   = (BlogIpv4Hdr_t *)data_p;

        /* Discontinue if non IPv4 or with IP options, or fragmented */
        if ( (ip_p->ver != 4) || (ip_p->ihl != 5)
             || (ip_p->flagsFrag & htons(BLOG_IP_FRAG_OFFSET|BLOG_IP_FLAG_MF)) )
            goto skip;

        if ( ip_p->proto == BLOG_IPPROTO_TCP )
        {
            BlogTcpHdr_t * th_p;
            th_p = (BlogTcpHdr_t*)( (uint8_t *)ip_p + BLOG_IPV4_HDR_LEN );

            /* Discontinue if TCP RST/FIN */
            if ( TCPH_RST(th_p) | TCPH_FIN(th_p) )
                goto skip;
            bTuple_p->port.source = th_p->sPort;
            bTuple_p->port.dest = th_p->dPort;
        }
        else if ( ip_p->proto == BLOG_IPPROTO_UDP )
        {
            BlogUdpHdr_t * uh_p;
            uh_p = (BlogUdpHdr_t *)( (uint8_t *)ip_p + BLOG_UDP_HDR_LEN );
            bTuple_p->port.source = uh_p->sPort;
            bTuple_p->port.dest = uh_p->dPort;
        }
        else
            goto skip;  /* Discontinue if non TCP or UDP upper layer protocol */

        bTuple_p->ttl = ip_p->ttl;
        bTuple_p->tos = ip_p->tos;
        bTuple_p->check = ip_p->chkSum;
        bTuple_p->saddr = blog_read32_align16( (uint16_t *)&ip_p->sAddr );
        bTuple_p->daddr = blog_read32_align16( (uint16_t *)&ip_p->dAddr );
        blog_p->key.protocol = ip_p->proto;
    }
    else if ( encap == L3_IPv6 )    /* Log the IPv6 Tuple */
    {
        printk("FIXME blog encap L3_IPv6 \n");
    }
    else    /* L2 encapsulation */
    {
        register short int * d;
        register const short int * s;

        blog_assertv( (bHdr_p->info.count < BLOG_ENCAP_MAX) );
        blog_assertv( ((len<=20) && ((len & 0x1)==0)) );
        blog_assertv( ((bHdr_p->length + len) < BLOG_HDRSZ_MAX) );

        bHdr_p->info.hdrs |= (1U << encap);
        bHdr_p->encap[ bHdr_p->info.count++ ] = encap;
        s = (const short int *)data_p;
        d = (short int *)&(bHdr_p->l2hdr[bHdr_p->length]);
        bHdr_p->length += len;

        switch ( len ) /* common lengths, using half word alignment copy */
        {
            case 20: *(d+9)=*(s+9);
                     *(d+8)=*(s+8);
                     *(d+7)=*(s+7);
            case 14: *(d+6)=*(s+6);
            case 12: *(d+5)=*(s+5);
            case 10: *(d+4)=*(s+4);
            case  8: *(d+3)=*(s+3);
            case  6: *(d+2)=*(s+2);
            case  4: *(d+1)=*(s+1);
            case  2: *(d+0)=*(s+0);
                 break;
            default:
                 goto skip;
        }
    }

    return;

skip:   /* Discontinue further logging by dis-associating Blog_t object */

    blog_skip( skb_p );

    /* DO NOT ACCESS blog_p !!! */
}


/*
 *------------------------------------------------------------------------------
 * Function     : blog_nfct_dump
 * Description  : Dump the nf_conn context
 *  dev_p       : Pointer to a net_device object
 * CAUTION      : nf_conn is not held !!!
 *------------------------------------------------------------------------------
 */
void blog_nfct_dump( struct sk_buff * skb_p, struct nf_conn * ct, uint32_t dir )
{
#if defined(BLOG_NF_CONNTRACK)
    struct nf_conn_help *help_p;
    struct nf_conn_nat  *nat_p;
    int bitix;
    if ( ct == NULL )
    {
        blog_error( "NULL NFCT error" );
        return;
    }

#ifdef CONFIG_NF_NAT_NEEDED
    nat_p = nfct_nat(ct);
#else
    nat_p = (struct nf_conn_nat *)NULL;
#endif

    help_p = nfct_help(ct);
    printk("\tNFCT: ct<0x%p>, info<%x> master<0x%p> mark<0x%p>\n"
           "\t\tF_NAT<%p> keys[%u %u] dir<%s>\n"
           "\t\thelp<0x%p> helper<%s>\n",
            ct, 
            (int)skb_p->nfctinfo, 
            ct->master,
#if defined(CONFIG_NF_CONNTRACK_MARK)
            ct->mark,
#else
            (void *)-1,
#endif
            nat_p, 
            ct->blog_key[IP_CT_DIR_ORIGINAL], 
            ct->blog_key[IP_CT_DIR_REPLY],
            (dir<IP_CT_DIR_MAX)?strIpctDir[dir]:strIpctDir[IP_CT_DIR_MAX],
            help_p,
            (help_p && help_p->helper) ? help_p->helper->name : "NONE" );

    printk( "\t\tSTATUS[ " );
    for ( bitix = 0; bitix <= IPS_BLOG_BIT; bitix++ )
        if ( ct->status & (1 << bitix) )
            printk( "%s ", strIpctStatus[bitix] );
    printk( "]\n" );
#endif /* defined(BLOG_NF_CONNTRACK) */
}


/*
 *------------------------------------------------------------------------------
 * Function     : blog_netdev_dump
 * Description  : Dump the contents of a net_device object.
 *  dev_p       : Pointer to a net_device object
 *
 * CAUTION      : Net device is not held !!!
 *
 *------------------------------------------------------------------------------
 */
static void blog_netdev_dump( struct net_device * dev_p )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    int i;
    printk( "\tDEVICE: %s dev<0x%08x> ndo_start_xmit[<0x%08x>]\n"
            "\t  dev_addr[ ", dev_p->name,
            (int)dev_p, (int)dev_p->netdev_ops->ndo_start_xmit );
    for ( i=0; i<dev_p->addr_len; i++ )
        printk( "%02x ", *((uint8_t *)(dev_p->dev_addr) + i) );
    printk( "]\n" );
#else
    int i;
    printk( "\tDEVICE: %s dev<0x%08x>: poll[<%08x>] hard_start_xmit[<%08x>]\n"
            "\t  hard_header[<%08x>] hard_header_cache[<%08x>]\n"
            "\t  dev_addr[ ", dev_p->name,
            (int)dev_p, (int)dev_p->poll, (int)dev_p->hard_start_xmit,
            (int)dev_p->hard_header, (int)dev_p->hard_header_cache );
    for ( i=0; i<dev_p->addr_len; i++ )
        printk( "%02x ", *((uint8_t *)(dev_p->dev_addr) + i) );
    printk( "]\n" );
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30) */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_tuple_dump
 * Description  : Dump the contents of a BlogTuple_t object.
 *  bTuple_p    : Pointer to the BlogTuple_t object
 *------------------------------------------------------------------------------
 */
static void blog_tuple_dump( BlogTuple_t * bTuple_p )
{
    printk( "\tIPv4:\n"
            "\t\tSrc" BLOG_IPV4_ADDR_PORT_FMT
             " Dst" BLOG_IPV4_ADDR_PORT_FMT "\n"
            "\t\tttl<%3u> tos<%3u> check<0x%04x>\n",
            BLOG_IPV4_ADDR(bTuple_p->saddr), bTuple_p->port.source,
            BLOG_IPV4_ADDR(bTuple_p->daddr), bTuple_p->port.dest,
            bTuple_p->ttl, bTuple_p->tos, bTuple_p->check );
}
 
/*
 *------------------------------------------------------------------------------
 * Function     : blog_tupleV6_dump
 * Description  : Dump the contents of a BlogTupleV6_t object.
 *  bTupleV6_p    : Pointer to the BlogTupleV6_t object
 *------------------------------------------------------------------------------
 */
static void blog_tupleV6_dump( BlogTupleV6_t * bTupleV6_p )
{
    printk( "\tIPv6:\n"
            "\t\tSrc" BLOG_IPV6_ADDR_PORT_FMT "\n"
            "\t\tDst" BLOG_IPV6_ADDR_PORT_FMT "\n"
            "\t\thop_limit<%3u>\n",
            BLOG_IPV6_ADDR(bTupleV6_p->saddr), bTupleV6_p->port.source,
            BLOG_IPV6_ADDR(bTupleV6_p->daddr), bTupleV6_p->port.dest,
            bTupleV6_p->hop_limit );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_l2_dump
 * Description  : parse and dump the contents of all L2 headers
 *  bHdr_p      : Pointer to logged header
 *------------------------------------------------------------------------------
 */
void blog_l2_dump( BlogHeader_t * bHdr_p )
{
    register int i, ix, length, offset = 0;
    BlogEncap_t type;
    char * value = bHdr_p->l2hdr;

    for ( ix=0; ix<bHdr_p->info.count; ix++ )
    {
        type = bHdr_p->encap[ix];

        switch ( type )
        {
            case PPP_1661   : length = BLOG_PPP_HDR_LEN;    break;
            case PPPoE_2516 : length = BLOG_PPPOE_HDR_LEN;  break;
            case VLAN_8021Q : length = BLOG_VLAN_HDR_LEN;   break;
            case ETH_802x   : length = BLOG_ETH_HDR_LEN;    break;
            case BCM_SWC    : 
                              if ( *((uint16_t *)(bHdr_p->l2hdr + 12) ) 
                                   == BLOG_ETH_P_BRCM4TAG)
                                  length = BLOG_BRCM4_HDR_LEN;
                              else
                                  length = BLOG_BRCM6_HDR_LEN;
                              break;

            case L3_IPv4    :
            case L3_IPv6    :
            case BCM_XPHY   :
            default         : printk( "Unsupported type %d\n", type );
                              return;
        }

        printk( "\tENCAP %d. %10s +%2d %2d [ ",
                ix, strBlogEncap[type], offset, length );

        for ( i=0; i<length; i++ )
            printk( "%02x ", (uint8_t)value[i] );

        offset += length;
        value += length;

        printk( "]\n" );
    }
}

void blog_virdev_dump( Blog_t * blog_p )
{
    int i;

    printk( " VirtDev: ");

    for (i=0; i<MAX_VIRT_DEV; i++)
        printk("<0x%08x> ", (int)blog_p->virt_dev_p[i]);

    printk("\n");
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_dump
 * Description  : Dump the contents of a Blog object.
 *  blog_p      : Pointer to the Blog_t object
 *------------------------------------------------------------------------------
 */
void blog_dump( Blog_t * blog_p )
{
    if ( blog_p == BLOG_NULL )
        return;

    blog_assertv( (_IS_BPTR_(blog_p)) );

    printk( "BLOG <0x%08x> owner<0x%08x> ct<0x%08x>\n"
            "\t\tL1 channel<%u> phyLen<%u> phy<%u> <%s>\n"
            "\t\tfdb_src<0x%08x> fdb_dst<0x%08x>\n"
            "\t\thash<%u> prot<%u> prio<0x%08x> mark<0x%08x> Mtu<%u>\n",
            (int)blog_p, (int)blog_p->skb_p, (int)blog_p->rx.ct_p,
            blog_p->key.l1_channel,
            rfc2684HdrLength[blog_p->key.l1_phy],
            blog_p->key.l1_phy,
            strBlogPhy[blog_p->key.l1_phy],
            (int)blog_p->fdb[0], (int)blog_p->fdb[1],
            blog_p->key.hash, blog_p->key.protocol,
            blog_p->priority, blog_p->mark, blog_p->minMtu);

    if ( blog_p->rx.ct_p )
        blog_nfct_dump( blog_p->skb_p, blog_p->rx.ct_p, blog_p->rx.nf_dir );

    printk( "  RX count<%u> channel<%02u> bmap<0x%x> phyLen<%u> phyHdr<%u> %s\n"
            "     hw_support<%u> wan_qdisc<%u> multicast<%u> fkbInSkb<%u>\n",
            blog_p->rx.info.count, blog_p->rx.info.channel,
            blog_p->rx.info.hdrs,
            rfc2684HdrLength[blog_p->rx.info.phyHdr],
            blog_p->rx.info.phyHdr, strBlogPhy[blog_p->rx.info.phyHdr],
            blog_p->rx.info.hw_support, blog_p->rx.info.wan_qdisc,
            blog_p->rx.info.multicast, blog_p->rx.info.fkbInSkb );
    if ( blog_p->rx.info.bmap.L3_IPv4 )
        blog_tuple_dump( &blog_p->rx.tuple );
    blog_l2_dump( &blog_p->rx );

    printk("  TX count<%u> channel<%02u> bmap<0x%x> phyLen<%u> phyHdr<%u> %s\n",
            blog_p->tx.info.count, blog_p->tx.info.channel,
            blog_p->tx.info.hdrs, rfc2684HdrLength[blog_p->tx.info.phyHdr],
            blog_p->tx.info.phyHdr, strBlogPhy[blog_p->tx.info.phyHdr] );
    if ( blog_p->tx.dev_p )
        blog_netdev_dump( blog_p->tx.dev_p );
    if ( blog_p->rx.info.bmap.L3_IPv4 )
        blog_tuple_dump( &blog_p->tx.tuple );
    blog_l2_dump( &blog_p->tx );
    blog_virdev_dump( blog_p );

    if ( blog_p->rx.info.bmap.L3_IPv6 )
        blog_tupleV6_dump( &blog_p->tupleV6 );

#if defined(CC_BLOG_SUPPORT_DEBUG)
    printk( "\t\textends<%d> free<%d> used<%d> HWM<%d> fails<%d>\n",
            blog_extends, blog_cnt_free, blog_cnt_used, blog_cnt_hwm,
            blog_cnt_fails );
#endif

}

/*
 *------------------------------------------------------------------------------
 * Function     : __init_blog
 * Description  : Incarnates the blog system during kernel boot sequence,
 *                in phase subsys_initcall()
 *------------------------------------------------------------------------------
 */
static int __init __init_blog( void )
{
    nfskb_p = alloc_skb( 0, GFP_ATOMIC );
    blog_refresh_fn = (blog_refresh_t)NULL;
    blog_extend( BLOG_POOL_SIZE_ENGG ); /* Build preallocated pool */
    BLOG_DBG( printk( CLRb "BLOG blog_dbg<0x%08x> = %d\n"
                           "%d Blogs allocated of size %d" CLRN,
                           (int)&blog_dbg, blog_dbg,
                           BLOG_POOL_SIZE_ENGG, sizeof(Blog_t) ););
    printk( CLRb "BLOG %s Initialized" CLRN, BLOG_VERSION );
    return 0;
}

subsys_initcall(__init_blog);

EXPORT_SYMBOL(strBlogAction);
EXPORT_SYMBOL(strBlogEncap);

EXPORT_SYMBOL(strRfc2684);
EXPORT_SYMBOL(rfc2684HdrLength);
EXPORT_SYMBOL(rfc2684HdrData);

#else   /* !defined(CONFIG_BLOG) */

int blog_dbg = 0;
int blog_support_mcast_g = BLOG_MCAST_DISABLE; /* = CC_BLOG_SUPPORT_MCAST; */
void blog_support_mcast(int enable) {blog_support_mcast_g = BLOG_MCAST_DISABLE;}

blog_refresh_t blog_refresh_fn = (blog_refresh_t) NULL;

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)   
spinlock_t blog_lock_g = SPIN_LOCK_UNLOCKED;      /* blogged packet flow */
#endif

/* Stub functions for Blog APIs that may be used by modules */
Blog_t * blog_get( void ) { return BLOG_NULL; }
void     blog_put( Blog_t * blog_p ) { return; }

Blog_t * blog_skb( struct sk_buff * skb_p) { return BLOG_NULL; }
Blog_t * blog_fkb( struct fkbuff * fkb_p ) { return BLOG_NULL; }

Blog_t * blog_snull( struct sk_buff * skb_p ) { return BLOG_NULL; }
Blog_t * blog_fnull( struct fkbuff * fkb_p ) { return BLOG_NULL; }

void     blog_free( struct sk_buff * skb_p ) { return; }

void     blog_skip( struct sk_buff * skb_p ) { return; }
void     blog_xfer( struct sk_buff * skb_p, const struct sk_buff * prev_p )
         { return; }
void     blog_clone( struct sk_buff * skb_p, const struct blog_t * prev_p )
         { return; }

void     blog_link( BlogNetEntity_t entity_type, Blog_t * blog_p,
                    void * net_p, uint32_t param1, uint32_t param2 ) { return; }

void     blog_notify( BlogNotify_t event, void * net_p,
                      uint32_t param1, uint32_t param2 ) { return; }

uint32_t blog_request( BlogRequest_t event, void * net_p,
                       uint32_t param1, uint32_t param2 ) { return 0; }

BlogAction_t blog_filter( Blog_t * blog_p )
         { return PKT_NORM; }

BlogAction_t blog_sinit( struct sk_buff * skb_p, void * dev_p,
                         uint32_t encap, uint32_t channel, uint32_t phyHdr )
         { return PKT_NORM; }

BlogAction_t blog_finit( struct fkbuff * fkb_p, void * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr )
         { return PKT_NORM; }

BlogAction_t blog_emit( void * nbuff_p, void * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr )
         { return PKT_NORM; }

void     blog_bind( BlogDevHook_t blog_rx, BlogDevHook_t blog_tx,
                    BlogNotifyHook_t blog_xx ) { return; }

void     blog( struct sk_buff * skb_p, BlogDir_t dir, BlogEncap_t encap,
               size_t len, void * data_p ) { return; }

void     blog_dump( Blog_t * blog_p ) { return; }

#endif  /* else !defined(CONFIG_BLOG) */

EXPORT_SYMBOL(blog_dbg);
EXPORT_SYMBOL(blog_support_mcast_g);
EXPORT_SYMBOL(blog_support_mcast);
EXPORT_SYMBOL(blog_refresh_fn);

#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)   
EXPORT_SYMBOL(blog_lock_g);
#endif

EXPORT_SYMBOL(blog_extend);
EXPORT_SYMBOL(blog_get);
EXPORT_SYMBOL(blog_put);
EXPORT_SYMBOL(blog_skb);
EXPORT_SYMBOL(blog_fkb);
EXPORT_SYMBOL(blog_snull);
EXPORT_SYMBOL(blog_fnull);
EXPORT_SYMBOL(blog_free);
EXPORT_SYMBOL(blog_dump);
EXPORT_SYMBOL(blog_skip);
EXPORT_SYMBOL(blog_xfer);
EXPORT_SYMBOL(blog_clone);
EXPORT_SYMBOL(blog_link);
EXPORT_SYMBOL(blog_notify);
EXPORT_SYMBOL(blog_request);
EXPORT_SYMBOL(blog_filter);
EXPORT_SYMBOL(blog_sinit);
EXPORT_SYMBOL(blog_finit);
EXPORT_SYMBOL(blog_emit);
EXPORT_SYMBOL(blog_bind);

EXPORT_SYMBOL(blog);
