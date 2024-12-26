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



#endif