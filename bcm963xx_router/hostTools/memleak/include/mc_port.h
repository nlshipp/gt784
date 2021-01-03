/**************************************************************************
 *	��Ȩ:	
 *		ף����
 *
 *	�ļ���:	
 *		wh_port.h
 *
 *	����:	
 *		ף���� 
 *
 *	��������:	
 *		��������ͷ�ļ� 
 *		������������������
 *
 *	�����б�:	
 *		��
 *              
 *      ��ʷ:
 *		$Id: mc_port.h,v 1.1 2011/04/02 02:42:28 xxia Exp $
 *		
 **************************************************************************/

/**************************************************************************
 *
 *  ��������
 *		wh_uint_t       �޷�������
 *		wh_int_t        ����
 *		wh_byte_t       �޷����ַ���
 *		wh_char_t;      �ַ���
 *		wh_void_t;      void ��
 *        
 *
 *  ��������
 *		g��     	��ʾȫ�ֱ���                   
 *		p:		��ʾָ��                       
 *		pp:		��ʾ˫��ָ��                   
 *
 *		ch:		��ʾchar��	      ����ch_msg
 *		by:		��ʾunsigned char��   ����by_msg 
 *		i:		��ʾint��             ����i_size
 *		ui:		��ʾunsigned int��    ����ui_size
 *
 *		b:		��ʾbool��            ����b_cleanup;
 *		str:		��ʾ�ַ�����          ����str_msg
 *		buf		��ʾ�ڴ���            ����msg_buf;
 **************************************************************************
 *  ���ݽṹ����
 *		ty:		��ʾtypedef��         ����typedef struct{
 *	    						}sip_info_ty;
 *
 *
 *  ���ݽṹ����
 *	    	st:		��ʾstruct��          ����sip_info_ty st_sip_info
 * 	    	e:		��ʾenum              ����e_true;
 **************************************************************************
 *  ����ֵ����
 *
 *	    	mc_rv_suc		
 *	    	mc_rv_err             
 *	    	mc_rv_err_param	
 *	    	mc_rv_fail	
 *	    	mc_rv_fail_mem
 *              mc_rv_fail_fn              
 **************************************************************************
 *  ��������
 *	      	fn:	      	��ʾ�ڲ�����           ����sip_fn_route()
 *       
 **************************************************************************/

#ifndef MC_PORT_H
#define MC_PORT_H


#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <sys/timeb.h>
#include <time.h>
#include <direct.h>
#include <Windows.h>
#else //LINUX
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>



/***********************************************************
 *
 *                     ͨ�����ݽṹ���Ͷ���
 *
 ************************************************************/
#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif


#ifdef _WIN32
#define inline
#define INLINE
#define __FUNCTION__ NULL
#endif

#ifdef _SOLARIS_CC
#define __FUNCTION__ NULL
#endif

#ifdef _DEBUG
#define DEBUG(act) act
#else
#define DEBUG(act)
#endif


#define NAME_SIZE               64
#define MAX_NAME_SIZE		128
#define MIN_BUF_SIZE            128
#define BUF_SIZE                1024
#define MAX_BUF_SIZE		4096
#define MAX_UINT_SIZE		4294967296
#define MAX_INT_SIZE		2147483648
#define MAX_USHORT_SIZE		65536
#define MAX_SHORT_SIZE		32768


typedef unsigned long           mc_u32_t;
typedef unsigned short          mc_u16_t;
typedef unsigned char           mc_u8_t;
typedef signed   long           mc_32_t;
typedef signed   short          mc_16_t;
typedef signed   char           mc_8_t;

typedef unsigned int            mc_uint_t;
typedef signed   int            mc_int_t;
typedef unsigned char           mc_byte_t;
typedef unsigned long           mc_ulong_t;
typedef signed   long           mc_long_t;

typedef char                    mc_char_t;
typedef unsigned char           mc_uchar_t;

