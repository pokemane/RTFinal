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

/*/////////////////////////////////////////////////////////////////////////////////
* =================================================================================
*	CE-426 Real-Time Embedded Systems Final Project, Spring 2014
*	Jack Cravener and Spencer Scott
*	---------------------------------------------------------------------------------
* Purpose:  
*			To write a program that uses real-time concepts to implement a text
*			messaging system via RS-232 with Keil ARM Development Board.
*	---------------------------------------------------------------------------------
*	UI:	System clock in upper left corner of screen, total stored message count in 
*			upper right.  Messages are displayed in the center of the screen, 4 lines of 
*			16 characters at a time.  Messages can be navigated by using the 4 direction
*			joystick to the left of the screen.  Deletion of messages can be achieved by
*			pushing center button of joystick, then selecting "YES" and pressing the
*			center button again to confirm.
*----------------------------------------------------------------------------------
*	Message TX/RX:
*			Uses PuTTY to send messages, configure serial port to COM1 and 115200 baud.
*			Force echo and local line editing if you wish, the code supports backspacing
*			before a message is sent.  RETURN key will send a message.  Messages will
*			automatically send if the character limit of 160 is reached during typing.
* =================================================================================
*//////////////////////////////////////////////////////////////////////////////////

// use this as the base pointer for an OS-managed memory pool later.
ListNode *Storage = (ListNode *)mySRAM_BASE;

// the box for the receive queue
_declare_box(poolRXQ, sizeof(ListNode), 4);

// event flag masks.  These could be defines, but eh.
uint16_t timer10Hz = 0x0002;
uint16_t timer1Hz = 0x0001;

uint16_t hourButton = 0x0010;
uint16_t minButton = 0x0020;

uint16_t txtRx = 0x0001;

uint16_t dispClock = 0x8000;
uint16_t dispUser = 0x001f;
uint16_t joyDir = JOY_LEFT | JOY_RIGHT | JOY_UP | JOY_DOWN;	//0x001B, all but the center button
uint16_t joyPush = JOY_CENTER;
uint16_t newMsg = 0x4000;

/*
* structures, variables, and mutexes
*/

OS_MUT mut_osTimestamp;
Timestamp osTimestamp = { 0, 0, 0 };

OS_MUT mut_msgList;
OS_MUT mut_cursor;
OS_MUT mut_LCD;

struct Cursor{
	ListNode* msg;
	uint8_t row;
};
// cursor for which message, and where in the message
struct Cursor cursor;
List lstStr = {0, NULL, NULL};
ListNode dfltMsg;

void printToScreen(uint8_t time[], uint8_t pos,ListNode* dispNode);
void timeToString(uint8_t time[], Timestamp* timestamp);

// delcare mailbox for serial buffer
os_mbx_declare(mbx_MsgBuffer, 4);

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



int main(void){
	// initialize hardware
	SerialInit();
	JOY_Init();
	LED_Init();
	SRAM_Init();
	GLCD_Init();
	KBD_Init();

	// InitTask
	os_sys_init_prio(InitTask, 250);

}


/*
*		OS Tasks
*/

// initialization task

