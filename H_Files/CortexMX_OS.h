/*
 * CortexMX_OS.h
 *
 *  Created on: Jun 13, 2023
 *      Author: Anwar Adly
 */

#ifndef CORTEXMX_OS_H_
#define CORTEXMX_OS_H_

#include "core_cm3.h"

extern int _estack;
extern int _eheap;
#define MainStackSize 3072 // 3KB
#define OS_SET_PSP(add) 			__asm volatile("mov r0, %0 \n\t msr PSP, r0" : : "r" (add))
#define OS_GET_PSP(add) 			__asm volatile("mrs r0, PSP \n\t mov %0, r0"   : "=r"(add))
#define OS_SWITCH_SP_to_PSP			__asm volatile("mrs r0, CONTROL \n\t mov r1, #0x02 \n\t orr r0, r1 \n\t msr CONTROL, r0")
#define OS_SWITCH_SP_to_MSP			__asm volatile("mrs r0, CONTROL \n\t mov r1, #0x05 \n\t and r0, r1 \n\t msr CONTROL, r0")
#define OS_SWITCH_to_privileged		__asm volatile("mrs r3, CONTROL \n\t lsr r3, #0x1 \n\t lsl r3, #0x1 \n\t msr CONTROL, r3")
#define OS_SWITCH_to_unprivileged	__asm volatile("mrs r3, CONTROL \n\t orr r3, #0x1 \n\t msr CONTROL, r3")

void trigger_PendSV();
void start_Ticker();

#endif /* CORTEXMX_OS_H_ */
