    .if __TI_EABI__
    .asg asm_c28x_add_s32_s32_s32_sat, _asm_c28x_add_s32_s32_s32_sat
    .asg asm_c28x_sub_s32_s32_s32_sat, _asm_c28x_sub_s32_s32_s32_sat
    .endif

		.def _asm_c28x_add_s32_s32_s32_sat

_asm_c28x_add_s32_s32_s32_sat:
;----------------------------------------------------------------------
;  The first argument of the function is in ACC, the second in *-SP[4]  
;  Clear the overflow flag
;  result = a + b;
;  Saturate the result
;  return result
;----------------------------------------------------------------------
		ZAP OVC ; Clear overflow counter
        ADDL      ACC,*-SP[4]
        SAT 	  ACC ; Saturate ACC based on OVC value         
        MOVL      *SP,ACC
        LRETR
		
		.def _asm_c28x_sub_s32_s32_s32_sat

_asm_c28x_sub_s32_s32_s32_sat:
;----------------------------------------------------------------------
;  The first argument of the function is in ACC, the second in *-SP[4]  
;  Clear the overflow flag
;  result = a - b;
;  Saturate the result
;  return result
;----------------------------------------------------------------------
		ZAP OVC                       ; Clear overflow counter
        SUBL      ACC,*-SP[4]
        SAT 	  ACC ; Saturate ACC based on OVC value         
        MOVL      *SP,ACC
        LRETR