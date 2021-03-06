// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

#include <AzureIoTHub.h>
#include "azure_iot.h"

static char propText[1024];

EXECUTE_COMMAND_RESULT FirmwareUpdate(Anemometer *device)
{
    (void)device;
    (void)printf("Initiating FirmwareUpdate.\r\n");
    return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT SetInterval(Anemometer *device, int Position)
{
    (void)device;
    (void)printf("Setting measurement interval to %d.\r\n", Position);
    return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT SetDiagnosticMode(Anemometer *device, bool Diagnostic)
{
    (void)device;
    (void)printf("Setting diagnostic mode to %d.\r\n", Diagnostic);
    return EXECUTE_COMMAND_SUCCESS;
}

static SEND_STATE sendState = NONE;

SEND_STATE getSendState()
{
    return sendState;
}

void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    unsigned int messageTrackingId = (unsigned int)(uintptr_t)userContextCallback;

    (void)printf("Message Id: %u Received.\r\n", messageTrackingId);

    (void)printf("Result Call Back Called! Result is: %s \r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

    sendState = COMPLETE;
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char *buffer, size_t size, Anemometer *myWeather)
{
    static unsigned int messageTrackingId;
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
    if (messageHandle == NULL)
    {
        printf("unable to create a new IoTHubMessage\r\n");
    }
    else
    {
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void *)(uintptr_t)messageTrackingId) != IOTHUB_CLIENT_OK)
        {
            printf("failed to hand over the message to IoTHubClient");
        }
        else
        {
            printf("IoTHubClient accepted the message for delivery\r\n");
        }
        IoTHubMessage_Destroy(messageHandle);
    }
    messageTrackingId++;
}

/*this function "links" IoTHub to the serialization library*/
static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char *buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        printf("unable to IoTHubMessage_GetByteArray\r\n");
        result = IOTHUBMESSAGE_ABANDONED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char *temp = (char*)malloc(size + 1);
        if (temp == NULL)
        {
            printf("failed to malloc\r\n");
            result = IOTHUBMESSAGE_ABANDONED;
        }
        else
        {
            (void)memcpy(temp, buffer, size);
            temp[size] = '\0';
            EXECUTE_COMMAND_RESULT executeCommandResult = EXECUTE_COMMAND(userContextCallback, temp);
            result =
                (executeCommandResult == EXECUTE_COMMAND_ERROR) ? IOTHUBMESSAGE_ABANDONED : (executeCommandResult == EXECUTE_COMMAND_SUCCESS) ? IOTHUBMESSAGE_ACCEPTED : IOTHUBMESSAGE_REJECTED;
            free(temp);
        }
    }
    return result;
}

IOTHUB_CLIENT_LL_HANDLE initializeAzureIot(const char* connectionString)
{
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
    if (platform_init() != 0)
    {
        (void)printf("Failed to initialize platform.\r\n");
    }
    else
    {
        if (serializer_init(NULL) != SERIALIZER_OK)
        {
            (void)printf("Failed on serializer_init\r\n");
        }
        else
        {
            iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
            if (iotHubClientHandle == NULL)
            {
                (void)printf("Failed on IoTHubClient_LL_Create\r\n");
            }
            else
            {
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
                // For mbed add the certificate information
                if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
                {
                    (void)printf("failure to set option \"TrustedCerts\"\r\n");
                }
#endif // SET_TRUSTED_CERT_IN_SAMPLES
            }
        }
    }
    return iotHubClientHandle;
}

Anemometer *initializeAnemometer(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const char* deviceId)
{
    Anemometer *myWeather = CREATE_MODEL_INSTANCE(WeatherStation, Anemometer);
    if (myWeather == NULL)
    {
        (void)printf("Failed on CREATE_MODEL_INSTANCE\r\n");
    }
    else
    {
        if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, IoTHubMessage, myWeather) != IOTHUB_CLIENT_OK)
        {
            printf("unable to IoTHubClient_SetMessageCallback\r\n");
        }
    }
    myWeather->DeviceId = deviceId;
    return myWeather;
}

void destroyAnemometer(Anemometer *instance)
{
    DESTROY_MODEL_INSTANCE(instance);
}

void sendUpdate(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, Anemometer *myWeather)
{
    sendState = SENDING;

    unsigned char *destination;
    size_t destinationSize;
    if (SERIALIZE(&destination, &destinationSize, myWeather->DeviceId, myWeather->WindSpeed, myWeather->DhtTemperature, myWeather->DhtHumidity) != CODEFIRST_OK)
    {
        (void)printf("Failed to serialize\r\n");
        sendState = NONE;
    }
    else
    {
        sendMessage(iotHubClientHandle, destination, destinationSize, myWeather);
        free(destination);
    }
}

void doWork(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle)
{
    IoTHubClient_LL_DoWork(iotHubClientHandle);
    ThreadAPI_Sleep(100);
}

void destroyAzureIoT(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle)
{
    IoTHubClient_LL_Destroy(iotHubClientHandle);
    serializer_deinit();
    platform_deinit();
}
