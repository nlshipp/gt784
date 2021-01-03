#include "act_common.h"
#include "act_mem_leak.h"
#include "act_profile.h"
#include "mc_socket.h"

void act_memleak_send_mq_local_disp(act_memleak_avl_node_t* root, act_int_t op);
unsigned int int_big2little(unsigned int val);


act_memleak_avl_node_t *gp_preload_mem_ref_root = NULL;

extern act_char_t g_file_table[512][256];
extern act_memleak_avl_node_t *gp_func_ref_root;
extern act_memleak_avl_node_t *gp_line_ref_root;

int gs_socket = 0;

extern int g_bind_pid;
extern int g_bind_level;

extern    int big2little;

static int err_dbfree_count = 1;
static int err_unfree_count = 1;
static int err_ovflow_count = 1;




act_memleak_pid_info_t g_pid_table[256];
act_int_t g_pid_max = 0;


act_rv_t act_memleak_add_pid(act_int_t pid, act_char_t *p_name)
{
    act_int_t i = g_pid_max++;
    
    if (g_pid_max >= 256){
        g_pid_max = 0;
    }

    ACT_VASSERT(pid != 0 && p_name != NULL);

    g_pid_table[i].pid = pid;
    sprintf(g_pid_table[i].name, "%s", p_name);
    return ACT_RV_SUC;
}

act_char_t *act_memleak_find_pid(act_int_t pid)
{
    act_int_t i;
    
    for (i = 0; i < g_pid_max; i++){
        if (g_pid_table[i].pid == pid){
            return g_pid_table[i].name;
        }
    }
    
    return NULL;
}


act_void_t act_memleak_dispaly_all_pid()
{
    act_int_t i;
    
    for (i = 0; i < g_pid_max; i++){
        act_printf("pid:%d app_name:%s\n", 
                   g_pid_table[i].pid, g_pid_table[i].name);
    }
}



/**********************************************************
 * 
 *                           ���ܺ���                 
 *
 *
 **********************************************************/

int g_quit = 0;
sem_t mutex;

/**************************************************************************
 *	[������]:
 *	        sbs_func_status_bind
 *              
 *	[��������]:
 *	        TCP/IP��������
 *              
 *	[��������]:
 *  		mc_int_t port        �˿ں�
 *              mc_int_t thread_numb �������߳���
 *              
 *	[��������ֵ]:
 *              MC_RV_SUC �ɹ�
 *              MC_RV_ERR ʧ��
 *              
 *      [��������]:
 *              1.�������������߳� select()
 *              2.����TCP/IP����  bind()
 *               
 **************************************************************************/
mc_int_t m_socket_id = -1;

mc_rv sbs_func_status_bind(mc_int_t port)
{
	
	mc_int_t 		c_socket_id = -1;
        struct sockaddr_in  	c_sin ;
	mc_int_t		c_sin_len   = 0;
	pthread_t 		tid;
	act_int_t *argv;
        fd_set set;
        pthread_attr_t attr;
        pthread_t tid_t[256] = {0};
        mc_int_t  tid_s[256] = {0};
        act_int_t tid_i = 0;
        act_int_t i = 0;

        sem_init(&mutex, 0, 1);

	FVASSERT_ACT((m_socket_id = mc_socket_tcp_bind(port)) > 0, act_printf("please set ipaddress as \"192.168.0.2\"\n");exit(-1));
        
	while(!g_quit){
            FD_ZERO(&set);
            FD_SET(m_socket_id, &set);
            select(m_socket_id+1, &set, NULL, NULL, NULL); 
            
            if (FD_ISSET(m_socket_id, &set)){
        //        act_printf("accepit....%d\n", m_socket_id);
                c_socket_id = accept(m_socket_id, (struct sockaddr *)&c_sin, (socklen_t *)&c_sin_len);
                
                if (c_socket_id < 0){
                    continue;
                }else if (g_quit != 0){
                    break;
                }
                
                
                argv = (int *)malloc(sizeof(int));
                *argv = c_socket_id;
         //       act_printf("thread....\n");
                pthread_attr_init(&attr);
                pthread_create(&tid, &attr, act_memleak_recv_mq_thread,argv );
                tid_t[tid_i] = tid;
                tid_s[tid_i++] = c_socket_id;

                if (tid_i >= 256){
                    tid_i = 0;
                }
            }
        }
        
     //   act_printf("begin main bind quit\n");
        
        for(i = 0; i < tid_i; i++){
            shutdown(tid_s[i], SHUT_RDWR);
            pthread_join(tid_t[i], NULL);
        }
        
     //   act_printf("main bind quit\n");
	
        return MC_RV_SUC;
}


