/*
linux �ں�ʹ�õ�����ṹ�������൱���

*/

#ifndef MC_LIST_H
#define MC_LIST_H

typedef struct mc_list_head{
		struct mc_list_head *prev;	//����ͷָ��
		struct mc_list_head *next; //����βָ��
}mc_list_head_t;


/*����ͷ*/	
static inline void MC_INIT_LIST_HEAD(mc_list_head_t *ptr)
{
	ptr->prev = ptr;
	ptr->next = ptr;
}
/*��������Ԫ��*/

static inline void mc_list_add(struct mc_list_head *new, struct mc_list_head *head)
{
	(head->next)->prev = new;
	new->next = (head->next);
	new->prev = head;
	head->next = new;
}

static inline void mc_list_del(struct mc_list_head *entry)
{
	(entry->next)->prev = entry->prev;
	(entry->prev)->next = entry->next;

	entry->next = entry->prev = entry;
}

static inline int mc_is_list_empty(struct mc_list_head *head)
{
	return head->next == head;
	
}

/*
��list ������뵽head������ȥ���������������

*/
static inline void mc_list_combine(struct mc_list_head *list,  struct mc_list_head *head )
{
	struct mc_list_head *first = list->next;
	
	if (first != list){
		struct mc_list_head *last = list->prev;
		struct mc_list_head *at = head->next;
		
		first->prev = head;
		head->next = first;
		
		last->next = at;
		at->prev = last;
	}
}


/*
����ľ������ڣ���������ɲ�ͬ�Ľṹ����ɣ���������������ԣ��乹˼�൱�����˵��һ�£�һ���ṹ��mc_list_head��ָ���ȥ��mc_list_head�ṹ����ṹ�е�ƫ��λ�����ɵ�֪��ṹ��ָ�롣
*/

#define mc_list_entry(ptr, type, member) ((type *)((unsigned char *)(ptr)-(unsigned char *)(&((type *)0)->member)))




 
#endif

