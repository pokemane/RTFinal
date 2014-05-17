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
OS_SEM sem_Timer1Hz;

__task void InitTask(void);
__task void TimerTask(void);
__task void JoystickTask(void);

void SerialInit(void);

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
	os_sem_init(&sem_Timer5Hz,0);
	os_sem_init(&sem_Timer1Hz,0);
	
	// initialize mutexes
	
	// initialize tasks
	os_tsk_create(TimerTask, 150);	// Timer task is relatively important.
	os_tsk_create(JoystickTask, 100);
	
	//sudoku
	os_tsk_delete_self();
}

// timer task for general-purpose polling and task triggering

__task void TimerTask(void){
	// timer with period 5Hz
	// 10Hz is a little too fast, since the joystick movements aren't really used to scroll super fast and can cause issues.
	// can actually set this to 100Hz, and then subdivide it and send out multiple timer semaphores for different timings.
	static uint32_t counter = 0;
	static uint32_t secondFreq = 1;				// frequencies in Hz of refresh rate for these events
	static uint32_t buttonInputFreq = 5;
	for(;;){
		os_dly_wait(10);	// base freq at 100Hz
		counter++;
		// set up if structure to get different timing frequencies
		if (counter%(100/buttonInputFreq) == 0){	// 100Hz/5Hz = 20, * 10ms = 200ms = 5Hz
			// 5Hz (can be 10Hz if we need) for the joystick and button checking.
			os_sem_send(&sem_Timer5Hz);
		}
		if (counter%(100/secondFreq) == 0){// 100*10ms = 1000ms = 1sec
			// this is for the per-second events (the timestamping basically)
			os_sem_send(&sem_Timer1Hz);
		}
		counter %= 100; // makes sure the counter doesn't go over 100 (100*10ms = 1000ms = 1sec)
	}
	
}

__task void JoystickTask(void){
	static uint32_t joystick;
	for(;;){
		os_sem_wait(&sem_Timer5Hz,0xffff);
		while(os_sem_wait(&sem_Timer5Hz,0x0) != OS_R_TMO){}	// clear the semaphore (force binary behavior)
		// really this shouldn't ever be anything more thana binary semaphore but, just in case.
		
		joystick = JOY_GetKeys();
		//os_mut_wait(&mutCursor,0xffff);
		
		switch(joystick){
			case JOY_DOWN:	// cursor left
				//CursorPos.col = (CursorPos.col+15)%16;	// edited to be the same as the other, for consistency.
				break;
			case JOY_UP:	// cursor right
				//CursorPos.col = (CursorPos.col+1)%16;
				break;
			case JOY_LEFT:	// cursor up
				//CursorPos.row = (CursorPos.row+5)%6;	// modulus by non-powers-of-2 is screwed up in a binary system without floats (F%6 = 3, not 6)
				break;
			case JOY_RIGHT:	// cursor down
				//CursorPos.row = (CursorPos.row+1)%6;
				break;
			default:
				break;
		}
		
		//os_mut_release(&mutCursor);
		//os_sem_send(&semDisplayUpdate);
		
	}
}


/*
*
*
*		Serial Initialization and ISR
*
*/

void SerialInit(void){
	SER_Init();
	NVIC->ISER[USART3_IRQn/32] = (uint32_t)1<<(USART3_IRQn%32); // enable USART3 IRQ
	NVIC->IP[USART3_IRQn] = 0xE0; // set priority for USART3 to 0xE0
	USART3->CR1 |= USART_CR1_RXNEIE; // enable device interrupt for data received and ready to read
}

void USART3_IRQHandler(void){
	uint8_t flag = USART3->SR & USART_SR_RXNE; // make a flag for the USART data buffer full & ready to read signal
	if(flag == USART_SR_RXNE){	// if the flag is set
		serBuff = SER_GetChar();	// get char in the buffer (will clear flag)
		// isr_sem_send(&semSerial);
	}
}