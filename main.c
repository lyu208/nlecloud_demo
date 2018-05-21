#include<stdio.h>      /*标准输入输出定义*/  
#include<stdlib.h>     /*标准函数库定义*/  
#include<unistd.h>     /*Unix 标准函数定义*/  
#include<sys/types.h>   
#include<sys/stat.h>     
#include<fcntl.h>      /*文件控制定义*/  
#include<termios.h>    /*PPSIX 终端控制定义*/  
#include<errno.h>      /*错误号定义*/  
#include<string.h>  
#include <time.h>
#include <pthread.h>

#include "tcpsock.h"
#include "cJSON.h"
#include "cloud.h"

#define MAIN_DBG(fmt, args...)	printf("DEBUG:%s:%d, "fmt, __FUNCTION__, __LINE__, ##args)
#define MAIN_ERR(fmt, args...)	printf("ERR:%s:%d, "fmt, __FUNCTION__, __LINE__, ##args)



#define SERVER_IP	"120.77.58.34"
#define	SERVER_PORT	8600

int is_auth_ok = 0;
void* Connect_demo(void* param){
	int ret;
	CON_REQ con_req;
	POST_REQ post_req;
	PACKET packet;
	struct timespec sleep_time;
	int sock = *((int*)param);
	con_req.msg_type = PACKET_TYPE_CONN_REQ;
	con_req.device_id = "xinxi208";
	con_req.key = "066ee3f24ce84a5ca1847457fc1c42a4";
	con_req.ver = "V1.0";
	packet = packet_msg(&con_req);
	ret = send_packet(sock, packet, strlen(packet), 0);
	if(ret < 0){
		MAIN_ERR("PACKET_TYPE_CONN_REQ error\n");
	}
	free_packet_msg(packet);
	//延时10ms
	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = 100000000;
	memset(&post_req, 0, sizeof(post_req));
}

void* Send_process(void* param)
{
int ret;
	PACKET packet;
	POST_REQ post_req;
	int send_flag = 0;
	int sock = *((int*)param);
	int num;
	char data[100]={0};
		srand(time(0));
		num = rand() % (30 - 22) + 22;  //模拟温度数据
		//数据类型为1（JSON格式1字符串）
		sprintf(data,"{\
		\"nl_temperature\":\"%d\"\
		}",num);//构造数据格式
		post_req.msg_type = PACKET_TYPE_POST_DATA;
		post_req.msg_id++;
		post_req.data_type = 1;
		post_req.data =data;
		post_req.data_len = strlen(post_req.data);
		packet = packet_msg(&post_req);
		if(packet == NULL){
			MAIN_ERR("packet_msg JSON 1 error\n");
		}else{
			MAIN_DBG("POST JSON 1 \n");
			send_flag = 1;
		}
		if(send_flag){
			ret = send_packet(sock, packet, strlen(packet), 0);
			if(ret < 0){
				MAIN_ERR("PACKET_TYPE_POST_DATA error\n");
			}
		free_packet_msg(packet);
		send_flag = 0;
			}
		

}

#define	KEEP_ALIVE_MSG		"$#AT#\r"
#define	KEEP_ALIVE_RSP		"$OK##\r"

