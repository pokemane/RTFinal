#include <stm32f2xx.h>
#include "rtl.h"
#include "boardlibs\GLCD.h"
#include "boardlibs\Serial.h"
#include "boardlibs\JOY.h"
#include "boardlibs\LED.h"
#include "boardlibs\I2C.h"
#include "boardlibs\sram.h"
#include "boardlibs\KBD.h"


OS_SEM sem_Timer5Hz;
OS_SEM sem_Timer10Hz;


__task void InitTask(void);
__task void TimerTask(void);

int main (void){
	// initialize hardware
	
	// InitTask
	os_sys_init_prio(InitTask, 250);
	
}

/*
*
*
*		OS Tasks
*
*/

// initialization task

__task void InitTask(void){
	// initialize semaphores
	
	// initialize mutexes
	
	// initialize tasks
	
	//sudoku
}

// timer task for general-purpose polling and task triggering

__task void TimerTask(void){
	// timer with period 5Hz
	// 10Hz is a little too fast, since the joystick movements aren't really used to scroll super fast and can cause issues.
	// can actually set this to 100Hz, and then subdivide it and send out multiple timer semaphores for different timings.
	static uint32_t counter = 0;
	for(;;){
		os_dly_wait(10);
		counter++;
		// set up if structure to get different timing frequencies
		if (counter%20 == 0){	// 20*10ms = 200ms = 5Hz
			os_sem_send(&sem_Timer5Hz);
		}
		counter %= 100; // makes sure the counter doesn't go over 100 (100*10ms = 1000ms = 1sec)
	}
	
}
