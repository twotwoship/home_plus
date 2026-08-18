#include "device_driver.h"
#include "timer.h"

#define TIM2_TICK         	(20) 					// usec
#define TIM2_FREQ 	  		(1000000./TIM2_TICK)	// Hz
#define TIME2_PLS_OF_1ms  	(1000./TIM2_TICK)
#define TIM2_MAX	  		(0xffffffffu)


#define TIM4_TICK	  		(20) 				// usec
#define TIM4_FREQ 	  		(1000000/TIM4_TICK) // Hz
#define TIME4_PLS_OF_1ms  	(1000/TIM4_TICK)
#define TIM4_MAX	  		(0xffffu)

static volatile int timer_wait_done;
volatile int TIM5_Expired = 0;

/* 센서 타이머 초기화 인터럽트 키기 */
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

/* 카운터 반환해서 시간 읽어오기 임마로 구분 */
uint32_t Timer2_Micros(void)
{
	return TIM2->CNT;
}

/* 타이머 예약 함수, 채널별 레지스터에 작성 하고 인터럽트 활성화하기 */
static void Timer2_Arm(unsigned int channel, uint32_t delay_us)
{
	volatile uint32_t *ccr = &TIM2->CCR1 + channel;
	uint32_t flag = 1U << (channel + 1U);
	*ccr = TIM2->CNT + delay_us;
	TIM2->SR = ~flag;
	TIM2->DIER |= flag;
}

/* 타이머 예약 취소 함수 */
static void Timer2_Cancel(unsigned int channel)
{
	uint32_t flag = 1U << (channel + 1U);
	TIM2->DIER &= ~flag;
	TIM2->SR = ~flag;
}

/* 각 모듈별 쓰는 타이머들 */
void Timer2_Arm_CC1(uint32_t us) { Timer2_Arm(0U, us); }
void Timer2_Arm_CC2(uint32_t us) { Timer2_Arm(1U, us); }
void Timer2_Arm_CC3(uint32_t us) { Timer2_Arm(2U, us); }
void Timer2_Cancel_CC1(void) { Timer2_Cancel(0U); }
void Timer2_Cancel_CC2(void) { Timer2_Cancel(1U); }
void Timer2_Cancel_CC3(void) { Timer2_Cancel(2U); }


/* 타임아웃 플래그 설정 */
void Timer2_Timeout_Start(uint32_t delay_us)
{
	if (delay_us == 0U) delay_us = 1U;
	timer_wait_done = 0;
	Timer2_Arm(3U, delay_us);
}

/* 예약 취소 */
void Timer2_Timeout_Cancel(void)
{
	Timer2_Cancel(3U);
}

/* 타임아웃 플래그 확인 */
int Timer2_Timeout_Expired(void)
{
	return timer_wait_done;
}

/* 깡통 딴데서 만들어 놨음. */
/* 1 알람 2 뭐고 3 뭐고 */
__attribute__((weak)) void Timer2_CC1_Callback(void) { }
__attribute__((weak)) void Timer2_CC2_Callback(void) { }
__attribute__((weak)) void Timer2_CC3_Callback(void) { }

/* 타이마 인터럽트 */
void TIM2_IRQHandler(void)
{
	/* 타이머 인터럽트 발생한거 확인해서 알람끄고 작업 시행. */
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

/* CPU 대기모드 */
void Timer_Wait_Ms(uint32_t delay_ms)
{
	if (delay_ms == 0U) return;
	Timer2_Timeout_Start(delay_ms * 1000U);
	while (!timer_wait_done) __WFI();
}

#define TIM5_TICK	  		(20) 				// usec
#define TIM5_FREQ 	  		(1000000/TIM5_TICK) // Hz
#define TIME5_PLS_OF_1ms  	(1000/TIM5_TICK)
#define TIM5_MAX	  		(0xffffu)


void TIM5_Repeat_Interrupt_Enable(int time)
 {
		// TIM5 Clock On
		Macro_Set_Bit(RCC->APB1ENR, 3);

		TIM5->CR1 = (1U << 4) | (0U << 3);
		TIM5->PSC = (unsigned int)(TIMXCLK/(double)TIM5_FREQ + 0.5)-1;
		TIM5->ARR = TIME5_PLS_OF_1ms * time;
		TIM5->CNT = TIM5->ARR;
		Macro_Set_Bit(TIM5->EGR,0);

		// TIM5 Pending Clear
		// NVIC Pending Clear
		Macro_Clear_Bit(TIM5->SR, 0);
		NVIC_ClearPendingIRQ(TIM5_IRQn);
		// TIM5 Interrupt Enable
		// NVIC Interrupt Enable
		Macro_Set_Bit(TIM5->DIER, 0);
		NVIC_EnableIRQ(TIM5_IRQn);
		// TIM5 Start		
		Macro_Set_Bit(TIM5->CR1, 0);
 }

 void TIM5_IRQHandler(void)
{
    // TIM5 Update Interrupt Flag clear
    Macro_Clear_Bit(TIM5->SR, 0);
	NVIC_ClearPendingIRQ(TIM5_IRQn);
    // Main에서 처리할 이벤트 발생 표시
    TIM5_Expired = 1;
}