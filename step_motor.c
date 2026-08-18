#include "device_driver.h"
#include "step_motor.h"

#define STEPS_PER_REVOLUTION 4096

static volatile int step_current;
static volatile int step_target;
static volatile unsigned int step_phase;

static const unsigned char half_step[8] = {
	0x8, 0xc, 0x4, 0x6, 0x2, 0x3, 0x1, 0x9
};

static void Step_Write(unsigned int pattern)
{
	GPIOC->BSRR = (pattern & 0x8U) ? (1U << 7) : (1U << (7 + 16));
	GPIOB->BSRR = (pattern & 0x4U) ? (1U << 6) : (1U << (6 + 16));
	GPIOA->BSRR = (pattern & 0x2U) ? (1U << 7) : (1U << (7 + 16));
	GPIOA->BSRR = (pattern & 0x1U) ? (1U << 6) : (1U << (6 + 16));
}

void Step_Motor_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Set_Bit(RCC->AHB1ENR, 2);
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x1, 14); /* PC7 */
	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 12); /* PB6 */
	Macro_Write_Block(GPIOA->MODER, 0xf, 0x5, 12); /* PA6, PA7 */
	Macro_Clear_Bit(GPIOC->OTYPER, 7);
	Macro_Clear_Bit(GPIOB->OTYPER, 6);
	Macro_Clear_Bit(GPIOA->OTYPER, 6);
	Macro_Clear_Bit(GPIOA->OTYPER, 7);
	Step_Write(0);

	Macro_Set_Bit(RCC->APB1ENR, 2);
	TIM4->CR1 = 0;
	TIM4->PSC = (TIMXCLK / 1000000U) - 1U;
	TIM4->ARR = 2000U - 1U;                         /* 2 ms per half-step */
	TIM4->DIER = 1U;
	TIM4->SR = 0;
	TIM4->EGR = 1U;
	NVIC_ClearPendingIRQ(TIM4_IRQn);
	NVIC_EnableIRQ(TIM4_IRQn);
	step_current = 0;
	step_target = 0;
	step_phase = 0;
}

void step_motor_control(int position_percent)
{
	if (position_percent < 0) position_percent = 0;
	if (position_percent > 100) position_percent = 100;
	step_target = (position_percent * STEPS_PER_REVOLUTION) / 100;
	if (step_target != step_current) {
		TIM4->CNT = 0;
		TIM4->SR = 0;
		Macro_Set_Bit(TIM4->CR1, 0);
	}
}

int Step_Motor_Is_Moving(void)
{
	return step_current != step_target;
}

void TIM4_IRQHandler(void)
{
	if ((TIM4->SR & 1U) == 0U) return;
	TIM4->SR = ~1U;
	if (step_current < step_target) {
		Step_Write(half_step[step_phase]);
		step_phase = (step_phase + 1U) & 7U;
		++step_current;
	} else if (step_current > step_target) {
		step_phase = (step_phase - 1U) & 7U;
		Step_Write(half_step[step_phase]);
		--step_current;
	} else {
		Macro_Clear_Bit(TIM4->CR1, 0);
		/* Keep the final phase energized so the commanded position is held. */
	}
}