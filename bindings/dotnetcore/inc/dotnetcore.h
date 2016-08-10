// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef DOTNETCORE_H
#define DOTNETCORE_H

#include "module.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct DOTNETCORE_HOST_CONFIG_TAG
{
    const char* dotnetcore_module_path;
    const char* dotnetcore_module_entry_class;
    const char* dotnetcore_module_args;
    const char* core_clr_path;
}DOTNETCORE_HOST_CONFIG;

MODULE_EXPORT const MODULE_APIS* MODULE_STATIC_GETAPIS(DOTNETCORE_HOST)(void);

extern __declspec(dllexport) bool Module_DotNetCoreHost_PublishMessage(MESSAGE_BUS_HANDLE bus, MODULE_HANDLE sourceModule, const unsigned char* source, int32_t size);

#ifdef __cplusplus
}
#endif

// TODO: Windows
#ifdef (__APPLE__)
#define CORE_CLR_DLL = "libcoreclr.dylib"
#else
#define CORE_CLR_DLL = "libcoreclr.so"
#endif // __APPLE__
#endif /*DOTNETCORE_H*/