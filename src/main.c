#include "loop.h"

int main(void)
{
	setServerAddr("127.0.0.1:10080");
	setBrokerAddr("127.0.0.1:1883");
	loop();
	return 0;
}
