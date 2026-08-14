#ifndef POSTGRES_STAND_HANDLERS_H
#define POSTGRES_STAND_HANDLERS_H

#include "rest/rest_server.h"

extern void handle_set_delay(Request *request, Response *response);
extern void handle_get_delays(Request *request, Response *response);
extern void handle_change_delay(Request *request, Response *response);

#endif //POSTGRES_STAND_HANDLERS_H
