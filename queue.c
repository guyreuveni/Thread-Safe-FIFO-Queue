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

typedef struct waiting_threads_queue
{
    thread_node *head;
    thread_node *tail;
};

atomic_size_t size = 0;
atomic_size_t waiting_threads_num = 0;
atomic_size_t visited_elements_num = 0;

mtx_t insertion_lock;
mtx_t pop_lock;
cnd_t notEmpty;