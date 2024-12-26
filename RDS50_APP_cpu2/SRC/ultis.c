#include <math.h>
#include <ultis.h>


ChirpSignal chirp_signal_;

void chirp_reset(ChirpSignal *signal)
{   
    signal->startFreq = 0.01f;
    signal->endFreq = 10.0f;
    signal->sweepTime = 30.0f;
    signal->amplitude = 0.1f;
    signal->currentTime = 0;
    signal->position = 0;
    signal->velocity = 0;
    signal->acceleration = 0;
}
bool GenerateChirp(ChirpSignal *signal,const float f0, const float f1, const float T, const float t)
{

    float k = (f1 - f0) / T;              // Frequency sweep rate
    float phase;
    float instantFreq;
    float amplitude = signal->amplitude;

    if(t <= T) {
        // Upward sweep phase
        phase = 2.0f * M_PI * (f0 * t + 0.5f * k * t * t);
        instantFreq = 2.0f * M_PI * (f0 + k * t);
    } else if(t <= 2*T) {
        // Downward sweep phase
        float t_down = t - T;
        float k_down = -(f1 - f0)/T;  // Use same rate as upward
        float phase_T = 2.0f * M_PI * (f0 * T + 0.5f * k * T * T);
        phase = phase_T + 2.0f * M_PI * (f1 * t_down + 0.5f * k_down * t_down * t_down);
        instantFreq = 2.0f * M_PI * (f1 + k_down * t_down);
    } else {
        // Hold at zero frequency
        phase = 0;
        instantFreq = 0;
        return true;
    }
    
    // Generate outputs
    signal->position = amplitude * cosf(phase);
    signal->velocity = -amplitude * instantFreq * sinf(phase);
    signal->acceleration = -amplitude * (instantFreq * instantFreq) * cosf(phase);
    
    return false;

}
