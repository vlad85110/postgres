#include "utils/stand_log.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "storage/fd.h"

#define TEST_DELAY_DELTA_MS    200
#define TEST_DELAY_MAX_MS      60000
#define STAND_TELEMETRY_LOG_PATH "/PATH/TO/replica-telemetry.jsonl" // и еще нужно дать права на этот файл postgres'у, \
    // если он не лежит в директории репозитория

long WriteDelay = 500;
long FlushDelay = 500;
long ApplyDelay = 500;

/*
 * User Signal handler for change write- and flush- delay
 */
static volatile sig_atomic_t test_delay_increment_pending = false;

void
SignalHandlerForChangeDelays(SIGNAL_ARGS)
{
    test_delay_increment_pending = true;
}

void
ChangeWFDelays() // WF - Write and Flush
{
    if (!test_delay_increment_pending)
        return;

    test_delay_increment_pending = false;

    WriteDelay = Min(WriteDelay + TEST_DELAY_DELTA_MS,
                     TEST_DELAY_MAX_MS);
    FlushDelay = Min(FlushDelay + TEST_DELAY_DELTA_MS,
                     TEST_DELAY_MAX_MS);

    stand_log("/PATH/TO/wal_receiver.log",
        "walreceiver test delays increased: write=%d ms, flush=%d ms",
         WriteDelay,
         FlushDelay);
}

void
stand_telemetry_log(const char *process, const char *stage, int64 duration_us)
{
    FILE	   *f;
    time_t		now;
    char		timebuf[16];

    f = fopen(STAND_TELEMETRY_LOG_PATH, "a");
    if (f == NULL)
        return;

    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&now));

    fprintf(f,
            "{\"ts\":\"%s\","
            "\"process\":\"%s\","
            "\"stage\":\"%s\","
            "\"duration_us\":%lld}\n",
            timebuf,
            process,
            stage,
            (long long) duration_us);

    fflush(f);
    fclose(f);
}

void
print_log(const char *filename, const char *message, ...)
{
    FILE *f = fopen(filename, "a");
    if (f == NULL)
        return;

    time_t now = time(NULL);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] ", timebuf);

    va_list args;
    va_start(args, message);
    vfprintf(f, message, args);
    va_end(args);

    fprintf(f, "\n");
    fflush(f);
    fclose(f);
}