#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <endian.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ev.h>

#include "kfifo.h"
#include "common.h"

#ifdef ENABLE_UART
#include "uart.h"
#endif

#define HEADERLEN 4
#define BACKLOG 20
#define MAXCLIENTS 100

// clients numbers
int g_clients;

static int createServerHandler(const char* addr, uint16_t port)
{
	int fd;
	struct sockaddr_in server;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htobe16(port);
	if (inet_pton(AF_INET, addr, &server.sin_addr) != 1)
		goto out;

	// this function must called before bind()
	int option = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	if (bind(fd, (struct sockaddr*)&server, sizeof(server)) < 0)
		goto out;

	if (listen(fd, BACKLOG) < 0)
		goto out;

	return fd;

out:
	close(fd);
	return -1;
}

// 1. stop watcher
// 2. close handler
// 3. free watcher
static void disconnect(EV_P_ ev_io* w)
{
	ev_io_stop(EV_A_ w);
	close(w->fd);
	free(w);
	--g_clients;
	assert(g_clients >= 0);
}

// |0x0a|0x05|len_high_byte|len_low_byte|.....|
static void readCB(EV_P_ ev_io* w, int e)
{
	char header[HEADERLEN] = {0};
	char buf[CMDLEN] = {0};
	char* temp = header;
	ssize_t count = 0, len = ARRAYSIZE(header);
	tsThreadParams* param = (tsThreadParams*)ev_userdata(EV_A);

	// read header
	while ((count = read(w->fd, temp, len)) > 0) {
		if (count == len)
			break;
		len -= count;
		temp += count;
	}
	// because read unblocked, so it is not error, just ignore
	if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	// something goes wrong or disconnected, stop the watcher
	// close the handler and free the watcher
	if (count <= 0) {
		disconnect(EV_A_ w);
		return;
	}
	// check header
	if (header[0] != 0x0a || header[1] != 0x05)
		return;
	len = header[2] << 8 | header[3];
	if (len >= CMDLEN)
		len = CMDLEN - 1;

	temp = buf;
	// read the command string
	while ((count = read(w->fd, temp, len)) > 0) {
		if (count == len)
			break;
		len -= count;
		temp += count;
	}
	if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	if (count <= 0) {
		disconnect(EV_A_ w);
		return;
	}
	// read command string success, put it into queue
	// and wakeup main thread to handler it
	uint64_t val = 1;
	kfifo_put(param->queue, (uint8_t*)buf, ARRAYSIZE(buf));
	write(param->eventfd, &val, sizeof(val));
}

static void acceptCB(EV_P_ ev_io* w, int e)
{
	int connfd;
	socklen_t len;
	struct ev_io* readWatcher;
	struct sockaddr_in client;

	len = sizeof(client);
#ifdef _GNU_SOURCE
	connfd = accept4(w->fd, (struct sockaddr*)&client, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
	connfd = accept(w->fd, (struct sockaddr*)&client, &len);
#endif
	if (connfd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	if (connfd <= 0) {
		ev_io_stop(EV_A_ w);
		close(w->fd);
		ev_break(EV_A_ EVBREAK_ALL);
		return;
	}
	LOGD("new connection from [%s:%d]\n", inet_ntoa(client.sin_addr), be16toh(client.sin_port));
	if (++g_clients > MAXCLIENTS) {
		LOGE("warning, client number:%d Exceed max clients (%d)\n", g_clients, MAXCLIENTS);
		--g_clients;
		close(connfd);
		return;
	}
	// set nonblock flag
	fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL));

	readWatcher = (ev_io*)malloc(sizeof(*readWatcher));
	if (readWatcher) {
		ev_io_init(readWatcher, readCB, connfd, EV_READ);
		ev_io_start(EV_A_ readWatcher);

	} else {
		LOGE("malloc failed");
		close(connfd);
	}
}

#ifdef ENABLE_UART
static void readUartCB(EV_P_ ev_io* w, int e)
{
	char buf[128];

	ssize_t count = readUart((uint8_t*)buf, sizeof(buf));
	LOGD("read %ld bytes from uart\n", count);
	// TODO
}
#endif

static void startLoop(void* args)
{
#ifdef EV_MULTIPLICITY
	struct ev_loop* loop;
#else
	int loop;
#endif
	int fd;
	ev_io* acceptWatcher;

	tsThreadParams* param = (tsThreadParams*)args;
	LOGD("start server thread, listen on: %s:%d\n", param->addr, param->port);

	fd = createServerHandler(param->addr, param->port);
	loop = ev_default_loop(0);
	acceptWatcher = (ev_io*)malloc(sizeof(*acceptWatcher));
	assert(loop && acceptWatcher && fd > 0);

	ev_set_userdata(EV_A_ param);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL));

	ev_io_init(acceptWatcher, acceptCB, fd, EV_READ);
	ev_io_start(EV_A_ acceptWatcher);

#ifdef ENABLE_UART
	int uartfd;
	ev_io* uartWatcher;

	uartfd = uartInit();
	uartWatcher = (ev_io*)malloc(sizeof(*uartWatcher));
	assert(uartfd > 0 && uartWatcher);

	ev_io_init(uartWatcher, readUartCB, uartfd, EV_READ);
	ev_io_start(EV_A_ uartWatcher);
#endif

	ev_run(EV_A_ 0);

	// breakout loop
	ev_loop_destroy(EV_A);
}

void* serverHandler(void* args)
{
	struct sigaction sa;

	pthread_detach(pthread_self());

	// ignore SIGPIPE
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);	

	startLoop(args);
	return 0;
}
