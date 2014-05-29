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

#include "TextMessage.h"

// set up the SRAM memory spaces for the two lists
#define MESSAGE_STR_OFFSET 4*sizeof(ListNode)
uint32_t *MsgRXQ = (uint32_t *)mySRAM_BASE;	// the receive queue list
uint32_t *MsgSTR = (uint32_t *)(mySRAM_BASE+MESSAGE_STR_OFFSET);	// the long-term storage list space

// semaphores and events
OS_SEM sem_Timer10Hz;
OS_SEM sem_Timer1Hz;
OS_SEM sem_TextRx;

uint16_t timer10Hz = 0x0002;
uint16_t timer1Hz = 0x0001;

uint16_t hourButton = 0x0010;
uint16_t minButton	= 0x0020;

uint16_t txtRx 		= 0x0001;

/*
* structures, variables, and mutexes
*/

OS_MUT mut_osTimestamp;
Timestamp osTimestamp = {0,0,0};

OS_MUT mut_msgList;
OS_MUT mut_freeList;
List *lstRXQ;
List *lstStrUsed;
List *lstStrFree;

// delcare mailbox for serial buffer
os_mbx_declare(mbx_MsgBuffer,4);

__task void InitTask(void);
__task void TimerTask(void);
OS_TID idTimerTask;
__task void ClockTask(void);
OS_TID idClockTask;
__task void JoystickTask(void);
OS_TID idJoyTask;
__task void TextParse(void);
OS_TID idTextParse;

void SerialInit(void);


int main (void){
	// initialize hardware
	SerialInit();
	JOY_Init();
	LED_Init();
	SRAM_Init();
	
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
	os_sem_init(&sem_Timer10Hz,0);
	os_sem_init(&sem_Timer1Hz,0);
	os_sem_init(&sem_TextRx, 0);
	
	// initialize mutexes
	os_mut_init(&mut_osTimestamp);
	// the message input queue
	lstRXQ->count = 4;
	lstRXQ->first = NULL;
	lstRXQ->last  = NULL;
	lstRXQ->startAddr = MsgRXQ;
	// the storage lists
	os_mut_init(&mut_msgList);
	// free spaces
	os_mut_init(&mut_msgList);
	lstStrFree->count = 1000;
	lstStrFree->first = NULL;
	lstStrFree->last  = NULL;
	lstStrFree->startAddr = MsgSTR;
	// used spaces
	lstStrUsed->count = 0;
	lstStrUsed->first = NULL;
	lstStrUsed->last  = NULL;
	lstStrUsed->startAddr = NULL;
	
	List_init(lstRXQ);
	List_init(lstStrFree);
	
	// initialize mailboxes
	os_mbx_init(&mbx_MsgBuffer, sizeof(mbx_MsgBuffer));
	
	// initialize tasks
	idTimerTask = os_tsk_create(TimerTask, 150);		// Timer task is relatively important.
	idJoyTask 	= os_tsk_create(JoystickTask, 100);
	idClockTask = os_tsk_create(ClockTask, 175);		// high prio since we need to timestamp messages
	idTextParse = os_tsk_create(TextParse, 170);
	
	//sudoku
	os_tsk_delete_self();
}

// timer task for general-purpose polling and task triggering

__task void TimerTask(void){
	static uint32_t counter = 0;
	static uint32_t secondFreq = 1;				// frequencies in Hz of refresh rate for these events
	static uint32_t buttonInputFreq = 10;
	for(;;){
		os_dly_wait(10);	// base freq at 100Hz
		counter++;
		// set up if structure to get different timing frequencies
		if (counter%(100/buttonInputFreq) == 0){	// 100Hz/5Hz = 20, * 10ms = 200ms = 5Hz
			os_evt_set(timer10Hz, idJoyTask);
		}
		if (counter%(100/secondFreq) == 0){// 100*10ms = 1000ms = 1sec
			os_evt_set(timer1Hz, idClockTask);
		}
		counter %= 100; // makes sure the counter doesn't go over 100 (100*10ms = 1000ms = 1sec)
	}
}

