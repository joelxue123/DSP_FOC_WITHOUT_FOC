

#include "driverlib.h"
#include "device.h"
#include "f28p65x_device.h"
#include "f28p65x_examples.h"
#include "string.h"
#include <math.h>

#define PIEMASK0                       64
#define IFRMASK                        1

#define M_PI (3.14159265358979323846f)
#define LIMIT(value, max, min) ((value) > (max) ? (max) : ((value) < (min) ? (min) : (value)))

float32 GetParm[48];
enum MOTOR_PARM_E
{
    MOTOR_PARM_TORQUE_TEST_FRE = 0,
    MOTOR_PARM_TKP,
    MOTOR_PARM_TKD,
    MOTOR_PARM_QKP,
    MOTOR_PARM_QKD,
    MOTOR_PARM_traj_position_flag,
    MOTOR_PARM_motor_control_mode,
    MOTOR_PARM_SPEED_SETPOINT,
    MOTOR_PARM_POS_FRE,
    MOTOR_PARM_POS_MAX,
    MOTOR_PARM_torque_SETPOINT,

};

/*
 * *
 Te = 1.5*0.03;
 dw= 2*pi*(1000+1000)/60
 dt = 0.125  4HZ,锟斤拷锟斤拷锟斤拷0.125ms
 J = Te / ( dw / dt) = 8e-05
 锟斤拷锟斤拷锟斤拷锟斤拷转锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷去锟斤拷锟� 锟皆匡拷锟脚观诧拷锟斤拷锟剿★拷
 * *
 */
float32 t_ =0;
float32  omega_ = 2*M_PI*10.0f;
acceleration_ = 0;
float32 velocity_ =0;
float32 position_ = 0;
float32 A_ = 0.02f;
float32 phi_=0;
int last_traj_position_flag =0;
int traj_start_ = 0;
void traj(float32 omega )
{
    if(traj_start_  == 0)
        return;
    if(t_<=30.0f)
    {
        A_ = GetParm[MOTOR_PARM_POS_MAX]/100.f;
        acceleration_ = -A_*omega*omega*sinf(omega*t_ + phi_);
        velocity_ = A_*omega*cosf(omega*t_ + phi_);
        position_ = A_*sinf(omega*t_ + phi_);
         t_+=0.0001;
    }
    else
    {
        acceleration_ = 0;
        traj_start_ = 0;
        velocity_ = 0;
        position_ = 0;
    }
}



typedef struct {
    Uint32  BeginTimerCnt;
    Uint32  LastCycleTimerCnt;
    Uint32  MaxLoadTimeTick;
    float32  CycleTime_us;
    float32  LoadTime_us;
    float32  MaxLoadTime_us;
} CYCLELOADTIME_STRUCT;

typedef struct
{
    float32 MotorAxisEncoder_rad;
    float32 AllCloseLoopEncoder_rad;
    float32 MotorAxisEncoderSpeed_radps;
    float32 AllCloseLoopEncoder_radps;
    float32 MotorCurrent_A;
    float32 TorqueSensor_Nm;    //锟斤拷cpu2锟斤拷锟斤拷锟斤拷锟斤拷
    float32 TargetPos_rad;
    float32 TargetSpeed_radps;
    float32 TargetTorque_Nm;
    float32 PosCtrlKp;
    float32 PosCtrlKi;
    float32 PosCtrlKd;
    float32 Reserve1;
    float32 Reserve2;
    float32 Reserve3;
}IPC_dataFromCPU1_t;

typedef struct
{
    float32 TargetCurrent_A;
    float32 Reserve1;
    float32 Reserve2;
    float32 Reserve3;
    Uint32  LifeTick;
}IPC_dataToCPU1_t;

void BaseBoardInit(void);
void ADCB_init(void);
void ScheduleDriveInit(void);

#pragma DATA_SECTION(ipcCPU2ToCPU1Data, "MSGRAM_CPU2_TO_CPU1")
IPC_dataToCPU1_t    ipcCPU2ToCPU1Data;

#pragma DATA_SECTION(ipcCPU1ToCPU2Data, "MSGRAM_CPU1_TO_CPU2")
IPC_dataFromCPU1_t   ipcCPU1ToCPU2Data;

CYCLELOADTIME_STRUCT Task50usTime;
CYCLELOADTIME_STRUCT Task1msTime;

Uint16 Task1msTick=0;
Uint16 Task10msTick=0;
Uint16 LastTask1msTick=0;
float32 GetDatBuff[20];
Uint32  LastCanFdInitSetFlag=0;
Uint32  LastCanFdStartSetFlag=0;
int32  TorqueSensorDiffAdSample=0;
int32  TorqueSensorSigleAdSample=0;
int32  TorqueSensorDiffAd=0;
int32  TorqueSensorSigleAd=0;
int32  TorqueSensorDiffAdSum=0;
float32 diff_torque_offset = 0;
int32 single_torque_offset = 0;
int32  TorqueSensorSigleAdSum=0;
Uint16 TorqueSensorSumTick=0;
float32 TorqueSensorSigleAd_pu = 0.0f;
float32 TorqueSensorDiffAd_pu = 0.0f;
float32 TorqueSensorDiffAd_pu_filter = 0;
bool is_first_torqute_init_ = true;





