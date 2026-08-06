#ifndef HTTP_HANDLE_H
#define HTTP_HANDLE_H

#include "rest/rest_server.h"

extern const char *handle_status(const char *method, const char *body, void *user_data);
extern const char *handle_wal_position(const char *method, const char *body, void *user_data);
extern const char *handle_info(const char *method, const char *body, void *user_data);

#endif