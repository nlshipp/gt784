/***********************************************************************
 *
 *  Copyright (c) 2006  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# This program may be linked with other software licensed under the GPL. 
# When this happens, this program may be reproduced and distributed under 
# the terms of the GPL. 
# 
# 
# 1. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
#    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
#    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH 
#    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND 
#    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, 
#    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR 
#    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE 
#    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR 
#    PERFORMANCE OF THE SOFTWARE. 
# 
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR 
#    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, 
#    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY 
#    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN 
#    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; 
#    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE 
#    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS 
#    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY 
#    LIMITED REMEDY. 
#
 * 
 ************************************************************************/

#include <string.h>
#include "cms.h"
#include "cms_mem.h"
#include "cms_ast.h"
#include "oal.h"
#include "bget.h"


/** Macro to round up to nearest 4 byte length */
#define ROUNDUP4(s)  (((s) + 3) & 0xfffffffc)


/** Macro to calculate how much we need to allocate for a given user size request.
 *
 * We need the header even when not doing MEM_DEBUG to keep
 * track of the size and allocFlags that was passed in during cmsMem_alloc.
 * This info is needed during cmsMem_realloc.
 */
#ifdef CMS_MEM_DEBUG
#define REAL_ALLOC_SIZE(s) (CMS_MEM_HEADER_LENGTH + ROUNDUP4(s) + CMS_MEM_FOOTER_LENGTH)
#else
#define REAL_ALLOC_SIZE(s) (CMS_MEM_HEADER_LENGTH + (s))
#endif


static CmsMemStats mStats;

#ifdef MDM_SHARED_MEM

/** Macro to determine if a pointer is in shared memory or not. */
#define IS_IN_SHARED_MEM(p) ((((UINT32) (p)) >= mStats.shmAllocStart) && \
                             (((UINT32) (p)) < mStats.shmAllocEnd))


void cmsMem_initSharedMem(void *addr, UINT32 len)
{
   mStats.shmAllocStart = (UINT32) addr;
   mStats.shmAllocEnd = mStats.shmAllocStart + len;
   mStats.shmTotalBytes = len;

   cmsLog_notice("shm pool: %p-%p", mStats.shmAllocStart, mStats.shmAllocEnd);

   bpool(addr, len);
}

void cmsMem_initSharedMemPointer(void *addr, UINT32 len)
{
   /*
    * You might be tempted to do a memset(&mStats, 0, sizeof(mStats)) here.
    * But don't do it.  mStats will be initialized to all zeros anyways since
    * it is in the bss.  And smd will start using the memory allocator before
    * it calls cmsMem_initSharedMemPointer, so if we zero out the structure
    * at the beginning of this function, the counters will be wrong.
    */
   mStats.shmAllocStart = (UINT32) addr;
   mStats.shmAllocEnd = mStats.shmAllocStart + len;
   mStats.shmTotalBytes = len;

   cmsLog_notice("shm pool: %p-%p", mStats.shmAllocStart, mStats.shmAllocEnd);

   bcm_secondary_bpool(addr);
}

#endif

void cmsMem_cleanup(void)
{

#ifdef MDM_SHARED_MEM
   bcm_cleanup_bpool();
#endif

   return;
}