act_rv_t act_memleak_mq_maps(act_memleak_mq_t1 *p_mq_ref1, act_char_t *p_name)
{
    act_int_t i;
    act_memleak_avl_node_t **pp_func_root;
    act_memleak_avl_node_t **pp_file_line_root;
    act_memleak_ref_root_t *p_ref_root;

    if (big2little){
        p_mq_ref1->pid = int_big2little(p_mq_ref1->pid);
    }

    for(i = 0; i < MAPS_MAX; i++){
        p_mq_ref1->u.maps[i].add1 = int_big2little(p_mq_ref1->u.maps[i].add1);
        p_mq_ref1->u.maps[i].add2 = int_big2little(p_mq_ref1->u.maps[i].add2);
        p_mq_ref1->u.maps[i].add3 = int_big2little(p_mq_ref1->u.maps[i].add3);
        p_mq_ref1->u.maps[i].add4 = int_big2little(p_mq_ref1->u.maps[i].add4);
        
        printf("add1=0x%x add2=0x%x add3=0x%x add4=0x%x name:%s\n",
               p_mq_ref1->u.maps[i].add1, 
               p_mq_ref1->u.maps[i].add2,
               p_mq_ref1->u.maps[i].add3,
               p_mq_ref1->u.maps[i].add4,
               p_mq_ref1->u.maps[i].name);
    }        
    
    p_ref_root = act_memleak_ref_root_find(p_mq_ref1->pid);
    pp_func_root = &p_ref_root->p_func_ref_root;
    pp_file_line_root = &p_ref_root->p_file_line_ref_root;
    
    
    if (p_mq_ref1->u.maps[MAPS_MAIN].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, p_mq_ref1->u.maps[MAPS_MAIN].name, 0, 0, 0);
        sprintf(p_name, "%s", p_mq_ref1->u.maps[MAPS_MAIN].name);
    }


    if (p_mq_ref1->u.maps[MAPS_LIB_CMS_CORE].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcms_core.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_CORE].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_CORE].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_CORE].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_CMS_UTIL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcms_util.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_UTIL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_UTIL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_UTIL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_CMS_DAL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcms_dal.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_DAL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_DAL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_DAL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_CMS_MSG].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcms_msg.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_MSG].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_MSG].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_MSG].add3);
    }
    
    if (p_mq_ref1->u.maps[MAPS_LIB_NANO_XML].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libnanoxml.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_NANO_XML].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_NANO_XML].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_NANO_XML].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_XDSL_CTL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libxdslctl.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_XDSL_CTL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_XDSL_CTL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_XDSL_CTL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_ATM_CTL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libatmctl.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_ATM_CTL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_ATM_CTL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_ATM_CTL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_CMS_BOARD_CTL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcms_boardctl.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_BOARD_CTL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_BOARD_CTL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CMS_BOARD_CTL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_WL_MNGR].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libwlmngr.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_MNGR].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_MNGR].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_MNGR].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_WL_CTL].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libwlctl.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_CTL].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_CTL].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_WL_CTL].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_NVRAM].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libnvram.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_NVRAM].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_NVRAM].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_NVRAM].add3);
    }

    if (p_mq_ref1->u.maps[MAPS_LIB_CGI].add1 != 0){
        parse_elf(pp_func_root, pp_file_line_root, 
                  "./libcgi.so", 
                  p_mq_ref1->u.maps[MAPS_LIB_CGI].add1, 
                  p_mq_ref1->u.maps[MAPS_LIB_CGI].add2, 
                  p_mq_ref1->u.maps[MAPS_LIB_CGI].add3);
    }

    act_printf("END MQPS pid:%d name:%s\n", p_mq_ref1->pid, p_mq_ref1->u.maps[MAPS_MAIN].name);

    act_memleak_add_pid(p_mq_ref1->pid, p_mq_ref1->u.maps[MAPS_MAIN].name);

    return ACT_RV_SUC;
}



