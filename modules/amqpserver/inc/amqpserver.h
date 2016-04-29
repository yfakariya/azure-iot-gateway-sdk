// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AMQPSERVER_H
#define AMQPSERVER_H

#include "module.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct AMQPSERVER_CONFIG_TAG
{
    const char* endpoint;
    const char* deviceId;
    const char* deviceKey;
}
AMQPSERVER_CONFIG; /*this needs to be passed to the Module_Create function*/

#ifdef __cplusplus
}
#endif

#endif /*AMQPSERVER_H*/
