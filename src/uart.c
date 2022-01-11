#include <fcntl.h>
#include <unistd.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "uart.h"

#define UART "/dev/ttyUSB0"

int uartfd;

int uartInit()
{
	struct termios tp;
	uartfd = open(UART, O_RDWR | O_NOCTTY);
	if (uartfd < 0)
		return -1;

	// attributes
	tcgetattr(uartfd, &tp);
	tp.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	tp.c_iflag = IGNBRK | IGNPAR;
	tp.c_oflag = 0;
	tp.c_lflag = 0;

	cfsetspeed(&tp, 115200);
	tcflush(uartfd, TCIFLUSH);
	tcsetattr(uartfd, TCSAFLUSH, &tp);

	fcntl(uartfd, F_SETFL, fcntl(uartfd, F_GETFL, 0) | O_NONBLOCK);
	return uartfd;
}

ssize_t writeUart(unsigned char* buf, int len)
{
	ssize_t count, total = 0;

	while ((count = write(uartfd, buf, len)) > 0) {
		total += count;
		if (count == len)
			break;
		buf += count;
		len -= count;
	}

	return total;
}

ssize_t readUart(unsigned char* buf, int len)
{
	ssize_t count, total = 0;

	while ((count = read(uartfd, buf, len)) > 0) {
		buf += count;
		len -= len;
		total += count;
	}

	return total;
}