float32 calculate_friction_force(float32 velocity)
{
    if (velocity == 0) {
        return 0; // 锟斤拷态摩锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷未锟斤拷锟藉，通锟斤拷锟斤拷锟斤拷锟斤拷时锟斤拷锟斤拷
    } else {
        // 锟斤拷态摩锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟劫度凤拷锟斤拷锟洁反
        if (velocity > 30) {
            return 0.3f; // 锟斤拷锟斤拷锟斤拷锟斤拷锟狡讹拷锟斤拷摩锟斤拷锟斤拷锟斤拷锟斤拷
        } else if (velocity < -30) {
            return -0.3f; // 锟斤拷锟斤拷锟斤拷锟斤拷锟狡讹拷锟斤拷摩锟斤拷锟斤拷锟斤拷锟斤拷
        }
    }
    return 0; // 锟劫讹拷为0时锟斤拷摩锟斤拷锟斤拷锟斤拷锟斤拷确锟斤拷通锟斤拷锟斤拷锟斤拷锟诫动态锟斤拷锟斤拷
}




//#pragma CODE_SECTION(CycleLoadTimerCalcBegin, ".TI.ramfunc");
void inline CycleLoadTimerCalcBegin(CYCLELOADTIME_STRUCT * clt)
{
    clt->BeginTimerCnt = CpuTimer1.RegsAddr->TIM.all;

    clt->CycleTime_us = (float32)((Uint32)(clt->LastCycleTimerCnt - clt->BeginTimerCnt)) * 0.005F;

    clt->LastCycleTimerCnt = clt->BeginTimerCnt;
}

/**
 * @param t
 */
//#pragma CODE_SECTION(CycleLoadTimerCalcEnd, ".TI.ramfunc");
void inline CycleLoadTimerCalcEnd(CYCLELOADTIME_STRUCT * clt)
{
    clt->LoadTime_us = (float32)((Uint32)(clt->BeginTimerCnt - CpuTimer1.RegsAddr->TIM.all)) * 0.005F;

    if(clt->LoadTime_us > clt->MaxLoadTime_us){
        clt->MaxLoadTime_us = clt->LoadTime_us;
    }
    if(clt->MaxLoadTimeTick++ > 20000){
        clt->MaxLoadTime_us = 0;
        clt->MaxLoadTimeTick = 0;
    }
}


typedef struct {
    float32 alpha;  // 锟剿诧拷锟斤拷系锟斤拷
    float32 prevOut; // 锟斤拷一锟轿碉拷锟斤拷锟街�
} IIRLowPassFilter;

IIRLowPassFilter filter;
IIRLowPassFilter filter_force;
IIRLowPassFilter filter_force_derived;
void initFilter(IIRLowPassFilter *filter, float32 cutoffFreq, float32 sampleRate) {
    filter->alpha = 1.0f - exp(-2.0f * 3.1415f * cutoffFreq / sampleRate); // 锟斤拷锟斤拷alpha值
    filter->prevOut = 0.0f; // 锟斤拷始锟斤拷锟斤拷一锟轿碉拷锟斤拷锟街�
}

// 应锟斤拷IIR锟斤拷通锟剿诧拷锟斤拷
float32 applyFilter(IIRLowPassFilter *filter, float32 input) {
    float32 output = filter->alpha * input + (1.0f - filter->alpha) * filter->prevOut;
    filter->prevOut = output; // 锟斤拷锟斤拷锟斤拷一锟轿碉拷锟斤拷锟街�
    return output;
}

//
// Main
//
void main(void)
{
    BaseBoardInit();
    ADCB_init();
    initFilter(&filter, 800.0f,10000.0f);
    initFilter(&filter_force, 10.0f,10000.0f);
    initFilter(&filter_force_derived, 400.0f,10000.0f);

    ScheduleDriveInit();
    memset(&ipcCPU2ToCPU1Data, 0, sizeof(ipcCPU2ToCPU1Data));
    //
    // Take conversions indefinitely in loop
    //
    while(1){
        if((Task1msTick - LastTask1msTick) >= 10){
            LastTask1msTick = Task1msTick;

            //....10ms main task.......
            if(Task10msTick++ < 100){
                GPIO_writePin(72, 0);
            }else if(Task10msTick < 200){
                GPIO_writePin(72, 1);
            }else{
                Task10msTick = 0;
            }
        }
    }
}


//====================Task Fun============================//
//
// Task1ms
//
#pragma CODE_SECTION(TINT0_isr, ".TI.ramfunc");
interrupt void TINT0_isr(void)
{
    volatile unsigned int PIEIER1_stack_save = PieCtrlRegs.PIEIER1.all;

    PieCtrlRegs.PIEIER1.all &= ~PIEMASK0;      /* disable group1 lower/equal priority interrupts */

    asm(" RPT #5 || NOP");               /* wait 5 cycles */
    IFR &= ~IFRMASK;                           /* eventually disable lower/equal priority pending interrupts */
    PieCtrlRegs.PIEACK.all = IFRMASK;          /* ACK to allow other interrupts from the same group to fire */
    IER |= 1;
    EINT;                                /* global interrupt enable */

    CycleLoadTimerCalcBegin(&Task1msTime);

    //user 1ms Task...
    {
        Task1msTick++;

    }

    CycleLoadTimerCalcEnd(&Task1msTime);

    DINT;                                /* disable global interrupts during context switch, CPU will enable global interrupts after exiting ISR */
    PieCtrlRegs.PIEIER1.all = PIEIER1_stack_save;/*restore PIEIER register that was modified */
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}








