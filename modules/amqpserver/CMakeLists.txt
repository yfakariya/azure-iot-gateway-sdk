#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

# this is CMakeLists for the amqpserver
cmake_minimum_required(VERSION 2.8.11)

set(amqpserver_sources
	./src/amqpserver.c
)

set(amqpserver_headers
	./inc/amqpserver.h
)

include_directories(./inc)
include_directories(${GW_INC})
include_directories(${UAMQP_INC_FOLDER})

add_library(amqpserver MODULE ${amqpserver_sources} ${amqpserver_headers})

linkSharedUtil(amqpserver)

if (WIN32)
	#windows needs this define
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	target_link_libraries(amqpserver gateway
		uamqp
		aziotsharedutil
		ws2_32
		secur32)

else()
	target_link_libraries(amqpserver gateway uamqp aziotsharedutil ssl crypto)
endif()
