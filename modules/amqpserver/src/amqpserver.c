// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#define __STDC_FORMAT_MACROS

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>

#include "azure_c_shared_utility/gballoc.h"

#include "azure_c_shared_utility/gb_stdio.h"
#include "azure_c_shared_utility/gb_time.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/iot_logging.h"
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/vector.h"
#include "azure_c_shared_utility/crt_abstractions.h"

// 2 MESSAGE_HANDLE:  Consider renaming in original message.h 
#define MESSAGE_HANDLE GW_MESSAGE_HANDLE
#include "message.h"
#include "module.h"
#include "message_bus.h"
#include "messageproperties.h"
#include "amqpserver.h"
#undef MESSAGE_HANDLE
#undef MESSAGE_H

#define MESSAGE_HANDLE UAMQP_MESSAGE_HANDLE
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/socket_listener.h"
#include "azure_uamqp_c/header_detect_io.h"
#include "azure_uamqp_c/consolelogger.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/link.h"
#undef MESSAGE_HANDLE

typedef struct AMQPSERVER_DATA_TAG
{
    MESSAGE_BUS_HANDLE bus;
    bool amqpServerRunning;
    THREAD_HANDLE amqpServerthread;

    DLIST_ENTRY connections;
    VECTOR_HANDLE mappings;

} AMQPSERVER_DATA;

typedef struct AMQPSERVER_CONNECTION_DATA_TAG
{
    AMQPSERVER_DATA* module_data;
    CONNECTION_HANDLE connection;
    DLIST_ENTRY link;
    bool closed;
}
AMQPSERVER_CONNECTION_DATA;

typedef struct AMQPSERVER_SESSION_DATA_TAG
{
    AMQPSERVER_CONNECTION_DATA* connection_data;
    SESSION_HANDLE session;
}
AMQPSERVER_SESSION_DATA;

typedef struct AMQPSERVER_LINK_DATA_TAG
{
    AMQPSERVER_SESSION_DATA* session_data;
    LINK_HANDLE link;
    MESSAGE_RECEIVER_HANDLE message_receiver;
    STRING_HANDLE device_id;
    STRING_HANDLE device_key;
}
AMQPSERVER_LINK_DATA;

typedef struct AMQPSERVER_MAPPING_TAG
{
    STRING_HANDLE		endpoint;
    STRING_HANDLE       device_id;
    STRING_HANDLE       device_key;
}
AMQPSERVER_MAPPING;

static void FreeLinkData(AMQPSERVER_LINK_DATA* link_data)
{
    if (link_data != NULL)
    {
        if (link_data->message_receiver)
        {
            messagereceiver_destroy(link_data->message_receiver);
            link_data->message_receiver = NULL;
        }
        else if (link_data->link)
        {
            link_destroy(link_data->link);
            link_data->link = NULL;
        }
        if (link_data->device_id)
        {
            STRING_delete(link_data->device_id);
            link_data->device_id = NULL;
        }
        if (link_data->device_key)
        {
            STRING_delete(link_data->device_key);
            link_data->device_key = NULL;
        }
        free(link_data);
    }
}

static void FreeSessionData(AMQPSERVER_SESSION_DATA* session_data)
{
    if (session_data != NULL)
    {
        if (session_data->session)
        {
            session_destroy(session_data->session);
            session_data->session = NULL;
        }
        free(session_data);
    }
}

static void FreeConnectionData(AMQPSERVER_CONNECTION_DATA* connection_data)
{
    if (connection_data != NULL)
    {
        if (connection_data->connection)
        {
            connection_destroy(connection_data->connection);
            connection_data->connection = NULL;

            LogInfo("AMQP Client disconnected.");
        }
        free(connection_data);
    }
}

static void FreeMappingContent(AMQPSERVER_MAPPING* mapping)
{
    if (mapping->device_id)
    {
        STRING_delete(mapping->device_id);
        mapping->device_id = NULL;
    }
    if (mapping->device_key)
    {
        STRING_delete(mapping->device_key);
        mapping->device_key = NULL;
    }
    if (mapping->endpoint)
    {
        STRING_delete(mapping->endpoint);
        mapping->endpoint = NULL;
    }
}

static void FreeMappingVector(VECTOR_HANDLE mappings)
{
    if (mappings != NULL)
    {
        for (size_t i = 0; i < VECTOR_size(mappings); i++)
        {
            FreeMappingContent((AMQPSERVER_MAPPING*)VECTOR_element(mappings, i));
        }
        VECTOR_destroy(mappings);
    }
}

