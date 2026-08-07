#include "postgres.h"
#include "access/xlogdefs.h"
#include "miscadmin.h"
#include "rest/endpoint_handlers.h"
#include <stdio.h>

extern XLogRecPtr GetXLogReplayRecPtr(void);
extern int port;

const char *
handle_wal_position(const char *method, const char *body)
{
    static char result[128];
    XLogRecPtr pos = GetXLogReplayRecPtr();
    snprintf(result, sizeof(result), "{\"wal_lsn\": \"%X/%08X\"}\n", (uint32)(pos >> 32), (uint32)pos);
    return result;
}

const char *
handle_status(const char *method, const char *body)
{
    return "{\"status\": \"ok\"}\n";
}

const char *
handle_info(const char *method, const char *body)
{
    static char result[128];
    snprintf(result, sizeof(result), "{\"process\": \"%d\", \"port\": %d}\n", MyBackendType, port);
    return result;
}