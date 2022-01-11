target := main

CC := gcc
CFLAGS += -Iinc -g -O0 -Wall -Werror -fPIC
LDFLAGS += -lpthread -lev -lmosquitto

src := \
	src/main.c \
	src/loop.c \
	src/mqttHandler.c \
	src/serverHandler.c \
	src/uart.c

obj := $(patsubst %.c, %.o, $(src))

$(target):$(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o:%c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY:clean
clean:
	@rm -rf $(target) $(obj)
