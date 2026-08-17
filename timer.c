#include "device_driver.h"
#include "timer.h"

#define TIM2_TICK         	(20) 					// usec
#define TIM2_FREQ 	  		(1000000./TIM2_TICK)	// Hz
#define TIME2_PLS_OF_1ms  	(1000./TIM2_TICK)
#define TIM2_MAX	  		(0xffffffffu)

static volatile int timer_wait_done;

/*
 * Shared 1 MHz, 32-bit time base for sensor state machines.
 * SysTick is deliberately not touched because it is owned by the UART module.
 */
void Sensor_Timer_Init(void)
{
	Macro_Set_Bit(RCC->APB1ENR, 0);
	TIM2->CR1 = 0;
	TIM2->PSC = (TIMXCLK / 1000000U) - 1U;
	TIM2->ARR = 0xffffffffU;
	TIM2->CNT = 0;
	TIM2->DIER = 0;
	TIM2->SR = 0;
	TIM2->EGR = 1U;
	TIM2->CR1 = 1U;
	NVIC_ClearPendingIRQ(TIM2_IRQn);
	NVIC_EnableIRQ(TIM2_IRQn);
}

uint32_t Timer2_Micros(void)
{
	return TIM2->CNT;
}

static void Timer2_Arm(unsigned int channel, uint32_t delay_us)
{
	volatile uint32_t *ccr = &TIM2->CCR1 + channel;
	uint32_t flag = 1U << (channel + 1U);
	*ccr = TIM2->CNT + delay_us;
	TIM2->SR = ~flag;
	TIM2->DIER |= flag;
}

static void Timer2_Cancel(unsigned int channel)
{
	uint32_t flag = 1U << (channel + 1U);
	TIM2->DIER &= ~flag;
	TIM2->SR = ~flag;
}

void Timer2_Arm_CC1(uint32_t us) { Timer2_Arm(0U, us); }
void Timer2_Arm_CC2(uint32_t us) { Timer2_Arm(1U, us); }
void Timer2_Arm_CC3(uint32_t us) { Timer2_Arm(2U, us); }
void Timer2_Cancel_CC1(void) { Timer2_Cancel(0U); }
void Timer2_Cancel_CC2(void) { Timer2_Cancel(1U); }
void Timer2_Cancel_CC3(void) { Timer2_Cancel(2U); }

void Timer2_Timeout_Start(uint32_t delay_us)
{
	if (delay_us == 0U) delay_us = 1U;
	timer_wait_done = 0;
	Timer2_Arm(3U, delay_us);
}

void Timer2_Timeout_Cancel(void)
{
	Timer2_Cancel(3U);
}

int Timer2_Timeout_Expired(void)
{
	return timer_wait_done;
}

__attribute__((weak)) void Timer2_CC1_Callback(void) { }
__attribute__((weak)) void Timer2_CC2_Callback(void) { }
__attribute__((weak)) void Timer2_CC3_Callback(void) { }

void TIM2_IRQHandler(void)
{
	uint32_t pending = TIM2->SR & TIM2->DIER;
	if (pending & (1U << 1)) {
		Timer2_Cancel_CC1();
		Timer2_CC1_Callback();
	}
	if (pending & (1U << 2)) {
		Timer2_Cancel_CC2();
		Timer2_CC2_Callback();
	}
	if (pending & (1U << 3)) {
		Timer2_Cancel_CC3();
		Timer2_CC3_Callback();
	}
	if (pending & (1U << 4)) {
		Timer2_Cancel(3U);
		timer_wait_done = 1;
	}
}

void Timer_Wait_Ms(uint32_t delay_ms)
{
	if (delay_ms == 0U) return;
	Timer2_Timeout_Start(delay_ms * 1000U);
	while (!timer_wait_done) __WFI();
}

#define TIM2_TICK         	(20) 				// usec
#define TIM2_FREQ 	  		(1000000/TIM2_TICK)	// Hz
#define TIME2_PLS_OF_1ms  	(1000/TIM2_TICK)
#define TIM2_MAX	  		(0xffffu)

#define TIM4_TICK	  		(20) 				// usec
#define TIM4_FREQ 	  		(1000000/TIM4_TICK) // Hz
#define TIME4_PLS_OF_1ms  	(1000/TIM4_TICK)
#define TIM4_MAX	  		(0xffffu)

void TIM2_Stopwatch_Start(void)
{
	Macro_Set_Bit(RCC->APB1ENR, 0);

	TIM2->CR1 = (1<<4)|(1<<3);
	TIM2->PSC = (unsigned int)(TIMXCLK/50000.0 + 0.5)-1;
	TIM2->ARR = TIM2_MAX;

	Macro_Set_Bit(TIM2->EGR,0);
	Macro_Set_Bit(TIM2->CR1, 0);
}

unsigned int TIM2_Stopwatch_Stop(void)
{
	unsigned int time;

	Macro_Clear_Bit(TIM2->CR1, 0);
	time = (TIM2_MAX - TIM2->CNT) * TIM2_TICK;
	return time;
}

/* Delay Time Max = 65536 * 20use = 1.3sec */

#if 0

void TIM2_Delay(int time)
{
	Macro_Set_Bit(RCC->APB1ENR, 0);

	TIM2->CR1 = (1<<4)|(1<<3);
	TIM2->PSC = (unsigned int)(TIMXCLK/(double)TIM2_FREQ + 0.5)-1;
	TIM2->ARR = TIME2_PLS_OF_1ms * time;

	Macro_Set_Bit(TIM2->EGR,0);
	Macro_Clear_Bit(TIM2->SR, 0);
	Macro_Set_Bit(TIM2->CR1, 0);

	while(Macro_Check_Bit_Clear(TIM2->SR, 0));

	Macro_Clear_Bit(TIM2->CR1, 0);
}