typedef void      		mc_void_t;
typedef void		        *mc_pvoid_t;

typedef size_t		        mc_size_t;


#define MC_B_FALSE 0
#define MC_B_TRUE  1  
typedef int mc_bool;

typedef enum{
	mc_b_true		= 1,
	mc_b_false		= 0
} mc_bool_t;


#define MC_RV_SUC               0
#define MC_RV_ERR               -10
#define MC_RV_ERR_PARM          -11
#define MC_RV_FAIL              -20
#define MC_RV_FAIL_MEM          -21   
#define MC_RV_FAIL_FUNC         -22
typedef int mc_rv;


typedef enum{
	mc_rv_suc               =  0,
	mc_rv_err               = -10,
	mc_rv_err_parm          = -11,
	mc_rv_fail 	        = -20,
	mc_rv_fail_mem          = -21,
	mc_rv_fail_func         = -22      
} mc_rv_t;



/***********************************************************
 *
 *                     ��չӦ�ú�
 *
 ************************************************************/

#ifdef _DEBUG
#define printf(format, arg...) 
#endif


/* mc_rv ����ֵ����*/
#define VASSERT_RV(cond, rv)                                     \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
		return rv;                                       \
	}                                                        \
}

#define VASSERT_RV_ACT(cond, rv, action)                         \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
	        {action;}                                        \
		return rv;                                       \
	}                                                        \
}

/* void ����ֵ����*/
#define VASSERT_VRV(cond)                                        \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
	       return ;                                          \
	}                                                        \
}

#define VASSERT_VRV_ACT(cond, action)                            \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
	        {action;}                                        \
		return ;                                         \
	}                                                        \
}

/* ����ֵ������*/
#define VASSERT_NRV(cond)                                        \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s,line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
	}                                                        \
}

