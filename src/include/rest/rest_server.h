#ifndef REST_SERVER_H
#define REST_SERVER_H

#include "postgres.h"
#include <stdbool.h>

typedef const char *(*endpoint_handler)(const char *method, const char *body, void *user_data,
                                        int *status_code, const char **status_text, const char **content_type);

typedef struct
{
    const char *url;
    endpoint_handler handler;
    void *user_data;

} Endpoint;

typedef struct
{
    bool active;
    int fd;
    int event_pos;
    char response[4096];
    size_t response_len;
    char read_buffer[4096];
    size_t read_pos;
    size_t written;
    bool response_ready;

} Client;

extern void rest_init(int child_type);
extern void register_endpoint(const char *url, endpoint_handler handler, void *user_data);
extern void rest_server_poll(void);
extern int server_socket;
extern bool enable_rest_server;

#endif