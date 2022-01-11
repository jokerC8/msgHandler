#ifndef __UART_H__
#define __UART_H__

#include <sys/types.h>

int uartInit();

ssize_t writeUart(unsigned char* buf, int len);

ssize_t readUart(unsigned char* buf, int len);

#endif
