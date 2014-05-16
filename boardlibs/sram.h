#include "stm32f2xx.h"

#ifndef _CY7C1071DV33_LIBRARY
	#define _CY7C1071DV33_LIBRARY

	void SRAM_Init(void);

	#define	mySRAM_BASE   (0x60000000UL | 0x08000000UL)
#endif
