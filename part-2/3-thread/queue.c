/* 
 * queue.c
 * ----------------
 * Generic Queue Structure
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


#ifndef			__QUEUE_C__
#define			__QUEUE_C__




#include 		<stdbool.h>
#include 		<string.h>




typedef struct {
	void *_array;
	size_t _array_length;	// the number of indices in array
	size_t _element_size;

	int _front, _back;
	int _size;
} Queue;


// set array to queue and return queue handler
#define 		Queue__init(array)						__Queue__init((void *)(array), sizeof(array), sizeof(*array))
Queue __Queue__init(void *array, int array_size, int element_size)
{
	Queue q;
	q._array = array;
	q._array_length = array_size / element_size;
	q._element_size = element_size;

	q._front = q._back = 0;
	q._size = 0;
	return q;
}


#define			__Queue__addr(_q, _idx) 				(void *)((unsigned long)(_q)->_array + (_idx) * (_q)->_element_size)


#define 		Queue__push(q, element)					__Queue__push((q), (void *)&(element))
bool __Queue__push(Queue *q, const void *element)
{
	int tmp;
	tmp = (q->_back + 1) % q->_array_length;
	if (tmp == q->_front)
		return false;
	q->_back = tmp;
	q->_size++;
	memcpy(__Queue__addr(q, q->_back), element, q->_element_size);
	return true;
}

bool Queue__pop(Queue *q)
{
	if (q->_front == q->_back)
		return false;
	q->_front = (q->_front + 1) % q->_array_length;
	q->_size--;
	return true;
}


#define 		Queue__front(q, __type__)				*(__type__ *)__Queue__front(q)
void *__Queue__front(const Queue *q)
{
	return __Queue__addr(q, (q->_front + 1) % q->_array_length);
}

#define 		Queue__back(q, __type__)				*(__type__ *)__Queue__back(q))
void *__Queue__back(const Queue *q)
{
	return __Queue__addr(q, (q->_back + 1) % q->_array_length);
}


bool Queue__empty(const Queue *q)
{
	return q->_front == q->_back;
}

int Queue__size(const Queue *q)
{
	return q->_size;
}




#endif