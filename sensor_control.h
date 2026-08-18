#ifndef SENSOR_CONTROL_H
#define SENSOR_CONTROL_H

#define SENSOR_ERROR (-1)

void Sensor_Control_Init(void);

int temp_measurement(void);
int ultra_sonic_measurement(void);
int lumen__measurement(void);

void led_control(int on);
void dc_motor_control(int on);
void step_motor_control(int position_percent);
int Step_Motor_Is_Moving(void);
void alarm_control(int on);
int Alarm_Is_Playing(void);

#endif
