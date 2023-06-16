/*
 * CortexMX_OS.c
 *
 *  Created on: Jun 13, 2023
 *      Author: Anwar Adly
 */

#include "CortexMX_OS.h"

void HardFault_Handler (){
	while(1);
}
void MemManage_Handler (){
	while(1);
}
void BusFault_Handler (){
	while(1);
}
void UsageFault_Handler (){
	while(1);
}


__attribute ((naked)) void SVC_Handler(){ //naked to just only execute assembly code not be a c code and take place in stack
	//used to know which SP used and then jump to OS_SVC
	__asm("tst lr, #4 	 \n\t" //AND 4(100) with bit in EXEC_RET in LR
		  "ITE EQ     	 \n\t" //Check Equally
		  "mrseq r0, MSP \n\t" //if 0 (equal)    so it MSP
		  "mrsne r0, PSP \n\t" //if 1 (not equal)so it PSP
		  "B OS_SVC");		   //Jump to SVC C code to get imm value and do requirement functionality
}
void trigger_PendSV(){// used for context switching
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk ;
}
void start_Ticker(){
	SysTick_Config(8000); // to be 1 ms
}
unsigned char Systick_Led;
void SysTick_Handler(){
	Systick_Led ^= 1;
	MyRTOS_Update_TasksWaitingTime();
	Decide_whatNext();
	trigger_PendSV();
}

void Hw_init(){ // Init clocktree ( rcc-> systick & cpu 8Mhz) & HW
				//1 count -> 0.125 us , x count -> 1 ms , x= 8000 count
	//Decrease PendSV priority to be equal or les than systick
	__NVIC_SetPriority(PendSV_IRQn, 15);
}
