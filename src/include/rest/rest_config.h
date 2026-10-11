#ifndef REST_CONFIG_H
#define REST_CONFIG_H

#include "postgres.h"
#include "common/jsonapi.h"

#define MAX_CONFIG_ENTRIES 16

typedef struct
{
    int port;
    char process_name[64];
} RestConfigEntry;

typedef struct
{
    RestConfigEntry entries[MAX_CONFIG_ENTRIES];
    int count;
} RestConfig;

typedef struct
{
	char target[64];
	int target_depth;
	char current_field[64];
	int depth;
} RestParseState;

extern RestConfig rest_config;
extern char *rest_config_file;
extern JsonParseErrorType rest_config_parse(const char *path);
extern int rest_config_get_port(const char *process_name);

#endif