void *_cmsMem_alloc(UINT32 size, UINT32 allocFlags)
{
   void *buf;
   UINT32 allocSize;

   allocSize = REAL_ALLOC_SIZE(size);

#ifdef MDM_SHARED_MEM
   if (allocFlags & ALLOC_SHARED_MEM)
   {
      buf = bget(allocSize);
   }
   else
#endif
   {
      buf = oal_malloc(allocSize);
      if (buf)
      {
         mStats.bytesAllocd += size;
         mStats.numAllocs++;
      }
   }


   if (buf != NULL)
   {
      UINT32 *intBuf = (UINT32 *) buf;
      UINT32 intSize = allocSize / sizeof(UINT32);


      if (allocFlags & ALLOC_ZEROIZE)
      {
         memset(buf, 0, allocSize);
      }
#ifdef CMS_MEM_POISON_ALLOC_FREE
      else
      {
         /*
          * Set alloc'ed buffer to garbage to catch use-before-init.
          * But we also allocate huge buffers for storing image downloads.
          * Don't bother writing garbage to those huge buffers.
          */
         if (allocSize < 64 * 1024)
         {
            memset(buf, CMS_MEM_ALLOC_PATTERN, allocSize);
         }
      }
#endif
         
      /*
       * Record the allocFlags in the first word, and the 
       * size of user buffer in the next 2 words of the buffer.
       * Make 2 copies of the size in case one of the copies gets corrupted by
       * an underflow.  Make one copy the XOR of the other so that there are
       * not so many 0's in size fields.
       */
      intBuf[0] = allocFlags;
      intBuf[1] = size;
      intBuf[2] = intBuf[1] ^ 0xffffffff;

      buf = &(intBuf[3]); /* this gets returned to user */

#ifdef CMS_MEM_DEBUG
      {
         UINT8 *charBuf = (UINT8 *) buf;
         UINT32 i, roundup4Size = ROUNDUP4(size);

         for (i=size; i < roundup4Size; i++)
         {
            charBuf[i] = CMS_MEM_FOOTER_PATTERN & 0xff;
         }

         intBuf[intSize - 1] = CMS_MEM_FOOTER_PATTERN;
         intBuf[intSize - 2] = CMS_MEM_FOOTER_PATTERN;
      }
#endif

   }

   return buf;
}


void *_cmsMem_realloc(void *origBuf, UINT32 size)
{
   void *buf;
   UINT32 origSize, origAllocSize, origAllocFlags;
   UINT32 allocSize;
   UINT32 *intBuf;

   if (origBuf == NULL)
   {
      cmsLog_error("cannot take a NULL buffer");
      return NULL;
   }

   if (size == 0)
   {
      _cmsMem_free(origBuf);
      return NULL;
   }

   allocSize = REAL_ALLOC_SIZE(size);


   intBuf = (UINT32 *) (((UINT32) origBuf) - CMS_MEM_HEADER_LENGTH);

   origAllocFlags = intBuf[0];
   origSize = intBuf[1];

   /* sanity check the original length */
   if (intBuf[1] != (intBuf[2] ^ 0xffffffff))
   {
      cmsLog_error("memory underflow detected, %d %d", intBuf[1], intBuf[2]);
      cmsAst_assert(0);
      return NULL;
   }

   origAllocSize = REAL_ALLOC_SIZE(origSize);

   if (allocSize <= origAllocSize)
   {
      /* currently, I don't shrink buffers, but could in the future. */
      return origBuf;
   }

   buf = _cmsMem_alloc(allocSize, origAllocFlags);
   if (buf != NULL)
   {
      /* got new buffer, copy orig buffer to new buffer */
      memcpy(buf, origBuf, origSize);
      _cmsMem_free(origBuf);
   }
   else
   {
      /*
       * We could not allocate a bigger buffer.
       * Return NULL but leave the original buffer untouched.
       */
   }

   return buf;
}



/** Free previously allocated memory
 * @param buf Previously allocated buffer.
 */