void* Recv_demo(void* param)
{
	char msg_buf[128];
	int ret;
	void* msg_unpacket;
	int* msg_type;
	int sock = *((int*)param);

	
		memset(msg_buf, 0, sizeof(msg_buf));
		ret = receive_packet(sock, msg_buf, sizeof(msg_buf), 0);
		if(ret > 0){
			//hex_dump((const unsigned char*)msg_buf, ret);
			msg_unpacket = unpacket_msg(msg_buf);
			if(msg_unpacket == NULL){
				if(strcmp(msg_buf, KEEP_ALIVE_MSG) == 0){
					MAIN_DBG("recv keep alive\n");
					ret = send_packet(sock, KEEP_ALIVE_RSP, strlen(KEEP_ALIVE_RSP), 0);
					if(ret < 0){
						MAIN_ERR("send keep alive error\n");
					}else{
						MAIN_DBG("send keep alive OK\n");
					}
				}else{
					hex_dump((const unsigned char*)msg_buf, ret);
					MAIN_DBG("not JSON msg or keep alive msg(%s) \n", msg_buf);
				}
			}else{
				MAIN_DBG("recv:\n%s\n", msg_buf);
				msg_type = (int*)msg_unpacket;
				switch(*msg_type){
					case PACKET_TYPE_CONN_RSP:{
						CON_REQ_RSP* con_req_rsp = (CON_REQ_RSP*)msg_unpacket;

						MAIN_DBG("unpacket, msg_type:%d, status:%d\n", con_req_rsp->msg_type, con_req_rsp->status);
						if(con_req_rsp->status == 0){
							MAIN_DBG("server authentication OK\n");
							is_auth_ok = 1;
						}
						break;
					}
					case PACKET_TYPE_POST_RSP:{
						POST_REQ_RSP* post_req_rsp = (POST_REQ_RSP*)msg_unpacket;

						MAIN_DBG("unpacket, msg_type:%d, msg_id:%d status:%d\n", post_req_rsp->msg_type, post_req_rsp->msg_id, post_req_rsp->status);
						if(post_req_rsp->status == 0){
							MAIN_DBG("POST SUCESS\n");
						}
						
						break;
					}
					case PACKET_TYPE_CMD_REQ:{
						CMD_REQ* cmd_rcv = (CMD_REQ*)msg_unpacket;
						int is_cmd_need_rsp = 0;
						PACKET packet;
						CMD_REQ_RSP* cmd_rsp = (CMD_REQ_RSP*)cmd_rcv;	//CMD_REQ struct is same with CMD_REQ_RSP struct

						MAIN_DBG("recv CMD, data type:%d\n", cmd_rcv->data_type);
						switch(cmd_rcv->data_type){
							case CMD_DATA_TYPE_NUM:
								is_cmd_need_rsp = 1;
								MAIN_DBG("unpacket, msg_type:%d, msg_id:%d apitag:%s, data:%d\n", 
										cmd_rcv->msg_type, cmd_rcv->cmd_id, cmd_rcv->api_tag, *((int*)cmd_rcv->data));
	
								break;
							case CMD_DATA_TYPE_DOUBLE:
								is_cmd_need_rsp = 1;
								MAIN_DBG("unpacket, msg_type:%d, msg_id:%d apitag:%s, data:%f\n", 
										cmd_rcv->msg_type, cmd_rcv->cmd_id, cmd_rcv->api_tag, *((double*)cmd_rcv->data));
								break;
							case CMD_DATA_TYPE_STRING:
								is_cmd_need_rsp = 1;
								MAIN_DBG("unpacket, msg_type:%d, msg_id:%d apitag:%s, data:%s\n", 
										cmd_rcv->msg_type, cmd_rcv->cmd_id, cmd_rcv->api_tag, (char*)cmd_rcv->data);
								break;
							case CMD_DATA_TYPE_JSON:
								is_cmd_need_rsp = 1;
								MAIN_DBG("unpacket, msg_type:%d, msg_id:%d apitag:%s, data:%s\n", 
										cmd_rcv->msg_type, cmd_rcv->cmd_id, cmd_rcv->api_tag, (char*)cmd_rcv->data);
								break;
							default:
								MAIN_ERR("data_type(%d) error\n", cmd_rcv->data_type);
						}
						if(is_cmd_need_rsp){
							cmd_rsp->msg_type = PACKET_TYPE_CMD_RSP;
							packet = packet_msg(cmd_rsp);
							cmd_rsp->msg_type = PACKET_TYPE_CMD_REQ;
							if(packet == NULL){
								MAIN_ERR("packet_msg PACKET_TYPE_CMD_RSP error\n");
								break;
							}
							ret = send_packet(sock, packet, strlen(packet), 0);
							if(ret < 0){
								MAIN_ERR("send PACKET_TYPE_CMD_RSP error\n");
							}else{
								MAIN_DBG("cmd rsp:\n%s\n", packet);
							}
							free_packet_msg(packet);
						}
						break;
					}
					default:
						MAIN_ERR("msg_type(%d) error\n", *msg_type);
						break;
				}
				free_unpacket_msg(msg_unpacket);
			}
		}else if(ret == 0){
			//break;
		}
	

	return 0;
}


int main(int argc, char **argv)
{
	int sock;
	pthread_t send_pid;
	pthread_t recv_pid;
	sock = open_client_port(0);
	if(sock == -1){
		MAIN_ERR("open_client_port error\n");
		return 0;
	}

	if(connection_server(sock, SERVER_IP, SERVER_PORT) < 0){
		MAIN_ERR("connection_server error\n");
		return 0;
	}else{
		MAIN_DBG("connect server OK\n");
	}
	pthread_create(&recv_pid, NULL, recv_process, &sock);
	Connect_demo(&sock);
	Recv_demo(&sock);
	while(1){
		Send_process(&sock);
		Recv_demo(&sock);
		sleep(30);
	}
	return 0;
}

