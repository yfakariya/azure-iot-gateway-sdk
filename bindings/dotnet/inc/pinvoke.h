#ifndef DOTNET_PINVOKE_H
#define DOTNET_PINVOKE_H

#include <parson.h>
#include "azure_c_shared_utility/vector.h"
#include "module.h"
#include "module_loader.h"
#include "gateway.h"

#ifdef __cplusplus
extern "C"
{
#endif

// APIs for P/Invoke to enable Gateway initialization from managed code.

// Gateway_CreateFromJson
/** @brief      Creates a gateway using a JSON configuration file as input
 *              which describes each module. Each module described in the
 *              configuration must support Module_CreateFromJson.
 *
 * @param       file_path   Path to the JSON configuration file for this
 *                          gateway.
 *
 *              Sample JSON configuration file:
 *
 *              {
 *                  "modules" :
 *                  [
 *                      {
                            "name": "sensor",
                            "loader": {
                                "name": "dotnet",
                                "entrypoint": {
                                    "class.name": "Microsoft.Azure.Gateway.SensorModule",
                                    "assembly.path": "./bin/Microsoft.Azure.Gateway.Modules.dll"
                                }
                            },
                            "args" : {
                                "power.level": 5
                            }
                        },
                        {
                            "name": "logger",
                            "loader": {
                                "name": "native",
                                "entrypoint": {
                                    "module.path": "./bin/liblogger.so"
                                }
                            },
                            "args": {
                                "filename": "/var/logs/gateway-log.json"
                            }
                        }
 *                  ],
 *                  "links":
 *                  [
 *                      {
 *                          "source": "sensor",
 *                          "sink": "logger"
 *                      }
 *                  ]
 *              }
 *
 * @return      A non-NULL #GATEWAY_HANDLE that can be used to manage the
 *              gateway or @c NULL on failure.
 */
extern MODULE_EXPORT GATEWAY_HANDLE Module_DotNetHost_CreateGatewayFromJson(const char* file_path);

// Gateway_Destroy
/** @brief      Destroys the gateway and disposes of all associated data.
 *
 *  @param      gw      #GATEWAY_HANDLE to be destroyed.
 */
extern MODULE_EXPORT void Module_DotNetHost_DestroyGateway(GATEWAY_HANDLE gw);

// Gateway_Create
/** @brief      Creates a new gateway using the provided #GATEWAY_PROPERTIES.
 *
 *  @param      properties      #GATEWAY_PROPERTIES structure containing
 *                              specific module properties and information.
 *
 *  @return     A non-NULL #GATEWAY_HANDLE that can be used to manage the
 *              gateway or @c NULL on failure.
 */
extern MODULE_EXPORT GATEWAY_HANDLE Module_DotNetHost_CreateGateway(const GATEWAY_PROPERTIES* properties);

// Gateway_Start
/** @brief      Tell the Gateway it's ready to start.
 *
 *  @param      gw      #GATEWAY_HANDLE to be destroyed.
 *
 *  @return     A #GATEWAY_START_RESULT to report the result of the start
 */
extern MODULE_EXPORT GATEWAY_START_RESULT Module_DotNetHost_StartGateway(GATEWAY_HANDLE gw);

// ModuleLoader_InitializeFromJson
/**
 * @brief Updates the global loaders array from a JSON that looks like this:
 *
 *   "loaders": [
 *       {
 *           "type": "node",
 *           "name": "node",
 *           "configuration": {
 *               "binding.path": "./bin/libnode_binding.so"
 *           }
 *       },
 *       {
 *           "type": "java",
 *           "name": "java",
 *           "configuration": {
 *               "jvm.options": {
 *                   "memory": 1073741824
 *               },
 *               "gateway.class.path": "./bin/gateway.jar",
 *               "binding.path": "./bin/libjava_binding.so"
 *           }
 *       },
 *       {
 *           "type": "dotnet",
 *           "name": "dotnet",
 *           "configuration": {
 *               "binding.path": "./bin/libdotnet_binding.so"
 *           }
 *       }
 *   ]
 */
extern MODULE_EXPORT MODULE_LOADER_RESULT Module_DotNetHost_InitializeModuleLoaderFromJson(const JSON_Value* loaders);

// ModuleLoader_Initialize
/**
 * @brief This function creates the default set of module loaders that the
 *        gateway supports.
 */
extern MODULE_EXPORT MODULE_LOADER_RESULT Module_DotNetHost_InitializeModuleLoader();

// ModuleLoader_Destroy
/**
 * @brief This function frees resources allocated for tracking module loaders.
 */
extern MODULE_EXPORT void Module_DotNetHost_DestroyModuleLoaders();

// ModuleLoader_FindByName
/**
 * @brief Searches the module loader collection given the loader's name.
 */
extern MODULE_EXPORT MODULE_LOADER* Module_DotNetHost_FindModuleLoaderByName(const char* name);

// VECTOR_create
extern MODULE_EXPORT VECTOR_HANDLE Module_DotNetHost_CreateVector(size_t elementSize);

// VECTOR_destroy
extern MODULE_EXPORT void Module_DotNetHost_DestroyVector(VECTOR_HANDLE handle);

// VECTOR_push_back
extern MODULE_EXPORT int Module_DotNetHost_PushBackVector(VECTOR_HANDLE handle, const void* elements, size_t numElements);

// json_parse_string
/*  Parses first JSON value in a string, returns NULL in case of error */
extern MODULE_EXPORT JSON_Value* Module_DotNetHost_ParseJsonString(const char* string);

// json_value_free
extern MODULE_EXPORT void Module_DotNetHost_FreeJsonValue(JSON_Value* value);

#ifdef __cplusplus
}
#endif

#endif // DOTNET_PINVOKE_H