#else

/* Delay Time Extended */

void TIM2_Delay(int time)
{
	int i;
	unsigned int t = TIME2_PLS_OF_1ms * time;

	Macro_Set_Bit(RCC->APB1ENR, 0);

	TIM2->PSC = (unsigned int)(TIMXCLK/(double)TIM2_FREQ + 0.5)-1;
	TIM2->CR1 = (1<<4)|(1<<3);
	TIM2->ARR = 0xffff;
	Macro_Set_Bit(TIM2->EGR,0);

	for(i=0; i<(t/0xffffu); i++)
	{
		Macro_Set_Bit(TIM2->EGR,0);
		Macro_Clear_Bit(TIM2->SR, 0);
		Macro_Set_Bit(TIM2->CR1, 0);
		while(Macro_Check_Bit_Clear(TIM2->SR, 0));
	}

	TIM2->ARR = t % 0xffffu;
	Macro_Set_Bit(TIM2->EGR,0);
	Macro_Clear_Bit(TIM2->SR, 0);
	Macro_Set_Bit(TIM2->CR1, 0);
	while (Macro_Check_Bit_Clear(TIM2->SR, 0));

	Macro_Clear_Bit(TIM2->CR1, 0);
}

#endif

void TIM4_Repeat(int time)
{
	Macro_Set_Bit(RCC->APB1ENR, 2);

	TIM4->CR1 = (1<<4)|(0<<3);
	TIM4->PSC = (unsigned int)(TIMXCLK/(double)TIM4_FREQ + 0.5)-1;
	TIM4->ARR = TIME4_PLS_OF_1ms * time - 1;

	Macro_Set_Bit(TIM4->EGR,0);
	Macro_Clear_Bit(TIM4->SR, 0);
	Macro_Set_Bit(TIM4->CR1, 0);
}

int TIM4_Check_Timeout(void)
{
	if(Macro_Check_Bit_Set(TIM4->SR, 0))
	{
		Macro_Clear_Bit(TIM4->SR, 0);
		return 1;
	}
	else
	{
		return 0;
	}
}

void TIM4_Stop(void)
{
	Macro_Clear_Bit(TIM4->CR1, 0);
}

void TIM4_Change_Value(int time)
{
	TIM4->ARR = TIME4_PLS_OF_1ms * time;
}

void TIM4_Repeat_Interrupt_Enable(int en, int time)
{
	if(en)
	{
		Macro_Set_Bit(RCC->APB1ENR, 2);

		TIM4->CR1 = (1<<4)|(0<<3);
		TIM4->PSC = (unsigned int)(TIMXCLK/(double)TIM4_FREQ + 0.5)-1;
		TIM4->ARR = TIME4_PLS_OF_1ms * time;
		Macro_Set_Bit(TIM4->EGR,0);

		Macro_Clear_Bit(TIM4->SR, 0);
		NVIC_ClearPendingIRQ(30);

		Macro_Set_Bit(TIM4->DIER, 0);
		NVIC_EnableIRQ(30);

		Macro_Set_Bit(TIM4->CR1, 0);
	}

	else
	{
		NVIC_DisableIRQ(30);
		Macro_Clear_Bit(TIM4->CR1, 0);
		Macro_Clear_Bit(TIM4->DIER, 0);
	}
}

#define TIM3_FREQ 	  			(8000000) 	      	// Hz
#define TIM3_TICK	  			(1000000/TIM3_FREQ)	// usec
#define TIME3_PLS_OF_1ms  		(1000/TIM3_TICK)

void TIM3_Out_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Set_Bit(RCC->APB1ENR, 1);

	Macro_Write_Block(GPIOB->MODER, 0x3, 0x2, 0);  	// PB0 => ALT
	Macro_Write_Block(GPIOB->AFR[0], 0xf, 0x2, 0); 	// PB0 => AF02

	Macro_Write_Block(TIM3->CCMR2,0xff, 0x60, 0);
	TIM3->CCER = (0<<9)|(1<<8);
}

void TIM3_Out_Freq_Generation(unsigned short freq)
{
	TIM3->PSC = (unsigned int)(TIMXCLK/(double)TIM3_FREQ + 0.5)-1;
	TIM3->ARR = (double)TIM3_FREQ/freq-1;
	TIM3->CCR3 = TIM3->ARR/2;

	Macro_Set_Bit(TIM3->EGR,0);
	TIM3->CR1 = (1<<4)|(0<<3)|(0<<1)|(1<<0);
}

void TIM3_Out_Stop(void)
{
	Macro_Clear_Bit(TIM3->CR1, 0);
}


#define TIM5_TICK	  		(20) 				// usec
#define TIM5_FREQ 	  		(1000000/TIM5_TICK) // Hz
#define TIME5_PLS_OF_1ms  	(1000/TIM5_TICK)
#define TIM5_MAX	  		(0xffffu)


void TIM5_Repeat_Interrupt_Enable(int time)
 {
		// TIM5 Clock On
		TIM5->CR1 = (1<<4)|(0<<3);
		TIM5->PSC = (unsigned int)(TIMXCLK/(double)TIM5_FREQ + 0.5)-1;
		TIM5->ARR = TIME5_PLS_OF_1ms * time;
		Macro_Set_Bit(TIM5->EGR,0);

		// TIM5 Pending Clear
		// NVIC Pending Clear
		Macro_Clear_Bit(TIM5->SR, 0);
		NVIC_ClearPendingIRQ(31);
		// TIM5 Interrupt Enable
		// NVIC Interrupt Enable
		Macro_Set_Bit(TIM5->DIER, 0);
		NVIC_EnableIRQ(31);
		// TIM5 Start		
		Macro_Set_Bit(TIM5->CR1, 0);
 }