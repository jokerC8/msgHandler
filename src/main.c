#include "loop.h"

int main(void)
{
	setServerAddr("127.0.0.1:10080");
	setBrokerAddr("1.116.148.224:1883");
	loop();
	return 0;
}
