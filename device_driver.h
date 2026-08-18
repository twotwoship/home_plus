
#include "stm32f4xx.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"

#include "ultrasonic.h"
#include "dc_motor.h"
#include "dht11.h"
#include "light_sensor.h"
#include "alarm.h"

// Uart.c

extern void Uart2_Init(int baud);
extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);
extern void Uart2_RX_Interrupt_Enable(int en);

extern void Uart2_Rx_Enqueue(char data);
extern int Uart2_Rx_GetChar(char *p_data);

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// Led.c

extern void LED_Init(void);
extern void LED_On(void);
extern void LED_Off(void);
extern void LED_Display(int on);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern void Key_Wait_Key_Pressed(void);
extern void Key_ISR_Enable(int en);

// Timer.c

extern void TIM2_Delay(int time);
extern void TIM2_Stopwatch_Start(void);
extern unsigned int TIM2_Stopwatch_Stop(void);
extern void TIM4_Repeat(int time);
extern int TIM4_Check_Timeout(void);
extern void TIM4_Stop(void);
extern void TIM4_Change_Value(int time);
extern void TIM4_Repeat_Interrupt_Enable(int en, int time);
extern void TIM3_Out_Init(void);
extern void TIM3_Out_Freq_Generation(unsigned short freq);
extern void TIM3_Out_Stop(void);
extern void TIM5_Repeat_Interrupt_Enable(int time);

// i2c.c

#define SC16IS752_IODIR				0x0A
#define SC16IS752_IOSTATE			0x0B

extern void I2C1_SC16IS752_Init(unsigned int freq);
extern void I2C1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void I2C1_SC16IS752_Config_GPIO(unsigned int config);
extern void I2C1_SC16IS752_Write_GPIO(unsigned int data);

// spi.c

extern void SPI1_SC16IS752_Init(unsigned int div);
extern void SPI1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void SPI1_SC16IS752_Config_GPIO(unsigned int config);
extern void SPI1_SC16IS752_Write_GPIO(unsigned int data);

// Adc.c

extern void ADC1_IN6_Init(void);
extern void ADC1_Start(void);
extern void ADC1_Stop(void);
extern int ADC1_Get_Status(void);
extern int ADC1_Get_Data(void);

// DMA

extern void DMA2_Stream0_M2M_Init(void);
extern void DMA2_Stream0_M2M_Start(void * src_addr, void * dst_addr, int num);
extern void DMA1_Stream6_USART2_TX_Satrt(void * src_addr, int num);

// Sensor/actuator public API

#include "sensor_control.h"
