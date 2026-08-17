#include "device_driver.h"
#include <stdio.h>

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

volatile int TIM4_Expired = 0;
volatile int TIM5_Expired = 0;

void TIM5_IRQHandler(void)
{
	Macro_Clear_Bit(TIM5->SR, 0);
	NVIC_ClearPendingIRQ(TIM5_IRQn);
	TIM5_Expired = 1;
}

void TIM4_IRQHandler(void)
{
	Macro_Clear_Bit(TIM4->SR, 0);
	NVIC_ClearPendingIRQ(30);
	TIM4_Expired = 1;
}

volatile int DMA2_STREAM0_DONE = 0;

void DMA2_Stream0_IRQHandler(void)
{
	DMA2->LIFCR = 0x3F << 0;
	NVIC_ClearPendingIRQ(56);
    DMA2_STREAM0_DONE = 1;
}

volatile int DMA1_STREAM6_DONE = 0;

void DMA1_Stream6_IRQHandler(void)
{
	DMA1->HIFCR = 0x3F << 16;
	NVIC_ClearPendingIRQ(17);
    DMA1_STREAM6_DONE = 1;
}

extern volatile int Uart_Data_In;
extern volatile unsigned char Uart_Data;

void USART2_IRQHandler(void)
{
	// 수신된 데이터는 Uart_Data에 저장
	Uart_Data = (unsigned char)USART2->DR;
	// Uart_Data_In Flag Setting
	Uart_Data_In = 1;
	// NVIC Pending Clear
	NVIC_ClearPendingIRQ(38);
}
