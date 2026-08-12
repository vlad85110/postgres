#include "stand/stand_handlers.h"
#include "stand/stand_delay_setter.h"

const char *
handle_set_delay(const char *method, const char *body, void *user_data,
                  int *status_code, const char **status_text, const char **content_type)
{
    char type_delay[6];
    long ms;
    int parsed;

    *content_type = "application/json";

    if (body == NULL)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"missing body\"}\n";
    }

    parsed = sscanf(body,
                     " { \"type_delay\" : \"%5[^\"]\" , \"ms\" : %ld",
                     type_delay, &ms);

    if (parsed != 2)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"expected {\\\"type_delay\\\": \\\"write|flush|apply\\\", \\\"ms\\\": <long>=0>}\"}\n";
    }

    if (ms < 0)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"'ms' must be >= 0\"}\n";
    }

    if (strcmp(type_delay, "write") == 0)
        ssd->WriteDelay = ms;
    else if (strcmp(type_delay, "flush") == 0)
        ssd->FlushDelay = ms;
    else if (strcmp(type_delay, "apply") == 0)
        ssd->ApplyDelay = ms;
    else
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"'type_delay' must be one of: write, flush, apply\"}\n";
    }

    *status_code = 200;
    *status_text = "OK";
    return "{\"status\": \"ok\"}\n";
}

const char *
handle_get_delays(const char *method, const char *body, void *user_data,
                  int *status_code, const char **status_text, const char **content_type)
{
    static char result[256];
    snprintf(result, sizeof(result),
        "{\"WriteDelay (ms)\": %ld, \"FlushDelay (ms)\": %ld, \"ApplyDelay (ms)\": %ld}\n",
        ssd->WriteDelay, ssd->FlushDelay, ssd->ApplyDelay);
    return result;
}

const char *
handle_change_delay(const char *method, const char *body, void *user_data,
                    int *status_code, const char **status_text, const char **content_type)
{
    char type_delay[6];
    long ms;
    int parsed;

    *content_type = "application/json";

    if (body == NULL)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"missing body\"}\n";
    }

    parsed = sscanf(body,
                     " { \"type_delay\" : \"%5[^\"]\" , \"ms\" : %ld",
                     type_delay, &ms);

    if (parsed != 2)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"expected {\\\"type_delay\\\": \\\"write|flush|apply\\\", \\\"ms\\\": <long>=0>}\"}\n";
    }

    if (ms < 0)
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"'ms' must be >= 0\"}\n";
    }

    if (strcmp(type_delay, "write") == 0)
        ssd->WriteDelay += ms;
    else if (strcmp(type_delay, "flush") == 0)
        ssd->FlushDelay += ms;
    else if (strcmp(type_delay, "apply") == 0)
        ssd->ApplyDelay += ms;
    else
    {
        *status_code = 400;
        *status_text = "Bad Request";
        return "{\"status\": \"error\", \"message\": \"'type_delay' must be one of: write, flush, apply\"}\n";
    }

    *status_code = 200;
    *status_text = "OK";
    return "{\"status\": \"ok\"}\n";
}

