#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"

enum Ultra_State { ULTRA_IDLE, ULTRA_WAIT_RISE, ULTRA_WAIT_FALL };

static volatile enum Ultra_State ultra_state;
static volatile unsigned int ultra_rise;
static volatile unsigned int ultra_pulse_us;
static volatile int ultra_done;
static volatile int ultra_error;

static void Ultrasonic_Finish(int error)
{
	TIM1->DIER &= ~(1U << 3);
	TIM1->CCER &= ~(1U << 8);
	Timer2_Cancel_CC2();
	Timer2_Cancel_CC3();
	Macro_Clear_Bit(GPIOB->ODR, 5);
	ultra_error = error;
	ultra_state = ULTRA_IDLE;
	ultra_done = 1;
}

void Ultrasonic_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Set_Bit(RCC->APB2ENR, 0);

	/* PB5: Trigger, push-pull output, initial low. */
	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 10);
	Macro_Clear_Bit(GPIOB->OTYPER, 5);
	Macro_Clear_Bit(GPIOB->ODR, 5);

	/* PA10: TIM1_CH3, AF1 input capture. */
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x2, 20);
	Macro_Write_Block(GPIOA->AFR[1], 0xf, 0x1, 8);
	Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x0, 20);

	TIM1->CR1 = 0;
	TIM1->PSC = (TIM1CLK / 1000000U) - 1U;
	TIM1->ARR = 0xffffU;
	TIM1->CCMR2 = (TIM1->CCMR2 & ~0x3U) | 0x1U;
	TIM1->CCER &= ~((1U << 8) | (1U << 9) | (1U << 11));
	TIM1->DIER &= ~(1U << 3);
	TIM1->SR = 0;
	TIM1->EGR = 1U;
	TIM1->CR1 = 1U;
	NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
	NVIC_EnableIRQ(TIM1_CC_IRQn);
	ultra_state = ULTRA_IDLE;
}

void Timer2_CC2_Callback(void)
{
	Macro_Clear_Bit(GPIOB->ODR, 5);
}

void Timer2_CC3_Callback(void)
{
	if (ultra_state != ULTRA_IDLE) Ultrasonic_Finish(1);
}

void TIM1_CC_IRQHandler(void)
{
	unsigned int captured;
	if (((TIM1->SR & (1U << 3)) == 0U) ||
	    ((TIM1->DIER & (1U << 3)) == 0U)) return;

	captured = TIM1->CCR3;
	TIM1->SR = ~(1U << 3);
	if (ultra_state == ULTRA_WAIT_RISE) {
		ultra_rise = captured;
		Macro_Set_Bit(TIM1->CCER, 9);
		ultra_state = ULTRA_WAIT_FALL;
	} else if (ultra_state == ULTRA_WAIT_FALL) {
		ultra_pulse_us = (captured - ultra_rise) & 0xffffU;
		Ultrasonic_Finish(0);
	}
}

int ultra_sonic_measurement(void)
{
	unsigned int pulse;
	if (ultra_state != ULTRA_IDLE) return -1;

	ultra_done = 0;
	ultra_error = 0;
	ultra_pulse_us = 0;
	ultra_state = ULTRA_WAIT_RISE;
	Macro_Clear_Bit(TIM1->CCER, 9);
	TIM1->SR = ~(1U << 3);
	Macro_Set_Bit(TIM1->DIER, 3);
	Macro_Set_Bit(TIM1->CCER, 8);

	Macro_Set_Bit(GPIOB->ODR, 5);
	Timer2_Arm_CC2(10U);
	Timer2_Arm_CC3(30000U);

	while (!ultra_done) __WFI();
	if (ultra_error) return -1;
	pulse = ultra_pulse_us;
	if (pulse < 116U || pulse > 23200U) return -1;
	return (int)((pulse + 29U) / 58U);
}