#define TORQ_LOOP_TIME_S (1.0f/10000)
int is_sin_wave = 0;
static float32 TestFrHz= 0;

float32 TestFr_Disturbance(float32 curr_amp) {
    static float32 theata=0.0F;
    float32 TestFrDisturbance =0 ;

    theata = theata + (TORQ_LOOP_TIME_S * 6.2831853F) * TestFrHz;
    while(theata > 6.2831853F){
        theata = theata - 6.2831853F;
    }
    if(is_sin_wave )
    {
        TestFrDisturbance = sinf(theata) * curr_amp; //__sin(theata);
    }
    else
    {
        TestFrDisturbance = (sinf(theata)> 0)? curr_amp:-curr_amp; //__sin(theata);
    }

    return TestFrDisturbance;
}

#define Ts 0.0001f
float32 fh,x11,x11_pre = 0,x22,x22_pre = 0,h = 0.0005f,r = 20000,xx11,xx22,xxx2;

int sign(float32 x)
{
    if(x > 0)
    return 1;
    else if(x < 0)
    return -1;
    else
    return 0;
}

float32 fhan(float32 x1,float32 x2,float32 r,float32 h)
{
    float32 d,d0,y,a0,a,fhan_value;
    d = r*h;
    d0 = h*d;
    y = x1 + h*x2;
    a0 = sqrt(d*d+8*r*fabsf(y));

    if(y <= d0 && y >=-d0)
    {
        a = x2 + y/h;
    }
    else
    {
        a = x2 + 0.5f*(a0 - d)*sign(y);
    }

    if(a <= d && a >= -d)
        fhan_value = -r*a/d;
    else
    {
        fhan_value = -r*sign(a);
    }
    return fhan_value;

}

void DifferentiateDiscreteSignal(float32 TorqueSensorDiffAd_pu)
{
    //锟斤拷锟斤拷位锟斤拷微锟斤拷
    fh = fhan(x11_pre - TorqueSensorDiffAd_pu,x22_pre,r,h);//
    x11 = x11_pre + Ts*x22_pre;
    x22 = x22_pre + Ts*fh;
    x11_pre = x11;
    x22_pre = x22;
    xx11 = x11;
    xx22 =x22;
    xxx2 = x22;//applyFilter(&filter_force_derived,x22);
}

#define ReductionRatio 100
#define KT 0.024f
#define J  2.68e-5f
#define INERTIA 0.065510f
#define MAX_CURRENT (22.f)
// ADRC锟斤拷锟斤拷
#define WC   150.0f
#define WO  (5*WC)     /* 锟桔诧拷锟斤拷锟斤拷锟斤拷 */
#define B0  (1*KT/J)      /* 锟斤拷锟斤拷锟斤拷锟斤拷 */
#define KP  (WC*WC)    /* PD锟斤拷锟斤拷锟斤拷P锟斤拷锟斤拷 */
#define KD  (2*WC)     /* PD锟斤拷锟斤拷锟斤拷D锟斤拷锟斤拷 */
#define BETA1  (3.0f*WO)
#define BETA2  (3*WO*WO)
#define BETA3  (WO*WO*WO)
#define DT  0.0001f    /* 执锟斤拷时锟戒步锟斤拷 */


/* ESO状态锟斤拷锟斤拷 */
static float32 z1 = 0.0, z2 = 0.0, z3 = 0.0;
/* 锟斤拷锟斤拷锟斤拷锟斤拷 */
static float32 u = 0.0;

/* 系统锟斤拷锟� */
static float32 y = 0.0;

// 锟斤拷锟截讹拷锟斤拷模锟斤拷
void plant_model(float32 u, float32 *y, float32 *x1, float32 *x2)
{
    float32 x1_dot, x2_dot;

    x1_dot = *x2;
    x2_dot = u - 2.0*(*x2) - (*x1);

    *x1 += DT*x1_dot;
    *x2 += DT*x2_dot;

    *y = *x1;
}

/* 锟斤拷锟斤拷锟斤拷锟斤拷ESO */
void eso_update(float32 r, float32 y_m)
{
    float32 z1_dot, z2_dot, z3_dot;

    z1_dot = z2 - BETA1*(z1 - y_m);
    z2_dot = z3 - BETA2*(z1 - y_m) + B0*u;
    z3_dot = -BETA3*(z1 - y_m);

    z1 +=   DT*z1_dot;
    z2 += DT*z2_dot;
    z3 += DT*z3_dot;


}
// PD锟斤拷锟斤拷锟斤拷
float32 pd_control(float32 r, float32 z1, float32 z2)
{
    float32 u0, e;

    e = r - z1;
    u0 = KP*e - KD*z2;

    return u0;
}

