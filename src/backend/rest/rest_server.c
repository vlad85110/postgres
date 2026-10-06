#include "rest/rest_server.h"
#include "rest/rest_config.h"
#include "utils/memutils.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include "miscadmin.h"
#include "replication/walsender.h"
#include "utils/guc.h"

char *rest_config_file = NULL;
extern int PostPortNumber;

RestServer *rest_server = NULL;

const char *
get_process_name(int child_type)
{
    switch (child_type)
    {
        case B_WAL_RECEIVER:    return "walreceiver";
        case B_WAL_SENDER:      return "walsender";
        case B_WAL_WRITER:      return "walwriter";
        case B_BG_WRITER:       return "bgwriter";
        case B_CHECKPOINTER:    return "checkpointer";
        case B_AUTOVAC_LAUNCHER:return "autovacuum";
        default:                return  NULL;
    }
}

void
register_endpoint(RestServer *server, const char *url, endpoint_handler handler, void *user_data)
{
    if (server == NULL)
    {
        return;
    }
    if (server->endpoints_count < MAX_ENDPOINTS)
    {
        server->endpoints[server->endpoints_count].url = url;
        server->endpoints[server->endpoints_count].handler = handler;
        server->endpoints[server->endpoints_count].user_data = user_data;
        server->endpoints_count++;
    }
}

RestServer *
rest_init(int child_type)
{
    RestServer *server;
    int port;

    const char *proc_name = get_process_name(child_type);
    if (proc_name == NULL)
    {
        return NULL;
    }

    port = rest_config_get_port(proc_name);
    if (port == -1)
    {
        return NULL;
    }

    server = palloc0(sizeof(RestServer));
    server->server_socket = -1;
    server->port = port;
    server->event_set = NULL;
    server->need_recreate = false;
    server->endpoints_count = 0;
    memset(server->clients, 0, sizeof(server->clients));

    if ((server->server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        elog(WARNING, "rest: socket error: %m");
        pfree(server);
        return NULL;
    }

    int flags = fcntl(server->server_socket, F_GETFL, 0);
    fcntl(server->server_socket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        elog(WARNING, "rest: bind error: %m");
        close(server->server_socket);
        server->server_socket = -1;
        pfree(server);
        return NULL;
    }

    listen(server->server_socket, 100);

    server->event_set = CreateWaitEventSet(NULL, MAX_CLIENTS + 1);

    AddWaitEventToSet(server->event_set, WL_SOCKET_READABLE, server->server_socket, NULL, NULL);

    elog(LOG, "rest: server started on port %d", server->port);

    return server;
}

static void
close_slot(RestServer *server, int slot)
{
    if (server->clients[slot].fd >= 0)
    {
        close(server->clients[slot].fd);
    }
    server->clients[slot].active = false;
    server->clients[slot].fd = -1;
    server->need_recreate = true;
}

static int
find_free_slot(RestServer *server)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (!server->clients[i].active)
        {
            return i;
        }
    }
    return -1;
}

static int
find_slot(RestServer *server, int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active && server->clients[i].fd == fd)
        {
            return i;
        }
    }
    return -1;
}

static void
rest_recreate_event_set(RestServer *server)
{
    FreeWaitEventSet(server->event_set);
    server->event_set = CreateWaitEventSet(NULL, MAX_CLIENTS + 1);
    AddWaitEventToSet(server->event_set, WL_SOCKET_READABLE, server->server_socket, NULL, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i].active)
        {
            AddWaitEventToSet(server->event_set, WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE, server->clients[i].fd, NULL, NULL);
        }
    }

    server->need_recreate = false;
}

static void
rest_connection_accept(RestServer *server)
{
    int client_socket = accept(server->server_socket, NULL, NULL);

    if (client_socket < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(WARNING, "rest: accept error: %m");
        return;
    }

    int slot = find_free_slot(server);

    if (slot == -1) {
        elog(WARNING, "rest: too many requests, try again later");
        close(client_socket);
        return;
    }

    server->clients[slot].active = true;
    server->clients[slot].fd = client_socket;
    server->clients[slot].read_pos = 0;
    server->clients[slot].response_ready = false;
    server->clients[slot].response_len = 0;
    server->clients[slot].written = 0;
    memset(server->clients[slot].read_buffer, 0, sizeof(server->clients[slot].read_buffer));
    memset(server->clients[slot].response, 0, sizeof(server->clients[slot].response));

    int client_flags = fcntl(server->clients[slot].fd, F_GETFL, 0);
    fcntl(server->clients[slot].fd, F_SETFL, client_flags | O_NONBLOCK);

    AddWaitEventToSet(server->event_set, WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE, server->clients[slot].fd, NULL, NULL);

    elog(DEBUG1, "rest: new connection accepted fd: %d, position: %d", client_socket, slot);
}

