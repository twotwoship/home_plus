#include "device_driver.h"
#include "dc_motor.h"

void DC_Motor_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 20); /* PB10 output */
	Macro_Clear_Bit(GPIOB->OTYPER, 10);
	Macro_Clear_Bit(GPIOB->ODR, 10);
}

void dc_motor_control(int on)
{
	if (on) Macro_Set_Bit(GPIOB->ODR, 10);
	else Macro_Clear_Bit(GPIOB->ODR, 10);
}
