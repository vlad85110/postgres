#ifndef REST_SERVER_H
#define REST_SERVER_H

#include "postgres.h"
#include <stdbool.h>
#include "storage/waiteventset.h"

#define MAX_ENDPOINTS 100
#define MAX_CLIENTS 20

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

typedef struct
{
    int server_socket;
    int port;
    WaitEventSet *event_set;
    bool need_recreate;

    Endpoint endpoints[MAX_ENDPOINTS];
    int endpoints_count;

    Client clients[MAX_CLIENTS];
} RestServer;

extern RestServer *rest_init(int child_type);
extern void register_endpoint(RestServer *server, const char *url, endpoint_handler handler, void *user_data);
extern void rest_server_poll(RestServer *server);
extern const char *get_process_name(int child_type);
extern char *rest_include_processes;
extern bool rest_enabled_for_process(int child_type);
extern RestServer *rest_server;

#endif