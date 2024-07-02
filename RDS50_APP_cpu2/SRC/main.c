

#include "driverlib.h"
#include "device.h"
#include "f28p65x_device.h"
#include "f28p65x_examples.h"
#include "string.h"
#include <math.h>

#define PIEMASK0                       64
#define IFRMASK                        1

/*
 * *
 Te = 1.5*0.06;
 dw= 2*pi*(1600+1600)/60
 dt = 0.125  4HZ,������0.125ms
 J = Te / ( dw / dt) = 3.3572e-05
 ��������ת������������ȥ��� �Կ��Ź۲����ˡ�
 * *
 */



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
    float32 TorqueSensor_Nm;    //��cpu2��������
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
        return 0; // ��̬Ħ����������δ���壬ͨ��������ʱ����
    } else {
        // ��̬Ħ�������������ٶȷ����෴
        if (velocity > 20) {
            return 0.3f; // ���������ƶ���Ħ��������
        } else if (velocity < -20) {
            return -0.3; // ���������ƶ���Ħ��������
        }
    }
    return 0; // �ٶ�Ϊ0ʱ��Ħ��������ȷ��ͨ�������붯̬����
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
    float32 alpha;  // �˲���ϵ��
    float32 prevOut; // ��һ�ε����ֵ
} IIRLowPassFilter;

IIRLowPassFilter filter;
IIRLowPassFilter filter_force;
void initFilter(IIRLowPassFilter *filter, float32 cutoffFreq, float32 sampleRate) {
    filter->alpha = 1.0f - exp(-2.0f * 3.1415f * cutoffFreq / sampleRate); // ����alphaֵ
    filter->prevOut = 0.0f; // ��ʼ����һ�ε����ֵ
}

// Ӧ��IIR��ͨ�˲���
float32 applyFilter(IIRLowPassFilter *filter, float32 input) {
    float32 output = filter->alpha * input + (1.0f - filter->alpha) * filter->prevOut;
    filter->prevOut = output; // ������һ�ε����ֵ
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

float32 TestFr_Disturbance(void) {
    static float32 theata=0.0F;
    float32 TestFrDisturbance =0 ;

    theata = theata + (TORQ_LOOP_TIME_S * 6.2831853F) * TestFrHz;
    while(theata > 6.2831853F){
        theata = theata - 6.2831853F;
    }
    if(is_sin_wave )
    {
        TestFrDisturbance = sinf(theata) * 1.5f; //__sin(theata);
    }
    else
    {
        TestFrDisturbance = (sinf(theata)> 0)? 1.5f:-1.5f; //__sin(theata);
    }

    return TestFrDisturbance;
}

#define Ts 0.0001f
float32 fh,x11,x11_pre = 0,x22,x22_pre = 0,h = 0.002f,r = 40000,xx11,xx22;

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
    //����λ��΢��
    fh = fhan(x11_pre - TorqueSensorDiffAd_pu,x22_pre,r,h);//
    x11 = x11_pre + Ts*x22_pre;
    x22 = x22_pre + Ts*fh;
    x11_pre = x11;
    x22_pre = x22;
    xx11 = x11;
    xx22 = x22;
}

#define ReductionRatio 100
#define KT 0.06f
#define J  3.3572e-05f

// ADRC����
#define WC   150.0f
#define WO  (5*WC)     /* �۲������� */
#define B0  (1*KT/J)      /* �������� */
#define KP  (WC*WC)    /* PD������P���� */
#define KD  (2*WC)     /* PD������D���� */
#define BETA1  (3.0f*WO)
#define BETA2  (3*WO*WO)
#define BETA3  (WO*WO*WO)
#define DT  0.0001f    /* ִ��ʱ�䲽�� */


/* ESO״̬���� */
static float32 z1 = 0.0, z2 = 0.0, z3 = 0.0;
/* �������� */
static float32 u = 0.0;

/* ϵͳ��� */
static float32 y = 0.0;

