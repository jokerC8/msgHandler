#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sys/eventfd.h>

#include "loop.h"
#include "kfifo.h"
#include "common.h"

// server addr and port
char serverAddr[128];
uint16_t serverPort;

// mqtt broker addr and port
char brokerAddr[128];
uint16_t brokerPort;

// message queue len
int messageQueueLen = MINQUEUELEN;

uint8_t quit;

// mqtt thread function
extern void* serverHandler(void* args);

// server thread function
extern void* mqttHandler(void* args);

void setServerAddr(const char* addr)
{
	char* p = strstr(addr, ":");
	memcpy(serverAddr, addr, p - addr);
	serverPort = atoi(p + 1);
}

void setBrokerAddr(const char* addr)
{
	char* p = strstr(addr, ":");
	memcpy(brokerAddr, addr, p - addr);
	brokerPort = atoi(p + 1);
}

void setMsgQueueLen(int len)
{
	if (len > MINQUEUELEN)
		messageQueueLen = len;
}

void loop()
{
	int fd;
	pthread_t pids[2];
	struct kfifo* queue;
	pthread_mutex_t lock;

	pthread_mutex_init(&lock, NULL);
	queue = kfifo_alloc(messageQueueLen, &lock);
	assert(queue);

	fd = eventfd(0, EFD_SEMAPHORE);
	tsThreadParams serverThreadParam = {
		.addr = serverAddr,
		.queue = queue,
		.eventfd = fd,
		.port = serverPort
	};
	tsThreadParams brokerThreadParam = {
		.addr = brokerAddr,
		.queue = queue,
		.eventfd = fd,
		.port = brokerPort
	};

	for (size_t i = 0; i < ARRAYSIZE(pids); ++i) {
		if (pthread_create(&pids[i], NULL, i == 0 ? serverHandler : mqttHandler,
					i == 0 ? &serverThreadParam : &brokerThreadParam) != 0) {
			LOGE("pthread_create failed (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	uint64_t val;
	char buf[CMDLEN];
	while (!quit) {
		// this will block utils message in coming
		read(fd, &val, ARRAYSIZE(buf));
		kfifo_get(queue, (uint8_t*)buf, ARRAYSIZE(buf));
		// process command string
		// TODO
		LOGD("%s\n", buf);
	}
}