__task void InitTask(void){

	// initialize mutexes
	os_mut_init(&mut_osTimestamp);
	os_mut_init(&mut_cursor);
	os_mut_init(&mut_LCD);
		
	// the storage list
	os_mut_init(&mut_msgList);
	lstStr.count = 0;
	lstStr.first = NULL;
	lstStr.last = NULL;
	
	// this is the default message to show when you don't have any friends
	// who want you to come play in the park
	strcpy((char*)dfltMsg.data.text, "No new messages at this time.");
	dfltMsg.data.cnt = 29;

	// initialize box for receive queue
	_init_box(poolRXQ, sizeof(poolRXQ), sizeof(ListNode));

	// This is best part.
	// Give it the pool start (mySRAM_BASE).
	// The size of the box, which is 1000*sizeof(ListNode) plus some overhead.
	// Box size is the sizeof(ListNode).
	// Math taken from the _declare_box() macro for overhead and alignment calcs.
	// Last mul by 4 is to account for the chip having addressing by 4-byte words
	// and SRAM going by byte.
	_init_box(Storage, (((sizeof(ListNode)+3) / 4)*(1000) + 3) * 4, sizeof(ListNode));

	// initialize mailboxes
	os_mbx_init(&mbx_MsgBuffer, sizeof(mbx_MsgBuffer));

	// initialize tasks
	idTimerTask = os_tsk_create(TimerTask, 190);	// Timer task is relatively important.
	idJoyTask = os_tsk_create(JoystickTask, 101);	
	idClockTask = os_tsk_create(ClockTask, 189);	// high prio since we need to timestamp messages
	idTextRX = os_tsk_create(TextRX, 200);				// the most important thing this program does
	idDispTask = os_tsk_create(DisplayTask, 100);	// we can tolerate some lag on display output

	GLCD_Clear(Black);
	os_evt_set(joyDir, idDispTask);		// these two are to make these tasks run on wakeup
	os_evt_set(timer1Hz, idClockTask);// once to draw the entire screen.
	
	//sudoku
	os_tsk_delete_self();
}

/*
*		Display Task
*
*/

__task void DisplayTask(void){
	uint8_t stime[] = "00:00:00";	// time string pointer to give to time string construction function
	uint16_t flags;
	uint8_t delMode = FALSE;
	uint8_t select = FALSE;
	for (;;){
		os_evt_wait_or(dispUser | newMsg, 0xffff);	// waits on either the user input or a new message
		flags = os_evt_get();
		
		// reserve the message list (to read a new message possibly), the cursor, and the screen
		os_mut_wait(&mut_msgList, 0xFFFF);
		os_mut_wait(&mut_cursor, 0xffff);
		os_mut_wait(&mut_LCD, 0xffff);
		
		if(!delMode && !(flags & joyPush)){	// if in normal mode, and not entering delete mode
			if(lstStr.count != 0){		// if there are messages from your buddies
				if(lstStr.count == 1){	// if there is only one thing to display
					cursor.msg = lstStr.last;	// point us at the tail (where new messages are pushed)
				}
				switch (flags & dispUser){	// handle button pushing
					case JOY_RIGHT:	// cursor down
						// Scroll down in the message until we can see lines 6-10, no roll.
						// This and its counterpart in JOY_LEFT check the size of the message and determine
						// how far to scroll.
						cursor.row = cursor.row == 6 ? 6 : (cursor.msg->data.cnt / 16) > 4 ? (cursor.row+1)%7 : 0;
						break;
					case JOY_LEFT:	// cursor up
						// scroll up to line zero but don't roll over
						cursor.row = cursor.row == 0 ? 0 : (cursor.msg->data.cnt / 16) > 4 ? (cursor.row+6)%7 : 0;
						break;
					case JOY_UP: // cursor right
						if(cursor.msg->next != NULL){	// can we even go next?
							cursor.msg = cursor.msg->next;
							cursor.row = 0;	// resets to top of msg so you don't get confused, 
							// only happens if there's another message to see 
							// (so you don't accidentally jump to top of single msg)
						}
						break;
					case JOY_DOWN: // cursor left
						if(cursor.msg->prev != NULL){	// can we go backward?
							cursor.msg = cursor.msg->prev;
							cursor.row = 0;
						}
						break;
				}
				// display our message after we've determined where the cursor should be
				printToScreen(stime, cursor.row, cursor.msg);
			} else { // you're lonely and nobody wants to talk to you at this moment in time
				printToScreen(stime, 0, &dfltMsg);
			}
			
		} else if((flags & joyPush) && !delMode && lstStr.count > 0){	// if entering delete mode with a message
			delMode = TRUE;
			// display the DELETE? YES/NO messages
			GLCD_SetTextColor(White);
			GLCD_SetBackColor(Black);
			GLCD_DisplayString(8,0,1,(uint8_t*)"Delete Msg?");
			GLCD_DisplayString(9,0,1,(uint8_t*)"Yes");
			GLCD_SetTextColor(Black);
			GLCD_SetBackColor(Red);
			GLCD_DisplayString(9,4,1,(uint8_t*)"No");
			select = FALSE;
		} else if (!(flags & joyPush) && delMode && lstStr.count > 0){ // if navigating in delete mode
			// swap between YES and NO
			select = select == FALSE ? TRUE : FALSE;
			// visually swap too
			if (select){
				GLCD_SetTextColor(Black);
				GLCD_SetBackColor(Red);
				GLCD_DisplayString(9,0,1,(uint8_t*)"Yes");
				GLCD_SetTextColor(White);
				GLCD_SetBackColor(Black);
				GLCD_DisplayString(9,4,1,(uint8_t*)"No");
			} else {
				GLCD_SetTextColor(White);
				GLCD_SetBackColor(Black);
				GLCD_DisplayString(9,0,1,(uint8_t*)"Yes");
				GLCD_SetTextColor(Black);
				GLCD_SetBackColor(Red);
				GLCD_DisplayString(9,4,1,(uint8_t*)"No");
			}
		} else if ((flags & joyPush) && delMode && lstStr.count > 0){ // if confirming choice.
			if (select){ // if we're deleting the message
				ListNode *delnode = List_remove(&lstStr, cursor.msg);
				// try to go one way, and then check the other, and if nothing, NULL.
				cursor.msg = delnode->prev ? delnode->prev : delnode->next ? delnode->next : NULL;
			}
			delMode = FALSE;
			select = FALSE;
			GLCD_SetBackColor(Black);
			GLCD_SetTextColor(White);
			GLCD_DisplayString(8,0,1,(uint8_t *)"           ");
			GLCD_DisplayString(9,0,1,(uint8_t *)"   ");
			GLCD_DisplayString(9,4,1,(uint8_t *)"  ");
			os_evt_set(newMsg, idDispTask);
		}
		
		// diplaying the number of total messages in storage
		GLCD_SetTextColor(White);
		GLCD_SetBackColor(Black);
		
		GLCD_DisplayChar(0,17,1,lstStr.count%10+0x30);
		GLCD_DisplayChar(0,16,1,(lstStr.count/10%10)+0x30);
		GLCD_DisplayChar(0,15,1,lstStr.count/100%10+0x30);
		GLCD_DisplayChar(0,14,1,lstStr.count/1000%10+0x30);
		
		os_mut_release(&mut_msgList);
		os_mut_release(&mut_cursor);
		os_mut_release(&mut_LCD);
		
		os_evt_clr(dispUser, idDispTask);	// ready for next go-around
	}
}


