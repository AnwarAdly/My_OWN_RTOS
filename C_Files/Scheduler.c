/*
 * Scheduler.c
 *
 *  Created on: Jun 13, 2023
 *      Author: Anwar Adly
 */

//#include "Scheduler.h"
#include "RTOS_FIFO.h"


FIFO_Buf_t Ready_Queue;
Task_ref* Ready_Queue_FIFO[100];
Task_ref Idle_Task;
struct{
	Task_ref* OSTasks[100];//Scheduable table
	unsigned int _S_MSP_Task;
	unsigned int _E_MSP_Task;
	unsigned int PSP_Task_Locater; // Same as Locater counter in linkerscript
	unsigned int NoOfActiveTasks;
	Task_ref* CurrentTask;
	Task_ref* NextTask; // For context switching between tasks
	enum{ //Modes of OS
		OS_Suspend,
		OS_Running
	}OSMode;
}OS_Control;

// Enum for SVC task state
typedef enum{
	ActivateTask,
	TerminateTask,
	TaskWaitingTime, // Call function to wait
	AcquireMutex,
	ReleaseMutex
}SVC_ID;

void MyRTOS_Create_MainStack(){
	OS_Control._S_MSP_Task = &_estack; // at start of main stack
	OS_Control._E_MSP_Task = (OS_Control._S_MSP_Task - MainStackSize); // after 3KB from start of main stack
	//Aligned 8 bytes spaces between MSP , PSP
	OS_Control.PSP_Task_Locater = (OS_Control._E_MSP_Task - 8); // As ARM stack is Full Descending order , so we start from stack_top high address then push by decrement
	// After 3KB of MSP + 8B space ,then it will be use PSP to reserve stacks to each stack created
	//if(_E_MSP_Task < &_eheap ) -> Error
}
unsigned char Idletask_Led;
void idleTask(){
	//"wfe" put cpu in sleep mode and waits for exceptional or physical irq event
	//"wfi" put cpu in sleep mode and waits for physical irq only , so it not used
	//when this event irq comes , it exit idle task and got to isr then return
	while(1){
		Idletask_Led ^= 1;
		__asm("wfe");//CPU utilization much decreases also the power , so increases performance
	}
}
OS_Error MyRTOS_init(){
	OS_Error error = NoError;
	//update OS mode (OS Suspend)
	OS_Control.OSMode = OS_Suspend;
	//Specify main stack for OS
	MyRTOS_Create_MainStack();
	//create OS ready queue (as it FIFO)
	if(FIFO_init(&Ready_Queue, Ready_Queue_FIFO, 100) != FIFO_NO_ERROR)
		error += Ready_Queue_Init_Error;
	//Config idle task
	strcpy (Idle_Task.TaskName , "IdleTask");
	Idle_Task.Priority = 255 ; // to be the lowest priority , only executes if no task found
	Idle_Task.p_TaskEntry = idleTask;
	Idle_Task.Stack_Size = 300; // 300 B
	error += MyRTOS_Create_Task(&Idle_Task);
	return error;
}
void MyRTOS_Create_TaskStack(Task_ref* Tref){
	/* Task Frame
	 * ******* They saved auto. during unstacking
	 * xPSR
	 * PC
	 * LR
	 * r12
	 * r3
	 * r2
	 * r1
	 * r0
	 * ******* they must be saved manually
	 * r4 to r11
	 */
	Tref->CurrentPSP = Tref->_S_PSP_Task;
	Tref->CurrentPSP--;
	*(Tref->CurrentPSP) = 0x01000000; //Put dummy value in PSR , all bits =0 except T_bit =1
	Tref->CurrentPSP--;
	*(Tref->CurrentPSP) = (unsigned int)Tref->p_TaskEntry; // PC
	Tref->CurrentPSP--;
	*(Tref->CurrentPSP) = 0xFFFFFFFD; //LR , EXEC_RET to be thread and PSP
	for(int i=0;i<13;i++){ // dummy push
		Tref->CurrentPSP--;
		*(Tref->CurrentPSP) = 0;
	}
}
OS_Error MyRTOS_Create_Task(Task_ref* Tref){
	OS_Error error = NoError;
	//Create it's own PSP Stack
	Tref->_S_PSP_Task = OS_Control.PSP_Task_Locater;
	Tref->_E_PSP_Task = (Tref->_S_PSP_Task - Tref->Stack_Size);
	//Check task size exceeds PSP stack
	if(Tref->_E_PSP_Task < (unsigned int)(&(_eheap)))
		return Task_Exceeded_StackSize;
	//Aligned 8 bytes spaces between PSP and other PSP
	OS_Control.PSP_Task_Locater = (Tref->_E_PSP_Task - 8);
	//Init PSP Task Stack
	MyRTOS_Create_TaskStack(Tref);
	//Update Sch table
	OS_Control.OSTasks[OS_Control.NoOfActiveTasks] = Tref;
	OS_Control.NoOfActiveTasks++;
	//Task State Update to be Suspended state
	Tref->TaskState = Suspend;
	return error;
}
void OS_BubbleSort(){
	unsigned int i , j , n;
	Task_ref* temp;
	n=OS_Control.NoOfActiveTasks;
	for(i=0;i<n-1;i++){
		for(j=0;j<n-i-1;j++){
			if(OS_Control.OSTasks[j]->Priority > OS_Control.OSTasks[j+1]->Priority){
				temp				    = OS_Control.OSTasks[j];
				OS_Control.OSTasks[j]   = OS_Control.OSTasks[j+1];
				OS_Control.OSTasks[j+1] = temp;
			}
		}
	}
}
void MyRTOS_Update_SchTable(void){
	Task_ref* temp=NULL;
	Task_ref* pTask;
	Task_ref* pNextTask;
	int i=0;
	//Bubble sort to sch table in OS_Control->OSTasks[100] based on priority high then low
	OS_BubbleSort();
	//Free ReadyQueue
	while(FIFO_dequeue(&Ready_Queue , &temp)!=FIFO_EMPTY);// &temp because it ptr to ptr
	//Update ReadyQueue
	while(i < OS_Control.NoOfActiveTasks){
		pTask = OS_Control.OSTasks[i];
		pNextTask = OS_Control.OSTasks[i+1];
		if(pTask->TaskState != Suspend){
			//in case i reached to the end of the tasks list
			if(pNextTask->TaskState == Suspend){
				FIFO_enqueue(&Ready_Queue, pTask);
				pTask->TaskState = Ready;
				break;
			}
			// check priority of next task equal current task or higher priority
			if(pTask->Priority < pNextTask->Priority){
				FIFO_enqueue(&Ready_Queue, pTask);
				pTask->TaskState = Ready;
				break;
			}
			// round robin case
			else if(pTask->Priority == pNextTask->Priority){
				FIFO_enqueue(&Ready_Queue, pTask);
				pTask->TaskState = Ready;
			}
		}
		i++;
	}
}
void Decide_whatNext(){
	//see what next task to execute to context switch
	//check if ready queue is empty and OS_ctrl->currenttask is not suspend state
	//so continue current task execution
	if(Ready_Queue.counter == 0 && OS_Control.CurrentTask->TaskState != Suspend){
		OS_Control.CurrentTask->TaskState = Running;
		FIFO_enqueue(&Ready_Queue, OS_Control.CurrentTask);
		OS_Control.NextTask = OS_Control.CurrentTask;
	}
	else{
		FIFO_dequeue(&Ready_Queue, &OS_Control.NextTask);
		OS_Control.NextTask->TaskState = Running;
		//update Ready queue to keep round robin algorithm
		//this if for assure that is same priority and in running state
		if((OS_Control.CurrentTask->Priority == OS_Control.NextTask->Priority)&&(OS_Control.CurrentTask->TaskState != Suspend)){
			FIFO_enqueue(&Ready_Queue, OS_Control.CurrentTask);
			OS_Control.CurrentTask->TaskState = Ready;
		}
	}
}
__attribute ((naked)) void PendSV_Handler(){//used for context switch
	//Save context of current task
	//get current task current (current psp from cpu register)
	OS_GET_PSP(OS_Control.CurrentTask->CurrentPSP);
	//using this PSP to store r4 to r11
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r4" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r5" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r6" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r7" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r8" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r9" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r10" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP--;
	__asm volatile("mov %0 , r11" : "=r"(*(OS_Control.CurrentTask->CurrentPSP)));

	//Restore context of next task
	if(OS_Control.NextTask != NULL){
		OS_Control.CurrentTask = OS_Control.NextTask;
		OS_Control.NextTask = NULL;
	}
	__asm volatile("mov r11 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r10 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r9 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r8 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r7 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r6 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r5 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	__asm volatile("mov r4 , %0 " : : "r"(*(OS_Control.CurrentTask->CurrentPSP)));
	OS_Control.CurrentTask->CurrentPSP++;
	//Update PSP and exit
	OS_SET_PSP(OS_Control.CurrentTask->CurrentPSP);
	__asm volatile("BX LR");

}
void OS_SVC(int* Stack_Frame){ //to execute specific os service
	unsigned char SVC_Number;
	SVC_Number = *((unsigned char*)(((unsigned char*)Stack_Frame[6])-2));
	switch(SVC_Number){
	case ActivateTask: //Activate Task
	case TerminateTask: //Terminate Task
		//update Sch table and ready queue
		MyRTOS_Update_SchTable();
		//OS is in running state
		if(OS_Control.OSMode == OS_Running){
			// check if it idle task or not becuase if it idle we already executes it manually at begin
			if(strcmp(OS_Control.CurrentTask->TaskName , "IdleTask")!=0){
				//decides what next
				Decide_whatNext();
				// set PendSV
				trigger_PendSV();
				//svc priority > systick priority > pendsv priority
				//we must put pendsv lowest or equal priority as systick to not interrupt systick as it come after systick isr
			}
		}
		break;
	case TaskWaitingTime:
		MyRTOS_Update_SchTable();//update Sch table
		break;
	case AcquireMutex:

		break;
	case ReleaseMutex:

		break;
	}
}
void MyRTOS_Set_SVC(SVC_ID ID){
	switch(ID){
	case ActivateTask:
		__asm("svc #0x00");
		break;
	case TerminateTask:
		__asm("svc #0x01");
		break;
	case TaskWaitingTime:
		__asm("svc #0x02");
		break;
	case AcquireMutex:
		__asm("svc #0x03");
		break;
	case ReleaseMutex:
		__asm("svc #0x04");
		break;
	}
}
OS_Error MyRTOS_Activate_Task(Task_ref* Tref){
	OS_Error error = NoError;
	Tref->TaskState = Waiting;
	// T2 Calls SVC to update Scheduler table and ReadyQueue
	MyRTOS_Set_SVC(ActivateTask);
	// Set PendSV and go to PendSV Handler which decide what next and switch context (T1 saved , T2 restored)
	return error;
}
OS_Error MyRTOS_Terminate_Task(Task_ref* Tref){
	OS_Error error = NoError;
	Tref->TaskState = Suspend;
	MyRTOS_Set_SVC(TerminateTask);
	return error;
}
OS_Error My_StartOS(){
	OS_Error error = NoError;
	OS_Control.OSMode = OS_Running;
	//Set default current task == idle task
	OS_Control.CurrentTask = &Idle_Task;
	MyRTOS_Activate_Task(&Idle_Task);
	//start Ticker
	start_Ticker(); // 1ms
	OS_SET_PSP(OS_Control.CurrentTask->CurrentPSP);
	// Switch thread msp to psp
	OS_SWITCH_SP_to_PSP;
	OS_SWITCH_to_unprivileged;
	Idle_Task.p_TaskEntry();
	return error;
}

OS_Error MyRTOS_Task_Wait(unsigned int ticks , Task_ref* Tref){
	OS_Error error = NoError;
	Tref->TimingWaiting.Blocking = enable;
	Tref->TimingWaiting.Ticks_Count = ticks;
	Tref->TaskState = Suspend;
	MyRTOS_Set_SVC(TerminateTask);
	return error;

}
void MyRTOS_Update_TasksWaitingTime(){

	for(int i=0;i<OS_Control.NoOfActiveTasks;i++){
		if(OS_Control.OSTasks[i]->TaskState == Suspend){// it blocking until meet the timeline
			if(OS_Control.OSTasks[i]->TimingWaiting.Blocking == enable){
				OS_Control.OSTasks[i]->TimingWaiting.Ticks_Count--;
				if(OS_Control.OSTasks[i]->TimingWaiting.Ticks_Count == 0){
					OS_Control.OSTasks[i]->TimingWaiting.Blocking = disable;
					OS_Control.OSTasks[i]->TaskState = Waiting;
					MyRTOS_Set_SVC(TaskWaitingTime);
				}
			}
		}
	}
}
OS_Error MyRTOS_Acquire_Mutex(Mutex_ref* Mref , Task_ref* Tref){
	OS_Error error = NoError;
	if(Mref->CurrentUser == NULL){//Not taken by any task
		Mref->CurrentUser = Tref;
	}
	else{
		if(Mref->NextUser == NULL){// not pending req by any task
			Mref->NextUser = Tref;
			Tref->TaskState = Suspend;
			MyRTOS_Set_SVC(TerminateTask);
		}
		else{
			return MutexisReachedMaxUser;
		}
	}
	return error;
}
OS_Error MyRTOS_Release_Mutex(Mutex_ref* Mref){
	OS_Error error = NoError;
	if(Mref->CurrentUser != NULL){//Not taken by any task
		Mref->CurrentUser = Mref->NextUser;
		Mref->NextUser = NULL;
		Mref->CurrentUser->TaskState = Waiting;
		MyRTOS_Set_SVC(ActivateTask);
	}
	return error;
}