__task void ClockTask(void){
	uint16_t flags;
	for(;;){
		// os_sem_wait(&sem_Timer1Hz,0xffff);
		os_evt_wait_or(timer1Hz|hourButton|minButton, 0xffff);
		// do the clock and timestamp stuff
		// check out the timestamp
		os_mut_wait(&mut_osTimestamp,0xffff);
		flags = os_evt_get();
		if (flags & timer1Hz == timer1Hz){
				osTimestamp.seconds++;
				osTimestamp.minutes += osTimestamp.seconds/60;
				osTimestamp.seconds %= 60;
				osTimestamp.hours 	+= osTimestamp.minutes/60;
				osTimestamp.minutes %= 60;
				osTimestamp.hours 	%= 24;
				os_evt_clr(timer1Hz, idClockTask);
		}
		if (flags & hourButton == hourButton){
				osTimestamp.hours++;
				osTimestamp.hours %= 24;
				os_evt_clr(hourButton, idClockTask);
		}
		if(flags & minButton == minButton){
				osTimestamp.minutes++;
				osTimestamp.minutes %= 60;
				os_evt_clr(minButton, idClockTask);
		}
		os_mut_release(&mut_osTimestamp);
	}
}

__task void JoystickTask(void){	// TODO:  must get this working after the data handling works.  Should set events for a screen task.
	static uint32_t newJoy, oldJoy = 0;
	for(;;){
		//os_sem_wait(&sem_Timer5Hz,0xffff);
		//while(os_sem_wait(&sem_Timer5Hz,0x0) != OS_R_TMO){}	// clear the semaphore (force binary behavior)
		// really this shouldn't ever be anything more thana binary semaphore but, just in case.
		os_evt_wait_and(timer10Hz,0xffff);
		
		newJoy = JOY_GetKeys();
		//os_mut_wait(&mutCursor,0xffff);
		if (newJoy != oldJoy){
			switch(newJoy){
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
			// send some event here
			oldJoy = newJoy;
		}
		//os_mut_release(&mutCursor);
		//os_sem_send(&semDisplayUpdate);
	
	}
}

// Text Parsing and Saving
__task void TextParse(void){
	static ListNode	*newmsg;
	for(;;){
		os_mbx_wait(&mbx_MsgBuffer, (void **)&newmsg, 0xffff);
		os_mut_wait(&mut_osTimestamp, 0xffff);
		os_mut_wait(&mut_msgList, 0xffff);
		
		
		
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
	static NodeData databuff;
	static uint8_t countData = 0;
	uint8_t sendflag = 0;
	uint8_t flag = USART3->SR & USART_SR_RXNE; // make a flag for the USART data buffer full & ready to read signal
	if(flag == USART_SR_RXNE){	// if the flag is set
		data = SER_GetChar();
		if(isr_mbx_check(mbx_MsgBuffer) > 0){
			if (data >= 0x20 && data <= 0x7E){	// exclude the backspace key, include space.  TODO implement a "backspace" feature!
				databuff.text[countData] = data; // store serial input to data buffer
				countData++;
			} else if (data == 0x7F){
				// backspace character
				countData--;
			} else if (data == 0x0D){
				// return
				databuff.cnt = countData;
				countData = 0;
				// send to mailbox
				sendflag = 1;
			}
			
			if (countData == 159){	// if we've filled a page
				databuff.cnt = countData;
				countData = 0;
				// send to mailbox
				sendflag = 1;
			}
			
			if (sendflag == 1){
				ListNode *rxnode = List_pop(lstStrFree);	// change to use OS stuff
				rxnode->data = databuff;
				isr_mbx_send(&mbx_MsgBuffer, rxnode);
			}
			
		}
		
	}		
}