void _cmsMem_free(void *buf)
{
   UINT32 size;

   if (buf != NULL)
   {
      UINT32 *intBuf = (UINT32 *) (((UINT32) buf) - CMS_MEM_HEADER_LENGTH);

      size = intBuf[1];

      if (intBuf[1] != (intBuf[2] ^ 0xffffffff))
      {
         cmsLog_error("memory underflow detected, %d %d", intBuf[1], intBuf[2]);
         cmsAst_assert(0);
         return;
      }

#ifdef CMS_MEM_DEBUG
      {
         UINT32 allocSize, intSize, roundup4Size, i;
         UINT8 *charBuf = (UINT8 *) buf;

         allocSize = REAL_ALLOC_SIZE(intBuf[1]);
         intSize = allocSize / sizeof(UINT32);
         roundup4Size = ROUNDUP4(intBuf[1]);

         for (i=intBuf[1]; i < roundup4Size; i++)
         {
            if (charBuf[i] != (UINT8) (CMS_MEM_FOOTER_PATTERN & 0xff))
            {
               cmsLog_error("memory overflow detected at idx=%d 0x%x 0x%x 0x%x",
                            i, charBuf[i], intBuf[intSize-1], intBuf[intSize-2]);
               cmsAst_assert(0);
               return;
            }
         }
               
         if ((intBuf[intSize - 1] != CMS_MEM_FOOTER_PATTERN) ||
             (intBuf[intSize - 2] != CMS_MEM_FOOTER_PATTERN))
         {
            cmsLog_error("memory overflow detected, 0x%x 0x%x",
                         intBuf[intSize - 1], intBuf[intSize - 2]);
            cmsAst_assert(0);
            return;
         }

#ifdef CMS_MEM_POISON_ALLOC_FREE
         /*
          * write garbage into buffer which is about to be freed to detect
          * users of freed buffers.
          */
         memset(intBuf, CMS_MEM_FREE_PATTERN, allocSize);
#endif
      }

#endif  /* CMS_MEM_DEBUG */

      buf = intBuf;  /* buf points to real start of buffer */


#ifdef MDM_SHARED_MEM
      if (IS_IN_SHARED_MEM(buf))
      {
         brel(buf);
      }
      else
#endif
      {
         oal_free(buf);
         mStats.bytesAllocd -= size;
         mStats.numFrees++;
      }
   }
}


char *cmsMem_strdup(const char *str)
{
   return cmsMem_strdupFlags(str, 0);
}


char *cmsMem_strdupFlags(const char *str, UINT32 flags)
{
   UINT32 len;
   void *buf;

   if (str == NULL)
   {
      return NULL;
   }

   /* this is somewhat dangerous because it depends on str being NULL
    * terminated.  Use strndup/strlen if not sure the length of the string.
    */
   len = strlen(str);

   buf = cmsMem_alloc(len+1, flags);
   if (buf == NULL)
   {
      return NULL;
   }

   strncpy((char *) buf, str, len+1);

   return ((char *) buf);
}



char *cmsMem_strndup(const char *str, UINT32 maxlen)
{
   return cmsMem_strndupFlags(str, maxlen, 0);
}


char *cmsMem_strndupFlags(const char *str, UINT32 maxlen, UINT32 flags)
{
   UINT32 len;
   char *buf;

   if (str == NULL)
   {
      return NULL;
   }

   len = cmsMem_strnlen(str, maxlen, NULL);

   buf = (char *) cmsMem_alloc(len+1, flags);
   if (buf == NULL)
   {
      return NULL;
   }

   strncpy(buf, str, len);
   buf[len] = 0;

   return buf;
}


UINT32 cmsMem_strnlen(const char *str, UINT32 maxlen, UBOOL8 *isTerminated)
{
   UINT32 len=0;

   while ((len < maxlen) && (str[len] != 0))
   {
      len++;
   }

   if (isTerminated != NULL)
   {
      *isTerminated = (str[len] == 0);
   }

   return len;
}


void cmsMem_getStats(CmsMemStats *stats)
{
   bufsize curalloc, totfree, maxfree;
   long nget, nrel;


   stats->shmAllocStart = mStats.shmAllocStart;
   stats->shmAllocEnd = mStats.shmAllocEnd;
   stats->shmTotalBytes = mStats.shmTotalBytes;

   /*
    * Get shared memory stats from bget.  The shared memory stats reflects
    * activity of all processes who are accessing the shared memory region,
    * not just this process.
    */
   bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);

   stats->shmBytesAllocd = (UINT32) curalloc;
   stats->shmNumAllocs = (UINT32) nget;
   stats->shmNumFrees = (UINT32) nrel;


   /* the private heap memory stats can come directly from our data structure */
   stats->bytesAllocd = mStats.bytesAllocd;
   stats->numAllocs = mStats.numAllocs;
   stats->numFrees = mStats.numFrees;

   return;
}

#define KB_IN_B  1024