static void FreeModuleData(AMQPSERVER_DATA* module_data)
{
    if (module_data != NULL)
    {
        if (module_data->mappings)
        {
            for (size_t i = 0; i < VECTOR_size(module_data->mappings); i++)
            {
                FreeMappingContent((AMQPSERVER_MAPPING*)VECTOR_element(module_data->mappings, i));
            }
            VECTOR_destroy(module_data->mappings);
        }
        free(module_data);
    }
}

static VECTOR_HANDLE CreateMappingVector(VECTOR_HANDLE mappingConfiguration)
{
    AMQPSERVER_MAPPING entry;
    AMQPSERVER_CONFIG* config;
    VECTOR_HANDLE result = VECTOR_create(sizeof(AMQPSERVER_MAPPING));
    if (result == NULL)
    {
        LogError("Failed to allocate vector.");
    }
    else
    {
        for (size_t i = 0; i < VECTOR_size(mappingConfiguration); i++)
        {
            config = (AMQPSERVER_CONFIG*)VECTOR_element(mappingConfiguration, i);
            if (config == NULL)
            {
                /* This could theoretically only happen if index 
                   out of bounds or mappingConfiguration is NULL, but better safe than sorry */
                return NULL;
            }

            entry.device_id = STRING_construct(config->deviceId);
            entry.device_key = STRING_construct(config->deviceKey);
            entry.endpoint = STRING_construct(config->endpoint);

            if (entry.device_id && entry.device_key && entry.endpoint &&
                0 == VECTOR_push_back(result, &entry, 1))
            {
                continue;
            }

            LogError("Failed to allocate vector entry.");
            FreeMappingContent(&entry);
            FreeMappingVector(result);
            return NULL;
        }
    }
    return result;
}

static void on_message_receiver_state_changed(const void* context, MESSAGE_RECEIVER_STATE new_state, MESSAGE_RECEIVER_STATE previous_state)
{
    AMQPSERVER_LINK_DATA* link_data = (AMQPSERVER_LINK_DATA*)context;

    if (new_state == previous_state)
    {
        /* Nothing to do */
    }

    else if (new_state == MESSAGE_RECEIVER_STATE_ERROR && !link_data->session_data->connection_data->closed)
    {
        LogError("Link error, closing connection.");
        link_data->session_data->connection_data->closed = true;
    }

    else if (new_state == MESSAGE_RECEIVER_STATE_IDLE && !link_data->session_data->connection_data->closed)
    {
        /* 
          If we do not close ourselves, and going idle from open, then 
          underlying socket was disconnected, clean up connection as result 
        */
        link_data->session_data->connection_data->closed = true;
    }
}

