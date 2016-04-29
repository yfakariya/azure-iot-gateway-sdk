#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
#this is CMakeLists for the opc_poc sample

set(opc_poc_c_sources
	./src/main.c
)

#setting the dynamic_loader file based on OS that it is used
if(WIN32)
	set(modules_c_file ./src/module_config_windows.c)
elseif(LINUX)
    set(modules_c_file ./src/module_config_linux.c)
endif()

include_directories(../inc ${IOTHUB_CLIENT_INC_FOLDER})
include_directories(${GW_INC})
include_directories(../../modules/common)

#link modules
linkModule(iothubhttp)
linkModule(identitymap)
linkModule(amqpserver)

add_executable(opc_gw_poc ${opc_poc_c_sources} ${modules_c_file})
target_link_libraries(opc_gw_poc gateway)
linkSharedUtil(opc_gw_poc)
linkHttp(opc_gw_poc)


