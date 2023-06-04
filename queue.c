#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "queue.h"

typedef struct q_node
{
    void *elem;
    struct q_node *next;
} q_node;

typedef struct queue
{
    q_node *head;
    q_node *tail;
};

typedef struct thread_node
{
    cnd_t thread_cv;
    struct thread_node *next;
} thread_node;

typedef struct threads_queue
{
    thread_node *head;
    thread_node *tail;
};

atomic_size_t size = 0;
atomic_size_t waiting_threads_num = 0;
atomic_size_t visited_elements_num = 0;
queue *main_q;
threads_queue *waiting_threads_q;

mtx_t q_lock;

void initQueue(void)
{
    mtx_init(&q_lock, mtx_plain);
    /*TODO: make sure need to lock here. and if tes, what to do if another thread does insertion
    before initQueue gets the lock*/
    mtx_lock(&q_lock);
    main_q = (queue *)malloc(sizeof(queue));
    main_q->head = NULL;
    main_q->tail = NULL;
    waiting_threads_q = (threads_queue *)malloc(sizeof(threads_queue));
    waiting_threads_q->head = NULL;
    waiting_threads_q->tail = NULL;
    size = 0;
    waiting_threads_num = 0;
    visited_elements_num = 0;
    mtx_unlock(&q_lock);
}

void destroyQueue(void)
{
    /*TODO: make sure need to lock here*/
    mtx_lock(&q_lock);
    free_main_q();
    free_threads_q();
    size = 0;
    waiting_threads_num = 0;
    visited_elements_num = 0;
    mtx_unlock(&q_lock);
    mtx_destroy(&q_lock);
}

size_t size(void)
{
    return size;
}

size_t waiting(void)
{
    return waiting_threads_num;
}

size_t visited(void)
{
    return visited_elements_num;
}

void enqueue(void *)
{
    mtx_lock(&q_lock);

    mtx_unlock(&q_lock);
}
