#include "device_driver.h"
#include "dc_motor.h"

#define DC_MOTOR_PIN   10U
#define DC_MOTOR_MASK  (1U << DC_MOTOR_PIN)

void DC_Motor_Init(void)
{
	RCC->AHB1ENR |= (1U << 1);  /* GPIOB clock */

	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 20);
	Macro_Clear_Bit(GPIOB->OTYPER, DC_MOTOR_PIN);

	/* 초기 상태: OFF */
	GPIOB->BSRR = DC_MOTOR_MASK << 16U;
}

void dc_motor_control(int on)
{
	if (on) {
		/* PB10 HIGH: 모터 ON */
		GPIOB->BSRR = DC_MOTOR_MASK;
	} else {
		/* PB10 LOW: 모터 OFF */
		GPIOB->BSRR = DC_MOTOR_MASK << 16U;
	}
}