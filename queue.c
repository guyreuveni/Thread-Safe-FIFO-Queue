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
    atomic_size_t size;
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
    atomic_size_t size;
} threads_queue;

/*TODO: not nessecirily need to be atomic types*/
atomic_size_t full_size = 0;
/*TODO: delete those in comments*/
/*atomic_size_t waiting_threads_q.size = 0;*/
atomic_size_t visited_elements_num = 0;
/*atomic_size_t main_q.size = 0;*/
queue main_q;
threads_queue waiting_threads_q;

mtx_t q_lock;

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
        free(node);
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
        (main_q.tail)->prev = new_node;
        new_node->next = main_q.tail;
        main_q.tail = new_node;
    }
}

thread_node *pop_from_thread_q(void)
{
    thread_node *head;
    head = waiting_threads_q.head;
    waiting_threads_q.head = head->prev;
    if (waiting_threads_q.head == NULL)
    {
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
    q_node *head;
    head = main_q.head;
    main_q.head = head->prev;
    if (main_q.head == NULL)
    {
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
    /*TODO: sinc the init and destroy funcs*/
    mtx_init(&q_lock, mtx_plain);
    /*TODO: make sure need to lock here. and if tes, what to do if another thread does insertion
    before initQueue gets the lock*/
    mtx_lock(&q_lock);
    main_q.head = NULL;
    main_q.tail = NULL;
    /*TODO: maka atomic?*/
    main_q.size = 0;
    waiting_threads_q.head = NULL;
    waiting_threads_q.tail = NULL;
    /*TODO: maka atomic?*/
    waiting_threads_q.size = 0;
    full_size = 0;
    visited_elements_num = 0;
    mtx_unlock(&q_lock);
}

void destroyQueue(void)
{
    /*TODO: make sure need to lock here*/
    mtx_lock(&q_lock);
    /*TODO: can we assume free doesn't fail? we are allowed to assume that malloc doesn't*/
    free_main_q();
    free_threads_q();
    mtx_unlock(&q_lock);
    mtx_destroy(&q_lock);
}

size_t size(void)
{
    /*TODO: maybe cahnge to return main_q.size. ask what is the definition*/
    /*TODO: make atomic*/
    return full_size;
}

size_t waiting(void)
{
    /*TODO: make atomic and ask what is the definition*/
    return waiting_threads_q.size;
}

size_t visited(void)
{
    /*TODO: make atomic and ask what is the definition*/
    return visited_elements_num;
}

void enqueue(void *elem)
{
    /*TODO: what comes before, taking a lock or declarations*/
    mtx_lock(&q_lock);
    q_node *new_node;
    thread_node *t_node;
    if (waiting_threads_q.size > 0)
    {
        t_node = pop_from_thread_q();
        t_node->elem = elem;
        t_node->legal = true;
        /*TODO: make the following op atomic and make sure when exactly a thread is not
        considered to be waiting. It is imporatant that it is being done before waking
        the thread. when does the new thread can run again? only after this function realeases
        the lock or after sending the signal??*/
        waiting_threads_q.size--;
        cnd_signal(&(t_node->cv));
    }
    else
    {
        new_node = (q_node *)malloc(sizeof(q_node));
        new_node->elem = elem;
        insert_to_main_q(new_node);
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q.size++;
    }
    /*TODO: make the following op atomic.*/
    full_size++;
    mtx_unlock(&q_lock);
}

void *dequeue(void)
{
    /*TODO: what comes before, taking a lock or declarations*/
    mtx_lock(&q_lock);
    thread_node *t_node;
    q_node *head;
    void *dequeued_elem;
    bool legal_pop = true;
    if (main_q.size == 0)
    {
        /*Thread needs to got to sleep. inserting it to the threads queue*/
        t_node = (thread_node *)malloc(sizeof(thread_node));
        cnd_init(&(t_node->cv));
        insert_to_threads_q(t_node);
        /*TODO: maybe do this op eariler. make it atomic anyway*/
        waiting_threads_q.size++;
        cnd_wait(&(t_node->cv), &q_lock);
        /*Returned from waiting*/
        dequeued_elem = t_node->elem;
        legal_pop = t_node->legal;
        cnd_destroy(&(t_node->cv));
        free(t_node);
    }
    else
    {
        head = pop_from_main_q();
        dequeued_elem = head->elem;
        free(head);
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q.size--;
    }
    if (legal_pop)
    {
        /*TODO: make the following ops atomic. is it ok to put these ops here? correctness wise
        of the functions that want the size*/
        visited_elements_num++;
        full_size--;
    }
    mtx_unlock(&q_lock);
    return dequeued_elem;
}

bool tryDequeue(void **elem_pointer)
{
    /*TODO: should i do try lock?*/
    mtx_lock(&q_lock);
    q_node *head;

    if (main_q.size > 0)
    {
        head = pop_from_main_q();
        *elem_pointer = head->elem;
        free(head);
        /*TODO: make the following op atomic. I think not needed acctualy*/
        main_q.size--;
        /*TODO: make the following op atomic*/
        full_size--;
        visited_elements_num++;
        mtx_unlock(&q_lock);
        return true;
    }
    mtx_unlock(&q_lock);
    return false;
}
