#include "device_driver.h"
#include "light_sensor.h"


#define LIGHT_ADC_MIN  2000
#define LIGHT_ADC_MAX  4000


static volatile unsigned int light_adc_raw;
static volatile int light_adc_done;

void Light_Sensor_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 0);  /* PA0 analog */
	Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x0, 0);

	Macro_Set_Bit(RCC->APB2ENR, 8);
	ADC1->CR1 = 1U << 5;                            /* EOC interrupt */
	ADC1->CR2 = 1U << 10;                           /* EOC after conversion */
	Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 0);   /* CH0, 480 cycles */
	Macro_Write_Block(ADC1->SQR1, 0xf, 0x0, 20);   /* one conversion */
	Macro_Write_Block(ADC1->SQR3, 0x1f, 0x0, 0);   /* channel 0 */
	Macro_Write_Block(ADC->CCR, 0x3, 0x1, 16);     /* 64 MHz / 4 = 16 MHz */
	Macro_Set_Bit(ADC1->CR2, 0);
	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);
}

void ADC_IRQHandler(void)
{
	if (ADC1->SR & (1U << 1)) {
		light_adc_raw = ADC1->DR & 0xfffU;
		light_adc_done = 1;
	}
}

int lumen__measurement(void)
{
	int raw;
	int percent;
	light_adc_done = 0;
	Macro_Set_Bit(ADC1->CR2, 30);
	while (!light_adc_done) __WFI();
	raw = light_adc_raw;
	    percent = (raw - LIGHT_ADC_MIN) * 100
              / (LIGHT_ADC_MAX - LIGHT_ADC_MIN);

    if (percent < 0) {
        percent = 0;
    }
    else if (percent > 100) {
        percent = 100;
    }
	return percent;
}
