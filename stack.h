#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdint.h>

#define STACK_SIZE 64

typedef enum {
	STACK_OK = 0,
	STACK_EMPTY,
	STACK_FULL,
	STACK_ERROR
} stack_status_t;

/* 문자형 스택 구조체 */
typedef struct {
	char buff[STACK_SIZE];
	volatile uint16_t top;  // -1 대신 0부터 시작하므로 부호 없는 정수 사용
} char_stack_t;

/* 정수형 스택 구조체 */
typedef struct {
	int buff[STACK_SIZE];
	volatile uint16_t top;  // unsigned integer로 처리 속도 향상
} int_stack_t;

// 함수 원형 선언 (char)
void char_stack_init(char_stack_t *p_stack);
stack_status_t char_stack_is_empty(const char_stack_t *p_stack);
stack_status_t char_stack_is_full(const char_stack_t *p_stack);
stack_status_t char_stack_push(char_stack_t *p_stack, char data);
stack_status_t char_stack_pop(char_stack_t *p_stack, char *p_data);
stack_status_t char_stack_peek(const char_stack_t *p_stack, char *p_data);

// 함수 원형 선언 (int)
void int_stack_init(int_stack_t *p_stack);
stack_status_t int_stack_is_empty(const int_stack_t *p_stack);
stack_status_t int_stack_is_full(const int_stack_t *p_stack);
stack_status_t int_stack_push(int_stack_t *p_stack, int data);
stack_status_t int_stack_pop(int_stack_t *p_stack, int *p_data);
stack_status_t int_stack_peek(const int_stack_t *p_stack, int *p_data);

#endif /* STACK_H */