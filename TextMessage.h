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

#ifndef __TXTMSG_H
#define __TXTMSG_H

// timestamp structure, to be used for the program itself and in each message
typedef struct _Timestamp {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
} Timestamp;


#endif /* __TXTMSG_H */
