#include        <stdio.h>
#include		"queue.c"


int main()
{
    int _q_container[100 + 1];
	Queue q = Queue__init(_q_container);

    for (int i = 0; i < 10; i++) {
        Queue__push(&q, i);
    }
    while (!Queue__empty(&q)) {
        printf("%d\n", Queue__front(&q, int));
        Queue__pop(&q);
    }
}