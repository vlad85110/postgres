#ifndef REST_SERVER_H
#define REST_SERVER_H

#include "postgres.h"
#include <stdbool.h>

typedef struct
{
    const char *method;
    const char *body;
    const char *url;
    void *user_data;
} Request;

typedef struct
{
    int status_code;
    const char *status_text;
    const char *content_type;
    char body[4096];
} Response;

typedef void (*endpoint_handler)(Request *request, Response *response);

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
extern const char *get_process_name(int child_type);
extern char *rest_include_processes;
extern bool rest_enabled_for_process(int child_type);
extern int server_socket;

#endif