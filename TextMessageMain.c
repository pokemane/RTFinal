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
//#define MESSAGE_STR_OFFSET 4*sizeof(ListNode)
//uint32_t *MsgRXQ = (uint32_t *)mySRAM_BASE;	// the receive queue list
//uint32_t *MsgSTR = (uint32_t *)(mySRAM_BASE+MESSAGE_STR_OFFSET);	// the long-term storage list space

ListNode *Storage = (ListNode *)mySRAM_BASE;

// the box for the receive queue
_declare_box(poolRXQ, sizeof(ListNode), 4);

// semaphores and events
OS_SEM sem_Timer10Hz;
OS_SEM sem_Timer1Hz;
OS_SEM sem_TextRx;
OS_SEM sem_dispUpdate;

// event flag masks
uint16_t timer10Hz = 0x0002;
uint16_t timer1Hz = 0x0001;

uint16_t hourButton = 0x0010;
uint16_t minButton	= 0x0020;

uint16_t txtRx 		= 0x0001;

uint16_t dispClock = 0x0001;
uint16_t dispUser = 0x0002;

/*
* structures, variables, and mutexes
*/

OS_MUT mut_osTimestamp;
Timestamp osTimestamp = {0,0,0};

OS_MUT mut_msgList;
OS_MUT mut_freeList;
List *lstRXQ;
List *lstStr;
// List *lstStrFree;

uint8_t *timeToString(void);

// delcare mailbox for serial buffer
os_mbx_declare(mbx_MsgBuffer,4);

__task void InitTask(void);
__task void TimerTask(void);
OS_TID idTimerTask;
__task void ClockTask(void);
OS_TID idClockTask;
__task void JoystickTask(void);
OS_TID idJoyTask;
__task void TextRX(void);
OS_TID idTextRX;
__task void DisplayTask(void);
OS_TID idDispTask;

void SerialInit(void);


int main (void){
	// initialize hardware
	SerialInit();
	JOY_Init();
	LED_Init();
	SRAM_Init();
	GLCD_Init();
	
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
	os_sem_init(&sem_dispUpdate,0);
	
	// initialize mutexes
	os_mut_init(&mut_osTimestamp);
	// the message input queue
	lstRXQ->count = 4;
	lstRXQ->first = NULL;
	lstRXQ->last  = NULL;
	// lstRXQ->startAddr = MsgRXQ;
	// the storage lists
	os_mut_init(&mut_msgList);
	// free spaces
	os_mut_init(&mut_msgList);
	// lstStrFree->startAddr = MsgSTR;
	// used spaces
	lstStr->count = 0;
	lstStr->first = NULL;
	lstStr->last  = NULL;
	// lstStrUsed->startAddr = NULL;
	
	// initialize box for receive queue
	_init_box(poolRXQ, sizeof(poolRXQ), sizeof(ListNode));
	
	// This is literally the holy grail part.
	// give it the pool start (mySRAM_BASE)
	// the size of the box, which is 1000*sizeof(ListNode) plus some overhead
	// box size is the sizeof(ListNode).
	// math taken from the _declare_box() macro for overhead and alignment calcs
	_init_box(Storage, (((sizeof(ListNode)+3)/4)*(1000) + 3)*4, sizeof(ListNode));
	
	
	// List_init(lstRXQ);
	// List_init(lstStrFree);
	
	// initialize mailboxes
	os_mbx_init(&mbx_MsgBuffer, sizeof(mbx_MsgBuffer));
	
	// initialize tasks
	idTimerTask = os_tsk_create(TimerTask, 150);		// Timer task is relatively important.
	idJoyTask 	= os_tsk_create(JoystickTask, 100);
	idClockTask = os_tsk_create(ClockTask, 175);		// high prio since we need to timestamp messages
	idTextRX = os_tsk_create(TextRX, 170);
	idDispTask = os_tsk_create(DisplayTask, 150);
	
	GLCD_Clear(Black);
	
	//sudoku
	os_tsk_delete_self();
}

/*
*		Display Task
*
*/

__task void DisplayTask(void){
	uint16_t flag;
	for(;;){
		os_evt_wait_or(dispClock|dispUser,0xffff);
		flag = os_evt_get();
		if (flag|dispClock == dispClock){
			os_mut_wait(&mut_osTimestamp,0xffff);
			GLCD_SetBackColor(Black);
			GLCD_SetTextColor(White);
			GLCD_DisplayString(0,0,1,timeToString());
			os_mut_release(&mut_osTimestamp);
		}
			
	}
}

