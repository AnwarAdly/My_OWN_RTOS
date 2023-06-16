/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Anwar Adly
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
#warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#include "Scheduler.h"
Task_ref task1 , task2 , task3 , task4;
unsigned char task1_LED , task2_LED , task3_LED , task4_LED;
Mutex_ref mutex1;
unsigned char payload[3]={1,2,3};
void Task1(){
	static int count = 0;
	while(1){
		task1_LED ^= 1;
		count++;
		if(count == 100){
			MyRTOS_Acquire_Mutex(&mutex1, &task1);
			MyRTOS_Activate_Task(&task2);
		}
		if(count == 200){
			count = 0;
			MyRTOS_Release_Mutex(&mutex1);
		}
	}
}
void Task2(){
	static int count = 0;
	while(1){
		task2_LED ^= 1;
		count++;
		if(count == 100){
			MyRTOS_Activate_Task(&task3);
		}
		if(count == 200){
			count = 0;
			MyRTOS_Terminate_Task(&task2);
		}
	}
}
void Task3(){
	static int count = 0;
	while(1){
		task3_LED ^= 1;
		count++;
		if(count == 100){
			MyRTOS_Activate_Task(&task4);
		}
		if(count == 200){
			count = 0;
			MyRTOS_Terminate_Task(&task3);
		}
	}
}
void Task4(){
	static int count = 0;
	while(1){
		task4_LED ^= 1;
		count++;
		if(count == 3){
			MyRTOS_Acquire_Mutex(&mutex1, &task4);
		}
		if(count == 200){
			count = 0;
			MyRTOS_Release_Mutex(&mutex1);
			MyRTOS_Terminate_Task(&task4);
		}
	}
}
int main(void)
{
	OS_Error Error;
	//HW_init (initialize clocktree , resetController)
	Hw_init();
	if(MyRTOS_init() != NoError)
		while(1);

	mutex1.PayloadSize = 3;
	mutex1.pPayload = payload;
	strcpy(mutex1.MutexName , "MutexShared_T1_T4");

	task1.Stack_Size = 1024;
	task1.p_TaskEntry = Task1;
	task1.Priority = 4;
	strcpy(task1.TaskName , "Task1");

	task2.Stack_Size = 1024;
	task2.p_TaskEntry = Task2;
	task2.Priority = 3;
	strcpy(task2.TaskName , "Task2");

	task3.Stack_Size = 1024;
	task3.p_TaskEntry = Task3;
	task3.Priority = 2;
	strcpy(task3.TaskName , "Task3");

	task4.Stack_Size = 1024;
	task4.p_TaskEntry = Task4;
	task4.Priority = 1;
	strcpy(task4.TaskName , "Task4");

	Error += MyRTOS_Create_Task(&task1);
	Error += MyRTOS_Create_Task(&task2);
	Error += MyRTOS_Create_Task(&task3);
	Error += MyRTOS_Create_Task(&task4);

	Error += MyRTOS_Activate_Task(&task1);

	My_StartOS();


	while(1){}
}
