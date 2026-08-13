#include "postgres.h"
#include "access/xlogdefs.h"
#include "miscadmin.h"
#include "rest/endpoint_handlers.h"
#include <stdio.h>

extern XLogRecPtr GetXLogReplayRecPtr(void);
extern int port;

void
handle_info(Request *request, Response *response)
{
    const char *proc_name = get_process_name(MyBackendType);
    snprintf(response->body, sizeof(response->body), "{\"process\": \"%s\", \"port\": %d}\n", proc_name, port);
}