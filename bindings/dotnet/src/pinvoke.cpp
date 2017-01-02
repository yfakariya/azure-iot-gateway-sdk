#include "pinvoke.h"

// Gateway_CreateFromJson
MODULE_EXPORT GATEWAY_HANDLE Module_DotNetHost_CreateGatewayFromJson(const char* file_path)
{
    return Gateway_CreateFromJson(file_path);
}

// Gateway_Destroy
MODULE_EXPORT void Module_DotNetHost_DestroyGateway(GATEWAY_HANDLE gw)
{
    return Gateway_Destroy(gw);
}

// Gateway_Create
MODULE_EXPORT GATEWAY_HANDLE Module_DotNetHost_CreateGateway(const GATEWAY_PROPERTIES* properties)
{
    return Gateway_Create(properties);
}

// Gateway_Start
MODULE_EXPORT GATEWAY_START_RESULT Module_DotNetHost_StartGateway(GATEWAY_HANDLE gw)
{
    return Gateway_Start(gw);
}

// ModuleLoader_InitializeFromJson
MODULE_EXPORT MODULE_LOADER_RESULT Module_DotNetHost_InitializeModuleLoaderFromJson(const JSON_Value* loaders)
{
    return ModuleLoader_InitializeFromJson(loaders);
}

// ModuleLoader_Initialize
MODULE_EXPORT MODULE_LOADER_RESULT Module_DotNetHost_InitializeModuleLoader()
{
    return ModuleLoader_Initialize();
}

// ModuleLoader_Destroy
MODULE_EXPORT void Module_DotNetHost_DestroyModuleLoaders()
{
    return ModuleLoader_Destroy();
}

// ModuleLoader_FindByName
MODULE_EXPORT MODULE_LOADER* Module_DotNetHost_FindModuleLoaderByName(const char* name)
{
    return ModuleLoader_FindByName(name);
}

// VECTOR_create
MODULE_EXPORT VECTOR_HANDLE Module_DotNetHost_CreateVector(size_t elementSize)
{
    return VECTOR_create(elementSize);
}

// VECTOR_destroy
MODULE_EXPORT void Module_DotNetHost_DestroyVector(VECTOR_HANDLE handle)
{
    return VECTOR_destroy(handle);
}

// VECTOR_push_back
MODULE_EXPORT int Module_DotNetHost_PushBackVector(VECTOR_HANDLE handle, const void* elements, size_t numElements)
{
    return VECTOR_push_back(handle, elements, numElements);
}

// json_parse_string
MODULE_EXPORT JSON_Value* Module_DotNetHost_ParseJsonString(const char* string)
{
    return json_parse_string(string);
}

// json_value_free
MODULE_EXPORT void Module_DotNetHost_FreeJsonValue(JSON_Value* value)
{
    return json_value_free(value);
}