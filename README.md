# HomePlus

> MCU 기반 Home IoT 제어 시스템  
> 센서 데이터를 주기적으로 측정하고 UART를 통해 전송하며, UART 명령으로 액추에이터를 제어합니다.

---

## 1. 프로젝트 개요

**HomePlus**는 STM32 MCU를 기반으로 구현한 간단한 Home IoT 시스템입니다.

주요 기능은 다음과 같습니다.

- 온도 / 습도 측정
- 초음파 거리 측정
- 조도 측정
- LED 제어
- DC Motor 제어
- Step Motor 제어
- Alarm 제어
- USART2 기반 명령 수신
- UART RX Interrupt + Queue
- UART TX DMA
- 센서 데이터 주기적 전송

---

## 2. 시스템 구성

### Sensors

| 센서 | 측정 데이터 | 측정 주기 |
| :--- | :--- | ---: |
| **DHT11** | 온도 / 습도 | 1초 |
| **초음파 센서** | 거리 | 500 ms |
| **GL5528** | 조도 | 500 ms |

#### Sensors TimeOut
```
return -1 처리
```

### Actuators

| 액추에이터 | 기능 |
| :--- | :--- |
| **LED** | LED ON/OFF |
| **DC Motor** | DC Motor 제어 |
| **Step Motor** | Step Motor 제어 |
| **Alarm** | Alarm ON/OFF |

---

## 3. 통신

### USART2

| 항목 | 설정 |
| :--- | :--- |
| Interface | USART2 |
| Baud Rate | `115200` |
| RX | Interrupt + Queue |
| TX | DMA |
| 데이터 포맷 | 문자열 프레임 |
| 제어 명령 | 5자리 고정 길이 |

---


## 4. 함수 구성
- Main() → 스케줄링
- Sensor_Data_Update() → 센서 측정 및 프레임 생성
- UART2_DMA_Send() → DMA 전송
- Command_Receive_Process() → UART 명령 파싱
- Command_Process() → 액추에이터 실행
