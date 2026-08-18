# HOME_PLUS 센서 제어 API

- MCU: STM32F411RE
- 제어 방식: 레지스터 직접 제어
- 시스템 클럭: 64MHz

## 공개 함수

```c
int temp_measurement(void);
int ultra_sonic_measurement(void);
int lumen__measurement(void);

void led_control(int on);
void dc_motor_control(int on);
void step_motor_control(int position_percent);
void alarm_control(int on);
```

# (주의사항) 센서 측정 함수 호출 제한

- 인터럽트 서비스 루틴 내부 호출 금지
- 메인 코드 또는 일반 함수에서 호출

## 센서 측정 함수

### 온습도 측정

```c
int temp_measurement(void);
```

| 항목 | 내용 |
|---|---|
| 센서 | DHT11 |
| 측정값 | 온도, 상대습도 |
| 반환 형식 | 온습도 결합 정수 |
| 반환 예시 | `235678` |
| 온도 | 23.5°C |
| 상대습도 | 67.8%RH |
| 오류·타임아웃 | `SENSOR_ERROR` (`-1`) |

```c
int value = temp_measurement();

int temperature_x10 = value / 1000;
int humidity_x10 = value % 1000;
```

### 초음파 거리 측정

```c
int ultra_sonic_measurement(void);
```

| 항목 | 내용 |
|---|---|
| 센서 | HC-SR04 |
| 측정값 | 거리 |
| 반환 형식 | 정수 |
| 단위 | cm |
| 오류·타임아웃 | `SENSOR_ERROR` (`-1`) |

### 조도 측정

```c
int lumen__measurement(void);
```

| 항목 | 내용 |
|---|---|
| 입력 | 조도센서 ADC 값 |
| 측정값 | 상대 밝기 |
| 반환 범위 | `0~100%` |
| 실제 lux/lm | 미지원 |
| 오류·타임아웃 | `SENSOR_ERROR` (`-1`) |

## 센서 측정 방식

### DHT11

| 항목 | 내용 |
|---|---|
| 호출 방식 | 동기식 |
| DATA 핀 감지 | EXTI8 |
| 인터럽트 핸들러 | `EXTI9_5_IRQHandler()` |
| 시간 측정 | TIM2 |
| TIM2 카운터 주파수 | 1MHz |
| 대기 방식 | `__WFI()` |
| 종료 조건 | 측정 완료 또는 타임아웃 |

### HC-SR04

| 항목 | 내용 |
|---|---|
| 호출 방식 | 동기식 |
| Echo 측정 | TIM1 입력 캡처 |
| 인터럽트 핸들러 | `TIM1_CC_IRQHandler()` |
| 거리 계산 기준 | Echo HIGH 유지시간 |
| 대기 방식 | `__WFI()` |
| 종료 조건 | 측정 완료 또는 타임아웃 |

### `__WFI()` 대기 중 동작 인터럽트

- USART2 인터럽트
- 스텝모터 TIM4 인터럽트
- 알람 타이머 인터럽트
- SysTick 인터럽트
- ADC 인터럽트


## 구동기 제어 함수

### LED 제어

```c
void led_control(int on);
```

| 입력값 | 동작 |
|---|---|
| `on != 0` | LED 켜기 |
| `on == 0` | LED 끄기 |

### DC 모터 제어

```c
void dc_motor_control(int on);
```

| 입력값 | 동작 |
|---|---|
| `on != 0` | DC 모터 켜기 |
| `on == 0` | DC 모터 끄기 |

- 제어 방식: ON/OFF
- 속도 제어: 미지원
- 방향 제어: 미지원

### 스텝모터 제어

```c
void step_motor_control(int position_percent);
```

| 항목 | 내용 |
|---|---|
| 동작 방식 | 비동기식 |
| 위치 기준 | 절대 목표 위치 |
| 입력 범위 | `0~100%` |
| `0%` | 부팅 시 위치 |
| `100%` | 약 4096하프스텝 |
| 출력축 이동량 | 약 1회전 |
| 구동 인터럽트 | TIM4 |
| 범위 초과 입력 | `0~100`으로 제한 |

#### 이동 상태 확인

```c
int Step_Motor_Is_Moving(void);
```

| 반환값 | 상태 |
|---|---|
| `0` | 정지 또는 목표 위치 도착 |
| `0이 아닌 값` | 이동 중 |

### 알람 제어

```c
void alarm_control(int on);
```

| 입력값 | 동작 |
|---|---|
| `on != 0` | 알람 멜로디 시작 |
| `on == 0` | 알람 정지 |

- 동작 방식: 비동기식
- 음 길이 제어: TIM2
- 부저 PWM 출력: TIM3

#### 알람 상태 확인

```c
int Alarm_Is_Playing(void);
```

| 반환값 | 상태 |
|---|---|
| `0` | 알람 정지 |
| `0이 아닌 값` | 알람 재생 중 |

## 인터럽트 담당 모듈

| 구분 | 인터럽트 핸들러 |
|---|---|
| 초음파센서 | `TIM1_CC_IRQHandler` |
| 공용 타이머·알람 | `TIM2_IRQHandler` |
| 스텝모터 | `TIM4_IRQHandler` |
| DHT11 | `EXTI9_5_IRQHandler` |
| 조도센서 | `ADC_IRQHandler` |

## 하드웨어 안전 주의사항

- HC-SR04 Echo 전압: 5V → 약 3.3V로 변환
- HC-SR04 Echo 연결 핀: PA10
- DHT11 DATA 풀업 전압: 3.3V
- DHT11 DATA 풀업 저항: `4.7~10kΩ`