// ���ض���ģ��
void plant_model(float32 u, float32 *y, float32 *x1, float32 *x2)
{
    float32 x1_dot, x2_dot;

    x1_dot = *x2;
    x2_dot = u - 2.0*(*x2) - (*x1);

    *x1 += DT*x1_dot;
    *x2 += DT*x2_dot;

    *y = *x1;
}

/* ��������ESO */
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
// PD������
float32 pd_control(float32 r, float32 z1, float32 z2)
{
    float32 u0, e;

    e = r - z1;
    u0 = KP*e - KD*z2;

    return u0;
}

// ������
float32 adrc_control(float32 r, float32 z1, float32 z2, float32 z3)
{
    float32 u0, u;

    u0 = pd_control(r, z1, z2);
    u = (-z3 + u0)/B0;  //-z3

    if(u > 5)
    {
        u = 5;
    }
    if( u < -5)
    {
        u = -5;
    }

    return u;
}


float32 get_test_fre(void)
{
    return GetDatBuff[12];
};

static float32 last_buf_12 = 0;
static int is_TorqueSensorDiffAd_stable_ = 0;


void timer_callbakc(void)
{
    is_TorqueSensorDiffAd_stable_ = 1;
}

typedef enum
{
    TEST =0,
    INTER =1,
    ADRC =2,

} TORQUE_SOURCE;
TORQUE_SOURCE torque_source  = INTER;

