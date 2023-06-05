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
    struct q_node *prev;
} q_node;

typedef struct queue
{
    q_node *head;
    q_node *tail;
};

typedef struct thread_node
{
    cnd_t cv;
    void *elem;
    struct thread_node *next;
    struct thread_node *prev;
} thread_node;

typedef struct threads_queue
{
    thread_node *head;
    thread_node *tail;
};

/*TODO: not nessecirily need to be atomic types*/
atomic_size_t size = 0;
atomic_size_t waiting_threads_num = 0;
atomic_size_t visited_elements_num = 0;
atomic_size_t main_q_size = 0;
queue *main_q;
threads_queue *waiting_threads_q;

mtx_t q_lock;

void insert_to_threads_q(thread_node *node)
{
}

void insert_to_main_q(q_node *node)
{
}

thread_node *pop_from_thread_q(void)
{
    thread_node *head;
    head = thread_queue->head;
    thread_queue->head = head->prev;
    if (thread_queue->head == NULL)
    {
        thread_queue->tail = NULL;
    }
    else
    {
        thread_queue->head->next = NULL;
    }
}

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
    /*TODO: implement free functions*/
    /*TODO: can we assume free doesn't fail? we are allowed to assume that malloc doesn't*/
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

void enqueue(void *elem)
{
    /*TODO: what comes before, taking a lock or declarations*/
    mtx_lock(&q_lock);
    q_node *new_node;
    thread_node *t_node;
    if (waiting_threads_num > 0)
    {
        t_node = pop_from_thread_q();
        t_node->elem = elem;
        /*TODO: make the following op atomic and make sure when exactly a thread is not
        considered to be waiting. It is imporatant that it is being done before waking
        the thread. when does the new thread can run again? only after this function realeases
        the lock or after sending the signal??*/
        waiting_threads_num--;
        cnd_signal(&(t_node->cv));
    }
    else
    {
        new_node = (q_node *)malloc(sizeof(q_node));
        new_node->elem = elem;
        new_node->prev = NULL;
        if (main_q->head == NULL)
        {
            /*main queue is empty*/
            main_q->head = t_node;
            main_q->tail = t_node;
            new_node->next = NULL;
        }
        else
        {
            (main_q->tail)->prev = new_node;
            new_node->next = main_q->tail;
            main_q->tail = new_node;
        }
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q_size++;
    }
    /*TODO: make the following op atomic.*/
    size++;
    mtx_unlock(&q_lock);
}

void *dequeue(void)
{
    /*TODO: what comes before, taking a lock or declarations*/
    mtx_lock(&q_lock);
    thread_node *t_node;
    q_node *head;
    void *dequeued_elem;
    if (main_q_size == 0)
    {
        /*Thread needs to got to sleep. inserting it to the threads queue*/
        t_node = (thread_node *)malloc(sizeof(thread_node));
        t_node->prev = NULL;
        cnd_init(&(t_node->cv));
        t_node->elem = NULL;
        if (threads_queue->head == NULL)
        {
            /*threads queue is empty*/
            threads_queue->head = t_node;
            threads_queue->tail = t_node;
            t_node->next = NULL;
        }
        else
        {
            (threads_queue->tail)->prev = t_node;
            t_node->next = threads_queue->tail;
            threads_queue->tail = t_node;
        }
        /*TODO: maybe do this op eariler. make it atomic anyway*/
        waiting_threads_num++ cnd_wait(&(t_node->cv), &q_lock);
        /*Returned from waiting*/
        dequeued_elem = t_node->elem;
        cnd_destroy(&(t_node->cv));
        free(t_node);
    }
    else
    {
        head = main_q->head;
        main_q->head = head->prev;
        if (main_q->head == NULL)
        {
            main_q->tail = NULL;
        }
        else
        {
            main_q->head->next = NULL;
        }
        dequeued_elem = head->elem;
        free(head);
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q_size--;
    }
    /*TODO: make the following ops atomic. is it ok to put these ops here? correctness wise
    of the functions that want the size*/
    visited_elements_num++;
    size--;
    mtx_unlock(&q_lock);
    return dequeued_elem;
}

bool tryDequeue(void **elem_pointer)
{
    mtx_lock(&q_lock);
    q_node *head;

    if (main_q_size > 0)
    {
        head = main_q->head;
        main_q->head = head->prev;
        if (main_q->head == NULL)
        {
            main_q->tail = NULL;
        }
        else
        {
            main_q->head->next = NULL;
        }
        elem_pointer = &(head->elem);
        free(head);
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q_size--;
        /*TODO: make the following op atomic*/
        size--;
        mtx_unlock(&q_lock);
        return true;
    }
    mtx_unlock(&q_lock);
    return false;
}
