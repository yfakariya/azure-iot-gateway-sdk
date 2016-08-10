#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(dotnetcore_sources
	./src/dotnetcore.cpp
)

set(dotnetcore_headers
	./inc/dotnetcore.h
)

set(dotnetcore_static_sources
	${dotnetcore_sources}
)

set(dotnetcore_static_headers
	${dotnetcore_headers}
)

set(dotnetcore_hl_sources
	./src/dotnetcore_hl.c
)

set(dotnetcore_hl_headers
	./inc/dotnetcore_hl.h
)

set(dotnetcore_hl_static_sources
	${dotnetcore_hl_sources}
)

set(dotnetcore_hl_static_headers
	${dotnetcore_hl_headers}
)


include_directories(./inc)
include_directories(${GW_INC})
include_directories(${IOTHUB_CLIENT_INC_FOLDER})

#this builds the dotnetcore dynamic library
add_library(dotnetcore MODULE ${dotnetcore_sources}  ${dotnetcore_headers})
target_link_libraries(dotnetcore gateway mscoree)

#this builds the dotnetcore static library
add_library(dotnetcore_static  ${dotnetcore_static_sources} ${dotnetcore_static_headers})
target_compile_definitions(dotnetcore_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(dotnetcore_static gateway mscoree)

#this builds the dotnetcore dynamic library (by default it uses dotnetcore module linked statically)
add_library(dotnetcore_hl MODULE ${dotnetcore_hl_sources} ${dotnetcore_hl_headers})
target_link_libraries(dotnetcore_hl dotnetcore_static gateway mscoree)

#this builds the dotnetcore static library (by default it uses dotnetcore module linked statically)
add_library(dotnetcore_hl_static ${dotnetcore_hl_static_sources} ${dotnetcore_hl_static_headers})
target_compile_definitions(dotnetcore_hl_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(dotnetcore_hl_static dotnetcore_static gateway mscoree)

linkSharedUtil(dotnetcore)
linkSharedUtil(dotnetcore_static)
linkSharedUtil(dotnetcore_hl)
linkSharedUtil(dotnetcore_hl_static)

add_subdirectory(tests)

add_module_to_solution(dotnetcore)