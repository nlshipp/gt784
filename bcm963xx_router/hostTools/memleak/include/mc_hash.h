/**************************************************************************
 *	��Ȩ:	
 *		 
 *
 *	�ļ���:	
 *		 mc_hash.h
 *
 *	����:	
 *		 ף���� 
 *
 *	��������:	
 *		 ��ϣɢ�б�ӿں���
 *			
 *              
 *      ��ʷ:
 *	         $Id: mc_hash.h,v 1.1 2011/04/02 02:42:28 xxia Exp $
 *		
 **************************************************************************/
 
#ifndef MC_HASH_H
#define MC_HASH_H

#include "mc_port.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_hash_t mc_hash_t;

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

mc_void_t mc_hash_value_func_destroy(IN mc_hash_t *p_hash, 
                                     IN void (*callback)(void **argv));
    
mc_void_t mc_hash_func_display(IN mc_hash_t *p_hash, 
                               IN void (*callback)(void *argv));

#ifdef __cplusplus
}
#endif

#endif
