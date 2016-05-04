#ifndef _socket_poll_h
#define _socket_poll_h


#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "common.h"


typedef struct event 
{
	void * s;
	int read;
	int write;
}event_t;


#ifdef __linux__


static int poll_create()
{
	return epoll_create(1024);
}

static void poll_release(int efd)
{
	close(efd);
	efd = -1;
}


static int  poll_add(int efd, int sock, void *ud) 
{
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1)
	{
		return (-1);
	}
	return 0;
}

static void  poll_del(int efd, int sock) 
{
	epoll_ctl(efd, EPOLL_CTL_DEL, sock , NULL);
}


static void  poll_write(int efd, int sock, void *ud, int enable)
{
	struct epoll_event ev;
	ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
	ev.data.ptr = ud;
	epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev);
}

static int poll_wait(int efd, event_t *e, int max) 
{
	struct epoll_event ev[max];
	int n = epoll_wait(efd , ev, max, -1);
	int i;
	for (i=0;i<n;i++) {
		e[i].s = ev[i].data.ptr;
		unsigned flag = ev[i].events;
		e[i].write = (flag & EPOLLOUT) != 0;
		e[i].read = (flag & EPOLLIN) != 0;
	}

	return n;
}

static void poll_nonblocking(int fd)
{
	int flag = fcntl(fd, F_GETFL, 0);
	if ( -1 == flag )
	{
		return;
	}
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}




#endif






#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"


static int poll_create() 
{
	return kqueue();
}

static void poll_release(int kfd)
{
	close(kfd);
}


static int poll_add(int kfd, int sock, void *ud)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1)
	{
		return 1;
	}
	EV_SET(&ke, sock, EVFILT_WRITE, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1)
	{
		EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		kevent(kfd, &ke, 1, NULL, 0, NULL);
		return 1;
	}
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DISABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1)
	{
		sp_del(kfd, sock);
		return 1;
	}
	return 0;
}



static void  poll_del(int kfd, int sock)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
}



static void poll_write(int kfd, int sock, void *ud, int enable)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_WRITE, enable ? EV_ENABLE : EV_DISABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1) 
	{
		// todo: check error
	}
}

static int  poll_wait(int kfd, event_t *e, int max) 
{
	struct kevent ev[max];
	int n = kevent(kfd, NULL, 0, ev, max, NULL);
	int i;
	for (i=0;i<n;i++) 
	{
		e[i].s = ev[i].udata;
		unsigned filter = ev[i].filter;
		e[i].write = (filter == EVFILT_WRITE);
		e[i].read = (filter == EVFILT_READ);
	}
	return n;
}

static void poll_nonblocking(int fd)
{
	int flag = fcntl(fd, F_GETFL, 0);
	if ( -1 == flag )
	{
		return;
	}
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


#endif




#endif  /*_socket_poll_h*/