void *act_memleak_recv_mq_thread(void *argv)
{
    act_int_t i = 0;
    act_memleak_mq_t mq_mem_ref;
    act_memleak_mq_t1 *p_mem_ref1;
    int mq_ret = -1;
    int m_sock =  *((int *)argv);
    act_memleak_avl_node_t **pp_mem_root;
    act_memleak_ref_root_t *p_ref_root;
    act_char_t programe[32] = "\0";

    free(argv);
    
    while(1){
        memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
        mq_ret =  mc_socket_tcp_read(m_sock, 
                                     (void *)&mq_mem_ref,
                                     sizeof(mq_mem_ref), 
                                     1);
        
 //       act_printf("RECV_MEM_RQ_READY %d %d %d\n", mq_ret, sizeof(mq_mem_ref),mq_mem_ref.mq_opcode);
        if (mq_ret < 0 ) {
            act_printf("read error thread_quit %d %d %s\n", pthread_self(),m_sock, programe);
            return NULL;
        }
        
        if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_MAPS){
            sem_wait(&mutex);
            act_memleak_mq_maps((act_memleak_mq_t1 *)&mq_mem_ref, programe);
            sem_post(&mutex);
            continue;
        }
        
        if (big2little){
            mq_mem_ref.pid = int_big2little(mq_mem_ref.pid);
            mq_mem_ref.p_mem = (act_uint_t *)int_big2little((act_uint_t)mq_mem_ref.p_mem);
            mq_mem_ref.mem_size = int_big2little(mq_mem_ref.mem_size);
            mq_mem_ref.time = int_big2little(mq_mem_ref.time);
            
            for (i = 0; i < ACT_MEMLEAK_MAX_STACK; i++){
                mq_mem_ref.stack_index[i] = int_big2little(mq_mem_ref.stack_index[i]);
            }
        }
       
        if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_QUIT){
            g_quit = 1;
            shutdown(m_socket_id, SHUT_RDWR);
    
            return NULL;
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_CLR){
     //       act_printf("MQ_CLR\n");
            
            sem_wait(&mutex);
            act_memleak_mem_ref_cleanup(&gp_preload_mem_ref_root);
            act_memleak_ref_root_cleanup();
            act_memleak_mem_statics_ref_cleanup();

            act_memleak_profile_func_cleanup();
            g_pid_max = 0;
            
            sem_post(&mutex);
            
      //      act_printf("CATCH CLR\n\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_SEGV){
     //       act_printf("MQ_SEGV\n");
            //           act_mem_show_func_line();
     //       act_printf("CATCH SEGV\n\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_ADD ){
     //       act_printf("MQ_OP_ADD %d 0x%x time:%d\n", mq_mem_ref.pid, mq_mem_ref.p_mem, mq_mem_ref.time);
            
            sem_wait(&mutex);
            
            if ((act_uint_t)mq_mem_ref.p_mem > 0x50000000UL){
                pp_mem_root = &gp_preload_mem_ref_root;
            }else {
                p_ref_root = act_memleak_ref_root_find(mq_mem_ref.pid);
                pp_mem_root = &p_ref_root->p_mem_ref_root;
            }
            
            act_memleak_mem_ref_add(pp_mem_root, 
                                    mq_mem_ref.p_mem,
                                    mq_mem_ref.mem_size,
                                        mq_mem_ref.stack_index,
                                    mq_mem_ref.pid);
            
            sem_post(&mutex);
            
     //       act_printf("END_MQ_OP_ADD\n\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_OVL ){
     //       act_printf("MQ_OP_OVL\n");
            
            sem_wait(&mutex);
            if ((act_uint_t)mq_mem_ref.p_mem >0x50000000UL){
                pp_mem_root = &gp_preload_mem_ref_root;
            }else {
                p_ref_root = act_memleak_ref_root_find(mq_mem_ref.pid);
                pp_mem_root = &p_ref_root->p_mem_ref_root;
            }

            act_memleak_mem_ref_mod(pp_mem_root, 
                                    (act_uint_t)mq_mem_ref.p_mem,
                                    ACT_MEMLEAK_MEM_ST_OFLOW);
            sem_post(&mutex);
            
      //      act_printf("END_MQ_OP_OVL\n\n");
        }else if (mq_mem_ref.p_mem != NULL && mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_DEL ){
      //      act_printf("MQ_OP_DEL %d 0x%x time:%d\n", mq_mem_ref.pid, mq_mem_ref.p_mem, mq_mem_ref.time);
            
            sem_wait(&mutex);
            
            if ((act_uint_t)mq_mem_ref.p_mem >0x50000000UL){
                pp_mem_root = &gp_preload_mem_ref_root;
            }else {
                p_ref_root = act_memleak_ref_root_find(mq_mem_ref.pid);
                pp_mem_root = &p_ref_root->p_mem_ref_root;
            }
                        
            act_memleak_mem_ref_del(pp_mem_root,
                                    (act_uint_t)mq_mem_ref.p_mem,
                                    mq_mem_ref.stack_index, 
                                    mq_mem_ref.pid);
            sem_post(&mutex);
            
       //     act_printf("END_MQ_OP_DEL\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_DISP){
      //      act_printf("MQ_OP_DISP\n");
            
            err_dbfree_count = 1;
            err_unfree_count = 1;
            err_ovflow_count = 1;
            
            act_memleak_mem_statics_ref_all_display();

       //     act_printf("END_MQ_OP_DISP\n");


            
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_DISP_UNFREE){
            

      //      act_printf("MQ_OP_DISP_UNFREE\n");
            err_unfree_count = 1;
            
            act_memleak_send_mq_local_disp(gp_preload_mem_ref_root, ACT_MEMLEAK_MQ_OP_DISP_UNFREE);

            if (g_bind_pid == 0 ){

                for (i = 0; i < g_pid_max; i++){
                    p_ref_root = act_memleak_ref_root_find(g_pid_table[i].pid);
                    pp_mem_root = &p_ref_root->p_mem_ref_root;
                    
                    act_memleak_send_mq_local_disp(*pp_mem_root, ACT_MEMLEAK_MQ_OP_DISP_UNFREE);
                }
            }else {
                 p_ref_root = act_memleak_ref_root_find(g_bind_pid);
                 pp_mem_root = &p_ref_root->p_mem_ref_root;
                 
                 act_memleak_send_mq_local_disp(*pp_mem_root, ACT_MEMLEAK_MQ_OP_DISP_UNFREE);
            }

        //    act_printf("END_MQ_OP_DISP_UNFREE\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_DISP_DBFREE){

       //     act_printf("MQ_OP_DISP_DBFREE\n");
            err_dbfree_count = 1;

            act_memleak_send_mq_local_disp(gp_preload_mem_ref_root, ACT_MEMLEAK_MQ_OP_DISP_DBFREE);
            
            if (g_bind_pid == 0 ){
                for (i = 0; i < g_pid_max; i++){
                    p_ref_root = act_memleak_ref_root_find(g_pid_table[i].pid);
                    pp_mem_root = &p_ref_root->p_mem_ref_root;
                    
                    act_memleak_send_mq_local_disp(*pp_mem_root, ACT_MEMLEAK_MQ_OP_DISP_DBFREE);
                }
            }else {
                p_ref_root = act_memleak_ref_root_find(g_bind_pid);
                pp_mem_root = &p_ref_root->p_mem_ref_root;
                
                act_memleak_send_mq_local_disp(*pp_mem_root, ACT_MEMLEAK_MQ_OP_DISP_DBFREE);
            }
       //     act_printf("END_MQ_OP_DISP_DBFREE\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_DISP_OVFLOW){

       //     act_printf("MQ_OP_DISP_OVFLOW\n");
            err_ovflow_count = 1;

            act_memleak_send_mq_local_disp(gp_preload_mem_ref_root, ACT_MEMLEAK_MQ_OP_DISP_OVFLOW);
            
            p_ref_root = act_memleak_ref_root_find(g_bind_pid);
            pp_mem_root = &p_ref_root->p_mem_ref_root;
            
            act_memleak_send_mq_local_disp(*pp_mem_root, ACT_MEMLEAK_MQ_OP_DISP_OVFLOW);
            
      //      act_printf("END_MQ_OP_DISP_OVFLOW\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_PROFILE_ADD){
      //      act_printf("MQ_OP_PROFILE_ADD\n");
            p_mem_ref1 = (act_memleak_mq_t1 *)&mq_mem_ref;
            
            act_memleak_profile_func_add(p_mem_ref1->u.profile.caller_pc,
                                         p_mem_ref1->u.profile.self_pc,
                                         p_mem_ref1->u.profile.time,
                                         p_mem_ref1->pid);
            
      //      act_printf("END_MQ_OP_PROFILE_ADD\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_PROFILE_END){
     //      act_printf("MQ_OP_PROFILE_END\n");
            p_mem_ref1 = (act_memleak_mq_t1 *)&mq_mem_ref;
            
            act_memleak_profile_func_end(p_mem_ref1->pid, 
                                         p_mem_ref1->u.profile.time);
                                         
            
     //       act_printf("END_MQ_OP_PROFILE_END\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_PROFILE_DISP){
            act_memleak_profile_func_stat_cleanup();

    //        act_printf("MQ_OP_PROFILE_DISP\n");
            if (g_bind_pid == 0 ){
                for (i = 0; i < g_pid_max; i++){
                    act_printf("PROFILE_DISP pid:%d app_name:%s\n", g_pid_table[i].pid, g_pid_table[i].name);
                    act_memleak_profile_func_travel(g_bind_level, g_pid_table[i].pid);
                    act_printf("PROFILE_DISP\n");
                }
            }else {
                act_memleak_profile_func_travel(g_bind_level, g_bind_pid);
            }
    //        act_printf("END_MQ_OP_PROFILE_DISP\n");
        }else if (mq_mem_ref.mq_opcode == ACT_MEMLEAK_MQ_OP_PROFILE_GEO){
    //        act_printf("MQ_OP_PROFILE_STAT_DISP\n");
            if (g_bind_pid == 0 ){
                for (i = 0; i < g_pid_max; i++){
                    act_printf("PROFILE_STAT_DISP pid:%d app_name:%s\n", g_pid_table[i].pid, g_pid_table[i].name);
                    act_memleak_profile_func_stat_disp(g_pid_table[i].pid);
                    act_printf("PROFILE_STAT_DISP\n");
                }
            }else {
                act_memleak_profile_func_stat_disp(g_bind_pid);
            }
    //        act_printf("END_MQ_OP_PROFILE_STAT_DISP\n");
        }else{
    //        act_printf("MQ_UNREASONABLE\n");
            
        }
    }
    
    return NULL;
}



