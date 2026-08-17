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

extern volatile int TIM4_Expired;
extern volatile int TIM5_Expired;
extern volatile int DMA1_STREAM6_DONE;

const char * str[] = {
    "[1] The DMA controller performs direct memory transfer by sharing the system bus with the Cortex™-M3 core.\n",
	"[2] The DMA request may stop the CPU access to the system bus for some bus cycles,\n",
	"[3] when the CPU and DMA are targeting the same destination (memory or peripheral).\n",
	"[4] The bus matrix implements round-robin scheduling, thus ensuring at least half of the system bus bandwidth (both to memory and peripheral) for the CPU.\n",
	"[5] After an event, the peripheral sends a request signal to the DMA Controller.\n",
	"[6] The DMA controller serves the request depending on the channel priorities.\n",
	"[7] As soon as the DMA Controller accesses the peripheral, an Acknowledge is sent to the peripheral by the DMA Controller.\n",
	"[8] The peripheral releases its request as soon as it gets the Acknowledge from the DMA Controller.\n",
	"[9] Once the request is deasserted by the peripheral, the DMA Controller release the Acknowledge.\n"
};

static void (*func[]) (int)={
    led_control,
    dc_motor_control,
    step_motor_control,
    alarm_control
};

static void Uart2_Wait_for_TX_Complete(void)
//UART마지막 글자가 DR에 존재, 버퍼는 빈 상태에서 버퍼의 인터럽트를 읽고 DR의 값을 DMA가 자신의 글로 덮어쓰는 경우 방지
{
    while(!Macro_Check_Bit_Set(USART2->SR, 6));
}

volatile int Uart_Data_In = 0;
volatile unsigned char Uart_Data = 0;

void Main(void)
{
    unsigned int str_num = 0;

    Sys_Init(115200);
    Uart2_RX_Interrupt_Enable(1);
    printf("DMA(M2P) Test - H/W (USART) Trigger\n\n");
    TIM5_Repeat_Interrupt_Enable(1000);
    Uart2_Wait_for_TX_Complete();

    for (;;)
    {     
        // static unsigned int led = 0;
        // LED_Display(led ^= 0x1);
        if(TIM5_Expired)
        {
            if (DMA1_STREAM6_DONE || (str_num == 0))
            {
                if (str_num < (sizeof(str) / sizeof(str[0])))
                {
                    Macro_Set_Bit(USART2->CR3, 7);
                    DMA1_Stream6_USART2_TX_Satrt((void * )str[str_num], strlen(str[str_num]));
                    str_num++;
                    DMA1_STREAM6_DONE = 0;                
                }
                else
                {
                    Macro_Clear_Bit(DMA1_Stream6->CR, 0);	
                    Macro_Clear_Bit(USART2->CR3, 7);                    

                    TIM5_Expired = 0;
                }
            }
        }        
    }
}


void led_control(int a)
{

}

void dc_motor_control(int a)
{

}

void step_motor_control(int a)
{

}

void alarm_control(int a)
{

}