#ifndef STAND_DELAY_SETTER
#define STAND_DELAY_SETTER

#include "c.h"


extern long WriteDelay;
extern long FlushDelay;
extern long ApplyDelay;

extern void SignalHandlerForChangeDelays(SIGNAL_ARGS);

extern void ChangeWFDelays();

#endif