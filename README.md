RTFinal
=======
Final project for Real-Time Embedded Systems at Kettering University.

Edited using Keil uVision for an ARM Cortex-M3 Keil Development Board.  STM32fxx.
####Use RTX-RTOS to implement an application that meets the following behavioral requirements:
- The top line of the LCD display must display the current time in hours:minutes:seconds. 
- Pressing the WAKEUP button must increment the hours by 1. 
- Pressing the TAMPER button must increment the minutes by 1. 
- Text messages are received at a standard baud rate on USART3. There is no upper bound on the time between characters. 
- Characters in the text message are limited to basic printable ASCII such as letters, numbers, and punctuation; so the system does not need to process other characters with the exception of a carriage return character, and the carriage return signals the end of the message. 
- Text messages are limited to a maximum of 160 characters, not including the carriage return (which does not need to be stored with the string). Thus, once 160 characters are received, the message is automatically ended, and the next character begins a new text message. 
- If there are no stored text messages, the text message area of the LCD must either be blank or display an appropriate message indicating such.
- If there is at least one message, the LCD must display four lines (64 characters) of the message in the center of the LCD. The line below those must show the system time that the current message was completely received (i.e. the time at which the last character was received.) 
- The system must be able to store at least 1000 text messages. 
- Pressing the joystick up and down (relative to how the board is oriented when the messages are being read) must scroll the text message up and down one line at a time if the message exceeds 4 lines. 
- Pressing the joystick left and right must cycle through the messages. The system should stop at the ends of the list, i.e. the newest message on one end and the oldest message on the other end. 
- Pressing the joystick should prompt the user if the current message should be deleted. The user must use the joystick to confirm or cancel to operation. There are no specifics on the prompt or joystick commands, but they must be obvious and user-friendly. 
- Messages must be displayed according to the order in which they were received. 

####Implementation Requirements: 
- Text messages must be stored in a doubly-linked list in the external SRAM. It is acceptable to have some messages in the microprocessor memory for the purposes of receiving them from the PC or displaying them on the LCD. 
- All access to shared variables/data structures/lists must use an associated mutex to ensure data integrity. 
- ISR’s must not access global variables. 
- The code must be well-commented.

