#include "queue.h"

/**
 * @brief Initialize the circular queue
 * @param p_queue Pointer to queue structure
 */
void queue_init(queue_t *p_queue)
{
	if (p_queue == NULL) {
		return;
	}
	p_queue->front = 0;
	p_queue->rear = 0;
}

/**
 * @brief Check if queue is empty
 * @param p_queue Pointer to queue structure
 * @return QUEUE_EMPTY if empty, QUEUE_OK otherwise
 */
queue_status_t queue_is_empty(const queue_t *p_queue)
{
	if (p_queue == NULL) {
		return QUEUE_ERROR;
	}
	
	if (p_queue->rear == p_queue->front) {
		return QUEUE_EMPTY;
	}

	return QUEUE_OK;
}

/**
 * @brief Check if queue is full
 * @param p_queue Pointer to queue structure
 * @return QUEUE_FULL if full, QUEUE_OK otherwise
 */
queue_status_t queue_is_full(const queue_t *p_queue)
{
	if (p_queue == NULL) {
		return QUEUE_ERROR;
	}
	
	// % QUEUE_SIZE 대신 & QUEUE_MASK 사용 (클럭 소모 대폭 감소)
	if (((p_queue->rear + 1) & QUEUE_MASK) == p_queue->front) {
		return QUEUE_FULL;
	}

	return QUEUE_OK;
}

/**
 * @brief Enqueue a character to the queue (FIFO)
 * @param p_queue Pointer to queue structure
 * @param data Character to enqueue
 * @return QUEUE_OK on success, QUEUE_FULL on queue full, QUEUE_ERROR on invalid input
 */
queue_status_t queue_enqueue(queue_t *p_queue, char data)
{
	if (p_queue == NULL) {
		return QUEUE_ERROR;
	}
	
	if (queue_is_full(p_queue) == QUEUE_FULL) {
		return QUEUE_FULL;
	}
	
	p_queue->buff[p_queue->rear] = data;
	// % 연산 대신 비트 마스킹으로 인덱스 랩어라운드(Wrap-around) 처리
	p_queue->rear = (p_queue->rear + 1) & QUEUE_MASK;

	return QUEUE_OK;
}

/**
 * @brief Dequeue a character from the queue (FIFO)
 * @param p_queue Pointer to queue structure
 * @param p_data Pointer to store dequeued character
 * @return QUEUE_OK on success, QUEUE_EMPTY on queue empty, QUEUE_ERROR on invalid input
 */
queue_status_t queue_dequeue(queue_t *p_queue, char *p_data)
{
	if ((p_queue == NULL) || (p_data == NULL)) {
		return QUEUE_ERROR;
	}
	
	if (queue_is_empty(p_queue) == QUEUE_EMPTY) {
		return QUEUE_EMPTY;
	}
	
	*p_data = p_queue->buff[p_queue->front];
	// % 연산 대신 비트 마스킹 적용
	p_queue->front = (p_queue->front + 1) & QUEUE_MASK;

	return QUEUE_OK;
}

/**
 * @brief Get queue current size
 * @param p_queue Pointer to queue structure
 * @return Number of elements in queue
 */
unsigned int queue_get_size(const queue_t *p_queue)
{
	if (p_queue == NULL) {
		return 0;
	}
	
	// [최적화 마법] 2의 거듭제곱 큐에서는 음수 언더플로우를 비트 마스크가 상쇄해 주므로 
	// 기존의 if-else 분기문 없이 단일 수식으로 완벽하게 크기 계산이 가능합니다.
	return (p_queue->rear - p_queue->front) & QUEUE_MASK;
}