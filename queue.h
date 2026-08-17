#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdint.h>

// [중요] 비트 연산 최적화를 위해 QUEUE_SIZE는 반드시 2의 거듭제곱(16, 32, 64, 128...)이어야 합니다.
#define QUEUE_SIZE 64  
#define QUEUE_MASK (QUEUE_SIZE - 1) // 64 - 1 = 63 (0x3F)

typedef enum {
	QUEUE_OK = 0,
	QUEUE_EMPTY,
	QUEUE_FULL,
	QUEUE_ERROR
} queue_status_t;

typedef struct {
	char buff[QUEUE_SIZE];
	// 인터럽트(ISR)와 메인 루프에서 동시 접근 시 컴파일러 최적화 방지
	volatile unsigned int front; 
	volatile unsigned int rear;  
} queue_t;

void queue_init(queue_t *p_queue);
queue_status_t queue_is_empty(const queue_t *p_queue);
queue_status_t queue_is_full(const queue_t *p_queue);
queue_status_t queue_enqueue(queue_t *p_queue, char data);
queue_status_t queue_dequeue(queue_t *p_queue, char *p_data);
unsigned int queue_get_size(const queue_t *p_queue);

#endif // QUEUE_H