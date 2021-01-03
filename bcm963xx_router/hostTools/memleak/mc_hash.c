/**************************************************************************
 *	��Ȩ:	
 *		 
 *
 *	�ļ���:	
 *		 mc_hash.c
 *
 *	����:	
 *		 ף���� 
 *
 *	��������:	
 *		 ��ϣɢ�б�ʵ��
 *			
 *              
 *      ��ʷ:
 *	         $Id: mc_hash.c,v 1.1 2011/04/02 02:43:23 xxia Exp $
 *		
 **************************************************************************/

#include "mc_hash_imp.h"
#include "mc_port.h"

static  inline mc_ulong_t  mc_partial_name_hash(mc_int_t c, mc_int_t prevhash)
{
	return (prevhash +(c << 4) + (c >> 4))*11;
	
}
static  inline mc_int_t  mc_full_name_hash(mc_char_t *name, mc_size_t len)
{
	mc_ulong_t hash = 0;
	while(len--){
		hash = mc_partial_name_hash(*name++, hash);
	}
	return hash%MC_HASH_MASK;
}

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
mc_hash_t * mc_hash_create()
{
	mc_int_t i;
	mc_hash_t *p_hash;

	
	//����ȫ��HASHɢ��������HASH�ṹ����;����MC_HASH_MASK+1����СΪlist_head���ڴ棬�ҳ�ʼ��Ϊ0
	MC_MALLOC_RV(p_hash, mc_hash_t, NULL);
	
	//��ʼ��HASHȫ�ֱ� 
	for(i = 0; i < MC_HASH_MASK; i++){
		MC_INIT_LIST_HEAD(&p_hash->list_head[i]);
	}
	mc_mutex_init(&p_hash->list_mutex);
	return p_hash;	    
}


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
mc_rv mc_hash_add(IN mc_char_t *p_key, 
		  IN mc_void_t *p_value,
		  IN mc_hash_t *p_hash)
{
	int hash;
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	
	
	VASSERT(p_key != NULL);
	VASSERT(p_value != NULL);
	VASSERT(p_hash != NULL);

	mc_mutex_lock(&p_hash->list_mutex);

	hash = mc_full_name_hash(p_key, strlen(p_key));
	head_hash = p_hash->list_head + hash;
	
	/*�жϱ�ͷ�Ƿ�Ϊ��*/
	if (mc_is_list_empty(head_hash)){
		/*��������뵽hashȫ��������ȥ*/
		goto new;
	}
			
	for (pos = head_hash->prev; pos != head_hash; pos = pos->prev){
		p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);
		if (strncmp(p_key, p_node->key,strlen(p_key)) == 0){
			/*�����Ѿ����ҳɹ�,�޸Ĳ���ֵȻ�󷵻�*/
			p_node->value = p_value;
			mc_mutex_unlock(&p_hash->list_mutex);
			return MC_RV_SUC;
		}
	}
	
 new:
	//û���ҵ����½�һ���ڵ㲢����
	MC_MALLOC_RV(p_node, mc_hash_node_t, MC_RV_FAIL_MEM);
	
	//strlen�������ַ���������,��snprintf�����\0
	sprintf(p_node->key, "%s", p_key);
	p_node->value = p_value;

	/*��������뵽hashȫ��������ȥ*/
	mc_list_add(&p_node->link_hash, head_hash);
	
	mc_mutex_unlock(&p_hash->list_mutex);
	
	return MC_RV_SUC;
}



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
mc_void_t *mc_hash_find(IN char *p_key, 
			IN mc_hash_t *p_hash)
{
	mc_int_t hash;
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
		
	
	VASSERT_RV(p_key != NULL, NULL);
	VASSERT_RV(p_hash != NULL, NULL);
	
	mc_mutex_lock(&p_hash->list_mutex);
	
	hash = mc_full_name_hash(p_key, strlen(p_key));
	head_hash = p_hash->list_head + hash;
	
	/*�жϱ�ͷ�Ƿ�Ϊ��*/
	if (mc_is_list_empty(head_hash)){
		mc_mutex_unlock(&p_hash->list_mutex);
		return NULL;	
	}
			
	for (pos = head_hash->prev; pos != head_hash; pos = pos->prev){
		p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);
		if (!strncmp(p_key, p_node->key,strlen(p_key))){
			/*�����Ѿ����ҳɹ�*/
			mc_mutex_unlock(&p_hash->list_mutex);
			return p_node->value;
		}
	}

	mc_mutex_unlock(&p_hash->list_mutex);

	return NULL;    
}



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
mc_rv mc_hash_del(IN mc_char_t *p_key, 
		  IN mc_hash_t *p_hash)
{
	mc_int_t hash;
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	

	VASSERT(p_key != NULL);
	VASSERT(p_hash != NULL);

	mc_mutex_lock(&p_hash->list_mutex);

	hash = mc_full_name_hash(p_key, strlen(p_key));
	head_hash = p_hash->list_head + hash;
	
	/*�жϱ�ͷ�Ƿ�Ϊ��*/
	if (mc_is_list_empty(head_hash)){
		mc_mutex_unlock(&p_hash->list_mutex);
		return MC_RV_ERR;	
	}
			
	for (pos = head_hash->prev; pos != head_hash; )	{
		p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);
		pos = pos->prev;
		if (!strncmp(p_key, p_node->key,strlen(p_key))){
			/*�ҵ��ýڵ㣬ɾ��֮*/
          		mc_list_del(&(p_node->link_hash));
			MC_FREE(p_node);		
       			mc_mutex_unlock(&p_hash->list_mutex);
			return MC_RV_SUC;
		}
	}

	mc_mutex_unlock(&p_hash->list_mutex);
	return MC_RV_ERR;       
}





