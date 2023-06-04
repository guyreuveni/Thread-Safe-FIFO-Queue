#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

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
    thrd_t tid;
    struct thread_node *next;
} thread_node;

typedef struct waiting_threds_queue
{
    thread_node *head;
    thread_node *tail;
};

atomic_size_t size = 0;
atomic_size_t waiting_threads_num = 0;
atomic_size_t visited_elements_num = 0;
