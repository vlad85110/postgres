#ifndef STAND_LOG_H
#define STAND_LOG_H

#include "c.h"

#define stand_log print_log

extern long WriteDelay;
extern long FlushDelay;
extern long ApplyDelay;

extern void SignalHandlerForChangeDelays(SIGNAL_ARGS);

extern void ChangeWFDelays();

extern void stand_telemetry_log(const char *process, const char *stage, int64 duration_us);

extern void print_log(const char *filename, const char *message, ...);

#endif