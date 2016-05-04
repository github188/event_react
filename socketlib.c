#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <time.h>																				
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "socket_poll.h"
#include "common.h"
#include "socketlib.h"



#undef  DBG_ON
#undef  FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"socketlib:"



#define   MAX_SOCKET_NUM	(1000u)

typedef  enum  solt_status
{
	SOLT_FREE,
	SOLT_USED,
}solt_status_m;


typedef enum socket_status
{
	SOCKET_STATUS_INIT,
	SOCKET_STATUS_OPEN,
	SOCKET_STATUS_CLOSE,
	SOCKET_STATUS_PLISTEN,
	SOCKET_STATUS_LISTEN,
	SOCKET_STATUS_PACCETP,
	SOCKET_STATUS_ACCETP,
}socket_status_m;


typedef enum socket_action
{
	SOCKET_ACTION_OPEN,
	SOCKET_ACTION_CLOSE,
	SOCKET_ACTION_LISTEN,
	SOCKET_ACTION_CONNECT,
	SOCKET_ACTION_ACCETP,
}socket_action_m;


typedef struct socket_solt
{
	int id;
	int fd;
	solt_status_m status_solt;
	socket_status_m status_socket;

}socket_solt_t;



typedef struct request_cmd
{
	int type;
	int length;
}request_cmd_t;


typedef struct request_listen
{
	int fd;
	char host[32];
}request_listen_t;

typedef struct request_connect
{
	int port;
	char host[32];
}request_connect_t;


typedef struct  socket_handle
{
	int pipe[2];
	int poll_fd;
	socket_solt_t solt[MAX_SOCKET_NUM];

}socket_handle_t;



static socket_handle_t * socket_system = NULL;



int socketlib_alloc_id(void * handle_system)
{
	if(NULL == handle_system)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	socket_handle_t * handle = (socket_handle_t*)handle_system;

	int i = 0;
	int id = -1;
	socket_solt_t * socket = &handle->solt[0];
	for(i=0;i<MAX_SOCKET_NUM;++i)
	{

		if(SOLT_FREE == socket->status_solt)
		{
			socket->status_solt = SOLT_USED;
			id = i;
			break;
		}
		socket += 1;
	}
	return(id);
}



int socketlib_keep_alive(int socket_fd)
{
	if(socket_fd < 0 )
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	int value = 1;
	setsockopt(socket_fd,SOL_SOCKET,SO_KEEPALIVE,(void *)&value , sizeof(value));
	return(0);
}



int socketlib_read_pipe(void * handle_system,void * out_buffer,int length)
{
	if(NULL==handle_system || NULL==out_buffer)
	{
		dbg_printf("please check the param!\n");
		return(-1);
	}
	socket_handle_t * handle = (socket_handle_t*)handle_system;

	int read_count = 0;
	int is_read = 1;
	while(is_read)
	{
		read_count = read(handle->pipe[0],out_buffer,length);
		if(read_count < 0)
		{
			if(EINTR==errno)
			{
				usleep(10*1000);
				continue;
			}
		}
		is_read = 0;
	}
	return(read_count);
}



int socketlib_send_cmd(void * handle_system,int type,void * data,int length)
{
	if(NULL == handle_system)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	socket_handle_t * handle = (socket_handle_t*)handle_system;
	char cmd_buff[length+128];
	
	request_cmd_t * pcmd = (request_cmd_t*)cmd_buff;
	pcmd->type = type;
	pcmd->length = length;
	void * pdata = pcmd+1;
	memmove(pdata,data,length);
	int is_write = 1;
	while(is_write)
	{
		int n = write(handle->pipe[1],cmd_buff, length+sizeof(request_cmd_t));
		if (n<0) 
		{
			if (errno == EINTR) 
			{	
				usleep(10*1000);
				continue;
			}
		}
		is_write = 0;
	}
	return(0);
}




int socketlib_do_listen(void * handle_system,const char * addr, int port, int backlog)
{
	if(NULL == handle_system)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	socket_handle_t * handle = (socket_handle_t*)handle_system;

	unsigned int ip_addr = INADDR_ANY;
	if (NULL != addr) 
	{
		ip_addr=inet_addr(addr);	
	}
	
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) 
	{
		dbg_printf("socket is fail!\n");
		return -1;
	}
	
	int reuse = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(int))==-1) 
	{
		dbg_printf("setsockopt is fail!\n");
		goto fail;
	}
	
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(struct sockaddr_in));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = ip_addr;
	
	if (bind(listen_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
	{
		dbg_printf("bind is fail!\n");
		goto fail;
	}
	
	if (listen(listen_fd, backlog) == -1) 
	{
		dbg_printf("listen is fail!\n");
		goto fail;
	}

	request_listen_t request;
	memset(&request,'\0',sizeof(request_listen_t));
	request.fd = listen_fd;
	socketlib_send_cmd(handle_system,SOCKET_ACTION_LISTEN,&request,sizeof(request_listen_t));

	return(0);


fail:

	if(listen_fd > 0)
	{
		close(listen_fd);
		listen_fd = -1;
	}
	return(-1);
}




int socketlib_do_connect(void * handle_system,const char * addr, int port)
{
	if(NULL == handle_system)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	socket_handle_t * handle = (socket_handle_t*)handle_system;
	int add_length = strlen(addr);
	if(add_length > 32)return(-1);
	request_connect_t request;
	memset(&request,'\0',sizeof(request_connect_t));
	request.port = port;
	memmove(&request.host,addr,add_length);
	socketlib_send_cmd(handle_system,SOCKET_ACTION_CONNECT,&request,sizeof(request));

	return(0);

}



int socketlib_init_handle(void)
{
	if(NULL != socket_system)
	{
		dbg_printf("the system has been inited!\n");
		return(-1);
	}

	int ret = -1;
	socket_handle_t * new_handle = calloc(1,sizeof(*new_handle));
	if(NULL == new_handle)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}

	new_handle->poll_fd = poll_create();
	if(new_handle->poll_fd < 0)
	{
		dbg_printf("poll_create is fail!\n");
		return(-1);
	}

	ret = pipe(new_handle->pipe);
	if(0 != ret)
	{
		dbg_printf("pipe is fail!\n");
		goto fail;

	}

	ret = poll_add(new_handle->poll_fd, new_handle->pipe[0], NULL);
	if(0 != ret)
	{
		dbg_printf("poll_add is fail!\n");
		goto fail;

	}

	int i = 0;
	socket_solt_t * socket = &new_handle->solt[0];
	for(i=0;i<MAX_SOCKET_NUM;++i)
	{
		socket->fd = -1;
		socket->id = -1;
		socket->status_solt = SOLT_FREE;
		socket->status_socket = SOCKET_STATUS_INIT;
		socket += 1;
	}

	socket_system = new_handle;

	return(0);


fail:
	if(new_handle->poll_fd > 0)
	{
		poll_release(new_handle->poll_fd);	
		new_handle->poll_fd = -1;
	}

	if(new_handle->pipe[0] > 0)
	{
		close(new_handle->pipe[0]);
		new_handle->pipe[0] = -1;
	}

	if(new_handle->pipe[1] > 0)
	{
		close(new_handle->pipe[1]);
		new_handle->pipe[1] = -1;
	}

	if(NULL != new_handle)
	{
		free(new_handle);
		new_handle = NULL;
	}
	
	return(-1);
}
