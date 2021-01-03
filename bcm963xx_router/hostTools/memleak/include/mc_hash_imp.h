/**************************************************************************
 *	��Ȩ:	
 *		 
 *
 *	�ļ���:	
 *		 mc_hash_imp.h
 *
 *	����:	
 *		 ף���� 
 *
 *	��������:	
 *		 ��ϣɢ�б��ڲ�ʵ��ͷ�ļ�
 *			
 *              
 *      ��ʷ:
 *	         $Id: mc_hash_imp.h,v 1.1 2011/04/02 02:42:28 xxia Exp $
 *		
 **************************************************************************/
 
#ifndef MC_HASH_IMP_H
#define MC_HASH_IMP_H

#include "mc_port.h"
#include "mc_list.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MC_HASH_KEY   256
#define MC_HASH_MASK  1024     //hashģ
#define MAX_HASH_KEY  128


/*����Hash��ṹ*/
typedef struct {
	mc_list_head_t	 list_head[MC_HASH_MASK];
	mc_mutex_t       list_mutex;
}mc_hash_t;

typedef struct mclist_struct{
	mc_char_t        key[MC_HASH_KEY];   //�ؼ���
	mc_void_t        *value;             //��Ӧ��ֵ
	mc_list_head_t   link_hash;	     //HASH����
}mc_hash_node_t;


/***********************************************************
 *	[������]:
 *		mc_hash_create
 *
 *	[��������]:
 *		��ʼ��Hashӳ���
 *
 *	[��������]: 
 *              ��
 *
 *	[��������ֵ]
 *		����hash��ľ��
 *
 ************************************************************/
mc_hash_t * mc_hash_create();


/***********************************************************
 *	[������]:
 *		mc_hash_add
 *
 *	[��������]:
 *		��Hash��������һ���ֵ��
 *
 *	[��������]
 *		IN mc_char_t *key        ��
 *		IN mc_void_t *value 	 ֵ
 *		IN mc_hash_t *p_hash     hash��ľ��

 *
 *	[��������ֵ]
 *		MC_RV_SUC           	 �ɹ�
 *		MC_RV_ERR                ʧ��
 *
 ************************************************************/
mc_rv mc_hash_add(IN mc_char_t *key, 
		  IN mc_void_t *value, 
		  IN mc_hash_t *p_hash);


/***********************************************************
 *	[������]:
 *		mc_hash_find
 *
 *	[��������]:
 *		��Hash���в��Ҽ�����Ӧ��ֵ
 *
 *	[��������]
 *		IN mc_char_t *key        ��
 *		IN mc_hash_t *p_hash     Hash����
 *
 *	[��������ֵ]
 *		���ؼ�����Ӧ��ֵ
 *
 ************************************************************/
mc_void_t *mc_hash_find(IN mc_char_t *key, 
			IN mc_hash_t *p_hash);


/***********************************************************
 *	[������]:
 *		mc_hash_del
 *
 *	[��������]:
 *		��Hash����ɾ��һ���ֵ��
 *
 *	[��������]
 *		IN char *key_buffer      ��
 *		IN mc_hash_t *p_hash     Hash����
 *
 *	[��������ֵ]
 *		MC_RV_SUC           	 �ɹ�
 *		MC_RV_ERR                ʧ��
 *
 ************************************************************/
mc_rv mc_hash_del(IN mc_char_t *key, 
		  IN mc_hash_t *p_hash);


/***********************************************************
 *	[������]:
 *		mc_hash_destroy
 *
 *	[��������]:
 *		�ͷ�Hash��
 *
 *	[��������]
 *		IN mc_hash_t *p_hash  Hash��ľ��
 *
 *	[��������ֵ]:
 *              ��
 *
 ************************************************************/
mc_void_t mc_hash_destroy(IN mc_hash_t *p_hash);


/***********************************************************
 *	[������]:
 *		mc_hash_value_destroy
 *
 *	[��������]:
 *		�ͷ�Hash��,��vlaueֵ
 *
 *	[��������]
 *		IN mc_hash_t *p_hash  Hash��ľ��
 *
 *	[��������ֵ]: 
 *              ��
 *
 ************************************************************/
mc_void_t mc_hash_value_destroy(IN mc_hash_t *p_hash);

#ifdef __cplusplus
}
#endif

#endif
