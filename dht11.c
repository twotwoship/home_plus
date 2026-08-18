#include "device_driver.h"
#include "timer.h"
#include "dht11.h"
#include "sensor_control.h"

/* 
 * DHT11 데이터 핀 설정 (NUCLEO PA8 = EXTI8)
 * - 45us 이상 High 유지되면 비트 '1'로 판단 (26~28us: 0 / 70us: 1)
 * - 데이터 수신 전체 타임아웃: 10ms
 */
#define DHT_PIN             8U
#define DHT_EXTI_MASK        (1U << DHT_PIN)
#define DHT_BIT_ONE_MIN_US   45U
#define DHT_FRAME_TIMEOUT_US 10000U

/* EXTI 인터럽트 기반 DHT11 상태 머신 */
typedef enum
{
    DHT_IDLE = 0,
    DHT_WAIT_RESPONSE_LOW,   /* 센서 응답 신호 (Low ~80us) 대기 */
    DHT_WAIT_RESPONSE_HIGH,  /* 센서 응답 신호 (High ~80us) 대기 */
    DHT_WAIT_FIRST_BIT_LOW,  /* 첫 비트 시작 전 Low 대기 */
    DHT_WAIT_BIT_HIGH,       /* 데이터 비트의 High 시작 시점 대기 */
    DHT_WAIT_BIT_LOW,        /* High 구간 폭(시간) 측정을 위해 Low 전환 대기 */
    DHT_DONE                 /* 40비트 전체 수신 완료 */
} DHT_State;

/* 전역/인터럽트 공유 변수 */
static volatile DHT_State dht_state;
static volatile unsigned char dht_data[5];  /* 습도 정수/소수, 온도 정수/소수, 체크섬 */
static volatile unsigned int dht_bit_index;
static volatile uint32_t dht_high_start;   /* High 구간 시작 시간 저장 (us) */

/* PA8을 Open-Drain 출력으로 설정 (DHT11 호환 및 내부 풀업 활성화) */
static void DHT_Pin_Output_OpenDrain(void)
{
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 16);  /* PA8 Mode: Output (01) */
    Macro_Set_Bit(GPIOA->OTYPER, DHT_PIN);          /* Open-Drain 설정 */
    Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x1, 16);  /* Pull-up 활성화 (01) */
}

/* PA8을 입력 모드로 전환 */
static void DHT_Pin_Input(void)
{
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x0, 16);  /* PA8 Mode: Input (00) */
    Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x1, 16);  /* Pull-up 유지 */
}

/* PA8 입력 레벨 읽기 (1 or 0) */
static int DHT_Pin_Read(void)
{
    return (GPIOA->IDR & DHT_EXTI_MASK) != 0U;
}

/* EXTI8 인터럽트 활성화 (플래그 먼저 락 해제 후 마스크 해제) */
static void DHT_EXTI_Enable(void)
{
    EXTI->PR = DHT_EXTI_MASK;       /* Pending 플래그 클리어 */
    EXTI->IMR |= DHT_EXTI_MASK;     /* Interrupt Mask 해제 */
}

/* EXTI8 인터럽트 비활성화 */
static void DHT_EXTI_Disable(void)
{
    EXTI->IMR &= ~DHT_EXTI_MASK;
    EXTI->PR = DHT_EXTI_MASK;
}

/* 타임아웃 발생 시 단계별 에러 코드 반환 (디버깅용) */
static int DHT_Timeout_Error(DHT_State state)
{
    /* TODO: 디버깅 편의를 위해 단계별로 -1, -2, -3 등 세분화 고려 */
    switch (state) {
    case DHT_WAIT_RESPONSE_LOW:  return -1;
    case DHT_WAIT_RESPONSE_HIGH: return -1;
    case DHT_WAIT_FIRST_BIT_LOW: return -1;
    case DHT_WAIT_BIT_HIGH:       return -1;
    case DHT_WAIT_BIT_LOW:        return -1;
    default:                     return -1;
    }
}

