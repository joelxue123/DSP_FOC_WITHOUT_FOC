/*
 * File: rtGetInf.c
 *
 * Code generated for Simulink model :LinearRampGenerator.
 *
 * Model version      : 1.232
 * Simulink Coder version    : 8.14 (R2018a) 06-Feb-2018
 * TLC version       : 8.14 (Feb 22 2018)
 * C/C++ source code generated on  : Mon Sep 21 13:14:10 2020
 *
 * Target selection: stm32.tlc
 * Embedded hardware selection: STMicroelectronics->STM32 32-bit Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 *
 *
 *
 * ******************************************************************************
 * * attention
 * *
 * * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 * *
 * ******************************************************************************
 */

/*
 * Abstract:
 *      Function to initialize non-finite, Inf
 */
#include "rtFrwUtilities.h"
#include "math.h"

real32_T Frwrt_roundf_snf(real32_T lu)
{
  real32_T ly;
  if ((real32_T)fabsf(lu) < 8.388608E+6F) {
    if (lu >= 0.5F) {
      ly = (real32_T)floor(lu + 0.5F);
    } else if (lu > -0.5F) {
      ly = lu * 0.0F;
    } else {
      ly = (real32_T)ceil(lu - 0.5F);
    }
  } else {
    ly = lu;
  }

  return ly;
}

/* File trailer for Real-Time Workshop generated code.
 *
 * [EOF] rtGetInf.c
 */
