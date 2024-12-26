
#include <math.h>
#include "test_motor.h"


StepSignal setp_signal_;

void step_reset(StepSignal *signal,const float system_time)
{   
    signal->currentTime = system_time;
    signal->period = 0.5f;
    signal->amplitude = 5;
    signal->start_pos = 0;
    signal->target_pos = 0;

}
bool step_update(StepSignal *signal, const float amplitude, const float T, const float system_time)
{
    float dt = system_time - signal->currentTime;
    
    if(system_time < T)
    {
        if(dt <= signal->period) {
            signal->target_pos = 0.0f;

        } else if(dt <= 2*signal->period) {
            signal->target_pos = amplitude;
        } else {
            signal->currentTime =system_time;
            signal->target_pos = 0.0f;
            
        }
    }
    else {
        signal->currentTime =system_time;
        signal->target_pos = 0.0f;
        return true;
    }

    return false;

}
