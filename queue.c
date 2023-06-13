#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "queue.h"

/*===================INITIALIZIONS=============================*/

typedef struct q_node
{
    void *elem;
    struct q_node *next;
    struct q_node *prev;
} q_node;

typedef struct queue
{
    q_node *head;
    q_node *tail;
} queue;

typedef struct thread_node
{
    cnd_t cv;
    void *elem;
    bool legal;
    struct thread_node *next;
    struct thread_node *prev;
} thread_node;

typedef struct threads_queue
{
    thread_node *head;
    thread_node *tail;
} threads_queue;

atomic_size_t full_size = 0;
atomic_size_t visited_elements_num = 0;
atomic_size_t waiting_threads_num = 0;
queue main_q;
threads_queue waiting_threads_q;
mtx_t q_lock;

/*HIGH-LEVEL DESCRIPTION:
main_q is a queue holding all the elements that were inserted to the queue
and are waiting for threds to deque them. An element which was inserted to the
queue while there was some thread waiting for it won't get inside this queue.
waiting_threads_q is a queue of the threads that are waiting for elements to
get in the queue. when an element is inserted, the head of this queue, if exists,
will be popped out, woken up, and get the element*/

/*=================== HELPER FUNCTIONS ========================*/

void free_main_q(void)
{
    q_node *node, *next;
    node = main_q.tail;
    while (node != NULL)
    {
        next = node->next;
        free(node);
        node = next;
    }
    main_q.tail = NULL;
    main_q.head = NULL;
}

void free_threads_q(void)
{
    thread_node *node, *next;
    node = waiting_threads_q.tail;
    while (node != NULL)
    {
        next = node->next;
        node->legal = false;
        node->elem = NULL;
        cnd_signal(&(node->cv));
        node = next;
    }
    waiting_threads_q.tail = NULL;
    waiting_threads_q.head = NULL;
}

void insert_to_threads_q(thread_node *node)
{
    node->prev = NULL;
    if (waiting_threads_q.head == NULL)
    {
        /*threads queue is empty*/
        waiting_threads_q.head = node;
        waiting_threads_q.tail = node;
        node->next = NULL;
    }
    else
    {
        /*inserting in the tail*/
        (waiting_threads_q.tail)->prev = node;
        node->next = waiting_threads_q.tail;
        waiting_threads_q.tail = node;
    }
}

void insert_to_main_q(q_node *new_node)
{
    new_node->prev = NULL;
    if (main_q.head == NULL)
    {
        /*main queue is empty*/
        main_q.head = new_node;
        main_q.tail = new_node;
        new_node->next = NULL;
    }
    else
    {
        /*inserting in the tail*/
        (main_q.tail)->prev = new_node;
        new_node->next = main_q.tail;
        main_q.tail = new_node;
    }
}

thread_node *pop_from_thread_q(void)
{
    /*popping the head*/
    thread_node *head;
    head = waiting_threads_q.head;
    waiting_threads_q.head = head->prev;
    if (waiting_threads_q.head == NULL)
    {
        /*threads queue got empty*/
        waiting_threads_q.tail = NULL;
    }
    else
    {
        waiting_threads_q.head->next = NULL;
    }
    return head;
}

q_node *pop_from_main_q(void)
{
    /*popping the head*/
    q_node *head;
    head = main_q.head;
    main_q.head = head->prev;
    if (main_q.head == NULL)
    {
        /*main queue got empty*/
        main_q.tail = NULL;
    }
    else
    {
        main_q.head->next = NULL;
    }
    return head;
}

void initQueue(void)
{
    mtx_init(&q_lock, mtx_plain);
    mtx_lock(&q_lock);
    main_q.head = NULL;
    main_q.tail = NULL;
    waiting_threads_q.head = NULL;
    waiting_threads_q.tail = NULL;
    full_size = 0;
    visited_elements_num = 0;
    waiting_threads_num = 0;
    mtx_unlock(&q_lock);
}

void destroyQueue(void)
{
    mtx_lock(&q_lock);
    free_main_q();
    free_threads_q();
    mtx_unlock(&q_lock);
    mtx_destroy(&q_lock);
}

size_t size(void)
{
    return (size_t)full_size;
}

size_t waiting(void)
{
    return (size_t)waiting_threads_num;
}

size_t visited(void)
{
    return (size_t)visited_elements_num;
}

void enqueue(void *elem)
{
    mtx_lock(&q_lock);
    q_node *new_node;
    thread_node *t_node;
    if (waiting_threads_q.head != NULL)
    {
        /*There is a sleeping thread waiting for an element to get inserted to the queue.
        popping the oldest one which is the head of the waiting_threads_q*/
        t_node = pop_from_thread_q();
        /*the sleeping thread will read elem from this field and will deque it*/
        t_node->elem = elem;
        t_node->legal = true;
        /*waking up the sleeping thread*/
        cnd_signal(&(t_node->cv));
    }
    else
    {
        /*No thread is waiting. the new element is inserted to main_q*/
        new_node = (q_node *)malloc(sizeof(q_node));
        new_node->elem = elem;
        insert_to_main_q(new_node);
    }
    full_size++;
    mtx_unlock(&q_lock);
}

void *dequeue(void)
{
    mtx_lock(&q_lock);
    thread_node *t_node;
    q_node *head;
    void *dequeued_elem;
    bool legal_pop = true;
    if (main_q.head == NULL)
    {
        /*Thread needs to got to sleep. inserting it to the threads queue*/
        t_node = (thread_node *)malloc(sizeof(thread_node));
        cnd_init(&(t_node->cv));
        insert_to_threads_q(t_node);
        waiting_threads_num++;
        cnd_wait(&(t_node->cv), &q_lock);
        /*Returned from waiting.*/
        dequeued_elem = t_node->elem;
        legal_pop = t_node->legal;
        if (legal_pop)
        {
            waiting_threads_num--;
        }
        cnd_destroy(&(t_node->cv));
        free(t_node);
    }
    else
    {
        head = pop_from_main_q();
        dequeued_elem = head->elem;
        free(head);
    }
    if (legal_pop)
    {
        /*the pop is illegal if the thread was woken up by the free method*/
        visited_elements_num++;
        full_size--;
    }
    mtx_unlock(&q_lock);
    return dequeued_elem;
}

bool tryDequeue(void **elem_pointer)
{
    mtx_lock(&q_lock);
    q_node *head;

    if (main_q.head != NULL)
    {
        /*there is an element in the queue that is free to be dequeued*/
        head = pop_from_main_q();
        *elem_pointer = head->elem;
        free(head);
        full_size--;
        visited_elements_num++;
        mtx_unlock(&q_lock);
        return true;
    }
    mtx_unlock(&q_lock);
    return false;
}
