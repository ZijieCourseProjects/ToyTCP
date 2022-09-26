//
// Created by Eric Zhao on 22/9/2022.
//

#ifndef TJU_TCP_SRC_QUEUE_H_
#define TJU_TCP_SRC_QUEUE_H_

struct Queue;

extern struct Queue *
newQueue(int capacity);

extern int
enqueue(struct Queue *q, void *value);

extern int
size(struct Queue *q);

extern void *
dequeue(struct Queue *q);

extern void
freeQueue(struct Queue *q);

#endif //TJU_TCP_SRC_QUEUE_H_
