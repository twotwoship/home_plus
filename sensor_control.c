#include "timer.h"
#include "dht11.h"
#include "ultrasonic.h"
#include "light_sensor.h"
#include "step_motor.h"
#include "alarm.h"
#include "dc_motor.h"
#include "led.h"
#include "sensor_control.h"

void Sensor_Control_Init(void)
{
	Sensor_Timer_Init();
	LED_Init();
	DC_Motor_Init();
	Step_Motor_Init();
	Alarm_Init();
	DHT11_Init();
	Ultrasonic_Init();
	Light_Sensor_Init();
}