#define handle_stack_address(c, X, stack) \
   if (c &&(__builtin_frame_address((X)) != 0L) && ((X) <= ACT_MEMLEAK_MAX_STACK)) \
   { \
        printf("handle_stack_address %d = %p\n", X, __builtin_return_address(X)-1);\
        stack[X] = (unsigned int)(__builtin_return_address(X))-1;\
   }else {\
        c = 0;\
   }

int memleak_tcp_write(char *buf, int buf_size)
{
    static int g_sock = -1;
    
    if (g_sock < 0){
        g_sock = mc_socket_tcp_open(MQ_SERVER_IP, MQ_SERVER_PORT);
    }
    
    if (g_sock > 0){
        if (mc_socket_tcp_write(g_sock, buf, buf_size, 0) < 0){
            close(g_sock);
            g_sock = -1;
        }
    }
    
    return 0;
}

void act_memleak_send_mq_op(act_int_t op_code, act_void_t *p_mem, act_uint_t mem_size)
{
    act_memleak_mq_t mq_mem_ref;
    act_int_t c = 1;
    act_int_t stack[ACT_MEMLEAK_MAX_STACK];
    
    memset(stack, 0, sizeof(stack));
    handle_stack_address(c,0, stack);
    handle_stack_address(c,1, stack);
    handle_stack_address(c,2, stack);
    handle_stack_address(c,3, stack);
    handle_stack_address(c,4, stack);
    handle_stack_address(c,5, stack);
    handle_stack_address(c,6, stack);
    handle_stack_address(c,7, stack);
    
    memset(&mq_mem_ref, 0, sizeof(mq_mem_ref));
    mq_mem_ref.p_mem = p_mem;
    mq_mem_ref.mem_size = mem_size;
    memcpy(mq_mem_ref.stack_index, stack, sizeof(mq_mem_ref.stack_index));
    mq_mem_ref.mq_opcode = op_code;

    memleak_tcp_write((act_char_t *)&mq_mem_ref, sizeof(mq_mem_ref));

    return;
}



