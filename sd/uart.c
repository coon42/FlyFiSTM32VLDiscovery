/*********************************************************************
 *
 * UART support - provides UARTx and UARTd support routines
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 *
 *********************************************************************
 * FileName:    uart.c
 * Depends:
 * Processor:   STM32F100RBT6B
 *
 * Author               Date       Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Kubik                16.11.2010 Initial code
 ********************************************************************/

//#include <stddef.h>
//#include <sys/unistd.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "uart.h"

//
// UARTx is used by printf

void InitializeUart(uint32_t baud_rate) {
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_ClocksTypeDef RCC_ClockFreq;

	// Configure peripherals used - enable their clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // Configure USART1 and USART2 Tx as alternate function push-pull
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Prepare parameters first - traditional 115.2 / 8b / no parity, no flow control
    USART_InitStructure.USART_BaudRate = baud_rate; // this is fast but sometimes you get transmission errors
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    // Configure and enable UARTx
    USART_Init(UARTx, &USART_InitStructure);
    USART_Cmd(UARTx, ENABLE);


    printf("UART test " __DATE__ "\r\n");
	RCC_GetClocksFreq(&RCC_ClockFreq);
	printf("SYSCLK = %d Mhz  HCLK = %d Mhz PCLK1 = %d Mhz PCLK2 = %d Mhz ADCCLK = %d Mhz\r\n", 
												RCC_ClockFreq.SYSCLK_Frequency / (1000 * 1000),
												RCC_ClockFreq.HCLK_Frequency / (1000 * 1000),
												RCC_ClockFreq.PCLK1_Frequency / (1000 * 1000),
												RCC_ClockFreq.PCLK2_Frequency / (1000 * 1000),
												RCC_ClockFreq.ADCCLK_Frequency / (1000 * 1000)
												);
}

unsigned char USART_getc(USART_TypeDef* USARTx)
{
	// wait until data register is empty
	/* Wait until the data is ready to be received. */
	while ((USARTx->SR & USART_SR_RXNE) == 0);

	// read RX data, combine with DR mask (we only accept a max of 9 Bits)
	return USARTx->DR & 0x1FF;
}

// This will print on usart 1 (the original function has been commented out in printf.c !!!
int fputc(int ch, FILE * f)
{
	/* Wait until transmit finishes */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

	/* Transmit the character using USART1 */
	USART_SendData(USART1, (u8) ch);


	return ch;
}

// receives an 32 bit integer from serial port. (little endian)
uint32_t USART1_recv_dword()
{
	uint32_t i, result = 0;
	for(i = 0; i < 4; i++)
	{
		uint32_t recv = USART_getc(USART1) << (i * 8);
		result |= recv;
	}

	return result;
}