/***********************************************************
 *	[������]:
 *		mc_hash_destroy
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
mc_void_t mc_hash_destroy(IN mc_hash_t *p_hash)
{
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	mc_int_t i;
	

	VASSERT_VRV(p_hash != NULL);
	mc_mutex_lock(&p_hash->list_mutex);

	/*ѭ���ͷſռ�*/
	for(i = 0; i<MC_HASH_MASK; i++){   
		head_hash = p_hash->list_head + i;
		if (mc_is_list_empty(head_hash)){
			continue;
		}	    
		for (pos = head_hash->prev; pos != head_hash;){
			p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);     
			pos = pos->prev;
			mc_list_del(&(p_node->link_hash));
			MC_FREE(p_node);	
		}
	}
	mc_mutex_unlock(&p_hash->list_mutex);
	
 	MC_FREE(p_hash);
}


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
mc_void_t mc_hash_value_destroy(IN mc_hash_t *p_hash)
{
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	mc_int_t i;
	

	VASSERT_VRV(p_hash != NULL);
	mc_mutex_lock(&p_hash->list_mutex);

	/*ѭ���ͷſռ�*/
	for(i = 0; i<MC_HASH_MASK; i++){   
		head_hash = p_hash->list_head + i;
		if (mc_is_list_empty(head_hash)){
			continue;
		}	    
		for (pos = head_hash->prev; pos != head_hash;){
			p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);     
			pos = pos->prev;
			mc_list_del(&(p_node->link_hash));
			MC_FREE(p_node->value);
			MC_FREE(p_node);	
		}
	}
	mc_mutex_unlock(&p_hash->list_mutex);
	
 	MC_FREE(p_hash);
}

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
mc_void_t mc_hash_value_func_destroy(IN mc_hash_t *p_hash, void (*callback)(void **argv))
{
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	mc_int_t i;
	

	VASSERT_VRV(p_hash != NULL);
	mc_mutex_lock(&p_hash->list_mutex);

	/*ѭ���ͷſռ�*/
	for(i = 0; i<MC_HASH_MASK; i++){   
		head_hash = p_hash->list_head + i;
		if (mc_is_list_empty(head_hash)){
			continue;
		}	    
		for (pos = head_hash->prev; pos != head_hash;){
			p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);     
			pos = pos->prev;
			mc_list_del(&(p_node->link_hash));
                        
                        if (callback != NULL){
                            callback(p_node->value);
                        }
                        
			MC_FREE(p_node->value);
			MC_FREE(p_node);	
		}
	}
	mc_mutex_unlock(&p_hash->list_mutex);
	
 	MC_FREE(p_hash);
}



mc_void_t mc_hash_func_display(IN mc_hash_t *p_hash, void (*callback)(void *argv))
{
	mc_list_head_t *head_hash, *pos;
	mc_hash_node_t *p_node; 
	mc_int_t i;
	

	VASSERT_VRV(p_hash != NULL);
	mc_mutex_lock(&p_hash->list_mutex);

	/*ѭ���ռ�*/
	for(i = 0; i<MC_HASH_MASK; i++){   
		head_hash = p_hash->list_head + i;
		if (mc_is_list_empty(head_hash)){
			continue;
		}	    
		for (pos = head_hash->prev; pos != head_hash;){
			p_node = mc_list_entry(pos, mc_hash_node_t, link_hash);     
			pos = pos->prev;
                        if (callback != NULL){
                            callback(p_node->value);	
                        }
		}
	}
	mc_mutex_unlock(&p_hash->list_mutex);
	
        
}
