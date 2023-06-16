/*
 * Scheduler.h
 *
 *  Created on: Jun 13, 2023
 *      Author: Anwar Adly
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "CortexMX_OS.h"

typedef enum {
	NoError,
	Ready_Queue_Init_Error,
	Task_Exceeded_StackSize,
	MutexisReachedMaxUser
}OS_Error;

typedef struct {
	unsigned int Stack_Size;
	unsigned char Priority;
	void (*p_TaskEntry)(void); // Ptr to task C function
	unsigned int _S_PSP_Task; //Not entered by the user
	unsigned int _E_PSP_Task; //To know the begin & end of task stack
	unsigned int* CurrentPSP; //to know address of current stack
	char TaskName[30];
	enum{
		Suspend,
		Running,
		Waiting,
		Ready
	}TaskState; // Not entered by the user
	struct{  //to know how much to wait in suspend state
		enum{//blocking depends on time or not
			enable,
			disable
		}Blocking;
		unsigned int Ticks_Count; // counter to be wait in suspend state
	}TimingWaiting;
}Task_ref;
// Mutex used by binary semaphore to know whose is taken by which task and task synch.
// And by buffer data ptr to array to used for data exchange
typedef struct{
	unsigned char* pPayload;
	unsigned int 	PayloadSize;
	Task_ref*		CurrentUser;
	Task_ref*		NextUser;
	char			MutexName[30];
}Mutex_ref;


OS_Error MyRTOS_init();
OS_Error MyRTOS_Create_Task(Task_ref* Tref);
OS_Error MyRTOS_Activate_Task(Task_ref* Tref);
OS_Error MyRTOS_Terminate_Task(Task_ref* Tref);
OS_Error My_StartOS();
OS_Error MyRTOS_Task_Wait(unsigned int ticks , Task_ref* Tref);
OS_Error MyRTOS_Acquire_Mutex(Mutex_ref* Mref , Task_ref* Tref);
OS_Error MyRTOS_Release_Mutex(Mutex_ref* Mref);
#endif /* SCHEDULER_H_ */