#define VASSERT_NRV_ACT(cond, action)                            \
{                                                                \
	if (!(cond)){                                            \
	       printf("ASSERT:(%s),%s %s,line %d\n",             \
		       #cond, __FUNCTION__, __FILE__, __LINE__); \
	        {action;}                                        \
	}                                                        \
}


/*Ĭ��VASSERTͨ�ú�*/
#define	VASSERT(cond)                    VASSERT_RV(cond, MC_RV_ERR_PARM)
#define VASSERT_ACT(cond, action)        VASSERT_RV_ACT(cond, MC_RV_ERR_PARM, action)

/*����ASSERT��*/
#define FASSERT(cond)                    VASSERT_RV((cond == MC_RV_SUC), MC_RV_FAIL_FUNC)
#define FASSERT_RV(cond, rv)             VASSERT_RV((cond == MC_RV_SUC), rv)
#define FASSERT_ACT(cond, act)           VASSERT_RV_ACT((cond == MC_RV_SUC), MC_RV_FAIL_FUNC, act)
#define FASSERT_RV_ACT(cond, rv, act)    VASSERT_RV_ACT((cond == MC_RV_SUC), rv, act)
#define FASSERT_NRV(cond)                VASSERT_NRV((cond == MC_RV_SUC))
#define FASSERT_NRV_ACT(cond, act)       VASSERT_NRV_ACT((cond == MC_RV_SUC), act)
#define FASSERT_VRV(cond)                VASSERT_VRV((cond == MC_RV_SUC))
#define FASSERT_VRV_ACT(cond, act)       VASSERT_VRV_ACT((cond == MC_RV_SUC), act)

/*��������ASSERT��*/
#define FVASSERT(cond)                   VASSERT_RV(cond, MC_RV_FAIL_FUNC)
#define FVASSERT_RV(cond, rv)            VASSERT_RV(cond, rv)
#define FVASSERT_ACT(cond, act)          VASSERT_RV_ACT(cond, MC_RV_FAIL_FUNC, act)
#define FVASSERT_RV_ACT(cond, rv, act)   VASSERT_RV_ACT(cond, rv, act)
#define FVASSERT_NRV(cond)               VASSERT_NRV(cond)
#define FVASSERT_NRV_ACT(cond, act)      VASSERT_NRV_ACT(cond, act)
#define FVASSERT_VRV(cond)               VASSERT_VRV(cond)
#define FVASSERT_VRV_ACT(cond, act)      VASSERT_VRV_ACT(cond, act)




#define MALLOC(size)            malloc(size) 
#define CALLOC(size, size_t)    calloc(size * size_t)
#define FREE(ptr)               free(ptr)



#define MC_SMALLOC_RV(ptr, type, size, rv)                                      \
{                                                                               \
	FVASSERT_RV((ptr = (type *)MALLOC(size)) != NULL, rv);                  \
        memset(ptr, 0, size);                                                   \
	VASSERT_RV(ptr != NULL, rv);                                            \
}

#define MC_SMALLOC_RV_ACT(ptr, type, size, rv, act)                             \
{                                                                               \
	FVASSERT_RV_ACT((ptr = (type *)MALLOC(size)) != NULL, rv, act);         \
        memset(ptr, 0, size);                                                   \
	VASSERT_RV_ACT(ptr != NULL, rv, act);                                   \
}
#define MC_SMALLOC(ptr, type, size)          MC_SMALLOC_RV(ptr, type, size, MC_RV_FAIL_MEM)
                 

#define MC_MALLOC_RV(ptr, type, rv)                                             \
{                                                                               \
	FVASSERT_RV((ptr = (type *)MALLOC(sizeof(type))) != NULL, rv);          \
        memset(ptr, 0, sizeof(type));                                           \
	VASSERT_RV(ptr != NULL, rv);                                            \
}                                          
#define MC_MALLOC_RV_ACT(ptr, type, rv, act)                                    \
{                                                                               \
	FVASSERT_RV_ACT((ptr = (type *)MALLOC(sizeof(type))) != NULL, rv, act); \
        memset(ptr, 0, sizeof(type));                                           \
	VASSERT_RV_ACT(ptr != NULL, rv, act);                                   \
}
#define MC_MALLOC_ACT(ptr, type, act)        MC_MALLOC_RV_ACT(ptr, type, MC_RV_FAIL_MEM, act)                    
#define MC_MALLOC(ptr, type)                 MC_MALLOC_RV(ptr, type, MC_RV_FAIL_MEM)                    



#define MC_FREE(ptr) FREE(ptr)

typedef pthread_mutex_t mc_mutex_t;
typedef pthread_cond_t	mc_cond_t;
typedef pthread_attr_t	mc_attr_t;
typedef	pthread_t	mc_thread_t;

#define MC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER;

#ifdef _WIN32
typedef HANDLE mc_sem_t;
#else

typedef sem_t mc_sem_t;
#endif

static inline mc_rv mc_mutex_init(mc_mutex_t *p_mutex)
{
	pthread_mutex_init(p_mutex, NULL);
	return mc_rv_suc;
}


static inline mc_rv mc_mutex_cleanup(mc_mutex_t *p_mutex)
{
	if (pthread_mutex_destroy(p_mutex))
		return mc_rv_fail;
	return mc_rv_suc;
}

static  inline mc_rv mc_mutex_lock(mc_mutex_t *p_mutex)
{
	if (pthread_mutex_lock(p_mutex))
		return mc_rv_fail;
	return mc_rv_suc;
}

static  inline mc_rv mc_mutex_trylock(mc_mutex_t *p_mutex)
{
	if (pthread_mutex_trylock(p_mutex))
		return mc_rv_fail;
	return mc_rv_suc;
}

static  inline mc_rv mc_mutex_unlock(mc_mutex_t *p_mutex)
{
	if (pthread_mutex_unlock(p_mutex))
		return mc_rv_fail;
	return mc_rv_suc;
}

#ifdef __cplusplus
}
#endif


#endif