#define  act_mem_show_func_line()  act_memleak_func_ra_display(p_mem_ref->pid, p_mem_ref->stack_index); act_memleak_mem_statics_ref_show(p_mem_ref);



void act_memleak_send_mq_local_disp(act_memleak_avl_node_t* root, act_int_t op) 
{
    act_memleak_mem_ref_t *p_mem_ref = NULL;


    if (root != NULL)
        {
            act_memleak_send_mq_local_disp(root->left, op);
  
            p_mem_ref = (act_memleak_mem_ref_t *)root->p_value;
            printf("NODE :%d %p key:%x\n", p_mem_ref->status, p_mem_ref->p_mem, root->key);

            if ((op & ACT_MEMLEAK_MQ_OP_DISP_OVFLOW) &&p_mem_ref->status != ACT_MEMLEAK_MEM_ST_DFREE && (p_mem_ref->status & ACT_MEMLEAK_MEM_ST_OFLOW) &&
                (g_bind_pid == 0 || g_bind_pid == p_mem_ref->pid)){
               p_mem_ref->status |= ACT_MEMLEAK_MEM_ST_OFLOW;
               act_printf("MQ_OP_DISP_OVERFLOW count:%d \n", err_ovflow_count);err_ovflow_count++;
               act_printf("overflow memory %p pid:%d size:%d\n", p_mem_ref->p_mem, p_mem_ref->pid, p_mem_ref->mem_size);
               act_mem_show_func_line();
               act_printf("END_MQ_OP_DISP_OVERFLOW\n\n");
             }
            
            if ((op & ACT_MEMLEAK_MQ_OP_DISP_UNFREE) && (p_mem_ref->status & ACT_MEMLEAK_MEM_ST_ALLOC) &&
                (g_bind_pid == 0 || g_bind_pid == p_mem_ref->pid)){
                
                act_printf("MQ_OP_DISP_UNFREE count:%d\n", err_unfree_count);err_unfree_count++;
                act_printf("unfreed memory %p pid:%d app_name:%s size:%d\n", p_mem_ref->p_mem, p_mem_ref->pid, act_memleak_find_pid(p_mem_ref->pid), p_mem_ref->mem_size);
                act_mem_show_func_line();
                act_printf("END_MQ_OP_DISP_UNFREE\n\n");
                
            }else if((op & ACT_MEMLEAK_MQ_OP_DISP_DBFREE) && (p_mem_ref->status & ACT_MEMLEAK_MEM_ST_DFREE)&&
                     (g_bind_pid == 0 || g_bind_pid == p_mem_ref->pid)){
                act_printf("MQ_OP_DISP_DOUBLE FREE count:%d\n", err_dbfree_count);err_dbfree_count++;
                act_printf("double freed memory %p pid:%d app_name:%s size:%d\n", p_mem_ref->p_mem, p_mem_ref->pid, act_memleak_find_pid(p_mem_ref->pid), p_mem_ref->mem_size);
                act_mem_show_func_line();
                act_printf("END_MQ_OP_DISP_DOUBLE FREE\n\n");

            }
            
            act_memleak_send_mq_local_disp(root->right, op);
        }
}
