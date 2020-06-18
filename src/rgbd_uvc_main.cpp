
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/signal.h>
#include <stdlib.h>

#include <netinet/in.h>
 #include <arpa/inet.h>

#include "RGBDClass.hpp"

TRGBDClass *c;
int exit_process=0;
//#define NETWORK_CLIENT
#if defined(NETWORK_CLIENT)
#define PORT	5032
int sock_fd;
const char* netaddr = "192.168.1.141";
#endif

/**
 *  @brief  "C" Terminate the program
 *  @param[in] sig   signals(SIGINT, SIGTERM, SIGABRT)
 *  @return none
*/
void signal_handler(int sig)
{
	c->Uninit();	
	delete c;
//	printf("sig %d\n", sig);
	exit_process = 1;
//	exit(0);
}

void get_rgbd_data(void *data)
{
		
//	printf("get_rgbd_data\n");
#if defined(NETWORK_CLIENT)
		send(sock_fd, data, 640*480*2, 0);		
#endif		
	
}

extern FILE *dfp;
int main(void)
{
	struct v4l2_buffer buf;	
	int ret=0;
	c = new TRGBDClass(4);
#if defined(NETWORK_CLIENT)

		int tcp_port = PORT;	
		const char* addr = netaddr;	
		struct sockaddr_in server_addr;
		
		sock_fd = socket(PF_INET, SOCK_STREAM, 0);
		if(sock_fd < 0){
			printf("==============socket failed");
			exit(-1);
		}
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(addr);
		server_addr.sin_port = htons(tcp_port);
		
	
		if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0){
			printf("================connect fail");
			exit(-1);
		}
#endif		
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);	
	dfp = stdout;
	
	ret = c->Init();
	if(ret){
		printf("ret %d\n", ret);		
	}

	c->RegisterCallback((void*)get_rgbd_data);
	
	while(!c->thr_data.g_depth_done){
		c->runner(&buf);	
		
		
		
		
	}
	if(exit_process){
		printf("exit_process\n");
		usleep(1000000);
	}else{
		c->Uninit();
		delete c;
	}
	
	return 0;
}