/* DHT11 초기화: 클록 공급, 핀 및 EXTI 인터럽트 기본 설정 */
void DHT11_Init(void)
{
    Macro_Set_Bit(RCC->AHB1ENR, 0);   /* GPIOA 클록 Enable */
    Macro_Set_Bit(RCC->APB2ENR, 14);  /* SYSCFG 클록 Enable (EXTI 매핑용) */

    DHT_Pin_Output_OpenDrain();
    GPIOA->BSRR = DHT_EXTI_MASK;       /* 데이터 라인 High(Release) 상태 유지 */

    /* PA8을 EXTI8에 매핑 및 양상승/하강 에지 트리가 모두 활성화 */
    Macro_Write_Block(SYSCFG->EXTICR[2], 0xf, 0x0, 0);
    EXTI->RTSR |= DHT_EXTI_MASK;       /* Rising edge 설정 */
    EXTI->FTSR |= DHT_EXTI_MASK;       /* Falling edge 설정 */
    DHT_EXTI_Disable();

    dht_state = DHT_IDLE;
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* 
 * 온도 및 습도 측정 함수
 * 반환값: (온도x10)*1000 + (습도x10) 형태의 정수 (예: 25.3도, 60.5% -> 253605)
 * 실패 시 음수(-1) 반환
 */
int temp_measurement(void)
{
    unsigned char data[5];
    unsigned int i;
    unsigned int checksum;
    int humidity_x10;
    int temperature_x10;
    int error;

    /* 측정 시작 전 변수 및 상태 초기화 */
    DHT_EXTI_Disable();
    dht_state = DHT_IDLE;
    dht_bit_index = 0U;
    for (i = 0U; i < 5U; ++i) dht_data[i] = 0U;

    /* [Host Start Signal Send] Low 신호를 최소 18ms(여기선 20ms) 이상 유지하여 센서 깨움 */
    DHT_Pin_Output_OpenDrain();
    GPIOA->BSRR = DHT_EXTI_MASK << 16; /* Line LOW */
    Timer_Wait_Ms(20U);
    GPIOA->BSRR = DHT_EXTI_MASK;       /* Line High (Release) */
    DHT_Pin_Input();                   /* 센서 응답을 받기 위해 입력 모드로 전환 */

    /* 응답 수신을 위한 EXTI 및 타임아웃 타이머 시작 */
    dht_state = DHT_WAIT_RESPONSE_LOW;
    DHT_EXTI_Enable();
    Timer2_Timeout_Start(DHT_FRAME_TIMEOUT_US);

    /* 
     * 인터럽트 기반 비동기 수신 대기
     * __WFI()를 통해 CPU는 슬립 모드에 들어가지만, 다른 통신/타이머 인터럽트는 정상 처리됨
     */
    while ((dht_state != DHT_DONE) && !Timer2_Timeout_Expired())
        __WFI();

    /* 타이머 및 인터럽트 정리 */
    Timer2_Timeout_Cancel();
    DHT_EXTI_Disable();

    /* 상태 검사 */
    if (dht_state != DHT_DONE)
        error = DHT_Timeout_Error(dht_state);
    else
        error = 0;

    /* 버스를 다시 안정적인 Release 상태로 복구 */
    DHT_Pin_Output_OpenDrain();
    GPIOA->BSRR = DHT_EXTI_MASK;
    dht_state = DHT_IDLE;

    if (error) return error;

    /* 5바이트 데이터 복사 및 체크섬 검증 */
    for (i = 0U; i < 5U; ++i) data[i] = dht_data[i];
    checksum = (unsigned char)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) return -1;     /* 체크섬 불일치 오류 */

    /* 데이터 파싱 (DHT11 포맷에 맞게 계산) */
    humidity_x10 = (int)data[0] * 10 + data[1];
    temperature_x10 = (int)(data[2] & 0x7fU) * 10 + data[3];
    if (data[2] & 0x80U) temperature_x10 = -temperature_x10; /* 최상위 비트 1이면 영하 */
    if (temperature_x10 < 0) return -1;     /* 예외 처리: 영하 온도는 에러 처리(필요 시 수정) */

    return temperature_x10 * 1000 + humidity_x10;
}

/* GPIO EXTI8 (Pin 5~9 통합) 인터럽트 핸들러 */
void EXTI9_5_IRQHandler(void)
{
    uint32_t now;
    uint32_t high_width;
    unsigned int byte_index;

    /* PA8 EXTI 인터럽트가 아니면 바로 리턴 */
    if ((EXTI->PR & DHT_EXTI_MASK) == 0U) return;
    EXTI->PR = DHT_EXTI_MASK; /* Pending 플래그 클리어 */
    
    now = Timer2_Micros();     /* 현재 시간 측정 (us 단위) */

    /* DHT11 프로토콜 상태 머신 */
    switch (dht_state) {
    case DHT_WAIT_RESPONSE_LOW:
        /* 센서가 응답으로 Low를 내렸는지 확인 */
        if (!DHT_Pin_Read()) dht_state = DHT_WAIT_RESPONSE_HIGH;
        break;

    case DHT_WAIT_RESPONSE_HIGH:
        /* 센서가 Low 끝내고 High로 올렸는지 확인 */
        if (DHT_Pin_Read()) dht_state = DHT_WAIT_FIRST_BIT_LOW;
        break;

    case DHT_WAIT_FIRST_BIT_LOW:
        /* 데이터 전송 시작을 알리는 첫 Low 신호 확인 */
        if (!DHT_Pin_Read()) dht_state = DHT_WAIT_BIT_HIGH;
        break;

    case DHT_WAIT_BIT_HIGH:
        /* 비트 데이터의 High 시작점 트래킹 (시간 측정 시작) */
        if (DHT_Pin_Read()) {
            dht_high_start = now;
            dht_state = DHT_WAIT_BIT_LOW;
        }
        break;

    case DHT_WAIT_BIT_LOW:
        /* High 신호 끝(Falling Edge) -> High의 길이를 재서 0인지 1인지 판별 */
        if (!DHT_Pin_Read()) {
            high_width = (uint32_t)(now - dht_high_start);
            byte_index = dht_bit_index >> 3; /* 비트 인덱스를 8로 나눠 바이트 인덱스 구함 */
            
            dht_data[byte_index] <<= 1;
            /* High 유지시간이 45us 이상이면 비트 '1', 아니면 '0' */
            if (high_width > DHT_BIT_ONE_MIN_US)
                dht_data[byte_index] |= 1U;

            dht_bit_index++;
            
            /* 총 40비트 (5바이트) 수신 완료 체크 */
            if (dht_bit_index >= 40U) {
                dht_state = DHT_DONE;
                EXTI->IMR &= ~DHT_EXTI_MASK; /* 수신 끝났으므로 인터럽트 Off */
            } else {
                dht_state = DHT_WAIT_BIT_HIGH; /* 다음 비트 수신 대기 */
            }
        }
        break;

    default:
        break;
    }
}