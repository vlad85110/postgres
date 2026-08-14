#ifndef STAND_DELAY_SETTER
#define STAND_DELAY_SETTER

#include "c.h"

typedef struct StandSharedData {
    long WriteDelay;
    long FlushDelay;
    long ApplyDelay;
} StandSharedData;

extern StandSharedData *ssd;

extern Size StandShmemSize(void);
extern void StandShmemInit(void);

extern void SignalHandlerForChangeDelays(SIGNAL_ARGS);
extern void ChangeWFDelays();

#endif