void cmsMem_dumpMemStats()
{
   CmsMemStats memStats;

   cmsMem_getStats(&memStats);

   printf("Total MDM Shared Memory Region : %dKB\n", (memStats.shmAllocEnd - MDM_SHM_ATTACH_ADDR)/KB_IN_B);
   printf("Shared Memory Usable           : %06dKB\n", memStats.shmTotalBytes/KB_IN_B);
   printf("Shared Memory in-use           : %06dKB\n", memStats.shmBytesAllocd/KB_IN_B);
   printf("Shared Memory free             : %06dKB\n", (memStats.shmTotalBytes - memStats.shmBytesAllocd)/KB_IN_B);
   printf("Shared Memory allocs           : %06d\n", memStats.shmNumAllocs);
   printf("Shared Memory frees            : %06d\n", memStats.shmNumFrees);
   printf("Shared Memory alloc/free delta : %06d\n", memStats.shmNumAllocs - memStats.shmNumFrees);
   printf("\n");

   printf("Heap bytes in-use     : %06d\n", memStats.bytesAllocd);
   printf("Heap allocs           : %06d\n", memStats.numAllocs);
   printf("Heap frees            : %06d\n", memStats.numFrees);
   printf("Heap alloc/free delta : %06d\n", memStats.numAllocs - memStats.numFrees);

   return;
}

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <sys/resource.h>
#include <arpa/inet.h>

#define ACT_MEMLEAK_MAX_STACK 300
#define ACT_MEM_LEAK_MEM_TAG_B   121
#define ACT_MEM_LEAK_MEM_TAG_E   99

#define ACT_MEMLEAK_MQ_OP_ADD    1
#define ACT_MEMLEAK_MQ_OP_DEL    2
#define ACT_MEMLEAK_MQ_OP_OVL    4
#define ACT_MEMLEAK_MQ_OP_CLR    8

#define ACT_MEMLEAK_MQ_OP_DISP_UNFREE 122
#define ACT_MEMLEAK_MQ_OP_DISP_DBFREE 123
#define ACT_MEMLEAK_MQ_OP_DISP_OVFLOW 124

#define ACT_MEMLEAK_MQ_OP_DISP  125
#define ACT_MEMLEAK_MQ_OP_SEGV  126
#define ACT_MEMLEAK_MQ_OP_QUIT  127

#define ACT_MEMLEAK_MQ_OP_PROFILE_ADD  80
#define ACT_MEMLEAK_MQ_OP_PROFILE_DISP 81
#define ACT_MEMLEAK_MQ_OP_PROFILE_END  82

#define ACT_MEMLEAK_MQ_OP_MAPS  111

#define MQ_SERVER_IP "192.168.0.2"
#define MQ_SERVER_PORT 1998



int gs_socket = 0;

#define MALLOC(p_ptr, size, flag) p_ptr = _cmsMem_alloc(size, flag)
#define REALLOC(p_ptr, size) p_ptr = _cmsMem_realloc(p_ptr, size)
#define FREE(p_ptr) _cmsMem_free(p_ptr)


typedef struct act_memleak_mq_s{
    unsigned int pid;
    void *p_mem;
    unsigned int mem_size;
    time_t time;
    unsigned int stack_index[ACT_MEMLEAK_MAX_STACK];
    char mq_opcode;
}act_memleak_mq_t;   

#define MAPS_STACK 0
#define MAPS_LIB_CMS_CORE 1
#define MAPS_LIB_CMS_UTIL 2
#define MAPS_LIB_CMS_DAL 3
#define MAPS_LIB_CMS_MSG 4
#define MAPS_MAIN        5

#define MAPS_MAX         6

typedef struct act_memleak_maps_s{
    unsigned int add1;
    unsigned int add2;
    unsigned int add3;
    unsigned int add4;
    char name[32];
}act_memleak_maps_t;

typedef struct act_profile_mq_s{
    unsigned int caller_pc;
    unsigned int self_pc;
    struct timespec time;
}act_profile_mq_t;

typedef struct act_memleak_mq_s1{
    unsigned int pid;
    void *p_mem;
    unsigned int mem_size;
    time_t time;
    union{
        unsigned int stack_index[ACT_MEMLEAK_MAX_STACK];
        act_memleak_maps_t maps[MAPS_MAX];
        act_profile_mq_t profile;
    }u;
    char mq_opcode;
}act_memleak_mq_t1;


