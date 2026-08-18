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


//DMA 전송
static void UART2_DMA_Send(const char *buffer, int length)
{
    if (length <= 0)
        return;

    if (!DMA1_STREAM6_DONE)
        return;

    Macro_Set_Bit(USART2->CR3, 7);

    DMA1_STREAM6_DONE = 0;

    DMA1_Stream6_USART2_TX_Satrt(
        (void *)buffer,
        length
    );
}

//
static void Uart2_Wait_for_TX_Complete(void)
//UART마지막 글자가 DR에 존재, 버퍼는 빈 상태에서 버퍼의 인터럽트를 읽고 DR의 값을 DMA가 자신의 글로 덮어쓰는 경우 방지
{
    while(!Macro_Check_Bit_Set(USART2->SR, 6));
}

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

        
        // 명령어 시작        
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

        //숫자 입력
        if (data >= '0' && data <= '9')
        {
            //명령어 확인
            if (command == 0)
            {
                continue;
            }

            value = value * 10 + (data - '0');
            digit_count++;

           //4자리 확인
            if (digit_count == 4)
            {
                Command_Process(command, value);
                //초기화
                command = 0;
                value = 0;
                digit_count = 0;
            }

            continue;
        }

        //그 외의 잘못된 문자가 들어오면 현재 명령 폐기
        command = 0;
        value = 0;
        digit_count = 0;    
    }
}

#define SENSOR_ERR_DHT    64U   // 8진수 4 = 10진수 4
#define SENSOR_ERR_LUMEN  8U   // 8진수 2 = 10진수 2
#define SENSOR_ERR_ULTRA  1U   // 8진수 1 = 10진수 1
// E0001 초음파
// E0010 조도센서 에러
// E0100 온습도 에러

// 센서 측정 및 프레임 생성
static int Sensor_Data_Update(void)
{
    static unsigned int led = 0;
    static unsigned int led_count = 0;
    static unsigned int error = 0;

    int lumen = 0;
    int ultra_sonic = 0;
    int dht_value = 0;

    lumen = lumen__measurement();
    ultra_sonic = ultra_sonic_measurement();

    if (lumen == 0)
    {
        error |= SENSOR_ERR_LUMEN;
    }
    else
    {
        error &= ~SENSOR_ERR_LUMEN;
    }

    if (ultra_sonic == -1)
    {
        error |= SENSOR_ERR_ULTRA;
    }
    else
    {
        error &= ~SENSOR_ERR_ULTRA;
    }

    led_count++;

    /* DHT11: 1초마다 */
    if (led_count >= 2)
    {
        led_count = 0;

        led ^= 0x1;
        LED_Display(led);

        dht_value = temp_measurement();

        if (dht_value == -1)
        {
            error |= SENSOR_ERR_DHT;
        }
        else
        {
            error &= ~SENSOR_ERR_DHT;
        }

        /*
         * 1초 주기:
         * 오류가 하나라도 있으면 오류 프레임 전송
         */
        if (error != 0)
        {
            return snprintf(
                uart_tx_buffer,
                sizeof(uart_tx_buffer),
                "E%04o\n\r",
                error
            );
        }

        /*
         * 모든 센서가 정상이면
         * 1초 주기 전체 센서 데이터 전송
         */
        {
            int temp = dht_value / 1000;
            int hum  = dht_value % 1000;

            return snprintf(
                uart_tx_buffer,
                sizeof(uart_tx_buffer),
                "T%04dH%04dU%04dB%04d\n\r",
                temp,
                hum,
                ultra_sonic,
                lumen
            );
        }
    }

    /*
     * 500ms 주기:
     * 이전에 저장된 오류가 있으면 전송하지 않음
     */
    if (error != 0)
    {
        return 0;
    }

    /*
     * 모든 센서가 정상일 때만
     * 500ms 데이터 전송
     */
    return snprintf(
        uart_tx_buffer,
        sizeof(uart_tx_buffer),
        "U%04dB%04d\n\r",
        ultra_sonic,
        lumen
    );
}

#if 1
void Main(void)
{    
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

        if (TIM5_Expired)
        {
            TIM5_Expired = 0;

            tx_len = Sensor_Data_Update();
            UART2_DMA_Send(uart_tx_buffer, tx_len);
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