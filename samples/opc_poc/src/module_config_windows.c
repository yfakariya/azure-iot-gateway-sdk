// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "gateway_ll.h"

void opcpocGatewayModulePaths(GATEWAY_PROPERTIES_ENTRY modules[6])
{
#ifdef _DEBUG
	/* IoT Hub module */
	modules[0].module_path = "..\\..\\modules\\iothubhttp\\Debug\\iothubhttp.dll";
	/* Identity map module */
	modules[1].module_path = "..\\..\\modules\\identitymap\\Debug\\identity_map.dll";
	/* BLE 1 module */
	modules[2].module_path = "..\\..\\modules\\simulated_device\\Debug\\simulated_device.dll";
	/* BLE 2 module */
	modules[3].module_path = "..\\..\\modules\\simulated_device\\Debug\\simulated_device.dll";
    /* logger module */
    modules[4].module_path = "..\\..\\modules\\logger\\Debug\\logglogger.dll";
    /* Endpoint 1 */
    modules[5].module_path = "..\\..\\modules\\amqpserver\\Debug\\amqpserver.dll";
#else
    /* IoT Hub module */
    modules[0].module_path = "iothubhttp.dll";
    /* Identity map module */
    modules[1].module_path = "identity_map.dll";
    /* BLE 1 module */
    modules[2].module_path = "simulated_device.dll";
    /* BLE 2 module */
    modules[3].module_path = "simulated_device.dll";
    /* logger module */
    modules[4].module_path = "logger.dll";
    /* Endpoint 1 */
    modules[5].module_path = "amqpserver.dll";
#endif
}
