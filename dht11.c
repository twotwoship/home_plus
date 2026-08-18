#include "device_driver.h"
#include "timer.h"
#include "dht11.h"
#include "sensor_control.h"


/* DHT11 DATA: NUCLEO D7 = PA8 = EXTI8 */
#define DHT_PIN              8U
#define DHT_EXTI_MASK        (1U << DHT_PIN)
#define DHT_BIT_ONE_MIN_US   45U
#define DHT_FRAME_TIMEOUT_US 10000U

typedef enum
{
	DHT_IDLE = 0,
	DHT_WAIT_RESPONSE_LOW,
	DHT_WAIT_RESPONSE_HIGH,
	DHT_WAIT_FIRST_BIT_LOW,
	DHT_WAIT_BIT_HIGH,
	DHT_WAIT_BIT_LOW,
	DHT_DONE
} DHT_State;

static volatile DHT_State dht_state;
static volatile unsigned char dht_data[5];
static volatile unsigned int dht_bit_index;
static volatile uint32_t dht_high_start;

static void DHT_Pin_Output_OpenDrain(void)
{
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 16);
	Macro_Set_Bit(GPIOA->OTYPER, DHT_PIN);
	Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x1, 16);
}

static void DHT_Pin_Input(void)
{
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x0, 16);
	Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x1, 16);
}

static int DHT_Pin_Read(void)
{
	return (GPIOA->IDR & DHT_EXTI_MASK) != 0U;
}

static void DHT_EXTI_Enable(void)
{
	EXTI->PR = DHT_EXTI_MASK;
	EXTI->IMR |= DHT_EXTI_MASK;
}

static void DHT_EXTI_Disable(void)
{
	EXTI->IMR &= ~DHT_EXTI_MASK;
	EXTI->PR = DHT_EXTI_MASK;
}

static int DHT_Timeout_Error(DHT_State state)
{
	/* find error change debug number*/
	switch (state) {
	case DHT_WAIT_RESPONSE_LOW:  return -1;
	case DHT_WAIT_RESPONSE_HIGH: return -1;
	case DHT_WAIT_FIRST_BIT_LOW: return -1;
	case DHT_WAIT_BIT_HIGH:      return -1;
	case DHT_WAIT_BIT_LOW:       return -1;
	default:                     return -1;
	}
}

void DHT11_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);   /* GPIOA clock */
	Macro_Set_Bit(RCC->APB2ENR, 14);  /* SYSCFG clock */

	DHT_Pin_Output_OpenDrain();
	GPIOA->BSRR = DHT_EXTI_MASK;       /* Release DATA line. */

	/* Route EXTI8 to PA8 and capture both edges. */
	Macro_Write_Block(SYSCFG->EXTICR[2], 0xf, 0x0, 0);
	EXTI->RTSR |= DHT_EXTI_MASK;
	EXTI->FTSR |= DHT_EXTI_MASK;
	DHT_EXTI_Disable();

	dht_state = DHT_IDLE;
	NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
	NVIC_EnableIRQ(EXTI9_5_IRQn);
}

int temp_measurement(void)
{
	unsigned char data[5];
	unsigned int i;
	unsigned int checksum;
	int humidity_x10;
	int temperature_x10;
	int error;

	DHT_EXTI_Disable();
	dht_state = DHT_IDLE;
	dht_bit_index = 0U;
	for (i = 0U; i < 5U; ++i) dht_data[i] = 0U;

	/* Host start signal. Timer2 CC4 sleeps without masking interrupts. */
	DHT_Pin_Output_OpenDrain();
	GPIOA->BSRR = DHT_EXTI_MASK << 16; /* DATA LOW */
	Timer_Wait_Ms(20U);
	GPIOA->BSRR = DHT_EXTI_MASK;       /* Release open-drain output. */
	DHT_Pin_Input();

	dht_state = DHT_WAIT_RESPONSE_LOW;
	DHT_EXTI_Enable();
	Timer2_Timeout_Start(DHT_FRAME_TIMEOUT_US);

	/*
	 * The API remains synchronous, but UART, motor, alarm and sensor
	 * interrupts continue while the caller sleeps between DHT edges.
	 */
	while ((dht_state != DHT_DONE) && !Timer2_Timeout_Expired())
		__WFI();

	Timer2_Timeout_Cancel();
	DHT_EXTI_Disable();

	if (dht_state != DHT_DONE)
		error = DHT_Timeout_Error(dht_state);
	else
		error = 0;

	/* Return the one-wire bus to its idle released state. */
	DHT_Pin_Output_OpenDrain();
	GPIOA->BSRR = DHT_EXTI_MASK;
	dht_state = DHT_IDLE;

	if (error) return error;

	for (i = 0U; i < 5U; ++i) data[i] = dht_data[i];
	checksum = (unsigned char)(data[0] + data[1] + data[2] + data[3]);
	if (checksum != data[4]) return -1;		/* find error change debug number*/

	humidity_x10 = (int)data[0] * 10 + data[1];
	temperature_x10 = (int)(data[2] & 0x7fU) * 10 + data[3];
	if (data[2] & 0x80U) temperature_x10 = -temperature_x10;
	if (temperature_x10 < 0) return -1;		/* find error change debug number*/

	return temperature_x10 * 1000 + humidity_x10;
}

void EXTI9_5_IRQHandler(void)
{
	uint32_t now;
	uint32_t high_width;
	unsigned int byte_index;

	if ((EXTI->PR & DHT_EXTI_MASK) == 0U) return;
	EXTI->PR = DHT_EXTI_MASK;
	now = Timer2_Micros();

	switch (dht_state) {
	case DHT_WAIT_RESPONSE_LOW:
		if (!DHT_Pin_Read()) dht_state = DHT_WAIT_RESPONSE_HIGH;
		break;

	case DHT_WAIT_RESPONSE_HIGH:
		if (DHT_Pin_Read()) dht_state = DHT_WAIT_FIRST_BIT_LOW;
		break;

	case DHT_WAIT_FIRST_BIT_LOW:
		if (!DHT_Pin_Read()) dht_state = DHT_WAIT_BIT_HIGH;
		break;

	case DHT_WAIT_BIT_HIGH:
		if (DHT_Pin_Read()) {
			dht_high_start = now;
			dht_state = DHT_WAIT_BIT_LOW;
		}
		break;

	case DHT_WAIT_BIT_LOW:
		if (!DHT_Pin_Read()) {
			high_width = (uint32_t)(now - dht_high_start);
			byte_index = dht_bit_index >> 3;
			dht_data[byte_index] <<= 1;
			if (high_width > DHT_BIT_ONE_MIN_US)
				dht_data[byte_index] |= 1U;

			dht_bit_index++;
			if (dht_bit_index >= 40U) {
				dht_state = DHT_DONE;
				EXTI->IMR &= ~DHT_EXTI_MASK;
			} else {
				dht_state = DHT_WAIT_BIT_HIGH;
			}
		}
		break;

	default:
		break;
	}
}