// 锟斤拷锟斤拷锟斤拷
float32 adrc_control(float32 r, float32 z1, float32 z2, float32 z3)
{
    float32 u0, u;

    u0 = pd_control(r, z1, z2);
    u = (-z3 + u0)/B0;  //-z3

    if(u > 15)
    {
        u = 15;
    }
    if( u < -15)
    {
        u = -15;
    }

    return u;
}


float32 get_test_fre(void)
{
    return GetDatBuff[12];
};

static float32 last_buf_12 = 0;
static int is_TorqueSensorDiffAd_stable_ = 0;


#define  wn 1000

float dw_est = 0;
float dTl = 0;
float Te = 0;
float Tl = 0;
float B = 0;
float g1 = 2*wn;
float g2 = -1*J * wn *wn;
float w = 0;
float w_est = 0;



void luenberger_load_observer(void)
{
    dw_est = (Te- Tl - B*w + g1*J*(w - w_est))/J;
    dTl = g2*(w - w_est);

    Tl += dTl * DT;
    w_est += dw_est *DT;
}




void timer_callbakc(void)
{
    is_TorqueSensorDiffAd_stable_ = 1;
}


enum {
    MOTOR_IDLE = 0,
    MOTOR_CURR = 1,
    MOTOR_IMPEDANCE = 2,
    MOTOR_SPEED =3,
    MOTOR_GRAVITY_COMPENSATION = 4,
};

typedef enum
{
    TEST =0,
    INTER =1,
    ADRC =2,
    DEMO_TEST_force_resolution,
    DEMO_TEST_arcmin,
    DEMO_TEST_Nm_per_arcmin,


} TORQUE_SOURCE;
TORQUE_SOURCE torque_source  = INTER ;




static float32 Kp = 250.0f;
static float32 Kd =1.0f;
static int erro_cnt = 0;
static int  TorqueSensorDiffAd_stable_cnt = 0;
float32  actual_positon = 0;
float32 postion_offset = 4.0f;
int stu_time = 0;
float32 gravity = 0;
float32 user_torque_setpoint = 0.f;
int speed_stop_flag_ =0;
float32 friction_force =0;
float32 TorqueSensorDiffAd_estimate =0;
float32 delta_TorqueSensorDiffAd = 0;
float32 TorqueSensorDiffAd_derived = 0;
typedef struct {

    float32 amplify;
    float32 ramp_period;
    float32 current;
    float32 t;
    float32 ramp_rising_time;
    float32 ramp_step;
} RampGenerator;
#define RAMP_MAX (0.7f)
RampGenerator ramp_generator_= 
{
    .amplify =RAMP_MAX,
    .ramp_period = 10.f,
    .current = -RAMP_MAX,
    .t = 0.0f,
    .ramp_rising_time = 10.0/2.f,
    .ramp_step = 4*RAMP_MAX * DT / 10.0f,
};

#define RAMP_PERIOD 0.1f

RampGenerator force_ramp_generator_= 
{
    .amplify =RAMP_MAX,
    .ramp_period = RAMP_PERIOD,
    .current = 0.f,
    .t = 0.0f,
    .ramp_rising_time = RAMP_PERIOD/2.f,
    .ramp_step = 2*RAMP_MAX * DT / RAMP_PERIOD,
};

float32 ramp_update(RampGenerator *ramp_generator)
{

if(ramp_generator->t < ramp_generator->ramp_rising_time/2.f)
{
    ramp_generator->current += ramp_generator->ramp_step;
}
else if( ramp_generator->t < ramp_generator->ramp_rising_time)
{
    ramp_generator->current -= ramp_generator->ramp_step;
}
else if( ramp_generator->t < 3*ramp_generator->ramp_rising_time/2)
{
    ramp_generator->current -= ramp_generator->ramp_step;
}
else if( ramp_generator->t < 2*ramp_generator->ramp_rising_time)
{
    ramp_generator->current += ramp_generator->ramp_step;
}
else
{
    ramp_generator->current = 0;
    ramp_generator->t = 0;
}
ramp_generator->t += DT;
return ramp_generator->current;
}

#define STEP_FORCE_MAX 0.339f
float32 step_update(RampGenerator *ramp_generator)
{
    if(ramp_generator->t < ramp_generator->ramp_rising_time)
    {
        ramp_generator->current = STEP_FORCE_MAX;
    }
    else if( ramp_generator->t < 2*ramp_generator->ramp_rising_time)
    {
        ramp_generator->current = 0;
    }
    else
    {
        ramp_generator->current = 0;
        ramp_generator->t = 0;
    }
    ramp_generator->t += DT;
    return ramp_generator->current;
}

