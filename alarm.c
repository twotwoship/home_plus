#include "device_driver.h"
#include "timer.h"
#include "alarm.h"

#define NOTE_C5  523U
#define NOTE_E5  659U
#define NOTE_G5  784U

typedef struct
{
	unsigned short frequency_hz;
	unsigned short duration_ms;
} Alarm_Note;

static const Alarm_Note alarm_melody[] =
{
	/* 기상나팔 */
    /* 1. 솔 미 */
    { NOTE_G5, 540U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 미 미 미 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 미 솔 미 도 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 340U }, { 0U, 60U },
    /* 미 솔 미 도 미 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 솔 미 미 */
    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 도 도 도 도 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 540U }, { 0U, 160U },


    /* 2. 미 미 미 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 미 솔 미 도 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 340U }, { 0U, 60U },
    /* 미 솔 미 도 미 */
    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 솔 미 미 */
    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 340U }, { 0U, 60U },
    /* 도 도 도 도 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 740U }, { 0U, 220U },


    /* 3. 솔 솔 미 */
    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 도 미 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 도 미 도 미 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 솔 길게 */
    { NOTE_G5, 1320U }, { 0U, 220U },
    /* 4. 도 도 미 미 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 솔 솔 미 도 미 */
    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_G5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 도 미 도 미 */
    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },    { NOTE_C5, 140U }, { 0U, 60U },    { NOTE_E5, 140U }, { 0U, 60U },
    /* 마지막 솔 */
    { NOTE_G5, 1800U }, { 0U, 500U }
};
#define ALARM_MELODY_LENGTH \
	(sizeof(alarm_melody) / sizeof(alarm_melody[0]))

static volatile unsigned int alarm_note_index;
static volatile int alarm_enabled;

static void Alarm_Output_Frequency(unsigned int frequency_hz)
{
	unsigned int period;

	if (frequency_hz == 0U) {
		Macro_Clear_Bit(TIM3->CCER, 0);
		return;
	}

	/* TIM3 counter clock is 1 MHz, so ARR selects the note frequency. */
	period = 1000000U / frequency_hz;
	TIM3->ARR = period - 1U;
	TIM3->CCR1 = period / 2U;
	TIM3->CNT = 0U;
	TIM3->EGR = 1U;
	Macro_Set_Bit(TIM3->CCER, 0);
	Macro_Set_Bit(TIM3->CR1, 0);
}

static void Alarm_Play_Current_Note(void)
{
	const Alarm_Note *note = &alarm_melody[alarm_note_index];

	Alarm_Output_Frequency(note->frequency_hz);
	Timer2_Arm_CC1((uint32_t)note->duration_ms * 1000U);
}

void Alarm_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Set_Bit(RCC->APB1ENR, 1);
	Macro_Write_Block(GPIOB->MODER, 0x3, 0x2, 8);   /* PB4 alternate */
	Macro_Write_Block(GPIOB->AFR[0], 0xf, 0x2, 16); /* PB4 AF2 TIM3_CH1 */
	Macro_Write_Block(GPIOB->PUPDR, 0x3, 0x0, 8);

	TIM3->CR1 = 0;
	TIM3->PSC = (TIMXCLK / 1000000U) - 1U;
	TIM3->ARR = 250U - 1U;                          /* 4 kHz */
	TIM3->CCR1 = 125U;
	TIM3->CCMR1 = (6U << 4) | (1U << 3);            /* PWM mode 1, preload */
	TIM3->CCER = 0;
	TIM3->EGR = 1U;
	alarm_note_index = 0U;
	alarm_enabled = 0;
}

void alarm_control(int on)
{
	if (on) {
		/* Repeated calls while ON must not restart the current note. */
		if (!alarm_enabled) {
			alarm_enabled = 1;
			alarm_note_index = 0U;
			Alarm_Play_Current_Note();
		}
	} else {
		alarm_enabled = 0;
		Timer2_Cancel_CC1();
		Macro_Clear_Bit(TIM3->CCER, 0);
		Macro_Clear_Bit(TIM3->CR1, 0);
	}
}

int Alarm_Is_Playing(void)
{
	return alarm_enabled;
}

/* TIM2 CC1 is reserved for advancing the non-blocking buzzer melody. */
void Timer2_CC1_Callback(void)
{
	if (!alarm_enabled) return;

	alarm_note_index++;
	if (alarm_note_index >= ALARM_MELODY_LENGTH)
		alarm_note_index = 0U; /* Repeat until alarm_control(0). */

	Alarm_Play_Current_Note();
}
