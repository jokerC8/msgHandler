#ifndef __LOOP_H__
#define __LOOP_H__

void setServerAddr(const char* addr);
void setBrokerAddr(const char* addr);
void setMsgQueueLen(int len);

void loop();

#endif