void mcount(unsigned int pc1, unsigned int pc2);
void profile_end();

static inline int mc_socket_tcp_open(char *ip,
                       int  port)
{
	struct sockaddr_in 	address;
	 int 		handle = -1;

        signal(13, SIG_IGN);
        
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(ip);
	address.sin_port = htons((unsigned short)port);
        
        
        handle = socket(AF_INET, SOCK_STREAM, 0);
 
        if( connect(handle, (struct sockaddr *)&address, sizeof(address)) < 0) {

            close(handle);
            return -1;
	}else {

            return handle;
	}
}


static inline int mc_socket_tcp_close(int  socket_id)
{
	return close(socket_id);
}


static inline int mc_socket_tcp_write(int sock_id, 
                        char *buf, 
                        int buf_size,
                        int flag)
{
        int 	size = 0;
        int 	res  = 0;

	
	if (sock_id < 0)
            return -1;
	if (buf == NULL)
            return -1;
	if (buf_size < 0)
            return -1;
	
	while(size < buf_size){
        	res = send(sock_id, (char *)buf + size, buf_size - size, 0);
   		if (res <= 0){
			return -1;
		}
        	size += res;
		if (!flag){
			return size;
		}
        }
	
	return size;
	
}

static int g_sock = -1;

static inline void memleak_tcp_write(char *buf, int buf_size)
{
    if (g_sock < 0){
        g_sock = mc_socket_tcp_open(MQ_SERVER_IP, MQ_SERVER_PORT);
    }

    if (g_sock > 0){
        if (mc_socket_tcp_write(g_sock, buf, buf_size, 1) < 0){
            //  printf("TCP WRITE FAILED !!!!!!!!!!\n");
            close(g_sock);
            g_sock = -1;
        }
    }else {
        ;//printf("TCP OPEN FAILED !!!!!!!!!!\n");
    }
}

static unsigned int g_stack_max = 0;

