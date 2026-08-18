### home_plus
home iot

## 주변장치 함수

온도 습도 측정함수  
int temp_measurement(void); int 단위 235678(23.5도 67.8%)  
초음파거리 측정함수  
int ultra_sonic_measurement(void); int 단위 cm  
조도 측정함수  
int lumen__measurement(void); int 단위 %  
전등 통제 함수  
void led_control(int); / int 단위 0 off 1 on  
DC 모터 통제 함수  
void dc_motor_control(int); / int 단위 0 off 1 on  
스탭 모터 통제 함수  
void step_motor_control(int); / int 단위 0 ~ 100  
알람 통제 함수   
void alarm_control(int); / int 단위 0 off 1 on  

## 검증 결과

| 함수 | 검증 | 방법 |
| --- | --- | --- |
| 온도 습도 측정함수 | 완료 | temp_measurement() |
| 초음파거리 측정함수 | 완료 | ultra_sonic_measurement() |
| 조도 측정함수 | 완료 | lumen__measurement() |
|전등 통제 함수 | 완료 | led_control(1) |
| DC 모터 통제 함수 | 완료 | dc_motor_control(1) |
|스탭 모터 통제 함수 | 완료 | step_motor_control(100) |
|알람 통제 함수 | 완료 | alarm_control(1) |

## 검증간 사용 코드

```c
#include "device_driver.h"
#include "timer.h"
#include <stdio.h>

static void Sys_Init(int baud)
{
	SCB->CPACR |= (0x3 << 10*2)|(0x3 << 11*2);
	Clock_Init();
	Uart2_Init(baud);
	setvbuf(stdout, NULL, _IONBF, 0);
	Sensor_Control_Init();
}

void Main(void){
    volatile int i;
	int test = 1;
    Sys_Init(115200);
    printf("\n=== HOME_PLUS individual device test ===\n");
	for (i = 0; i < 6400000; i++){ __NOP(); }
    for (;;)    {
		for (i = 0; i < 6400000; i++){ __NOP(); }
		led_control(test);
		dc_motor_control(test);
		alarm_control(test);
		step_motor_control(100);
		for (i = 0; i < 640000; i++){ __NOP(); }
		printf("lumen = %d\n", lumen__measurement());
		int temp = temp_measurement();
		for (i = 0; i < 640000; i++){ __NOP(); }
		printf("temp = %d\n", temp);
		for (i = 0; i < 640000; i++){ __NOP(); }
        printf("ultra_sonic = %d\n", ultra_sonic_measurement());
		if(test == 1){
			test = 0;
		}else{ test = 1;}
    }
}

```
<img width="359" height="342" alt="image" src="https://github.com/user-attachments/assets/08361597-4afd-43f5-9a47-eb376e6d686c" />


## 핀맵

| 장치 | 핀 | 설정 | AF 필요여부 |
| --- | --- | --- | --- |
| LED | PA3 | GPIO Output | 불필요 |
| 부저 | PB4 | TIM3_CH1_PWM | AF2 |
| DHT11 | PA8 | GPIO Input/Output | 불필요 |
| HC-SRO4 Trigger | PB5 | GPIO Output | 불필요 |
| HC-SRO4 Echo | PA10 | TIM1_CH3 Input Capture | AF1 |
| 스탭모터 4핀 | PC7/PB6/PA7/PA6 | GPIO Output | 불필요 |
| DC모터 | PB10 | GPIO Output | 불필요 |
| CDS | PA0 | ADC1_IN0_Analog | GPIO AF와 별도 |