static bool CopyProperties(UAMQP_MESSAGE_HANDLE message, MAP_HANDLE properties)
{
    bool result = false;
    uint32_t count;
    const char* name;
    const char* string_value;
    uint64_t value_buffer = 0;
    char buffer[64];
    AMQP_TYPE type;
    AMQP_VALUE amqpvalue, key = NULL, value = NULL;

    // Parse content
    if (0 != message_get_application_properties(message, &amqpvalue))
    {
        LogError("Failure accessing application properties.");
    }
    else
    {
        amqpvalue = amqpvalue_get_inplace_described_value(amqpvalue);
        if (amqpvalue == NULL)
        {
            LogError("Failure accessing application properties.");
        }
        else if (0 != amqpvalue_get_map_pair_count(amqpvalue, &count))
        {
            LogError("Couldnt get number of properties.");
        }
        else
        {
            result = true;

            // Set message properties from properties
            for (uint32_t index = 0; index < count; index++)
            {
                value_buffer = 0;

                if (key)
                {
                    amqpvalue_destroy(key);
                }
                if (value)
                {
                    amqpvalue_destroy(value);
                }

                if (0 != amqpvalue_get_map_key_value_pair(amqpvalue, index, &key, &value) ||
                    0 != amqpvalue_get_string(key, &name))
                {
                    LogError("Failure accessing key value pair at index %d.", index);
                    result = false;
                    break;
                }

                type = amqpvalue_get_type(value);
                if (type == AMQP_TYPE_STRING)
                {
                    amqpvalue_get_string(value, &string_value);
                    Map_Add(properties, name, string_value);
                    continue;
                }

                if (type == AMQP_TYPE_CHAR)
                {
                    amqpvalue_get_char(value, (uint32_t*)&value_buffer);
                    sprintf(buffer, "'%C'", (uint32_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_UBYTE)
                {
                    amqpvalue_get_ubyte(value, (unsigned char*)&value_buffer);
                    sprintf(buffer, "%" PRIu8, (uint8_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_USHORT)
                {
                    amqpvalue_get_ushort(value, (uint16_t*)&value_buffer);
                    sprintf(buffer, "%" PRIu16, (uint16_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }
                
                if (type == AMQP_TYPE_UINT)
                {
                    amqpvalue_get_uint(value, (uint32_t*)&value_buffer);
                    sprintf(buffer, "%" PRIu32, (uint32_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }
                
                if (type == AMQP_TYPE_ULONG)
                {
                    amqpvalue_get_ulong(value, &value_buffer);
                    sprintf(buffer, "%" PRIu64, value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_BYTE)
                {
                    amqpvalue_get_byte(value, (char*)&value_buffer);
                    sprintf(buffer, "%" PRIi8, (int8_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_SHORT)
                {
                    amqpvalue_get_short(value, (int16_t*)&value_buffer);
                    sprintf(buffer, "%" PRIi16, (int16_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_INT)
                {
                    amqpvalue_get_int(value, (int32_t*)&value_buffer);
                    sprintf(buffer, "%" PRIi32, (int32_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_LONG)
                {
                    amqpvalue_get_long(value, (int64_t*)&value_buffer);
                    sprintf(buffer, "%" PRIi64, (int64_t)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                if (type == AMQP_TYPE_FLOAT)
                {
                    amqpvalue_get_float(value, (float*)&value_buffer);
                    sprintf(buffer, "%f", (float)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }
                
                if (type == AMQP_TYPE_DOUBLE)
                {
                    amqpvalue_get_double(value, (double*)&value_buffer);
                    sprintf(buffer, "%f", (float)value_buffer);
                    Map_Add(properties, name, buffer);
                    continue;
                }

                LogError("Cannot encode property %s.", name);
            }
        }
    }

    if (key)
    {
        amqpvalue_destroy(key);
    }
    if (value)
    {
        amqpvalue_destroy(value);
    }
    return result;
}

static AMQP_VALUE on_message_received(const void* context, UAMQP_MESSAGE_HANDLE message)
{
    AMQP_VALUE result;
    MESSAGE_CONFIG newMessageCfg;
    BINARY_DATA body;
    MAP_HANDLE newProperties;
    MESSAGE_BODY_TYPE body_type;
    AMQPSERVER_LINK_DATA* link_data = (AMQPSERVER_LINK_DATA*)context;

    if (link_data == NULL)
    {
        LogError("Link data was null...");
        result = messaging_delivery_released();
    }
    else
    {
        newProperties = Map_Create(NULL);
        if (newProperties == NULL)
        {
            LogError("Failed to create message properties.");
            result = messaging_delivery_released();
        }
        else
        {
            if (Map_Add(newProperties, GW_SOURCE_PROPERTY, "mapping") != MAP_OK)
            {
                LogError("Failed to set source property.");
                result = messaging_delivery_released();
            }
            else if (Map_Add(newProperties, GW_DEVICENAME_PROPERTY, STRING_c_str(link_data->device_id)) != MAP_OK)
            {
                LogError("Failed to set device id property.");
                result = messaging_delivery_released();
            }
            else if (Map_Add(newProperties, GW_DEVICEKEY_PROPERTY, STRING_c_str(link_data->device_key)) != MAP_OK)
            {
                LogError("Failed to set device key property.");
                result = messaging_delivery_released();
            }
            else if (!CopyProperties(message, newProperties))
            {
                LogError("Failed to copy application properties.");
                result = messaging_delivery_released();
            }
            else if (0 != message_get_body_type(message, &body_type) || body_type != MESSAGE_BODY_TYPE_DATA ||
                0 != message_get_body_amqp_data(message, 0, &body))
            {
                result = messaging_delivery_rejected("Bad body", "Bad body");
            }
            else
            {
                newMessageCfg.sourceProperties = newProperties;
                newMessageCfg.size = body.length;
                newMessageCfg.source = body.bytes;

                GW_MESSAGE_HANDLE newMessage = Message_Create(&newMessageCfg);
                if (newMessage == NULL)
                {
                    LogError("Failed to create new message.");
                    result = messaging_delivery_released();
                }
                else
                {
                    if (MessageBus_Publish(link_data->session_data->connection_data->module_data->bus, newMessage) != MESSAGE_BUS_OK)
                    {
                        result = messaging_delivery_rejected("Failed to publish to the message bus", "Failed to publish to the message bus");
                    }
                    else
                    {
                        result = messaging_delivery_accepted();
                    }

                    Message_Destroy(newMessage);
                }
            }
            Map_Destroy(newProperties);
        }
    }

    return result;
}

static bool on_new_link_attached(void* context, LINK_ENDPOINT_HANDLE new_link_endpoint, const char* name, role role, AMQP_VALUE source, AMQP_VALUE target)
{
    const char* endpoint;
    TARGET_HANDLE target_handle;
    AMQPSERVER_SESSION_DATA* session_data = (AMQPSERVER_SESSION_DATA*)context;
    AMQPSERVER_LINK_DATA* link_data;
    AMQPSERVER_MAPPING* mapping;
    
    link_data = malloc(sizeof(AMQPSERVER_LINK_DATA));
    if (link_data == NULL)
    {
        LogError("Failed to create link data struct due to oom, close session and connection.");
        session_data->connection_data->closed = true;
    }
    else
    {
        link_data->session_data = session_data;

        if (0 != amqpvalue_get_target(target, &target_handle) ||
            0 != target_get_address(target_handle, &target) ||
            0 != amqpvalue_get_string(target, &endpoint))
        {
            LogError("Failed to get target from target amqp value, reject session and close connection.");
            session_data->connection_data->closed = true;
        }
        else
        {
            /* Lookup mapping */
            for (size_t i = 0; i < VECTOR_size(session_data->connection_data->module_data->mappings); i++)
            {
                mapping = (AMQPSERVER_MAPPING*)VECTOR_element(session_data->connection_data->module_data->mappings, i);
                if (0 == strcmp(STRING_c_str(mapping->endpoint), endpoint))
                {
                    link_data->device_id = STRING_clone(mapping->device_id);
                    link_data->device_key = STRING_clone(mapping->device_key);
                    break;
                }
            }

            if (link_data->device_id == NULL || link_data->device_key == NULL)
            {
                LogError("Failed to get mapping, reject session and close connection.");
                session_data->connection_data->closed = true;
            }
            else
            {
                link_data->link = link_create_from_endpoint(session_data->session, new_link_endpoint, name, role, source, target);
                if (link_data->link == NULL)
                {
                    LogError("Error creating link, reject session and close connection.");
                    session_data->connection_data->closed = true;
                }
                else if (0 != link_set_rcv_settle_mode(link_data->link, receiver_settle_mode_first))
                {
                    LogError("Error setting receive settle mode on link, reject session and close connection.");
                    session_data->connection_data->closed = true;
                }
                else
                {
                    link_data->message_receiver = messagereceiver_create(link_data->link, on_message_receiver_state_changed, link_data);
                    if (link_data->message_receiver == NULL)
                    {
                        LogError("Error creating message receive, reject session and close connection.");
                        session_data->connection_data->closed = true;
                    }
                    else if (0 != messagereceiver_open(link_data->message_receiver, on_message_received, link_data))
                    {
                        LogError("Error opening message receiver, reject session and close connection.");
                        session_data->connection_data->closed = true;
                    }
                    else
                    {
                        /* Success */
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static bool on_new_session_endpoint(void* context, ENDPOINT_HANDLE new_endpoint)
{
    AMQPSERVER_CONNECTION_DATA* connection_data = (AMQPSERVER_CONNECTION_DATA*)context;
    AMQPSERVER_SESSION_DATA* session_data;

    session_data = malloc(sizeof(AMQPSERVER_SESSION_DATA));
    if (session_data == NULL)
    {
        LogError("Failed to create session data struct due to oom, close connection.");
        connection_data->closed = true;
    }
    else
    {
        session_data->connection_data = connection_data;
        session_data->session = session_create_from_endpoint(connection_data->connection, new_endpoint, on_new_link_attached, session_data);
        if (session_data->session == NULL)
        {
            LogError("Failed to create session from endpoint, close connection.");
            connection_data->closed = true;
        }
        else if (0 != session_set_incoming_window(session_data->session, 10000))
        {
            LogError("Failed to set incoming window, close connection.");
            connection_data->closed = true;
        }
        else if (0 != session_begin(session_data->session))
        {
            LogError("Failed to begin session, close connection.");
            connection_data->closed = true;
        }
        else
        {
            /* Success */
            return true;
        }
    }

    return false;
}

static void on_socket_accepted(void* context, XIO_HANDLE io)
{
    AMQPSERVER_DATA* module_data = (AMQPSERVER_DATA*)context;
    AMQPSERVER_CONNECTION_DATA* connection_data;
    HEADERDETECTIO_CONFIG header_detect_io_config = { io };
    XIO_HANDLE header_detect_io = xio_create(headerdetectio_get_interface_description(), &header_detect_io_config, NULL);

    connection_data = malloc(sizeof(AMQPSERVER_CONNECTION_DATA));
    if (connection_data == NULL)
    {
        LogError("Failed to create connection data struct.");
    }
    else
    {
        connection_data->module_data = module_data;
        connection_data->closed = false;
        connection_data->connection = connection_create(header_detect_io, NULL, "1", on_new_session_endpoint, connection_data);
        if (connection_data->connection == NULL)
        {
            LogError("Failed to create connection.");
            FreeConnectionData(connection_data);
        }
        else if (0 != connection_listen(connection_data->connection))
        {
            LogError("Failed to start listening.");
            FreeConnectionData(connection_data);
        }
        else
        {
            DList_InsertTailList(&module_data->connections, &connection_data->link);
            LogInfo("AMQP Client connected!");
        }
    }
}

static void AmqpServer_Receive(MODULE_HANDLE moduleHandle, GW_MESSAGE_HANDLE messageHandle)
{
}

static void AmqpServer_Destroy(MODULE_HANDLE moduleHandle)
{
    if (moduleHandle == NULL)
    {
        LogError("Attempt to destroy NULL module.");
    }
    else
    {
        AMQPSERVER_DATA* module_data = (AMQPSERVER_DATA*)moduleHandle;
        int result;

        /* Tell thread to stop */
        module_data->amqpServerRunning = false;
        /* join the thread */
        ThreadAPI_Join(module_data->amqpServerthread, &result);

        /* free module data */
        FreeModuleData(module_data);
    }
}


static int AmqpServer_worker(void * user_data)
{
    int result;
    AMQPSERVER_DATA* module_data = (AMQPSERVER_DATA*)user_data;
    AMQPSERVER_CONNECTION_DATA* connection_data;
    PDLIST_ENTRY p;

    if (user_data == NULL)
    {
        result = -1;
    }
    else
    {
        SOCKET_LISTENER_HANDLE socket_listener = socketlistener_create(5672);
        if (socketlistener_start(socket_listener, on_socket_accepted, user_data) != 0)
        {
            LogError("Failed to start listening for socket connections, fatal!");
            result = -1;
        }
        else
        {
            while (module_data->amqpServerRunning == true)
            {
                socketlistener_dowork(socket_listener);

                for (p = module_data->connections.Flink; p != &module_data->connections; )
                {
                    connection_data = containingRecord(p, AMQPSERVER_CONNECTION_DATA, link);
                    connection_dowork(connection_data->connection);
                    p = p->Flink;

                    if (connection_data->closed)
                    {
                        DList_RemoveEntryList(&connection_data->link);
                        FreeConnectionData(connection_data);
                    }
                }
            }

            result = 0;

            while (!DList_IsListEmpty(&module_data->connections))
            {
                FreeConnectionData(containingRecord(DList_RemoveHeadList(&module_data->connections), AMQPSERVER_CONNECTION_DATA, link));
            }
        }

        socketlistener_destroy(socket_listener);
        platform_deinit();
    }

#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    return result;
}

static MODULE_HANDLE AmqpServer_Create(MESSAGE_BUS_HANDLE busHandle, const void* configuration)
{
    AMQPSERVER_DATA * result;
    if (busHandle == NULL || configuration == NULL)
    {
        LogError("invalid AmqpServer module args.");
        result = NULL;
    }
    else
    {
        result = (AMQPSERVER_DATA*)malloc(sizeof(AMQPSERVER_DATA));
        if (result == NULL)
        {
            LogError("couldn't allocate memory for AmqpServer.");
        }
        else
        {
            DList_InitializeListHead(&result->connections);
            result->bus = busHandle;
            result->mappings = CreateMappingVector((VECTOR_HANDLE)configuration);
            result->amqpServerRunning = true;
            
            if (result->mappings == NULL || ThreadAPI_Create(
                &(result->amqpServerthread),
                AmqpServer_worker,
                (void*)result) != THREADAPI_OK)
            {
                LogError("ThreadAPI_Create failed.\r");
                FreeModuleData(result);
                result = NULL;
            }
            else
            {
                LogInfo("AMQP Server Module started.");
            }
        }
    }
    return result;
}

/*
 *	Required for all modules:  the public API and the designated implementation functions.
 */
static const MODULE_APIS AmqpServer_APIS_all =
{
    AmqpServer_Create,
    AmqpServer_Destroy,
    AmqpServer_Receive
};

MODULE_EXPORT const MODULE_APIS* Module_GetAPIS(void)
{
    return &AmqpServer_APIS_all;
}