/*	
*	printToScreen(), helper function to display messages on-screen with timestamp.
*	@time[] 		is pointer to time char array to be modified and displayed
* @pos 				is our current vertical cursor position, which line have we selected
* @dispNode* 	is the node from which to read the text data
*
*/
void printToScreen(uint8_t time[], uint8_t pos, ListNode* dispNode){
	uint8_t i=0;
	uint8_t lineOffset=3;	// beginning positions on screen
	uint8_t colOffset=2;
	for(i=0;i<64;i++){	// we can only print 4 lines of 16 char (64 char)
		if((i+pos*16)<dispNode->data.cnt){	// if the character we're trying to print lies within the message bounds
			GLCD_SetTextColor(White);
			GLCD_SetBackColor(Black);
			// give the row, column, and character to display.
			GLCD_DisplayChar((i/16)+lineOffset, (i%16)+colOffset, 1, dispNode->data.text[i+pos*16]);
		} else {	// if we're outside the bounds of the message.
			GLCD_DisplayChar((i/16)+lineOffset, (i%16)+colOffset, 1, *" ");
		}
	}
	// interpret and display time.
	timeToString(time, &(dispNode->data.time));
	GLCD_DisplayString(9,12,1,time);
}
/*
*	timeToString(), helper function to turn message and OS timestamps into a string format
*	@time[] 		char array which will be displayed
*	@timestamp* is the timestamp to be interpreted.  Can be from a message or the os's timestamp.
*/
void timeToString(uint8_t time[], Timestamp* timestamp){
	time[0] = timestamp->hours / 10 + 0x30;
	time[1] = timestamp->hours % 10 + 0x30;
	time[3] = timestamp->minutes / 10 + 0x30;
	time[4] = timestamp->minutes % 10 + 0x30;
	time[6] = timestamp->seconds / 10 + 0x30;
	time[7] = timestamp->seconds % 10 + 0x30;
}

