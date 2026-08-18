#include "device_driver.h"
#include "light_sensor.h"


#define LIGHT_ADC_MIN  2000
#define LIGHT_ADC_MAX  4000


static volatile unsigned int light_adc_raw;
static volatile int light_adc_done;

void Light_Sensor_Init(void)
{
    Macro_Set_Bit(RCC->AHB1ENR, 0);                 /* GPIOA 클록 공급 */
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 0);   /* PA0 핀을 아날로그 모드로 설정 */
    Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x0, 0);   

    Macro_Set_Bit(RCC->APB2ENR, 8);                 /* ADC1 클록 공급 */
    ADC1->CR1 = 1U << 5;                            /* EOC (변환 완료) 인터럽트 활성화 */
    ADC1->CR2 = 1U << 10;                           /* 각 변환이 끝날 때마다 EOC 플래그 생성 */
    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 0);    /* 채널 0 샘플링 타임: 480 주기  */
    Macro_Write_Block(ADC1->SQR1, 0xf, 0x0, 20);    /* 변환할 채널 개수: 1개 */
    Macro_Write_Block(ADC1->SQR3, 0x1f, 0x0, 0);    /* 1순위로 변환할 채널: 채널 0 (PA0) */
    Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16);      /* ADC 분주주기: 96 MHz / 6 = 16 MHz */
    Macro_Set_Bit(ADC1->CR2, 0);                    /* ADC1 전원 켜기  */
    NVIC_ClearPendingIRQ(ADC_IRQn);
    NVIC_EnableIRQ(ADC_IRQn);                       /* NVIC ADC 인터럽트 허용 */
}

void ADC_IRQHandler(void)
{
    if (ADC1->SR & (1U << 1)) {              /* EOC 플래그 확인 */
        light_adc_raw = ADC1->DR & 0xfffU;   /* 12비트 결과값 읽기 */
        light_adc_done = 1;                  /* 변환 완료 플래그 1로 설정 */
    }
}

int lumen__measurement(void)
{
    int raw;
    int percent;
    light_adc_done = 0;                     /* 완료 플래그 초기화 */
    Macro_Set_Bit(ADC1->CR2, 30);           /* SWSTART: 소프트웨어로 ADC 변환 즉시 시작 */
    while (!light_adc_done) __WFI();        /* 인터럽트가 발생할 때까지 저전력 대기 */
    raw = light_adc_raw;                  
    percent = (raw - LIGHT_ADC_MIN) * 100 
            / (LIGHT_ADC_MAX - LIGHT_ADC_MIN);

    if (percent < 0) {
        percent = 0;
    }
    else if (percent > 100) {
        percent = 100;
    }
    return percent;                         /* ⑦ 최종 밝기 백분율 반환 */
}