static float now_TargetTorque_Nm_filter = 0;
#pragma CODE_SECTION(CIPC1_INT_isr, ".TI.ramfunc");
interrupt void CIPC1_INT_isr(void)
{
  static int16 waitTick=0;
  bool is_target_current_control_by_cpu2 = 0;
  float32 last_TargetTorque_Nm =0;
  float32 now_TargetTorque_Nm = 0;
  ADC_forceMultipleSOC(ADCB_BASE, (ADC_FORCE_SOC0 | ADC_FORCE_SOC1 | ADC_FORCE_SOC2 | ADC_FORCE_SOC3));
  CycleLoadTimerCalcBegin(&Task50usTime);

    float32 pll_kp_ = 2.0f * 2000;
    float32 pll_ki_ = 0.25f * (pll_kp_ * pll_kp_);;
    float32 torque_base = 118.0f;
    float32 ADC_FULL_SCALE = 2048.f;

  float32 erro = 0;
  float32 derivative  = 0;
  float32 TorqueSensorDiffAd_origin = 0;
  float32 motor_control_mode = MOTOR_CURR;
  float32 load_M =  1.88087f;
  { //锟斤拷锟捷达拷锟斤拷锟斤拷锟斤拷锟�50us锟斤拷锟斤拷始锟斤拷锟斤拷
      //锟斤拷取CPU1锟斤拷锟斤拷
      GetDatBuff[0] = ipcCPU1ToCPU2Data.MotorAxisEncoder_rad;
      GetDatBuff[1] = ipcCPU1ToCPU2Data.AllCloseLoopEncoder_rad;
      GetDatBuff[2] = ipcCPU1ToCPU2Data.MotorAxisEncoderSpeed_radps;
      GetDatBuff[3] = ipcCPU1ToCPU2Data.AllCloseLoopEncoder_radps;
      GetDatBuff[4] = ipcCPU1ToCPU2Data.MotorCurrent_A;
      GetDatBuff[5] = ipcCPU1ToCPU2Data.TorqueSensor_Nm; //
      GetDatBuff[6] = ipcCPU1ToCPU2Data.TargetPos_rad;
      GetDatBuff[7] = ipcCPU1ToCPU2Data.TargetSpeed_radps;
      GetDatBuff[8] = ipcCPU1ToCPU2Data.TargetTorque_Nm; // 锟劫度伙拷PI锟斤拷锟�
      GetDatBuff[9] = ipcCPU1ToCPU2Data.PosCtrlKp;
      GetDatBuff[10] = ipcCPU1ToCPU2Data.PosCtrlKi;
      GetDatBuff[11] = ipcCPU1ToCPU2Data.PosCtrlKd;
      GetParm[(short)GetDatBuff[10]] = GetDatBuff[11];
      GetDatBuff[12] = ipcCPU1ToCPU2Data.Reserve1; // 3001-17
      GetDatBuff[13] = ipcCPU1ToCPU2Data.Reserve2; //锟斤拷锟斤拷
      GetDatBuff[14] = ipcCPU1ToCPU2Data.Reserve3; //锟斤拷锟斤拷
      GetDatBuff[15] = TorqueSensorDiffAdSample * 3.3F * 0.00001526F;
      GetDatBuff[16] = TorqueSensorSigleAdSample * 3.3F * 0.00001526F;
      GetDatBuff[17] = Task50usTime.CycleTime_us;
      GetDatBuff[18] = Task50usTime.LoadTime_us;

      w =GetDatBuff[2];
      Te = GetDatBuff[4]*KT;


      is_target_current_control_by_cpu2 = (GetDatBuff[12] > 1)?  1 : 0;
      last_TargetTorque_Nm  = ipcCPU1ToCPU2Data.TargetTorque_Nm;

      { //锟斤拷锟截诧拷锟斤拷锟斤拷锟斤拷


          TorqueSensorSumTick++;
          if(TorqueSensorSumTick > 1){

              TorqueSensorDiffAd_origin = TorqueSensorDiffAdSample; //锟斤拷取锟斤拷锟斤拷锟斤拷锟紸D
              TorqueSensorSigleAd = TorqueSensorSigleAdSample; //锟斤拷取锟斤拷锟斤拷锟斤拷锟斤拷AD
              TorqueSensorSumTick = 0;
              TorqueSensorDiffAdSum = 0;
              TorqueSensorSigleAdSum = 0;

              TorqueSensorDiffAd =  applyFilter(&filter,TorqueSensorDiffAd_origin);

              luenberger_load_observer();

              if(  last_buf_12 != get_test_fre())
              {

                  TorqueSensorDiffAd_stable_cnt ++;
                  if(TorqueSensorDiffAd_stable_cnt > 100)
                  {
                      Tl = 0;
                      z1 = 0;
                      z2 = 0;
                      z3 = 0;
                      TorqueSensorDiffAd_stable_cnt = 0;
                      last_buf_12 = get_test_fre();
                      diff_torque_offset += TorqueSensorDiffAd_pu_filter * ADC_FULL_SCALE;
                      postion_offset = GetDatBuff[1];

                  }


              }

              if( is_first_torqute_init_ )
              {

                  is_first_torqute_init_ = false;
                  diff_torque_offset = TorqueSensorDiffAd_origin;
                  single_torque_offset = TorqueSensorSigleAd;
              }

              TorqueSensorDiffAd -= diff_torque_offset;
              TorqueSensorSigleAd -= single_torque_offset;

              TorqueSensorDiffAd_pu = TorqueSensorDiffAd/ADC_FULL_SCALE;
              TorqueSensorDiffAd_pu_filter = applyFilter(&filter_force,TorqueSensorDiffAd_pu);
              if(TorqueSensorDiffAd_pu > 0.5f  || TorqueSensorDiffAd_pu < -0.5f)
              {
                  erro_cnt++;
                 // if(erro_cnt > 5 )
                 //     while(1);
              }

              //锟斤拷取微锟斤拷锟脚猴拷


               friction_force = calculate_friction_force(GetDatBuff[2]);
             // friction_force  = Tl /0.03f;
              float32 positon_kp = GetParm[3]/100.f;
              float32  speed_kd = GetParm[4]/10000.f;
              Kp = GetParm[MOTOR_PARM_TKP]/100.f;
              Kd = GetParm[MOTOR_PARM_TKD]/1000.f;
              float32 pos_ref =0;
              float32 w_ref=0;


            actual_positon = GetDatBuff[1]-postion_offset;

            actual_positon = 0 - actual_positon;
            while( actual_positon > M_PI) actual_positon = actual_positon - 2*M_PI;
            while( actual_positon < -M_PI) actual_positon = actual_positon + 2*M_PI;

              switch(torque_source)
              {
                  case TEST:
                      TestFrHz = get_test_fre();
                       now_TargetTorque_Nm = TestFr_Disturbance(1.0f);
                       //DifferentiateDiscreteSignal(TorqueSensorDiffAd_pu);
                       TorqueSensorDiffAd_estimate += 0.0001f * TorqueSensorDiffAd_derived;
                       delta_TorqueSensorDiffAd =  TorqueSensorDiffAd - TorqueSensorDiffAd_estimate;
                      TorqueSensorDiffAd_estimate += 0.0001f * pll_kp_ * delta_TorqueSensorDiffAd;
                      TorqueSensorDiffAd_derived += 0.0001f * pll_ki_ * delta_TorqueSensorDiffAd;

                  break;

                  case INTER:
                        motor_control_mode = GetParm[6];
                        gravity= -1.0f*load_M *sinf( (GetDatBuff[1]>postion_offset)? (GetDatBuff[1]-postion_offset) : (2*M_PI + GetDatBuff[1]-postion_offset) );
                      //  friction_force = friction_force - gravity/ReductionRatio;
                        if(motor_control_mode == MOTOR_IMPEDANCE)
                        {

                            if(last_traj_position_flag != GetParm[5] )
                            {
                                t_ = 0;
                                last_traj_position_flag =GetParm[5]  ;
                                phi_ = actual_positon;
                                traj_start_ =1 ;
                            }
                            float32  omega = 2*M_PI*GetParm[MOTOR_PARM_POS_FRE];
                            traj(omega);
                             
                            user_torque_setpoint = positon_kp*(position_ -actual_positon) + speed_kd*( velocity_*50.f - w) + gravity/torque_base + acceleration_*INERTIA/torque_base;
                        }
                        else if(motor_control_mode == MOTOR_SPEED)
                        {
                            w_ref = GetParm[MOTOR_PARM_SPEED_SETPOINT];
                            user_torque_setpoint = speed_kd*( w_ref - w) ;// + gravity/torque_base;
                            if(0)// TorqueSensorDiffAd_pu*torque_base > gravity+0.3f )
                            {
                                stu_time++;
                                if(stu_time > 400)
                                {
                                    speed_stop_flag_ =1;
                                }

                            }
                            else
                            {
                                stu_time =0;

                            }

                            if(speed_stop_flag_ ==1)
                            {
                                user_torque_setpoint =(gravity)/torque_base;
                            }


                        }
                        else if(motor_control_mode==MOTOR_GRAVITY_COMPENSATION)
                        {
                            user_torque_setpoint = gravity/torque_base;
                        }
                        else if(motor_control_mode==MOTOR_CURR)
                        {
                          //  TestFrHz = GetParm[0];
                          //  user_torque_setpoint = GetParm[MOTOR_PARM_torque_SETPOINT]*TestFr_Disturbance(1.0f)/ADC_FULL_SCALE +  gravity/torque_base;
                            user_torque_setpoint = step_update(&force_ramp_generator_);
                        }
                        else if(motor_control_mode==MOTOR_IDLE)
                        {
                            speed_stop_flag_ =0;
                            traj_start_ =0;
                            last_traj_position_flag =GetParm[5] ;
                            user_torque_setpoint = gravity/torque_base;
                        }


                       erro = user_torque_setpoint - TorqueSensorDiffAd_pu;
                      // DifferentiateDiscreteSignal(erro);
                      // derivative  = xxx2;
                       TorqueSensorDiffAd_estimate += 0.0001f * TorqueSensorDiffAd_derived;
                       delta_TorqueSensorDiffAd =  erro - TorqueSensorDiffAd_estimate;
                      TorqueSensorDiffAd_estimate += 0.0001f * pll_kp_ * delta_TorqueSensorDiffAd;
                      TorqueSensorDiffAd_derived += 0.0001f * pll_ki_ * delta_TorqueSensorDiffAd;
                      derivative  = TorqueSensorDiffAd_derived;
                      now_TargetTorque_Nm = Kp * erro + Kd *derivative + user_torque_setpoint*torque_base/(KT * ReductionRatio) + friction_force ;//+ friction_force;// gravity/(KT * ReductionRatio);// + friction_force ;//+  gravity_compensation/ (KT * ReductionRatio) ; 1.0375,0.065510 0.1779
                      now_TargetTorque_Nm = LIMIT(now_TargetTorque_Nm, MAX_CURRENT, -MAX_CURRENT);
                      now_TargetTorque_Nm_filter += 0.2f * (now_TargetTorque_Nm - now_TargetTorque_Nm_filter);
                      break;
                  case ADRC:
                      r = user_torque_setpoint;
                      y = TorqueSensorDiffAd_pu;
                      eso_update(r, y);
                      u = adrc_control(r, z1, z2, z3);
                      now_TargetTorque_Nm = u;
                      break;
                 case DEMO_TEST_arcmin:

                       now_TargetTorque_Nm = ramp_update(&ramp_generator_);

                      break;
                  default:
                  break;
              }
              

          }

      }

      
      
      if( is_target_current_control_by_cpu2 )
      {

          if(last_TargetTorque_Nm != now_TargetTorque_Nm_filter)
          {
            
            ipcCPU2ToCPU1Data.TargetCurrent_A = now_TargetTorque_Nm_filter;  //锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷值 A   0.5->0x3000 -7  6.0->0x3000-4
          }
          else
          {
              //set erro: 锟斤拷锟斤拷锟斤拷锟斤拷同值
          }
          ipcCPU2ToCPU1Data.LifeTick++;
      }


      ipcCPU2ToCPU1Data.Reserve1 = (TorqueSensorDiffAd_pu)*torque_base;//now_TargetTorque_Nm_filter;//(TorqueSensorDiffAd_pu)*torque_base;//position_*180.f/M_PI;// velocity_*1500.f/M_PI;//  position_*180.f/M_PI;//(user_torque_setpoint)*torque_base;//now_TargetTorque_Nm*KT * ReductionRatio;//GetParm[0];//TorqueSensorDiffAd_pu*torque_base;// ipcCPU1ToCPU2Data.TargetTorque_Nm*1000/7; -1*9.8f*1.0375f*0.1779f
      ipcCPU2ToCPU1Data.Reserve2 = (user_torque_setpoint)*torque_base;//now_TargetTorque_Nm;//(user_torque_setpoint)*torque_base;//actual_positon*180.f/M_PI;// w*30.f/M_PI;//actual_positon*180.f/M_PI;//TorqueSensorDiffAd_pu * torque_base;//TorqueSensorDiffAd_pu*torque_base;//xx22*torque_base;//xx22;//Task50usTime.CycleTime_us;//锟斤拷锟�5
      ipcCPU2ToCPU1Data.Reserve3 = Task50usTime.LoadTime_us; //锟斤拷锟斤拷




  }



  //Current loop sycn Task here...50us
  {
      //锟斤拷锟斤拷锟姐法锟斤拷锟斤拷30us锟斤拷锟斤拷桑锟斤拷煞锟斤拷募锟斤拷恪ｏ拷锟斤拷锟�

  }


  waitTick = 80;
  while(ADC_getInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1) == false){ //AdcbRegs.ADCINTFLG.bit.ADCINT1 == false
      if(waitTick-- < 0){
          break;
      }
  }
  ADC_clearInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1);

  TorqueSensorDiffAdSample = ((Uint32)((Uint32)AdcbResultRegs.ADCRESULT0 + (Uint32)AdcbResultRegs.ADCRESULT1))>>1;
  TorqueSensorSigleAdSample = ((Uint32)((Uint32)AdcbResultRegs.ADCRESULT2 + (Uint32)AdcbResultRegs.ADCRESULT3))>>1;

  CycleLoadTimerCalcEnd(&Task50usTime);

  IPC_ackFlagRtoL(IPC_CPU2_L_CPU1_R, IPC_FLAG1);
  PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}