void maps(act_memleak_mq_t1 *p_mq_ref1)
{
    FILE *fp = NULL;
    int len;
    char buf[8192] = "\0";
    char file[64] = "\0";
    char *ptr = NULL;
    char *line = NULL;
    
    unsigned int add1;
    unsigned int add2;
    char attr[16];
    unsigned int offset;
    char time[16];
    unsigned int inode;
    char name[128];
    char program_name[128] = "\0";

    
    if (g_stack_max != 0)
        return;
    
    g_stack_max = 1;

    memset(p_mq_ref1, 0, sizeof(act_memleak_mq_t1));
    p_mq_ref1->pid =  getpid();
    
    sprintf(file, "/proc/%d/status", p_mq_ref1->pid);
    
    fp = fopen(file, "rb"); 
    if(fp == NULL){
        return ;
    }
    
    fread(buf,1, 8192, fp);
    fclose(fp);
    fp = NULL;
    
    ptr = buf;
    
    line = strsep(&ptr, "\r\n");
        
    sscanf(line, "%s\t%s", name, program_name);
 
    sprintf(file, "/proc/%d/maps", p_mq_ref1->pid);
    
    fp = fopen(file, "rb"); 
    if(fp == NULL){
        return ;
    }

    
    fread(buf,1, 8192, fp);
    fclose(fp);
    

    
    len = strlen(buf);
    ptr = buf;
    
    
    while(ptr != NULL && ptr < buf + len){
        memset(name, 0, sizeof(name));
        line = strsep(&ptr, "\r\n");

        sscanf(line, "%x-%x %s %x %s %x %s", &add1, &add2, attr, &offset, time, &inode, name);

        if (strstr(name, "[stack]")){
            strcpy(p_mq_ref1->u.maps[0].name, "[stack]");
            p_mq_ref1->u.maps[0].add1 = add1;
            p_mq_ref1->u.maps[0].add2 = add2;
            g_stack_max = add2;
        }else if (strstr(name, "libcms_core.so")){
            if (strstr(p_mq_ref1->u.maps[1].name, "libcms_core.so")){
                p_mq_ref1->u.maps[1].add3 = add1;
                p_mq_ref1->u.maps[1].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[1].name, "libcms_core.so");
                p_mq_ref1->u.maps[1].add1 = add1;
                p_mq_ref1->u.maps[1].add2 = add2;
            }
        }else if (strstr(name, "libcms_util.so")){
            if (strstr(p_mq_ref1->u.maps[2].name, "libcms_util.so")){
                p_mq_ref1->u.maps[2].add3 = add1;
                p_mq_ref1->u.maps[2].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[2].name, "libcms_util.so");
                p_mq_ref1->u.maps[2].add1 = add1;
                p_mq_ref1->u.maps[2].add2 = add2;
            }
        }else if (strstr(name, "libcms_dal.so")){
            if (strstr(p_mq_ref1->u.maps[3].name, "libcms_dal.so")){
                p_mq_ref1->u.maps[3].add3 = add1;
                p_mq_ref1->u.maps[3].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[3].name, "libcms_dal.so");
                p_mq_ref1->u.maps[3].add1 = add1;
                p_mq_ref1->u.maps[3].add2 = add2;
            }
        }else if (strstr(name, "libcms_msg.so")){
            if (strstr(p_mq_ref1->u.maps[4].name, "libcms_msg.so")){
                p_mq_ref1->u.maps[4].add3 = add1;
                p_mq_ref1->u.maps[4].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[4].name, "libcms_msg.so");
                p_mq_ref1->u.maps[4].add1 = add1;
                p_mq_ref1->u.maps[4].add2 = add2;
            }
        }else if (strstr(name, "libnanoxml.so")){
            if (strstr(p_mq_ref1->u.maps[6].name, "libnanoxml.so")){
                p_mq_ref1->u.maps[6].add3 = add1;
                p_mq_ref1->u.maps[6].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[6].name, "libnanoxml.so");
                p_mq_ref1->u.maps[6].add1 = add1;
                p_mq_ref1->u.maps[6].add2 = add2;
            }
        }else if (strstr(name, "libxdslctl.so")){
            if (strstr(p_mq_ref1->u.maps[7].name, "libxdslctl.so")){
                p_mq_ref1->u.maps[7].add3 = add1;
                p_mq_ref1->u.maps[7].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[7].name, "libxdslctl.so");
                p_mq_ref1->u.maps[7].add1 = add1;
                p_mq_ref1->u.maps[7].add2 = add2;
            }
        }else if (strstr(name, "libatmctl.so")){
            if (strstr(p_mq_ref1->u.maps[8].name, "libatmctl.so")){
                p_mq_ref1->u.maps[8].add3 = add1;
                p_mq_ref1->u.maps[8].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[8].name, "libatmctl.so");
                p_mq_ref1->u.maps[8].add1 = add1;
                p_mq_ref1->u.maps[8].add2 = add2;
            }
        }else if (strstr(name, "libcms_boardctl.so")){
            if (strstr(p_mq_ref1->u.maps[9].name, "libcms_boardctl.so")){
                p_mq_ref1->u.maps[9].add3 = add1;
                p_mq_ref1->u.maps[9].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[9].name, "libcms_boardctl.so");
                p_mq_ref1->u.maps[9].add1 = add1;
                p_mq_ref1->u.maps[9].add2 = add2;
            }                  
        }else if (strstr(name, "libwlmngr.so")){
            if (strstr(p_mq_ref1->u.maps[10].name, "libwlmngr.so")){
                p_mq_ref1->u.maps[10].add3 = add1;
                p_mq_ref1->u.maps[10].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[10].name, "libwlmngr.so");
                p_mq_ref1->u.maps[10].add1 = add1;
                p_mq_ref1->u.maps[10].add2 = add2;
            }
        }else if (strstr(name, "libwlctl.so")){
            if (strstr(p_mq_ref1->u.maps[11].name, "libwlctl.so")){
                p_mq_ref1->u.maps[11].add3 = add1;
                p_mq_ref1->u.maps[11].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[11].name, "libwlctl.so");
                p_mq_ref1->u.maps[11].add1 = add1;
                p_mq_ref1->u.maps[11].add2 = add2;
            }
        }else if (strstr(name, "libnvram.so")){
            if (strstr(p_mq_ref1->u.maps[12].name, "libnvram.so")){
                p_mq_ref1->u.maps[12].add3 = add1;
                p_mq_ref1->u.maps[12].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[12].name, "libnvram.so");
                p_mq_ref1->u.maps[12].add1 = add1;
                p_mq_ref1->u.maps[12].add2 = add2;
            }
        }else if (strstr(name, "libcgi.so")){
            if (strstr(p_mq_ref1->u.maps[13].name, "libcgi.so")){
                p_mq_ref1->u.maps[13].add3 = add1;
                p_mq_ref1->u.maps[13].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[13].name, "libcgi.so");
                p_mq_ref1->u.maps[13].add1 = add1;
                p_mq_ref1->u.maps[13].add2 = add2;
            }
        }else if (strstr(name, program_name)){
            if (strstr(p_mq_ref1->u.maps[5].name, program_name)){
                p_mq_ref1->u.maps[5].add3 = add1;
                p_mq_ref1->u.maps[5].add4 = add2;
            }else {
                strcpy(p_mq_ref1->u.maps[5].name, program_name);
                p_mq_ref1->u.maps[5].add1 = add1;
                p_mq_ref1->u.maps[5].add2 = add2;
            }
        }
    }

    if (strlen(p_mq_ref1->u.maps[5].name) <= 1){
        strcpy(p_mq_ref1->u.maps[5].name, program_name);
        p_mq_ref1->u.maps[5].add1 = 0x40000;
    }
    
    p_mq_ref1->mq_opcode = ACT_MEMLEAK_MQ_OP_MAPS;
    memleak_tcp_write((char *)p_mq_ref1, sizeof(act_memleak_mq_t1));

    //atexit(profile_end);
}

