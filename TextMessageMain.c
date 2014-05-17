#include <stm32f2xx.h>
#include <rtl.h>
#include "boardlibs\GLCD.h"
#include "boardlibs\Serial.h"
#include "boardlibs\JOY.h"
#include "boardlibs\LED.h"
#include "boardlibs\I2C.h"
#include "boardlibs\sram.h"
#include "boardlibs\KBD.h"
#include "userlibs\LinkedList.h"
#include "userlibs\dbg.h"

// memory pools for the linked list of messages
_declare_box(listPool, sizeof(List), 1);	
// should only need one block, since we only have 1 list to worry about.

// semaphores
OS_SEM sem_Timer5Hz;
OS_SEM sem_Timer1Hz;
OS_SEM sem_TextRx;

/* 
* variables and mutexes
*/

// time structure
typedef struct _Timestamp{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
}Timestamp;

OS_MUT mut_Timestamp;
Timestamp osTimestamp = {0,0,0};

// delcare mailbox for serial buffer
// give it 1000 slots, should be more than enough even at 115200 baud
os_mbx_declare(mbx_SerialBuffer,1000);

__task void InitTask(void);
__task void TimerTask(void);
__task void ClockTask(void);
__task void JoystickTask(void);
__task void TextParse(void);

void SerialInit(void);

int main (void){
	// initialize hardware
	SerialInit();
	JOY_Init();
	LED_Init();
	
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
	os_sem_init(&sem_TextRx, 0);
	
	// initialize mutexes
	
	// initialize mailboxes
	os_mbx_init(&mbx_SerialBuffer, sizeof(mbx_SerialBuffer));
	
	// initialize tasks
	os_tsk_create(TimerTask, 150);	// Timer task is relatively important.
	os_tsk_create(JoystickTask, 100);
	os_tsk_create(ClockTask, 175);		// high prio since we need to timestamp messages
	
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

__task void ClockTask(void){
	
	for(;;){
		os_sem_wait(&sem_Timer1Hz,0xffff);
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

// Text Parsing and Saving
__task void TextParse(void){
	static uint8_t charLimit = 160;
	static uint32_t queueSpace;
	static uint32_t i,j;
	for(;;){
		os_sem_wait(&sem_TextRx,0xffff);
		
		queueSpace = os_mbx_check(&mbx_SerialBuffer);
		
		for(i = 0 ; i<= (1000-queueSpace)/160 + 1 ; i++){ // for each required list element
			// create new list element, link to last and tail
			for(j = 0; j<charLimit; j++){  // fill list element with 160 characters max
				// pseudocode for filling the message field in the list element
				//listElement[i].message[j] = os_mbx_wait(&mbx_SerialBuffer,0xffff);
			}
		}
		
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
	static uint8_t data;
	uint8_t flag = USART3->SR & USART_SR_RXNE; // make a flag for the USART data buffer full & ready to read signal
	if(flag == USART_SR_RXNE){	// if the flag is set
		data = SER_GetChar();
		if(isr_mbx_check(&mbx_SerialBuffer) > 0 && data >= 0x20 && data <= 0x7E){	// exclude the delete key, include space.  TODO implement a "backspace" feature!
			isr_mbx_send(&mbx_SerialBuffer, data);	// wtf is this error even.  What is type void* ?
		}
		if(data == 0x0D){
			isr_sem_send(&sem_TextRx);
		}
	}
}
