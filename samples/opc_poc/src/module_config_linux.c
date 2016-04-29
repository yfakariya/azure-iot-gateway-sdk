// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "gateway_ll.h"

void opcpocGatewayModulePaths(GATEWAY_PROPERTIES_ENTRY modules[6])
{
	/* IoT Hub module */
	modules[0].module_path = "../../modules/iothubhttp/libiothubhttp.so";
	/* Identity map module */
	modules[1].module_path = "../../modules/identitymap/libidentity_map.so";
	/* BLE 1 module */
	modules[2].module_path = "../../modules/simulated_device/libsimulated_device.so";
	/* BLE 2 module */
	modules[3].module_path = "../../modules/simulated_device/libsimulated_device.so";
	/* logger module */
	modules[4].module_path = "../../modules/logger/liblogger.so";
    /* Endpoint 1 */
    modules[5].module_path = "../../modules/amqpserver/amqpserver.so";
}