// timer task for general-purpose polling and task triggering
__task void TimerTask(void){
	static uint32_t counter = 0;
	static uint32_t secondFreq = 1;				// frequencies in Hz of refresh rate for these events
	static uint32_t buttonInputFreq = 10;
	for (;;){
		os_dly_wait(10);	// base freq at 100Hz
		counter++;
		// set up if structure to get different timing frequencies
		if (counter % (100 / buttonInputFreq) == 0){	// 100Hz/10Hz = 10 * 10ms = 100ms = 10Hz
			os_evt_set(timer10Hz, idJoyTask);
		}
		if (counter % (100 / secondFreq) == 0){// 100*10ms = 1000ms = 1sec
			os_evt_set(timer1Hz, idClockTask);
		}
		counter %= 100; // makes sure the counter doesn't go over 100 (100*10ms = 1000ms = 1sec)
	}
}


/*
*		Clock Task.  Runs at 1Hz off the timer, or when the user wants to set the time.
*/
__task void ClockTask(void){
	uint16_t flags;
	uint8_t time[] = "00:00:00";
	for (;;){
		os_evt_wait_or(timer1Hz | hourButton | minButton, 0xffff);	// wait for timer or buttons
		flags = os_evt_get();
		if (flags & timer1Hz){	// if it's time to increment by one second
			os_mut_wait(&mut_osTimestamp, 0xffff);
			// increment seconds, roll overflow down to hours if need be.
			osTimestamp.seconds++;
			osTimestamp.minutes += osTimestamp.seconds / 60;
			osTimestamp.seconds %= 60;
			osTimestamp.hours += osTimestamp.minutes / 60;
			osTimestamp.minutes %= 60;
			osTimestamp.hours %= 24;

			os_evt_clr(timer1Hz, idClockTask);
			flags = os_evt_get();		// clear and get new flag, in case a button and 1Hz
															// happened at the same time
			os_mut_release(&mut_osTimestamp);
		}
		if (flags & hourButton){	// hour button, increment and roll, no overflow
			os_mut_wait(&mut_osTimestamp, 0xffff);

			osTimestamp.hours++;
			osTimestamp.hours %= 24;
			os_evt_clr(hourButton, idClockTask);
			flags = os_evt_get();
			os_mut_release(&mut_osTimestamp);
		}
		if (flags & minButton){		// minute button, increment and roll, no overflow
			os_mut_wait(&mut_osTimestamp, 0xffff);

			osTimestamp.minutes++;
			osTimestamp.minutes %= 60;
			os_evt_clr(minButton, idClockTask);
			flags = os_evt_get();
			os_mut_release(&mut_osTimestamp);
		}
		// display the system time in the top left of the screen.
		os_mut_wait(&mut_osTimestamp, 0xffff);
		os_mut_wait(&mut_LCD,0xffff);
		
		GLCD_SetBackColor(Black);
		GLCD_SetTextColor(White);
		timeToString(time,&osTimestamp);
		GLCD_DisplayString(0, 0, 1, time);
		
		os_mut_release(&mut_osTimestamp);
		os_mut_release(&mut_LCD);
		// ---------------
	}
}

