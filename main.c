/*********************************************************************
 *
 * Code testing the basic functionality of STM32 on VL discovery kit
 * The code displays message via UART1 using printf mechanism
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 *
 *********************************************************************
 * FileName:    main.c
 * Depends:
 * Processor:   STM32F100RBT6B
 *
 * Author               Date       Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Kubik                13.11.2010 Initial code
 * Kubik                14.11.2010 Added debug code
 * Kubik                15.11.2010 Debug now goes to UART2
 * Kubik                ??.11.2010 Added test code for quadrature encoder
 * Kubik                 4.12.2010 Implemented SD/MMC card support
 ********************************************************************/

// important:
//
// - commented out int fputc(int ch, FILE * f) in printf.c and redefined in main.h to get printf sending directly to USART1
// - commented out struct _reent *_impure_ptr = &r; in printf.c to prevent a linker error


//-------------------------------------------------------------------
// Includes
#include <stdio.h>
#include <inttypes.h>
#include "stm32f10x.h"
#include "uart.h"
#include "MoppyEx.h"





//***********************************************************************************************
// System-Einstellungen
//***********************************************************************************************

void SetupSystem (void)
{
	SystemInit();
	SystemCoreClockUpdate();
	InitializeUart(115200);
	//InitializeUart(921600);


	GPIO_InitTypeDef GPIO_InitStructure;
	uint32_t i, j;

	// Output SYSCLK clock on MCO pin
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	RCC_MCOConfig(RCC_MCO_SYSCLK);

	GPIO_WriteBit(GPIOA, GPIO_Pin_0, Bit_SET);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);


	if (SysTick_Config(SystemCoreClock / 1000)) {
		printf("Systick failed, system halted\n");
		while (1);
	}

	mount_sd_card();
	SetupMoppyEx();
}






//-------------------------------------------------------------------
// Defines

//---------------------------------------------------------------------------
// Static variables

//---------------------------------------------------------------------------
// Local functions



//---------------------------------------------------------------------------
// Local functions

int main(void) {
	SetupSystem();        // System-Einstellungen


	playMidiFile("haddaway.mid");
	//playMidiFile("overworld_deep3.mid");
	//playMidiFile("jammind3.mid");

	//floppy_play_midi_note(2, 40);

    // loop forever
    while(1);
}






















