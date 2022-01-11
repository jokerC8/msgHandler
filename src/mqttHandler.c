#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include <mosquitto.h>

#include "common.h"
#include "kfifo.h"

#define USERNAME "blackcat"
#define PASSWORD "2**5=32"
#define KEEPALIVE 30
#define TOPIC "/parent/child/topic"

void onSubscribe(struct mosquitto* object, void* data, int mid, int qos_count, const int* granted_qos)
{
	LOGD("onSubscribe\n");
}

void onConnect(struct mosquitto* object, void* data, int rc)
{
	LOGD("onConnect\n");
}

void onMessage(struct mosquitto* object, void* data, const struct mosquitto_message* message)
{
	char buf[CMDLEN] = {0};
	tsThreadParams* param = (tsThreadParams*)data;

	if (message->payloadlen >= CMDLEN) {
		LOGE("message is too long\n");
		return;
	}

	kfifo_put(param->queue, message->payload, message->payloadlen);
	kfifo_put(param->queue, (uint8_t*)buf, CMDLEN - message->payloadlen);
	uint64_t val = 1;
	write(param->eventfd, &val, sizeof(val));
}

void* mqttHandler(void* args)
{
	struct mosquitto* object;
	tsThreadParams* param = (tsThreadParams*)args;

	LOGD("start mqttHandler thread, broker:%s:%d\n", param->addr, param->port);
	pthread_detach(pthread_self());

	mosquitto_lib_init();

retry:
	object = mosquitto_new(NULL, true, param);
#ifdef ENABLE_USERNAME_PASSWORD
	mosquitto_username_pw_set(object, USERNAME, PASSWORD);
#endif

	if (mosquitto_connect(object, param->addr, param->port, KEEPALIVE) != MOSQ_ERR_SUCCESS) {
		poll(0, 0, 3000);
		mosquitto_destroy(object);
		goto retry;
	}
	mosquitto_connect_callback_set(object, onConnect);

	if ((mosquitto_subscribe(object, NULL, TOPIC, 1)) != MOSQ_ERR_SUCCESS) {
		mosquitto_destroy(object);
		goto retry;
	}
	mosquitto_subscribe_callback_set(object, onSubscribe);

	mosquitto_message_callback_set(object, onMessage);

	mosquitto_loop_forever(object, 3000, 1);

	mosquitto_destroy(object);
	mosquitto_lib_cleanup();
	return 0;
}
