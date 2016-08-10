// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "azure_c_shared_utility/gballoc.h"

#include "module.h"
#include "azure_c_shared_utility/iot_logging.h"
#include <stdio.h>
#include "dotnetcore.h"
#include "dotnetcore_hl.h"

#include "parson.h"

static MODULE_HANDLE DotNETCore_HL_Create(MESSAGE_BUS_HANDLE busHandle, const void* configuration)
{
	MODULE_HANDLE result;
	/* Codes_SRS_DOTNET_HL_04_001: [ If busHandle is NULL then DotNET_HL_Create shall fail and return NULL. ]  */
	/* Codes_SRS_DOTNET_HL_04_002: [ If configuration is NULL then DotNET_HL_Create shall fail and return NULL. ] */
	if (
		(busHandle == NULL) ||
		(configuration == NULL)
		)
	{
		LogError("NULL parameter detected bus=%p configuration=%p", busHandle, configuration);
		result = NULL;
	}
	else
	{
		JSON_Value* json = json_parse_string((const char*)configuration);
		if (json == NULL)
		{
			/* Codes_SRS_DOTNET_HL_04_003: [If configuration is not a JSON object, then DotNET_HL_Create shall fail and return NULL.] */
			LogError("unable to json_parse_string");
			result = NULL;
		}
		else
		{
			JSON_Object* root = json_value_get_object(json);
			if (root == NULL)
			{
				/* Codes_SRS_DOTNET_HL_04_003: [If configuration is not a JSON object, then DotNET_HL_Create shall fail and return NULL.] */
				LogError("unable to json_value_get_object");
				result = NULL;
			}
			else
			{
				const char* dotnetcore_module_path_string = json_object_get_string(root, "dotnetcore_module_path");
				if (dotnetcore_module_path_string == NULL)
				{
					/* Codes_SRS_DOTNET_HL_04_004: [ If the JSON object does not contain a value named "dotnet_module_path" then DotNET_HL_Create shall fail and return NULL. ] */
					LogError("json_object_get_string failed for property 'dotnet_module_path'");
					result = NULL;
				}
				else
				{
					const char* dotnetcore_module_entry_class_string = json_object_get_string(root, "dotnetcore_module_entry_class");
					if (dotnetcore_module_entry_class_string == NULL)
					{
						/* Codes_SRS_DOTNET_HL_04_005: [If the JSON object does not contain a value named "dotnet_module_entry_class" then DotNET_HL_Create shall fail and return NULL.] */
						LogError("json_object_get_string failed for property 'dotnet_module_entry_class'");
						result = NULL;
					}
					else
					{
						const char* dotnetcore_module_args_string = json_object_get_string(root, "dotnetcore_module_args");
						if (dotnetcore_module_args_string == NULL)
						{
							/* Codes_SRS_DOTNET_HL_04_006: [ If the JSON object does not contain a value named "dotnet_module_args" then DotNET_HL_Create shall fail and return NULL. ] */
							LogError("json_object_get_string failed for property 'dotnet_module_args'");
							result = NULL;
						}
						else
						{
								DOTNETCORE_HOST_CONFIG dotNet_config;
								dotNetCore_config.dotnetcore_module_path = dotnetcore_module_path_string;
								dotNetCore_config.dotnetcore_module_entry_class = dotnetcore_module_entry_class_string;
								dotNetCore_config.dotnetcore_module_args = dotnetcore_module_args_string;

								/* Codes_SRS_DOTNET_HL_04_007: [ DotNET_HL_Create shall pass busHandle and const char* configuration (dotnet_module_path, dotnet_module_entry_class and dotnet_module_args as string) to DotNET_Create. ] */
								result = MODULE_STATIC_GETAPIS(DOTNET_HOST)()->Module_Create(busHandle, (const void*)&dotNetCore_config);

								/* Codes_SRS_DOTNET_HL_04_008: [ If DotNET_Create succeeds then DotNET_HL_Create shall succeed and return a non-NULL value. ] */
								if (result == NULL)
								{
									LogError("DotNET_Host_Create failed.");
									/* Codes_SRS_DOTNET_HL_04_009: [ If DotNET_Create fails then DotNET_HL_Create shall fail and return NULL. ] */
								}
						}
					}
				}
			}
		}
	}

    return result;
}

static void DotNETCore_HL_Destroy(MODULE_HANDLE module)
{
	if (module != NULL)
	{
		/* Codes_SRS_DOTNET_HL_04_011: [ DotNET_HL_Destroy shall destroy all used resources for the associated module. ] */
		MODULE_STATIC_GETAPIS(DOTNETCORE_HOST)()->Module_Destroy(module);
	}
	else
	{
		/* Codes_SRS_DOTNET_HL_04_013: [ DotNET_HL_Destroy shall do nothing if module is NULL. ] */
		LogError("'module' is NULL");
	}
}

static void DotNETCore_HL_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{

	/* Codes_SRS_DOTNET_HL_04_010: [ DotNET_HL_Receive shall pass the received parameters to the underlying DotNET Host Module's _Receive function. ] */
    MODULE_STATIC_GETAPIS(DOTNETCORE_HOST)()->Module_Receive(moduleHandle, messageHandle);
}

static const MODULE_APIS DotNETCore_HL_APIS_all =
{
	DotNETCORE_HL_Create,
	DotNETCORE_HL_Destroy,
	DotNETCORE_HL_Receive
};

#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT const MODULE_APIS* MODULE_STATIC_GETAPIS(DOTNETCORE_HOST_HL)(void)
#else
MODULE_EXPORT const MODULE_APIS* Module_GetAPIS(void)
#endif
{
	/* Codes_SRS_DOTNET_HL_04_012: [ Module_GetAPIS shall return a non-NULL pointer to a structure of type MODULE_APIS that has all fields non-NULL. ] */
	return &DotNETCORE_HL_APIS_all;
}
