#include "device_driver.h"
#include <stdio.h>
#include <string.h>

static void Sys_Init(int baud) 
{
	SCB->CPACR |= (0x3 << 10*2)|(0x3 << 11*2); 
	Clock_Init();
	Uart2_Init(baud);
	setvbuf(stdout, NULL, _IONBF, 0);
	LED_Init();
}

extern volatile int TIM5_Expired;
extern volatile int DMA1_STREAM6_DONE;

#define UART_TX_BUFFER_SIZE    64
static char uart_tx_buffer[UART_TX_BUFFER_SIZE];

//함수 포인터
static void (*func[]) (int)={
    led_control,
    dc_motor_control,
    step_motor_control,
    alarm_control
};

#define CMD_LED       0
#define CMD_DC_MOTOR  1
#define CMD_STEP      2
#define CMD_ALARM     3

//명령어 처리 함수
static void Command_Process(char command, int value)
{
    int index;

    switch (command)
    {
        case 'L':
            index = CMD_LED;
            break;

        case 'D':  
            index = CMD_DC_MOTOR;
            break;

        case 'S':
            index = CMD_STEP;
            break;

        case 'A':
            index = CMD_ALARM;
            break;

        default:
            return;
    }

    func[index](value);
}

//파싱함수
static void Command_Receive_Process(void)
{
    static char command = 0;
    static int value = 0;
    static int digit_count = 0;

    char data;
    while (Uart2_Rx_GetChar(&data))
    {        
        if (data == '\r' || data == '\n')
        {
            continue;
        }

        /*
         * 명령어 시작
         */
        if (data == 'L' ||
            data == 'D' ||
            data == 'S' ||
            data == 'A')
        {
            command = data;
            value = 0;
            digit_count = 0;
            continue;
        }

        /*
         * 숫자 입력
         */
        if (data >= '0' && data <= '9')
        {
            /*
             * 명령어가 먼저 들어오지 않았다면 무시
             */
            if (command == 0)
            {
                continue;
            }

            value = value * 10 + (data - '0');
            digit_count++;

            /*
             * 4자리 숫자가 완성되면 실행
             */
            if (digit_count == 4)
            {
                Command_Process(command, value);

                /*
                 * 다음 명령 준비
                 */
                command = 0;
                value = 0;
                digit_count = 0;
            }

            continue;
        }

        /*
         * 그 외의 잘못된 문자가 들어오면 현재 명령 폐기
         */
        command = 0;
        value = 0;
        digit_count = 0;
    
    }
}

static void Uart2_Wait_for_TX_Complete(void)
//UART마지막 글자가 DR에 존재, 버퍼는 빈 상태에서 버퍼의 인터럽트를 읽고 DR의 값을 DMA가 자신의 글로 덮어쓰는 경우 방지
{
    while(!Macro_Check_Bit_Set(USART2->SR, 6));
}

#if 1
void Main(void)
{
    int lumen = 0;
    int temp = 0;
    int hum = 0;
    int ultra_sonic = 0;
    
    Sys_Init(115200);
    Sensor_Control_Init();

    Uart2_RX_Interrupt_Enable(1);
    printf("HomePlus\n\n");
    TIM5_Repeat_Interrupt_Enable(500);
    Uart2_Wait_for_TX_Complete();
    
    for (;;)
    {   
        int tx_len = 0;
        Command_Receive_Process();
        if(TIM5_Expired)
        {
            static unsigned int led = 0;
            static unsigned int led_count = 0;

            TIM5_Expired = 0;
            
            lumen = lumen__measurement();

            led_count++;            
            
            ultra_sonic = ultra_sonic_measurement();

            /* DHT11: 1초마다 */
            if (led_count >= 2)
            {                
                led_count = 0;
                
                led ^= 0x1;
                LED_Display(led);

                /*
                * temp_measurement()
                * return temperature_x10 * 1000 + humidity_x10;
                */
               
                int dht_value = temp_measurement();

                if (dht_value >= 0)
                {
                    temp = dht_value / 1000;
                    hum  = dht_value % 1000;
                    
                    tx_len = snprintf(
                    uart_tx_buffer,
                    sizeof(uart_tx_buffer),
                    "T%04dH%04dU%04dB%04d\n\r",
                    temp,
                    hum,
                    ultra_sonic,
                    lumen
                    );
                }
                else
                {
                    printf("DHT11 ERROR\n");
                }
                
            }
            else
            {
                if(lumen != -1 && ultra_sonic != -1)
                {
                    tx_len = snprintf(
                    uart_tx_buffer,
                    sizeof(uart_tx_buffer),
                    "U%04dB%04d\n\r",                
                    ultra_sonic,
                    lumen);
                }
                else
                {
                    if (lumen == -1)
                    {
                        printf("lumen ERROR\n");
                    }

                    if (ultra_sonic == -1)
                    {
                        printf("ultra_sonic ERROR\n");
                    }
                }
            }
            //printf("%s\n",uart_tx_buffer);
            /*
            * 이전 DMA 전송이 끝난 경우에만 새로운 전송 시작
            */
            if (DMA1_STREAM6_DONE && tx_len != 0)
            {                
                Macro_Set_Bit(USART2->CR3, 7);                

                DMA1_STREAM6_DONE = 0;
                DMA1_Stream6_USART2_TX_Satrt((void *)uart_tx_buffer, tx_len);
            }
        }        
    }        
}

#else

void Main(void){
    volatile int i;
	int test = 1;
    Sys_Init(115200);
    printf("\n=== HOME_PLUS individual device test ===\n");
	for (i = 0; i < 6400000; i++){ __NOP(); }
	alarm_control(1);
    for (;;){
		for (i = 0; i < 6400000; i++){ __NOP(); }
		led_control(test);
		dc_motor_control(test);
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
#endif