//=================Init Fun============================//
//
// ScheduleDriveInit
//
void ScheduleDriveInit(void) {

    __disable_interrupts();

    /* Configure CPU-Timer 0 to interrupt every base rate. */
    /* Parameters:  Timer Pointer, CPU Freq in MHz, Period in usec. */
    ConfigCpuTimer(&CpuTimer0, 200, 0.001 * 1000000);
    StartCpuTimer0();

    ConfigCpuTimer(&CpuTimer1, 1, 0xFFFFFFFF);
    CpuTimer1.RegsAddr->PRD.all = 0xFFFFFFFF;
    StartCpuTimer1();

    //enable_interrupts
    EALLOW;
    PieVectTable.TIMER0_INT = &TINT0_isr;/* Hook interrupt to the ISR*/
    EDIS;
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;   /* Enable interrupt TIMER0_INT*/
    IER |= M_INT1;
    EALLOW;
    PieVectTable.CIPC1_INT = &CIPC1_INT_isr;/* Hook interrupt to the ISR*/
    EDIS;
    PieCtrlRegs.PIEIER1.bit.INTx14 = 1;  /* Enable interrupt CIPC1_INT*/
    IER |= M_INT1;

    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG30);

    /* Enable global Interrupts and higher priority real-time debug events:*/
    EINT;                                /* Enable Global interrupt INTM*/
    ERTM;                               /* Enable Global realtime interrupt DBGM*/

    __enable_interrupts(); /* Enable Global Interrupt INTM and realtime interrupt DBGM */
}


