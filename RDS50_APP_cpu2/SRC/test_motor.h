#ifndef __UTILS_H
#define __UTILS_H

#include <stdbool.h>

typedef struct 
{

    float period;     // peroid duration (seconds)
    float amplitude;     // Motion amplitude
    float currentTime;   // Current time
    float start_pos;   // Start position
    float target_pos;   // End position
} StepSignal;

extern StepSignal step_signal_;
void step_reset(StepSignal *signal,const float system_time);
bool step_update(StepSignal *signal,  const float T, const float system_time);

#endif