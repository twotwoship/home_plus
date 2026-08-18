#include "device_driver.h"
#include <stdio.h>


void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

volatile int DMA2_STREAM0_DONE = 0;

void DMA2_Stream0_IRQHandler(void)
{
	DMA2->LIFCR = 0x3F << 0;
	NVIC_ClearPendingIRQ(56);
    DMA2_STREAM0_DONE = 1;
}

volatile int DMA1_STREAM6_DONE = 1;

void DMA1_Stream6_IRQHandler(void)
{
	DMA1->HIFCR = 0x3F << 16;
	NVIC_ClearPendingIRQ(17);
    DMA1_STREAM6_DONE = 1;
}


void USART2_IRQHandler(void)
{
	char data;

    if (USART2->SR & (1U << 5))
    {
        data = (char)USART2->DR;

        Uart2_Rx_Enqueue(data);
    }
	// NVIC Pending Clear
	NVIC_ClearPendingIRQ(38);
}
