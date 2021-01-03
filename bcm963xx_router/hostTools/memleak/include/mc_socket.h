/**************************************************************************
 *
 *	��Ȩ:	
 *              
 *
 *	�ļ���:	
 *              mc_socket.h
 *
 *	����:	
 *              ף���� 
 *
 *	��������:	
 *		
 *            
 *	�����б�:	
 *             
 *              
 *      ��ʷ:
 *              $Id: mc_socket.h,v 1.1 2011/04/02 02:42:29 xxia Exp $ 
 *		
 **************************************************************************/
#ifndef MC_SOCKET_H
#define MC_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mc_port.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#if 1
//linux
#if 0
#include <pcap/pcap.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/filter.h>
#endif
#else  
//solaris
#include <pcap.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#endif




/**************************************************************************
 *	[������]:
 *	        mc_socket_udp_bind
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/

mc_int_t
mc_socket_udp_bind(mc_int_t port);


/**************************************************************************
 *	[������]:
 *	        mc_socket_udp_read
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_char_t *
mc_socket_udp_read(IN mc_int_t sock_id, 
		 OUT struct sockaddr_in *p_cli_addr);


/**************************************************************************
 *	[������]:
 *	        mc_socket_udp_read_buf
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/

mc_rv
mc_socket_udp_read_buf(IN mc_int_t sock_id,
		     IN mc_char_t *p_buf, 
		     IN mc_int_t buf_size);

/**************************************************************************
 *	[������]:
 *	        mc_socket_udp_send
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_rv 
mc_socket_udp_send(IN mc_int_t sock_id,
		 IN mc_char_t *p_buf,
		 IN struct sockaddr_in *p_dst_addr);



/**************************************************************************
 *	[������]:
 *	        mc_socket_udp_write
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_rv
mc_socket_udp_write(IN mc_int_t sock_id, 
                    IN mc_char_t *p_buf, 
                    IN mc_int_t buf_size,
                    IN mc_char_t *p_dest,
                    IN mc_int_t d_port );




/**************************************************************************
 *	[������]:
 *	        mc_socket_raw_udp_bind
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_int_t 
mc_socket_raw_udp_bind(mc_int_t port);



/**************************************************************************
 *	[������]:
 *	        mc_socket_raw_udp_read
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_rv
mc_socket_raw_udp_read(IN mc_int_t sock_id,
		     IN mc_char_t *p_buf, 
		     IN mc_int_t buf_size);



/**************************************************************************
 *	[������]:
 *	        mc_socket_raw_udp_open
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_int_t mc_socket_raw_udp_open();



/**************************************************************************
 *	[������]:
 *	        mc_socket_raw_udp_write
 *
 *	[��������]:
 *              
 *
 *	[��������]
 *  		��
 *            
 *	[��������ֵ]
 *                        �ɹ�
 *              MC_RV_ERR ʧ��
 *
**************************************************************************/
mc_int_t
mc_socket_raw_udp_write(IN mc_int_t sock_id, 
		      IN mc_char_t *p_buf, 
		      IN mc_char_t *p_dest,
		      IN mc_int_t d_port );


/**************************************************************************
 *	[������]:
 *	        mc_socket_tcp_open
 *
 *	[��������]:
 *              ����TCP/IP����
 *
 *	[��������]
 *  		IN mc_char_t ip    �Է�IP��ַ
 *              IN mc_int_t  port  �Է��˿ں�
 *
 *	[��������ֵ]
 *              >=1       TCP/IP���Ӿ��
 *              MC_RV_ERR ʧ��
 *
 *************************************************************************/
mc_int_t mc_socket_tcp_open(IN mc_char_t *ip,
			    IN mc_int_t  port);

/**************************************************************************
 *	[������]:
 *	        mc_socket_tcp_close
 *
 *	[��������]:
 *              �ر�TCP/IP����
 *
 *	[��������]
 *  		IN mc_int_t  socket_id 
 *
 *	[��������ֵ]
 *              MC_RV_SUC �ɹ�
 *              MC_RV_ERR ʧ��
 *
 *************************************************************************/
mc_rv mc_socket_tcp_close(IN mc_int_t  socket_id);


/**************************************************************************
 *	[������]:
 *	        mc_socket_tcp_bind
 *
 *	[��������]:
 *              ����TCP/IP�˿�
 *
 *	[��������]
 *  		IN mc_int_t port �˿ں�
 *            
 *	[��������ֵ]
 *              >=1         ����SOCKET���
 *              MC_RV_ERR   ʧ��
 *
**************************************************************************/
mc_int_t 
mc_socket_tcp_bind(mc_int_t port);


/**************************************************************************
 *	[������]:
 *	        mc_socket_tcp_write
 *
 *	[��������]:
 *              дTCP/IP����
 *
 *	[��������]
 *  		IN mc_int_t  socket_id   SOCKET���
 *              IN mc_char_t *p_buf      �����ַ���
 *              IN mc_int_t  buf_size    �ַ�������
 *              IN mc_int_t  flag        1 ��ʾѭ����дֱ�����㳤�� 0��ʾֻ��д
 * 
 *	[��������ֵ]
 *              >= 1       д�����ݳ���
 *              MC_RV_ERR  ʧ��
 *
**************************************************************************/
mc_int_t mc_socket_tcp_write(IN mc_int_t sock_id, 
			     IN mc_char_t *p_buf, 
			     IN mc_int_t buf_size,
			     IN mc_int_t flag);


/**************************************************************************
 *	[������]:
 *	        mc_socket_tcp_read
 *
 *	[��������]:
 *              ��ȡTCP/IP����
 *
 *	[��������]
 *  		IN    mc_int_t  socket_id   SOCKET���
 *              INOUT mc_char_t *p_buf      ��ȡ�ַ���
 *              IN    mc_int_t  buf_size    �ַ�������
 *              IN mc_int_t  flag           1 ��ʾѭ����дֱ�����㳤�� 0��ʾֻ��д
 *            
 *	[��������ֵ]
 *              >= 1       ��ȡ���ݳ���
 *              MC_RV_ERR  ʧ��
 *
**************************************************************************/
mc_int_t mc_socket_tcp_read(IN    mc_int_t sock_id,
			    INOUT mc_char_t *p_buf, 
			    IN    mc_int_t buf_size,
			    IN    mc_int_t flag);
	






	
#ifdef __cplusplus
}
#endif

#endif






