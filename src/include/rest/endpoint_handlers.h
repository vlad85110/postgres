#ifndef HTTP_HANDLE_H
#define HTTP_HANDLE_H

#include "rest/rest_server.h"

extern void handle_status(Request *request, Response *response);
extern void handle_wal_position(Request *request, Response *response);
extern void handle_info(Request *request, Response *response);

#endif