void profile_end()
{
    act_memleak_mq_t1 mq_mem_ref;

    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_PROFILE_END;

    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
}

void __cyg_profile_func_enter(unsigned int pc2, unsigned int pc1)
{
    act_memleak_mq_t1 mq_mem_ref;

    maps(&mq_mem_ref);

    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();
    mq_mem_ref.u.profile.caller_pc = pc1;
    mq_mem_ref.u.profile.self_pc = pc2;
    
    clock_gettime(CLOCK_REALTIME, &mq_mem_ref.u.profile.time);

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_PROFILE_ADD;

    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
    
}

void __cyg_profile_func_exit(unsigned int pc2, unsigned int pc1)
{
    return;
}

void __mcount(unsigned int pc1, unsigned int pc2)
{
    act_memleak_mq_t1 mq_mem_ref;

    maps(&mq_mem_ref);

    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();
    mq_mem_ref.u.profile.caller_pc = pc1;
    mq_mem_ref.u.profile.self_pc = pc2;
    
    clock_gettime(CLOCK_REALTIME, &mq_mem_ref.u.profile.time);

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_PROFILE_ADD;

    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
    
}

#ifdef __PIC__
#define CPLOAD ".cpload $25;\n\t"
#define CPRESTORE ".cprestore 44\n\t"
#else
#define CPLOAD
#define CPRESTORE
#endif

#define MCOUNT asm(\
        ".globl _mcount;\n\t" \
        ".align 2;\n\t" \
        ".type _mcount,@function;\n\t" \
        ".ent _mcount\n\t" \
        "_mcount:\n\t" \
        ".frame $sp,44,$31\n\t" \
        ".set noreorder;\n\t" \
        ".set noat;\n\t" \
        CPLOAD \
        "subu $sp,$sp,48;\n\t" \
        CPRESTORE \
        "sw $a0,24($sp);\n\t" \
        "sw $a1,28($sp);\n\t" \
        "sw $a2,32($sp);\n\t" \
        "sw $a3,36($sp);\n\t" \
        "sw $v0,40($sp);\n\t" \
        "sw $at,16($sp);\n\t" \
        "sw $ra,20($sp);\n\t" \
        "move $a1,$ra;\n\t" \
        "move $a0,$at;\n\t" \
        "jal __mcount;\n\t" \
        "nop;\n\t" \
        "lw $a0,24($sp);\n\t" \
        "lw $a1,28($sp);\n\t" \
        "lw $a2,32($sp);\n\t" \
        "lw $a3,36($sp);\n\t" \
        "lw $v0,40($sp);\n\t" \
        "lw $ra,20($sp);\n\t" \
        "lw $at,16($sp);\n\t" \
        "addu $sp,$sp,56;\n\t" \
        "j $ra;\n\t" \
        "move $ra,$at;\n\t" \
        ".set reorder;\n\t" \
        ".set at\n\t" \
        ".end _mcount");