static float32 Kp = 250.0f;
static float32 Kd =0.01f;
static int erro_cnt = 0;
static int  TorqueSensorDiffAd_stable_cnt = 0;
#pragma CODE_SECTION(CIPC1_INT_isr, ".TI.ramfunc");
interrupt void CIPC1_INT_isr(void)
{
  static int16 waitTick=0;
  bool is_target_current_control_by_cpu2 = 0;
  float32 last_TargetTorque_Nm =0;
  float32 now_TargetTorque_Nm = 0;
  ADC_forceMultipleSOC(ADCB_BASE, (ADC_FORCE_SOC0 | ADC_FORCE_SOC1 | ADC_FORCE_SOC2 | ADC_FORCE_SOC3));
  CycleLoadTimerCalcBegin(&Task50usTime);


  float32 torque_base = 1024.f;
  float32 user_torque_setpoint  =0.0f;
  float32 erro = 0;
  float32 derivative  = 0;
  float32 TorqueSensorDiffAd_origin = 0;


  { //���ݴ��������50us����ʼ����
      //��ȡCPU1����
      GetDatBuff[0] = ipcCPU1ToCPU2Data.MotorAxisEncoder_rad;
      GetDatBuff[1] = ipcCPU1ToCPU2Data.AllCloseLoopEncoder_rad;
      GetDatBuff[2] = ipcCPU1ToCPU2Data.MotorAxisEncoderSpeed_radps;
      GetDatBuff[3] = ipcCPU1ToCPU2Data.AllCloseLoopEncoder_radps;
      GetDatBuff[4] = ipcCPU1ToCPU2Data.MotorCurrent_A;
      GetDatBuff[5] = ipcCPU1ToCPU2Data.TorqueSensor_Nm; //
      GetDatBuff[6] = ipcCPU1ToCPU2Data.TargetPos_rad;
      GetDatBuff[7] = ipcCPU1ToCPU2Data.TargetSpeed_radps;
      GetDatBuff[8] = ipcCPU1ToCPU2Data.TargetTorque_Nm; // �ٶȻ�PI���
      GetDatBuff[9] = ipcCPU1ToCPU2Data.PosCtrlKp;
      GetDatBuff[10] = ipcCPU1ToCPU2Data.PosCtrlKi;
      GetDatBuff[11] = ipcCPU1ToCPU2Data.PosCtrlKd;
      GetDatBuff[12] = ipcCPU1ToCPU2Data.Reserve1; // 3001-17
      GetDatBuff[13] = ipcCPU1ToCPU2Data.Reserve2; //����
      GetDatBuff[14] = ipcCPU1ToCPU2Data.Reserve3; //����
      GetDatBuff[15] = TorqueSensorDiffAdSample * 3.3F * 0.00001526F;
      GetDatBuff[16] = TorqueSensorSigleAdSample * 3.3F * 0.00001526F;
      GetDatBuff[17] = Task50usTime.CycleTime_us;
      GetDatBuff[18] = Task50usTime.LoadTime_us;

      is_target_current_control_by_cpu2 = (GetDatBuff[12] > 1)?  1 : 0;
      last_TargetTorque_Nm  = ipcCPU1ToCPU2Data.TargetTorque_Nm;


      { //���ز�������


          TorqueSensorSumTick++;
          if(TorqueSensorSumTick > 1){

              TorqueSensorDiffAd_origin = TorqueSensorDiffAdSample; //��ȡ�������AD
              TorqueSensorSigleAd = TorqueSensorSigleAdSample; //��ȡ��������AD
              TorqueSensorSumTick = 0;
              TorqueSensorDiffAdSum = 0;
              TorqueSensorSigleAdSum = 0;

              TorqueSensorDiffAd =  applyFilter(&filter,TorqueSensorDiffAd_origin);



              if(  last_buf_12 != get_test_fre())
              {

                  TorqueSensorDiffAd_stable_cnt ++;
                  if(TorqueSensorDiffAd_stable_cnt > 100)
                  {
                      z1 = 0;
                      z2 = 0;
                      z3 = 0;
                      TorqueSensorDiffAd_stable_cnt = 0;
                      last_buf_12 = get_test_fre();
                      diff_torque_offset += TorqueSensorDiffAd_pu_filter * torque_base;
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

              TorqueSensorDiffAd_pu = TorqueSensorDiffAd/torque_base;
              TorqueSensorDiffAd_pu_filter = applyFilter(&filter_force,TorqueSensorDiffAd_pu);
              if(TorqueSensorDiffAd_pu > 0.5f  || TorqueSensorDiffAd_pu < -0.5f)
              {
                  erro_cnt++;
                 // if(erro_cnt > 5 )
                 //     while(1);
              }

              //��ȡ΢���ź�


              float32 friction_force = calculate_friction_force(GetDatBuff[2] *10.0f);
              switch(torque_source)
              {
                  case TEST:
                      TestFrHz = get_test_fre();
                       now_TargetTorque_Nm = TestFr_Disturbance();
                  break;

                  case INTER:
                       erro = user_torque_setpoint - TorqueSensorDiffAd_pu;
                       DifferentiateDiscreteSignal(erro);
                       derivative  = xx22;
                      now_TargetTorque_Nm = Kp * erro + Kd *derivative + friction_force +  user_torque_setpoint/ (KT * ReductionRatio) ;
                      break;
                  case ADRC:
                      r = 0;
                      y = TorqueSensorDiffAd_pu;
                      eso_update(r, y);
                      u = adrc_control(r, z1, z2, z3);
                      now_TargetTorque_Nm = u;
                      break;
                  default:
                  break;
              }

          }

      }



      if( is_target_current_control_by_cpu2 )
      {
          if(last_TargetTorque_Nm != now_TargetTorque_Nm)
          {
              ipcCPU2ToCPU1Data.TargetCurrent_A = now_TargetTorque_Nm;  //����������ֵ A   0.5->0x3000 -7  6.0->0x3000-4
          }
          else
          {
              //set erro: ��������ֵͬ
          }
          ipcCPU2ToCPU1Data.LifeTick++;
      }

      ipcCPU2ToCPU1Data.Reserve1 = 0.0f-TorqueSensorDiffAd_pu;// ipcCPU1ToCPU2Data.TargetTorque_Nm*1000/7; //���4
      ipcCPU2ToCPU1Data.Reserve2 = xx22;//xx22;//Task50usTime.CycleTime_us;//���5
      ipcCPU2ToCPU1Data.Reserve3 = Task50usTime.LoadTime_us; //����




  }



  //Current loop sycn Task here...50us
  {
      //�����㷨����30us����ɣ��ɷ��ļ��㡣����

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

