// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>

#include "messageproperties.h"
#include "gateway_ll.h"
#include "identitymap.h"
#include "iothubhttp.h"
#include "azure_c_shared_utility/iot_logging.h"
#include "azure_c_shared_utility/vector.h"
#include "logger.h"
#include "amqpserver.h"

extern void opcpocGatewayModulePaths(GATEWAY_PROPERTIES_ENTRY modules[6]);

int main(void)
{
 	GATEWAY_HANDLE opcpocGateway;
    GATEWAY_PROPERTIES opcpocGatewayProperties;

    /* Setup: data for IoT Hub Module */
	IOTHUBHTTP_CONFIG iotHubConfig;
	iotHubConfig.IoTHubName = "<<insert here IoTHubName>>";
	iotHubConfig.IoTHubSuffix = "<<insert here IoTHubSuffix>>";

	/* Setup: data for the identity map module */
	IDENTITY_MAP_CONFIG bleMapping[2];
	bleMapping[0].deviceId = "<<insert here deviceId>>";
	bleMapping[0].deviceKey = "<<insert here deviceKey>>";
	bleMapping[0].macAddress = "01:01:01:01:01:01";
	bleMapping[1].deviceId = "<<insert here deviceId>>";
	bleMapping[1].deviceKey = "<<insert here deviceKey>>";
	bleMapping[1].macAddress = "02:02:02:02:02:02";

    /* Setup: We convert 2 endpoints into device messages */
    AMQPSERVER_CONFIG amqpMapping[2];
    amqpMapping[0].endpoint = "<<insert here topic://TopicName>>";
    amqpMapping[0].deviceId = "<<insert here deviceId>>";
    amqpMapping[0].deviceKey = "<<insert here deviceKey>>";
    amqpMapping[1].endpoint = "<<insert here topic://TopicName>>";
    amqpMapping[1].deviceId = "<<insert here deviceId>>";
    amqpMapping[1].deviceKey = "<<insert here deviceKey>>";


    /* Setup: compile this into a GatewayProperties */
	GATEWAY_PROPERTIES_ENTRY modules[6];
    opcpocGatewayModulePaths(modules);

	/* IoT Hub module */
	modules[0].module_configuration = &iotHubConfig;
	modules[0].module_name = "IoTHub";
	/* mapping module */
	modules[1].module_name = GW_IDMAP_MODULE;
	/* configuration is a vector created below */
	/* BLE 1 module */
	modules[2].module_name = "BLE1";
	modules[2].module_configuration = "01:01:01:01:01:01";
	/* BLE 2 module */
	modules[3].module_name = "BLE2";
	modules[3].module_configuration = "02:02:02:02:02:02";
	/* logger module */
	modules[4].module_name = "Logging";
	LOGGER_CONFIG loggingConfig;
	loggingConfig.selector = LOGGING_TO_FILE;
	loggingConfig.selectee.loggerConfigFile.name = "opcpocgw.log";
	modules[4].module_configuration = &loggingConfig;
    /* AMQP server module */
    /* configuration is a vector created now */
    modules[5].module_name = "AMQP";

    VECTOR_HANDLE amqpMappingVector = VECTOR_create(sizeof(AMQPSERVER_CONFIG));
    if (amqpMappingVector == NULL)
    {
        LogError("Could not create vector for identity mapping configuration.");
    }
    else
    {
        if (VECTOR_push_back(amqpMappingVector, &amqpMapping, 2) != 0)
        {
            LogError("Could not push data into vector for identity mapping configuration.");
        }
        else
        {
            modules[5].module_configuration = amqpMappingVector;

	        VECTOR_HANDLE bleMappingVector = VECTOR_create(sizeof(IDENTITY_MAP_CONFIG));
	        if (bleMappingVector == NULL)
	        {
		        LogError("Could not create vector for identity mapping configuration.");
	        }
	        else
	        {
		        if (VECTOR_push_back(bleMappingVector, &bleMapping, 2) != 0)
		        {
			        LogError("Could not push data into vector for identity mapping configuration.");
		        }
		        else
		        {
			        modules[1].module_configuration = bleMappingVector;
			        VECTOR_HANDLE gatewayProps = VECTOR_create(sizeof(GATEWAY_PROPERTIES_ENTRY));
			        if (gatewayProps == NULL)
			        {
				        LogError("Could not create gateway properties vector.");
			        }
			        else
			        {
				        if (VECTOR_push_back(gatewayProps, &modules, 6) != 0)
				        {
					        LogError("Could not push data into gateway properties vector");
				        }
				        else
				        {
					        /* Startup: Create gateway using LL libraries and setup data */
                            opcpocGatewayProperties.gateway_properties_entries = gatewayProps;
                            opcpocGateway = Gateway_LL_Create(&opcpocGatewayProperties);
					        if (opcpocGateway == NULL)
					        {
						        LogError("Failed to create gateway");
					        }
					        else
					        {
						        LogInfo("Gateway started");
						        /* Wait for user input to quit. */
						        (void)printf("Press any key to exit the application. \r\n");
						        (void)getchar();

						        Gateway_LL_Destroy(opcpocGateway);
					        }

				        }
				        VECTOR_destroy(gatewayProps);
			        }
		        }
		        VECTOR_destroy(bleMappingVector);
	        }
        }
        VECTOR_destroy(amqpMappingVector);
    }
    return 0;
}