MCOUNT

static inline void set_mem_tag(void *p_addr, unsigned int size)
{
    
    *(char *)p_addr = ACT_MEM_LEAK_MEM_TAG_B;
    *(int *)((char *)p_addr +1 ) = size;
    *((char *)p_addr+ 5) = ACT_MEM_LEAK_MEM_TAG_E;
    *((char *)p_addr+size + 6) = ACT_MEM_LEAK_MEM_TAG_E;
}

static inline int val_mem_tag(void *p_addr)
{
    int size = 0;
    
    if (*(char *)p_addr != ACT_MEM_LEAK_MEM_TAG_B){
        return -1;
    }
    if (*((char *)p_addr+ 5) != ACT_MEM_LEAK_MEM_TAG_E){
        return -1;
    }

    size = *(int *)((char *)p_addr +1 );

    if (*((char *)p_addr+size + 6) == ACT_MEM_LEAK_MEM_TAG_E){
        return -1;
    }

    return 0;
}


void  handle_stack(int *p_call_stack, unsigned int ra)
{
    
    int *cur_fp = NULL;
    int i = 0;

    asm("move %0,$fp":"=r"(cur_fp));

    memset(&p_call_stack[0], 0xffffffff, ACT_MEMLEAK_MAX_STACK*4);
    
    p_call_stack[0] = (unsigned int)__builtin_return_address(0);
    p_call_stack[1] = (unsigned int)((unsigned char*)cur_fp + 48 + 1264);
    p_call_stack[2] = (unsigned int)ra;

    i = (g_stack_max-(unsigned int)(cur_fp+12+316)-4);

    if (i > (ACT_MEMLEAK_MAX_STACK-3)*4){
        i = (ACT_MEMLEAK_MAX_STACK-3)*4;
    }

    memcpy(&p_call_stack[3], cur_fp + 12 + 316, i);
}

void *cmsMem_alloc(UINT32 size, UINT32 flag)
{
    void *p_ptr;
    act_memleak_mq_t mq_mem_ref;

    MALLOC(p_ptr, size, flag);
    
    maps((act_memleak_mq_t1 *)&mq_mem_ref);

    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();
    mq_mem_ref.p_mem = p_ptr;
    mq_mem_ref.mem_size = size;

    handle_stack(mq_mem_ref.stack_index, (unsigned int)__builtin_return_address(0)); 

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_ADD;

    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));

    return p_ptr;
}       

void *cmsMem_realloc(void *p_ptr, UINT32 size)
{
    act_memleak_mq_t mq_mem_ref;
    
    maps((act_memleak_mq_t1 *)&mq_mem_ref);
    
    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();
    mq_mem_ref.p_mem = p_ptr;

    handle_stack(mq_mem_ref.stack_index, (unsigned int) __builtin_return_address(0)); 

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_DEL;
   
    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
    
    REALLOC(p_ptr, size);
    
    mq_mem_ref.p_mem = p_ptr;
    mq_mem_ref.mem_size = size;
    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_ADD;

    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
    
    return p_ptr;
}

void cmsMem_free(void *p_ptr)
{
    act_memleak_mq_t mq_mem_ref;


    FREE(p_ptr);

    if (p_ptr == NULL){
        return;
    }
    
    maps((act_memleak_mq_t1 *)&mq_mem_ref);
    
    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.pid = getpid();
    mq_mem_ref.p_mem = p_ptr;

    handle_stack(mq_mem_ref.stack_index, (unsigned int)__builtin_return_address(0)); 

    mq_mem_ref.mq_opcode = ACT_MEMLEAK_MQ_OP_DEL;
    
    memleak_tcp_write((char *)&mq_mem_ref, sizeof(mq_mem_ref));
}