static bool
rest_find_endpoint(RestServer *server, const char *url, const char *method, const char *body, Response *response)
{
    Request request = {method, body, url, NULL};
    for (int i = 0; i < server->endpoints_count; i++)
    {
        if (strcmp(url, server->endpoints[i].url) == 0)
        {
            request.user_data = server->endpoints[i].user_data;
            server->endpoints[i].handler(&request, response);
            return true;
        }
    }
    return false;
}

static void
rest_build_response(Client *client, Response *response)
{
    snprintf(client->response, sizeof(client->response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            "%s",
            response->status_code,
            response->status_text,
            response->content_type,
            strlen(response->body), response->body);

    client->response_len = strlen(client->response);
    client->written = 0;
    client->response_ready = true;
    elog(DEBUG1, "rest: response ready (%zu bytes), waiting for write", client->response_len);
}

static void
rest_handle_request(RestServer *server, Client *client, int slot)
{
    ssize_t bytes_read = read(client->fd, client->read_buffer + client->read_pos, 
                                          sizeof(client->read_buffer) - client->read_pos - 1);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(WARNING, "rest: read error: %m");
        close_slot(server, slot);
        return;
    }
    else if (bytes_read == 0) {
        elog(DEBUG1, "rest: client closed connection");
        close_slot(server, slot);
        return;
    }

    client->read_pos += bytes_read;
    client->read_buffer[client->read_pos] = '\0';

    elog(DEBUG1, "rest: read %zd bytes, read %zu bytes in total", bytes_read, client->read_pos);

    if (strstr(client->read_buffer, "\r\n\r\n") == NULL)
    {
        return;
    }

    char method[16], url[256];

    Response response = {200, "OK", "application/json", ""};

    if (sscanf(client->read_buffer, "%15s %255s", method, url) != 2)
    {
        response.status_code = 400;
        response.status_text = "Bad Request";
        response.content_type = "text/plain";
        snprintf(response.body, sizeof(response.body), "Bad Request\n");
        rest_build_response(client, &response);
        return;
    }

    char *request_body = strstr(client->read_buffer, "\r\n\r\n");
    if (request_body)
    {
        request_body += 4;
    }

    if (rest_find_endpoint(server, url, method, request_body, &response))
    {
        rest_build_response(client, &response);
    }

    else
    {
        response.status_code = 404;
        response.status_text = "NotFound";
        response.content_type = "text/plain";
        const char *error_body = "Invalid request.\n"
                "Try: \ncurl -X <method> http:/<host>:<port>/<endpoint> -H <headers> -d <body>\n\n"
                "example:\n"
                "curl -X POST http:/localhost:8080/value/set "
                "-H 'Content-Type: application/json' "
                "-d '{\"value\": 300}'\n";

        snprintf(response.body, sizeof(response.body), "%s", error_body);
        rest_build_response(client, &response);
    }
}

static void
rest_handle_response(RestServer *server, Client *client, int slot)
{
    size_t remaining = client->response_len - client->written;

    ssize_t bytes_written = write(client->fd, client->response + client->written, remaining);
    if (bytes_written < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(WARNING, "rest: write error: %m");
        close_slot(server, slot);
        return;
    }
    client->written += bytes_written;
    elog(DEBUG1, "rest: %zd bytes written, %zu/%zu total", bytes_written, client->written, client->response_len);

    if (client->written >= client->response_len){
        elog(DEBUG1, "rest: response sent completely");
        close_slot(server, slot);
    }
}

void
rest_server_poll(RestServer *server)
{
    if (server == NULL || server->server_socket < 0 || server->event_set == NULL)
    {
        return;
    }

    if (server->need_recreate)
    {
        rest_recreate_event_set(server);
    }

    WaitEvent events[MAX_CLIENTS + 1];

    int number_of_fd = WaitEventSetWait(server->event_set, 0, events, MAX_CLIENTS + 1, 0);

    for (int i = 0; i < number_of_fd; i++)
    {
        if (events[i].fd == server->server_socket)
        {
            rest_connection_accept(server);
            continue;
        }

        int slot = find_slot(server, events[i].fd);
        if (slot == -1) {
            elog(WARNING, "rest: client not found");
            continue;
        }

        Client *client = &server->clients[slot];

        if (events[i].events & WL_SOCKET_READABLE && !client->response_ready)
        {
            rest_handle_request(server, client, slot);
        }

        if (events[i].events & WL_SOCKET_WRITEABLE && client->response_ready)
        {
            rest_handle_response(server, client, slot);
        }
    }
}
