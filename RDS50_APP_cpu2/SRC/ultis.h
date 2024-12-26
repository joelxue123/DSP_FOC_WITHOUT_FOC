
#ifndef __UTILS_H
#define __UTILS_H

#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define LIMIT(value, max, min) ((value) > (max) ? (max) : ((value) < (min) ? (min) : (value)))


typedef struct 
{
    float startFreq;     // Starting frequency (Hz)
    float endFreq;       // Ending frequency (Hz)
    float sweepTime;     // Sweep duration (seconds)
    float amplitude;     // Motion amplitude
    float currentTime;   // Current time
    float position;
    float velocity;
    float acceleration;
} ChirpSignal;


extern ChirpSignal chirp_signal_;
void chirp_reset(ChirpSignal *signal);
bool GenerateChirp(ChirpSignal *signal,const float f0, const float f1, const float T, float t);


#endif
