#include "stand/stand_handlers.h"
#include "stand/stand_delay_setter.h"

void
handle_set_delay(Request *request, Response *response)
{
    char type_delay[6];
    long ms;
    int parsed;

    response->content_type = "application/json";

    if (request->body == NULL)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"missing body\"}\n");
        return;
    }

    parsed = sscanf(request->body,
                     "{ \"type_delay\" : \"%5[^\"]\" , \"ms\" : %ld",
                     type_delay, &ms);

    if (parsed != 2)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body),
            "{\"status\": \"error\", \"message\": \"expected {\\\"type_delay\\\": \\\"write|flush|apply\\\", \\\"ms\\\": <long>=0>}\"}\n");
        return;
    }

    if (ms < 0)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"'ms' must be >= 0\"}\n");
        return;
    }

    if (strcmp(type_delay, "write") == 0)
        ssd->WriteDelay = ms;
    else if (strcmp(type_delay, "flush") == 0)
        ssd->FlushDelay = ms;
    else if (strcmp(type_delay, "apply") == 0)
        ssd->ApplyDelay = ms;
    else
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"'type_delay' must be one of: write, flush, apply\"}\n");
        return;
    }

    response->status_code = 200;
    response->status_text = "OK";
    snprintf(response->body, sizeof(response->body), "{\"status\": \"ok\"}\n");
}

void
handle_get_delays(Request *request, Response *response)
{
    static char result[256];
    snprintf(response->body, sizeof(response->body),
        "{\"WriteDelay (ms)\": %ld, \"FlushDelay (ms)\": %ld, \"ApplyDelay (ms)\": %ld}\n",
        ssd->WriteDelay, ssd->FlushDelay, ssd->ApplyDelay);
}

void
handle_change_delay(Request *request, Response *response)
{
    char type_delay[6];
    long ms;
    int parsed;

    response->content_type = "application/json";

    if (request->body == NULL)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"missing body\"}\n");
        return;
    }

    parsed = sscanf(request->body,
                     "{ \"type_delay\" : \"%5[^\"]\" , \"ms\" : %ld",
                     type_delay, &ms);

    if (parsed != 2)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"expected {\\\"type_delay\\\": \\\"write|flush|apply\\\", \\\"ms\\\": <long>=0>}\"}\n");
        return;
    }

    if (ms < 0)
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"'ms' must be >= 0\"}\n");
        return;
    }

    if (strcmp(type_delay, "write") == 0)
        ssd->WriteDelay += ms;
    else if (strcmp(type_delay, "flush") == 0)
        ssd->FlushDelay += ms;
    else if (strcmp(type_delay, "apply") == 0)
        ssd->ApplyDelay += ms;
    else
    {
        response->status_code = 400;
        response->status_text = "Bad Request";
        snprintf(response->body, sizeof(response->body), "{\"status\": \"error\", \"message\": \"'type_delay' must be one of: write, flush, apply\"}\n");
        return;
    }

    response->status_code = 200;
    response->status_text = "OK";
    snprintf(response->body, sizeof(response->body), "{\"status\": \"ok\"}\n");
}

