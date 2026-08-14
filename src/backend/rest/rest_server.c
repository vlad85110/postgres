#include "rest/rest_server.h"
#include "storage/waiteventset.h"
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

#define MAX_ENDPOINTS 100
#define MAX_CLIENTS 20

char *rest_include_processes = NULL;

extern int PostPortNumber;

static Endpoint endpoints[MAX_ENDPOINTS];
static Client clients[MAX_CLIENTS];

static int endpoints_count = 0;

WaitEventSet *event_set = NULL;

int server_socket = -1;
int port = -1;

static bool need_recreate = false;

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

bool
rest_enabled_for_process(int child_type)
{
    if (rest_include_processes == NULL || rest_include_processes[0] == '\0')
    {
        return false;
    }

    const char *proc_name = get_process_name(child_type);

    if (proc_name == NULL)
    {
        return false;
    }

    char *list = palloc(strlen(rest_include_processes) + 1);
    strcpy(list, rest_include_processes);
    char *token = strtok(list, ",");
    bool found = false;

    while (token != NULL)
    {
        while (*token == ' ')
        {
            token++;
        }

        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ')
        {
            end--;
        }
        *(end + 1) = '\0';

        if (strcmp(token, proc_name) == 0)
        {
            found = true;
            break;
        }
        token = strtok(NULL, ",");
    }

    pfree(list);
    return found;
}

void
register_endpoint(const char *url, endpoint_handler handler, void *user_data)
{
    if (endpoints_count < MAX_ENDPOINTS)
    {
        endpoints[endpoints_count].url = url;
        endpoints[endpoints_count].handler = handler;
        endpoints[endpoints_count].user_data = user_data;
        endpoints_count++;
    }
}

static int
rest_port(int child_type)
{
    switch(child_type)
    {
        case B_WAL_RECEIVER:    return 8080;
        case B_WAL_SENDER:      return 8081;
        case B_WAL_WRITER:      return 8082;
        case B_BG_WRITER:       return PostPortNumber + 3000;
        case B_CHECKPOINTER:    return PostPortNumber + 3100;
        case B_AUTOVAC_LAUNCHER:return PostPortNumber + 3200;
        default:                return -1;
    }
}

void
rest_init(int child_type)
{
    if (!rest_enabled_for_process(child_type))
    {
        return;
    }

    port = rest_port(child_type);

    if (port == -1)
    {
        return;
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        elog(ERROR, "rest: socket error");
        return;
    }

    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        elog(ERROR, "rest: bind error");
        close(server_socket);
        return;
    }

    listen(server_socket, 100);

    event_set = CreateWaitEventSet(NULL, MAX_CLIENTS + 1);

    AddWaitEventToSet(event_set, WL_SOCKET_READABLE, server_socket, NULL, NULL);

    elog(LOG, "rest: server started on port %d", port);
}

static void
close_slot(int slot)
{
    if (clients[slot].fd >= 0)
    {
        close(clients[slot].fd);
    }
    clients[slot].active = false;
    clients[slot].fd = -1;
    need_recreate = true;
}

static int
find_free_slot(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (!clients[i].active)
        {
            return i;
        }
    }
    return -1;
}

static int
find_slot(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active && clients[i].fd == fd)
        {
            return i;
        }
    }
    return -1;
}

static void
rest_recreate_event_set(void)
{
    FreeWaitEventSet(event_set);
    event_set = CreateWaitEventSet(NULL, MAX_CLIENTS + 1);
    AddWaitEventToSet(event_set, WL_SOCKET_READABLE, server_socket, NULL, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            AddWaitEventToSet(event_set, WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE, clients[i].fd, NULL, NULL);
        }
    }

    need_recreate = false;
}

static void
rest_connection_accept(void)
{
    int client_socket = accept(server_socket, NULL, NULL);

    if (client_socket < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(ERROR, "rest: accept error");
        return;
    }

    int slot = find_free_slot();

    if (slot == -1) {
        elog(ERROR, "rest: too many requests, try again later");
        close(client_socket);
        return;
    }

    clients[slot].active = true;
    clients[slot].fd = client_socket;
    clients[slot].read_pos = 0;
    clients[slot].response_ready = false;
    clients[slot].response_len = 0;
    clients[slot].written = 0;
    memset(clients[slot].read_buffer, 0, sizeof(clients[slot].read_buffer));
    memset(clients[slot].response, 0, sizeof(clients[slot].response));

    int client_flags = fcntl(clients[slot].fd, F_GETFL, 0);
    fcntl(clients[slot].fd, F_SETFL, client_flags | O_NONBLOCK);

    AddWaitEventToSet(event_set, WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE, clients[slot].fd, NULL, NULL);

    elog(DEBUG1, "rest: new connection accepted fd: %d, position: %d", client_socket, slot);
}

static bool
rest_find_endpoint(const char *url, const char *method, const char *body, Response *response)
{
    Request request = {method, url, body, NULL};
    for (int i = 0; i < endpoints_count; i++)
    {
        if (strcmp(url, endpoints[i].url) == 0)
        {
            request.user_data = endpoints[i].user_data;
            endpoints[i].handler(&request, response);
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
rest_handle_request(Client *client, int slot)
{
    ssize_t bytes_read = read(client->fd, client->read_buffer + client->read_pos, 
                                          sizeof(client->read_buffer) - client->read_pos - 1);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(ERROR, "rest: read error");
        close_slot(slot);
        return;
    }
    else if (bytes_read == 0) {
        elog(DEBUG1, "rest: client closed connection");
        close_slot(slot);
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

    if (rest_find_endpoint(url, method, request_body, &response))
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
rest_handle_response(Client *client, int slot)
{
    size_t remaining = client->response_len - client->written;

    ssize_t bytes_written = write(client->fd, client->response + client->written, remaining);
    if (bytes_written < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        elog(ERROR, "rest: write error");
        close_slot(slot);
        return;
    }
    client->written += bytes_written;
    elog(DEBUG1, "rest: %zd bytes written, %zu/%zu total", bytes_written, client->written, client->response_len);

    if (client->written >= client->response_len){
        elog(DEBUG1, "rest: response sent completely");
        close_slot(slot);
    }
}

void
rest_server_poll(void)
{
    if (server_socket < 0 || event_set == NULL)
    {
        return;
    }

    if (need_recreate)
    {
        rest_recreate_event_set();
    }

    WaitEvent events[MAX_CLIENTS + 1];

    int number_of_fd = WaitEventSetWait(event_set, 0, events, MAX_CLIENTS + 1, 0);

    for (int i = 0; i < number_of_fd; i++)
    {
        if (events[i].fd == server_socket)
        {
            rest_connection_accept();
            continue;
        }

        int slot = find_slot(events[i].fd);
        if (slot == -1) {
            elog(ERROR, "rest: client not found");
            continue;
        }

        Client *client = &clients[slot];

        if (events[i].events & WL_SOCKET_READABLE && !client->response_ready)
        {
            rest_handle_request(client, slot);
        }

        if (events[i].events & WL_SOCKET_WRITEABLE && client->response_ready)
        {
            rest_handle_response(client, slot);
        }
    }
}
