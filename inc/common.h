#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>

#define CMDLEN 1024

#define MINQUEUELEN 100*1024

#define ARRAYSIZE(arr) sizeof(arr)/sizeof(arr[0])

#define LOGE(format, args...) fprintf(stderr, "\033[;31m" "[func:%s line:%d]" format "\033[0m", __FUNCTION__, __LINE__, ##args)

#define LOGD(format, args...) fprintf(stdout, "\033[;32m" "[func:%s line:%d]" format "\033[0m", __FUNCTION__, __LINE__, ##args)

typedef struct tsThreadParams {
	char* addr; 
	void* queue;
	int eventfd;
	unsigned short port;
} tsThreadParams;

#endif
