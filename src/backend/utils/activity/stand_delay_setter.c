#include "utils/stand_delay_setter.h"

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "storage/fd.h"
#include "utils/elog.h"

#define TEST_DELAY_DELTA_MS 200
#define TEST_DELAY_MAX_MS 60000
#define FIXED_PHASE_DELAY_MS 1000

long WriteDelay = 500;
long FlushDelay = 0;
long ApplyDelay = 0;

static volatile sig_atomic_t test_delay_increment_pending = false;

void
SignalHandlerForChangeDelays(SIGNAL_ARGS)
{
    test_delay_increment_pending = true;
}

void
ChangeWFDelays(void) // WF - Write and Flush
{
    static bool fixed_phase_entered = false;

    if (!test_delay_increment_pending)
        return;

    test_delay_increment_pending = false;

    if (!fixed_phase_entered)
    {
        /* Первый полученный SIGUSR2: переход из "без задержки" в "фиксированную задержку". */
        fixed_phase_entered = true;
        WriteDelay = FIXED_PHASE_DELAY_MS;

        ereport(LOG,
                errmsg("Entered fixed delay phase: WriteDelay = %ld", WriteDelay));
        return;
    }

    /* Каждый следующий SIGUSR2: фаза роста задержки. */
    WriteDelay = Min(WriteDelay + TEST_DELAY_DELTA_MS, TEST_DELAY_MAX_MS);

    ereport(LOG,
            errmsg("Delays changed: WriteDelay = %ld, FlushDelay = %ld", WriteDelay, FlushDelay));
}