#ifndef TEDGE_HDR_COMMAND_H
#define TEDGE_HDR_COMMAND_H

#include "cJSON.h"

void tedge_command_version(void);

typedef enum
{
    TEDGE_COMMAND_STATUS_NONE = 0,
    TEDGE_COMMAND_STATUS_INIT = 1,
    TEDGE_COMMAND_STATUS_EXECUTING = 2,
    TEDGE_COMMAND_STATUS_VERIFYING = 3,
    TEDGE_COMMAND_STATUS_SUCCESSFUL = 4,
    TEDGE_COMMAND_STATUS_FAILED = 5,
} tedge_command_status_t;

typedef enum
{
    TEDGE_COMMAND_NONE = 0,
    TEDGE_COMMAND_RESTART = 1,
    TEDGE_COMMAND_FIRMWARE_UPDATE = 2,
} tedge_command_type_t;

typedef struct Command
{
    tedge_command_type_t op_type;
    tedge_command_status_t status;
    char *id;
    char *topic;
    cJSON *payload;
} tedge_command_t;

const char *tedge_tedge_command_status_to_name(tedge_command_status_t);
tedge_command_type_t tedge_command_get_type(char *);
tedge_command_status_t tedge_command_get_status(cJSON *);
const char *tedge_command_name(tedge_command_type_t);
char *tedge_command_get_id(char *);
void tedge_free_command(tedge_command_t *);
void tedge_command_print(tedge_command_t *);

void tedge_command_set_status(cJSON *, char *);
void tedge_command_set_failed(cJSON *, char *);

#endif
