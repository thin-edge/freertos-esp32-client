#include <stdio.h>
#include <string.h>

#include "tedge_command.h"

/*
    Convert operation status to a human readable format
*/
const char *tedge_tedge_command_status_to_name(tedge_command_status_t code)
{
    switch (code)
    {
    case TEDGE_COMMAND_STATUS_NONE:
        return "NONE";
    case TEDGE_COMMAND_STATUS_INIT:
        return "INIT";
    case TEDGE_COMMAND_STATUS_EXECUTING:
        return "EXECUTING";
    case TEDGE_COMMAND_STATUS_VERIFYING:
        return "VERIFYING";
    case TEDGE_COMMAND_STATUS_SUCCESSFUL:
        return "SUCCESSFUL";
    case TEDGE_COMMAND_STATUS_FAILED:
        return "FAILED";
    }
    return "NONE";
}

tedge_command_type_t tedge_command_get_type(char *topic)
{
    if (strstr(topic, "/cmd/restart/") != NULL)
    {
        return TEDGE_COMMAND_RESTART;
    }
    if (strstr(topic, "/cmd/firmware_update/") != NULL)
    {
        return TEDGE_COMMAND_FIRMWARE_UPDATE;
    }
    return TEDGE_COMMAND_NONE;
}

tedge_command_status_t tedge_command_get_status(cJSON *root)
{
    cJSON *statusObj = cJSON_GetObjectItem(root, "status");
    char *statusStr = statusObj->valuestring;
    tedge_command_status_t status = TEDGE_COMMAND_STATUS_NONE;

    if (strcmp(statusStr, "init") == 0)
    {
        status = TEDGE_COMMAND_STATUS_INIT;
    }
    else if (strcmp(statusStr, "executing") == 0)
    {
        return TEDGE_COMMAND_STATUS_EXECUTING;
    }
    else if (strcmp(statusStr, "verifying") == 0)
    {
        return TEDGE_COMMAND_STATUS_VERIFYING;
    }
    else if (strcmp(statusStr, "successful") == 0)
    {
        return TEDGE_COMMAND_STATUS_SUCCESSFUL;
    }
    else if (strcmp(statusStr, "failed") == 0)
    {
        return TEDGE_COMMAND_STATUS_FAILED;
    }
    return status;
}

/*
    Convert operation type to a string
*/
const char *tedge_command_name(tedge_command_type_t code)
{
    switch (code)
    {
    case TEDGE_COMMAND_RESTART:
        return "RESTART";
    case TEDGE_COMMAND_FIRMWARE_UPDATE:
        return "FIRMWARE_UPDATE";
    default:
        return "NONE";
    }
}

char *tedge_command_get_id(char *topic)
{
    int end = (int)strlen(topic);
    int x = 0;
    char sep = '/';
    for (x = end; x >= 0; --x)
    {
        if (topic[x] == sep)
        {
            char *value = calloc(64, sizeof(char));
            strcpy(value, topic + x + 1);
            return value;
        }
    }
    return NULL;
}

/*
    Free memory used by a command
*/
void tedge_free_command(tedge_command_t *cmd)
{
    if (cmd->payload != NULL)
    {
        cJSON_free(cmd->payload);
        cmd->payload = NULL;
    }
    if (cmd->id != NULL)
    {
        free(cmd->id);
        cmd->id = NULL;
    }
    if (cmd->topic != NULL)
    {
        free(cmd->topic);
        cmd->topic = NULL;
    }

    cmd->status = TEDGE_COMMAND_STATUS_NONE;
    cmd->op_type = TEDGE_COMMAND_NONE;
}

/*
    Print command details
*/
void tedge_command_print(tedge_command_t *cmd)
{
    printf("------------------------- command -------------------------\n");
    printf("TOPIC:     %s\n", cmd->topic);
    printf("ID:        %s\n", cmd->id);
    printf("STATUS:    %s\n", tedge_tedge_command_status_to_name(cmd->status));
    char *payload = cJSON_Print(cmd->payload);
    printf("PAYLOAD:   %s\n", payload);
    free(payload);
}

/*
    Set the command status
*/
void tedge_command_set_status(cJSON *payload, char *status)
{
    cJSON_SetValuestring(cJSON_GetObjectItem(payload, "status"), status);
}

/*
    Set command to failed and add a failure reason
*/
void tedge_command_set_failed(cJSON *payload, char *reason)
{
    tedge_command_set_status(payload, "failed");
    if (cJSON_HasObjectItem(payload, "reason"))
    {
        cJSON_SetValuestring(cJSON_GetObjectItem(payload, "reason"), reason);
    }
    else
    {
        cJSON_AddStringToObject(payload, "reason", reason);
    }
}