uint8_t* timeToString(void){
											 // 012345678
	volatile static uint8_t *time = "00:00:00";
	static uint8_t offset = 0x30;
	
	time[0] = osTimestamp.hours/10+offset;
	time[1] = osTimestamp.hours%10+offset;
	time[3] = osTimestamp.minutes/10+offset;
	time[4] = osTimestamp.minutes%10+offset;
	time[6] = osTimestamp.seconds/10+offset;
	time[7] = osTimestamp.seconds%10+offset;
	
	return time;
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
		os_evt_wait_or(timer1Hz|hourButton|minButton, 0xffff);
		flags = os_evt_get();
		if ((flags & timer1Hz) == timer1Hz){
			os_mut_wait(&mut_osTimestamp,0xffff);
		
			osTimestamp.seconds++;
			osTimestamp.minutes += osTimestamp.seconds/60;
			osTimestamp.seconds %= 60;
			osTimestamp.hours 	+= osTimestamp.minutes/60;
			osTimestamp.minutes %= 60;
			osTimestamp.hours 	%= 24;
			
			os_evt_clr(timer1Hz, idClockTask);
			flags = os_evt_get();
			os_mut_release(&mut_osTimestamp);
			os_evt_set(dispClock,idDispTask);
		}
		if ((flags & hourButton) == hourButton){
			os_mut_wait(&mut_osTimestamp,0xffff);
		
			osTimestamp.hours++;
			osTimestamp.hours %= 24;
			os_evt_clr(hourButton, idClockTask);
			flags = os_evt_get();
			os_mut_release(&mut_osTimestamp);
			os_evt_set(dispClock,idDispTask);
		}
		if((flags & minButton) == minButton){
			os_mut_wait(&mut_osTimestamp,0xffff);
		
			osTimestamp.minutes++;
			osTimestamp.minutes %= 60;
			os_evt_clr(minButton, idClockTask);
			flags = os_evt_get();
			os_mut_release(&mut_osTimestamp);
			os_evt_set(dispClock,idDispTask);
		}
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
__task void TextRX(void){
	static ListNode	*newmsg;
	static ListNode *message;
	for(;;){
		os_mbx_wait(&mbx_MsgBuffer, (void **)&newmsg, 0xffff);
		os_mut_wait(&mut_osTimestamp, 0xffff);
		os_mut_wait(&mut_msgList, 0xffff);
		
		message = _alloc_box(Storage);
		message->data = newmsg->data;
		message->data.time = osTimestamp;
		List_push(lstStr, message);	// put our thing as the most recent message
		// TODO:  implement a "You've Got Mail" counting semaphore maybe.
		_free_box(poolRXQ, newmsg);
		
		os_mut_release(&mut_osTimestamp);
		os_mut_release(&mut_msgList);
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
	static uint8_t data;	// static to keep a running tally
	static NodeData databuff;	// static buffer that just gets used over and over
	static uint8_t countData = 0;	// our place in the buffer
	static uint8_t sendflag = FALSE;
	static ListNode *rxnode;
	
	uint8_t flag = USART3->SR & USART_SR_RXNE; // make a flag for the USART data buffer full & ready to read signal

	if(flag == USART_SR_RXNE){	// if the flag is set.  If not, do nothing.
		data = SER_GetChar();
		if(isr_mbx_check(mbx_MsgBuffer) > 0){
			if (data >= 0x20 && data <= 0x7E){	// exclude the backspace key, include space.  TODO implement a "backspace" feature!
				databuff.text[countData] = data; // store serial input to data buffer
				countData++;
				if (countData == 160){	// if we've filled a page
					databuff.cnt = countData;	// record the size of the message
					countData = 0;	// reset the buffer data
					// send to mailbox
					sendflag = TRUE;
				}
			} else if (data == 0x7F){
				// backspace character
				countData = (countData + 159)%160;	// move the cursor back one, and clamp at zero
				
			} else if (data == 0x0D){
				// return
				databuff.cnt = countData;
				countData = 0;
				// send to mailbox
				sendflag = TRUE;
			}
			
			if (sendflag == TRUE){
				rxnode = _alloc_box(poolRXQ);	// TODO: change to use OS stuff
				rxnode->data = databuff;
				isr_mbx_send(&mbx_MsgBuffer, rxnode);
			}
		}
	}		
}
