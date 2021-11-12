#ifndef			__QUEUE_C__
#define			__QUEUE_C__



#include <stdbool.h>
#include <string.h>



typedef struct {
	void *_array;
	size_t _array_length;	// the number of indices in array
	size_t _element_size;

	int _front, _back;
	int _size;
} Queue;


#define 		Queue__init(array)						__Queue__init((void *)(array), sizeof(array), sizeof(*array))
Queue __Queue__init(void *array, int container_size, int element_size)
{
	Queue q;
	q._array = array;
	q._array_length = container_size / element_size;
	q._element_size = element_size;

	q._front = q._back = 0;
	q._size = 0;
	return q;
}


#define			__Queue__addr(_q, _idx) 				(void *)((unsigned long)(_q)->_array + (_idx) * (_q)->_element_size)


#define 		Queue__push(q, src)						__Queue__push((q), (void *)&(src))
bool __Queue__push(Queue *q, const void *src)
{
	int tmp;
	tmp = (q->_back + 1) % q->_array_length;
	if (tmp == q->_front)
		return false;
	q->_back = tmp;
	q->_size++;
	memcpy(__Queue__addr(q, q->_back), src, q->_element_size);
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


#define 		Queue__front(q, type)					*(type *)__Queue__front(q)
void *__Queue__front(const Queue *q)
{
	return __Queue__addr(q, (q->_front + 1) % q->_array_length);
}

#define 		Queue__back(q, type)					*(type *)__Queue__back(q))
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