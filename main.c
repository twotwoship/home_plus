#if 1

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
    volatile unsigned int i;
	int test = 1;
    Sys_Init(115200);
    printf("\n=== HOME_PLUS individual device test ===\n");
	for (i = 0; i < SYSCLK/10U; i++){ __NOP(); }
	alarm_control(1);
    for (;;){
		for (i = 0; i < SYSCLK/10U; i++){ __NOP(); }
		led_control(test);
		dc_motor_control(test);
		step_motor_control(100);
		for (i = 0; i < SYSCLK/100U; i++){ __NOP(); }
		printf("lumen = %d\n", lumen__measurement());
		int temp = temp_measurement();
		for (i = 0; i < SYSCLK/100U; i++){ __NOP(); }
		printf("temp = %d\n", temp);
		for (i = 0; i < SYSCLK/100U; i++){ __NOP(); }
        printf("ultra_sonic = %d\n", ultra_sonic_measurement());
		if(test == 1){
			test = 0;
		}else{ test = 1;}
    }
}

#else

#endif
