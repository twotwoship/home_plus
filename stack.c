#include "stack.h"

/* ========================================
   CHARACTER STACK IMPLEMENTATIONS
   ======================================== */

void char_stack_init(char_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return;
	}
	p_stack->top = 0; // 0으로 초기화 (unsigned)
}

stack_status_t char_stack_is_empty(const char_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	return (p_stack->top == 0) ? STACK_EMPTY : STACK_OK;
}

stack_status_t char_stack_is_full(const char_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	return (p_stack->top >= STACK_SIZE) ? STACK_FULL : STACK_OK;
}

stack_status_t char_stack_push(char_stack_t *p_stack, char data)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	
	// 내부 최적화: 함수 호출 대신 직접 변수 비교로 오버헤드 감소
	if (p_stack->top >= STACK_SIZE) {
		return STACK_FULL;
	}
	
	p_stack->buff[p_stack->top++] = data;
	return STACK_OK;
}

stack_status_t char_stack_pop(char_stack_t *p_stack, char *p_data)
{
	if ((p_stack == NULL) || (p_data == NULL)) {
		return STACK_ERROR;
	}
	
	if (p_stack->top == 0) {
		return STACK_EMPTY;
	}
	
	*p_data = p_stack->buff[--p_stack->top];
	return STACK_OK;
}

stack_status_t char_stack_peek(const char_stack_t *p_stack, char *p_data)
{
	if ((p_stack == NULL) || (p_data == NULL)) {
		return STACK_ERROR;
	}
	
	if (p_stack->top == 0) {
		return STACK_EMPTY;
	}
	
	// top은 '다음 들어갈 빈자리'이므로, 현재 최상단은 top - 1
	*p_data = p_stack->buff[p_stack->top - 1];
	return STACK_OK;
}

/* ========================================
   INTEGER STACK IMPLEMENTATIONS
   ======================================== */

void int_stack_init(int_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return;
	}
	p_stack->top = 0;
}

stack_status_t int_stack_is_empty(const int_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	return (p_stack->top == 0) ? STACK_EMPTY : STACK_OK;
}

stack_status_t int_stack_is_full(const int_stack_t *p_stack)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	return (p_stack->top >= STACK_SIZE) ? STACK_FULL : STACK_OK;
}

stack_status_t int_stack_push(int_stack_t *p_stack, int data)
{
	if (p_stack == NULL) {
		return STACK_ERROR;
	}
	
	if (p_stack->top >= STACK_SIZE) {
		return STACK_FULL;
	}
	
	p_stack->buff[p_stack->top++] = data;
	return STACK_OK;
}

stack_status_t int_stack_pop(int_stack_t *p_stack, int *p_data)
{
	if ((p_stack == NULL) || (p_data == NULL)) {
		return STACK_ERROR;
	}
	
	if (p_stack->top == 0) {
		return STACK_EMPTY;
	}
	
	*p_data = p_stack->buff[--p_stack->top];
	return STACK_OK;
}

stack_status_t int_stack_peek(const int_stack_t *p_stack, int *p_data)
{
	if ((p_stack == NULL) || (p_data == NULL)) {
		return STACK_ERROR;
	}
	
	if (p_stack->top == 0) {
		return STACK_EMPTY;
	}
	
	*p_data = p_stack->buff[p_stack->top - 1];
	return STACK_OK;
}