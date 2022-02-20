cmake_minimum_required(VERSION 3.18)
project(MsgHandler VERSION 1.0)
aux_source_directory(src SRC_LISTS)
add_executable(main
	src/loop.c
	src/main.c
	src/mqttHandler.c
	src/uart.c
	src/serverHandler.c)
include_directories(main inc)
add_compile_options(main -g -O2 -Wall -fPIC)

target_link_libraries(main ev mosquitto pthread)
