#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"

/* 초음파 거리 측정 */
enum Ultra_State { ULTRA_IDLE, ULTRA_WAIT_RISE, ULTRA_WAIT_FALL };

static volatile enum Ultra_State ultra_state;
static volatile unsigned int ultra_rise;       /* Echo 신호 Rising Edge 시점 카운트 (us) */
static volatile unsigned int ultra_pulse_us;   /* Echo High 유지 시간 (us) */
static volatile int ultra_done;                /* 측정 완료 플래그 (1: 완료) */
static volatile int ultra_error;               /* 에러 발생 플래그 (1: 에러/타임아웃) */

/* 측정 종료 및 하드웨어/상태값 리셋 처리 함수 */
static void Ultrasonic_Finish(int error)
{
    TIM1->DIER &= ~(1U << 3);     
    TIM1->CCER &= ~(1U << 8);    
    Timer2_Cancel_CC2();          /* Trigger 타이머 캔슬 */
    Timer2_Cancel_CC3();          /* 타임아웃 타이머 캔슬 */
    Macro_Clear_Bit(GPIOB->ODR, 5); /* Trig 핀 Low 복구 */
    
    ultra_error = error;
    ultra_state = ULTRA_IDLE;
    ultra_done = 1;
}

/* 초음파 센서 GPIO 및 TIM1 Input Capture 초기화 */
void Ultrasonic_Init(void)
{
    Macro_Set_Bit(RCC->AHB1ENR, 0); 
    Macro_Set_Bit(RCC->AHB1ENR, 1); 
    Macro_Set_Bit(RCC->APB2ENR, 0); 

    /* PB5: Trigger 핀 (Push-Pull Output, 초기값 Low) */
    Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 10);
    Macro_Clear_Bit(GPIOB->OTYPER, 5);
    Macro_Clear_Bit(GPIOB->ODR, 5);

    /* PA10: Echo 핀 (TIM1_CH3 Input Capture, AF1) */
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x2, 20);
    Macro_Write_Block(GPIOA->AFR[1], 0xf, 0x1, 8);  /* AF1 (TIM1_CH3) 지정 */
    Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x0, 20);  

    /* TIM1 타이머 설정 (1us 카운트 프리스케일러 적용) */
    TIM1->CR1 = 0;
    TIM1->PSC = (TIM1CLK / 1000000U) - 1U;          /* 1us 단위 카운팅 */
    TIM1->ARR = 0xffffU;                             /* 16비트 최대값 */
    TIM1->CCMR2 = (TIM1->CCMR2 & ~0x3U) | 0x1U;     /* CC3 채널을 TI3 입력으로 매핑 */
    TIM1->CCER &= ~((1U << 8) | (1U << 9) | (1U << 11)); /* 초기 설정: Rising Edge 캡처 */
    TIM1->DIER &= ~(1U << 3);                        /* CC3 인터럽트 Off */
    TIM1->SR = 0;
    TIM1->EGR = 1U;                               
    TIM1->CR1 = 1U;                                  /* 켜 */
    
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    ultra_state = ULTRA_IDLE;
}

/* Timer2 CC2 콜백: 10us 지난 후 Trig 핀을 Low로 내림 */
void Timer2_CC2_Callback(void)
{
    Macro_Clear_Bit(GPIOB->ODR, 5);
}

/* Timer2 CC3 콜백: Echo 신호 무응답 시 30ms 타임아웃 처리 */
void Timer2_CC3_Callback(void)
{
    if (ultra_state != ULTRA_IDLE) Ultrasonic_Finish(1);
}

/* TIM1 Capture/Compare 인터럽트: Echo 핀의 펄스 폭 측정 */
void TIM1_CC_IRQHandler(void)
{
    unsigned int captured;
    
    /* CC3 인터럽트 플래그 및 Enable 여부 확인 */
    if (((TIM1->SR & (1U << 3)) == 0U) ||
        ((TIM1->DIER & (1U << 3)) == 0U)) return;

    captured = TIM1->CCR3;   /* 캡처된 타이머 카운트값 읽기 */
    TIM1->SR = ~(1U << 3);   /* 플래그 클리어 */

    if (ultra_state == ULTRA_WAIT_RISE) {
        /* Rising Edge 감지: 시작 시간 기록 후 Falling Edge 감지 모드로 전환 */
        ultra_rise = captured;
        Macro_Set_Bit(TIM1->CCER, 9);  /* CC3P = 1 (Falling Edge 감지 설정) */
        ultra_state = ULTRA_WAIT_FALL;
    } else if (ultra_state == ULTRA_WAIT_FALL) {
        /* Falling Edge 감지: 유지 시간(us) 계산 후 측정 종료 */
        ultra_pulse_us = (captured - ultra_rise) & 0xffffU; /* 타이머 오버플로우 대비 비트마스크 */
        Ultrasonic_Finish(0);
    }
}

/* 
 * 초음파 거리 측정 함수 (cm 단위 반환)
 * - 반환값: 거리(cm), 측정 실패 시 -1 반환
 */
int ultra_sonic_measurement(void)
{
    unsigned int pulse;
    
    if (ultra_state != ULTRA_IDLE) return -1; /* 이미 측정 중이면 리턴 빨리 호출하지 말라*/

    /* 상태 및 변수 초기화 */
    ultra_done = 0;
    ultra_error = 0;
    ultra_pulse_us = 0;
    ultra_state = ULTRA_WAIT_RISE;
    
    /* Input Capture 설정: Rising Edge 감지 준비 및 인터럽트 Enable */
    Macro_Clear_Bit(TIM1->CCER, 9);  /* CC3P = 0 (Rising Edge) */
    TIM1->SR = ~(1U << 3);
    Macro_Set_Bit(TIM1->DIER, 3);    /* CC3 인터럽트 Enable */
    Macro_Set_Bit(TIM1->CCER, 8);    /* CC3 캡처 Enable */

    /* Trig 핀 High 출력 후 소프트웨어 타이머 세팅 */
    Macro_Set_Bit(GPIOB->ODR, 5);
    Timer2_Arm_CC2(10U);             /* 10us 후 Trig 핀 Low로 내리기 */
    Timer2_Arm_CC3(30000U);          /* 30ms 수신 타임아웃 설정 (최대 측정 거리 초과 시) */

    /* 측정 완료(인터럽트) 시까지 슬립 대기 */
    while (!ultra_done) __WFI();
    
    if (ultra_error) return -1;
    
    pulse = ultra_pulse_us;
    /* 신뢰할 수 있는 거리 범위를 벗어난 펄스 시간 예외 처리 (약 2cm ~ 400cm 범위) */
    if (pulse < 116U || pulse > 23200U) return -1;
    
    /* 음속(340m/s) 기준 시간(us) -> 거리(cm) 변환 공식 (pulse / 58) */
    return (int)((pulse + 29U) / 58U);
}