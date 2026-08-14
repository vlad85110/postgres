#include "postgres.h"
#include "access/xlogdefs.h"
#include "miscadmin.h"
#include "rest/endpoint_handlers.h"
#include <stdio.h>

extern XLogRecPtr GetXLogReplayRecPtr(void);
extern int port;

void
handle_wal_position(Request *request, Response *response)
{
    XLogRecPtr pos = GetXLogReplayRecPtr();
    snprintf(response->body, sizeof(response->body), "{\"wal_lsn\": \"%X/%08X\"}\n", (uint32)(pos >> 32), (uint32)pos);
}

void
handle_status(Request *request, Response *response)
{
    response->status_code = 200;
    response->status_text = "ok!";
    response->content_type = "text/plain";
    snprintf(response->body, sizeof(response->body), "{\"status\": \"%s\"}\n", response->status_text);
}

void
handle_info(Request *request, Response *response)
{
    const char *proc_name = get_process_name(MyBackendType);
    snprintf(response->body, sizeof(response->body), "{\"process\": \"%s\", \"port\": %d}\n", proc_name, port);
}