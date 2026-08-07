#ifndef HTTP_HANDLE_H
#define HTTP_HANDLE_H

#include "rest/rest_server.h"

extern const char *handle_status(const char *method, const char *body, void *user_data,
                                int *status_code, const char **status_text, const char **content_type);
extern const char *handle_wal_position(const char *method, const char *body, void *user_data,
                                int *status_code, const char **status_text, const char **content_type);
extern const char *handle_info(const char *method, const char *body, void *user_data,
                                int *status_code, const char **status_text, const char **content_type);

#endif