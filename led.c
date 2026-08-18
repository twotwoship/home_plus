#include "device_driver.h"

void LED_Init(void)
{
	/* 아래 코드 수정 금지 : Port-A Clock Enable */
	Macro_Set_Bit(RCC->AHB1ENR, 0); 

	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 10);
	Macro_Clear_Bit(GPIOA->OTYPER, 5);
	Macro_Clear_Bit(GPIOA->ODR, 5); 
	
	//외부 LED설정
	// LED를 출력으로 설정하고 초기 OFF
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 8);
	Macro_Clear_Bit(GPIOA->OTYPER, 4);
	Macro_Clear_Bit(GPIOA->ODR, 4); 
}

void LED_On(void)
{
	Macro_Set_Bit(GPIOA->ODR, 5); 
}

void LED_Off(void)
{
	Macro_Set_Bit(GPIOA->ODR, 5); ; 
}

void LED_Display(int on)
{
	Macro_Write_Block(GPIOA->ODR, 0x1, on & 0x1, 5);
}


void led_control(int on)
{
	if(on) GPIOA->BSRR = (1U << 4);
	else GPIOA->BSRR = (1U << (4 + 16));
}