//
// BaseBoardInit
//
void BaseBoardInit(void)
{
    DisableDog();

    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);

    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, DEVICE_FLASH_WAITSTATES);

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.CPUTIMER0 = 1;
    CpuSysRegs.PCLKCR0.bit.CPUTIMER1 = 1;
    CpuSysRegs.PCLKCR0.bit.CPUTIMER2 = 1;
    EDIS;

    DINT;
    IER = 0x0000;
    IFR = 0x0000;
    InitPieCtrl();
    InitPieVectTable();
    InitCpuTimers();

    EALLOW;
    CpuSysRegs.PCLKCR13.bit.ADC_B = 1;
    EDIS;

    IPC_waitForFlag(IPC_CPU2_L_CPU1_R, IPC_FLAG29);

    IPC_clearFlagLtoR(IPC_CPU2_L_CPU1_R, IPC_FLAG3);
}

//
// ADCB_init
//
void ADCB_init(){
    //
    // ADC Initialization: Write ADC configurations and power up the ADC
    //
    // Configures the analog-to-digital converter module prescaler.
    //
    ADC_setPrescaler(ADCB_BASE, ADC_CLK_DIV_8_0);
    //
    // Configures the analog-to-digital converter resolution and signal mode.
    //
    ADC_setMode(ADCB_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_DIFFERENTIAL);
    //
    // Sets the timing of the end-of-conversion pulse
    //
    ADC_setInterruptPulseMode(ADCB_BASE, ADC_PULSE_END_OF_CONV);
    //
    // Powers up the analog-to-digital converter core.
    //
    ADC_enableConverter(ADCB_BASE);
    //
    // Delay for 1ms to allow ADC time to power up
    //
    DEVICE_DELAY_US(500);
    //
    // Enable alternate timings for DMA trigger
    //
    ADC_enableAltDMATiming(ADCB_BASE);
    //
    // SOC Configuration: Setup ADC EPWM channel and trigger settings
    //
    // Disables SOC burst mode.
    //
    ADC_disableBurstMode(ADCB_BASE);
    //
    // Sets the priority mode of the SOCs.
    //
    ADC_setSOCPriority(ADCB_BASE, ADC_PRI_ALL_ROUND_ROBIN);
    //
    // Start of Conversion 0 Configuration
    //
    //
    // Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
    //      SOC number      : 0
    //      Trigger         : ADC_TRIGGER_SW_ONLY
    //      Channel         : ADC_CH_ADCIN0_ADCIN1
    //      Sample Window   : 200 SYSCLK cycles
    //      Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
    //
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN0_ADCIN1, 200U);
    ADC_setInterruptSOCTrigger(ADCB_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE);
    //
    // Start of Conversion 1 Configuration
    //
    //
    // Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
    //      SOC number      : 1
    //      Trigger         : ADC_TRIGGER_SW_ONLY
    //      Channel         : ADC_CH_ADCIN0_ADCIN1
    //      Sample Window   : 200 SYSCLK cycles
    //      Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
    //
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN0_ADCIN1, 200U);
    ADC_setInterruptSOCTrigger(ADCB_BASE, ADC_SOC_NUMBER1, ADC_INT_SOC_TRIGGER_NONE);
    //
    // Start of Conversion 2 Configuration
    //
    //
    // Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
    //      SOC number      : 2
    //      Trigger         : ADC_TRIGGER_SW_ONLY
    //      Channel         : ADC_CH_ADCIN2
    //      Sample Window   : 200 SYSCLK cycles
    //      Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
    //
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER2, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN2, 200U);
    ADC_setInterruptSOCTrigger(ADCB_BASE, ADC_SOC_NUMBER2, ADC_INT_SOC_TRIGGER_NONE);
    //
    // Start of Conversion 3 Configuration
    //
    //
    // Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
    //      SOC number      : 3
    //      Trigger         : ADC_TRIGGER_SW_ONLY
    //      Channel         : ADC_CH_ADCIN2
    //      Sample Window   : 200 SYSCLK cycles
    //      Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
    //
    ADC_setupSOC(ADCB_BASE, ADC_SOC_NUMBER3, ADC_TRIGGER_SW_ONLY, ADC_CH_ADCIN2, 200U);
    ADC_setInterruptSOCTrigger(ADCB_BASE, ADC_SOC_NUMBER3, ADC_INT_SOC_TRIGGER_NONE);
    //
    // ADC Interrupt 1 Configuration
    //      Source  : ADC_INT_TRIGGER_EOC3
    //      Interrupt Source: enabled
    //      Continuous Mode : enabled
    //
    //
    ADC_setInterruptSource(ADCB_BASE, ADC_INT_NUMBER1, ADC_INT_TRIGGER_EOC3);
    ADC_clearInterruptStatus(ADCB_BASE, ADC_INT_NUMBER1);
    ADC_enableContinuousMode(ADCB_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCB_BASE, ADC_INT_NUMBER1);
}