/*
*		Joystick (button and keyboard) handler.
*		This really only polls the keyboard and joystick and then
*		notifies the events that should be concerned about what happens.
*/
__task void JoystickTask(void){	
	static uint32_t newJoy, oldJoy, newKeys, oldKeys = 0;
	for (;;){
		os_evt_wait_and(timer10Hz, 0xffff);	// polling speed
		newJoy = JOY_GetKeys();
		if (newJoy != oldJoy){	// No repeating key presses without letting go first
			os_evt_set(newJoy, idDispTask);	// send the input to the display task
			oldJoy = newJoy;
		}
		
		newKeys = KBD_GetKeys();
		if (newKeys != oldKeys){	// same story as above
			switch (newKeys){
				case 1:	// wakeup
					os_evt_set(hourButton, idClockTask);	// notify clock
					break;
				case 2:	// tamper
					os_evt_set(minButton, idClockTask);
					break;
			}
			oldKeys = newKeys;
		}

	}
}

/*
*		Test Rx handling.
*		waits for the ISR to put a completed message in the mailbox,
*		and then copies the information over to the linked list
*/
__task void TextRX(void){
	static ListNode	*newmsg;
	static ListNode *message;
	for (;;){
		os_mbx_wait(&mbx_MsgBuffer, (void **)&newmsg, 0xffff);
		os_mut_wait(&mut_osTimestamp, 0xffff);
		os_mut_wait(&mut_msgList, 0xffff);

		message = _alloc_box(Storage);
		message->data = newmsg->data;		// I'm so happy this works the way I expected.
		message->data.time = osTimestamp;	// time = stamped
		List_push(&lstStr, message);		// put our thing as the most recent message
		_free_box(poolRXQ, newmsg);			// free up the memory used for the mailbox
		os_evt_set(newMsg, idDispTask);

		os_mut_release(&mut_osTimestamp);
		os_mut_release(&mut_msgList);
	}
}


/*
*		Serial Initialization and ISR
*/

void SerialInit(void){
	SER_Init();
	NVIC->ISER[USART3_IRQn / 32] = (uint32_t)1 << (USART3_IRQn % 32); // enable USART3 IRQ
	NVIC->IP[USART3_IRQn] = 0xE0; // set priority for USART3 to 0xE0
	USART3->CR1 |= USART_CR1_RXNEIE; // enable device interrupt for data received and ready to read
}


/*
*		Ingress point for data.  Does sizing calculations, key parsing, and 
*		determines when to send the data on to the rest of the program.
*		It's a little bulky for an ISR, but it works on a relatively
*		high serial baud rate, even with ~100 pages of copy-pasted
*		text thrown directly into PuTTY.  So, we're satisfied overall.
*/
void USART3_IRQHandler(void){
	static uint8_t data;	// static to keep a running tally
	static NodeData databuff;	// static buffer that just gets used over and over
	static uint8_t countData = 0;	// our place in the buffer
	static uint8_t sendflag = FALSE;
	static ListNode *rxnode;

	uint8_t flag = USART3->SR & USART_SR_RXNE; // make a flag for the USART data buffer full & ready to read signal

	if (flag == USART_SR_RXNE){	// if the flag is set.  If not, do nothing.
		data = SER_GetChar();
		if (isr_mbx_check(mbx_MsgBuffer) > 0){
			if (data >= 0x20 && data <= 0x7E){	// exclude the backspace key, include space.
				databuff.text[countData] = data; // store serial input to data buffer
				countData++;
				if (countData == 160){	// if we've filled a page
					databuff.cnt = countData;	// record the size of the message
					countData = 0;	// reset the buffer data
					// send to mailbox
					sendflag = TRUE;
				}
			}else if (data == 0x7F){ // backspace character
				countData = (countData + 159) % 160;	// move the cursor back one, and clamp at zero

			}else if (data == 0x0D){ // return
				databuff.cnt = countData;
				if(countData >0){	// send to mailbox, reset index
					sendflag = TRUE;
					countData = 0;
				}
				
				
			}

			if (sendflag == TRUE){	// we're sending
				rxnode = _alloc_box(poolRXQ);	// get some memory from the rx pool
				strcpy((char*)rxnode->data.text, (char*)databuff.text);	// copy data into new block
				rxnode->data.cnt = databuff.cnt;	// record how many characters we care about.
																					// Could use \0 but eh.
				isr_mbx_send(&mbx_MsgBuffer, rxnode);
				sendflag = FALSE;
			}
		}
	}
}
