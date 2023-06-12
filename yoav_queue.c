#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "queue.h"

typedef struct Node {
    void *data;
    struct Node *next;
} Node;

typedef struct CondNode {
    void *data;
    cnd_t cond;
    struct CondNode *next;
} CondNode;

typedef struct Queue {
    Node *first, *end;
    CondNode *wait_list_first, *wait_list_end;
    mtx_t queue_mutex;
    atomic_int queue_size;
    size_t waiting_count;
    atomic_int visited_count;
} Queue;

Queue queue;

void initQueue(void) {
    /* TODO: take lock here, separate lines. */
    mtx_init(&queue.queue_mutex, mtx_plain);
    mtx_lock(&queue.queue_mutex);
    queue.first = queue.end = NULL;
    queue.wait_list_first = queue.wait_list_end = NULL;
    queue.queue_size = 0;
    queue.waiting_count = 0;
    queue.visited_count = 0;
    mtx_unlock(&queue.queue_mutex);
}

void destroyQueue(void) {
    mtx_lock(&queue.queue_mutex);
    while(queue.first != NULL) {
        Node *temp_1 = queue.first;
        queue.first = queue.first->next;
        free(temp_1);
    }
    queue.end = NULL;

    while(queue.wait_list_first != NULL) {
        CondNode *temp_2 = queue.wait_list_first;
        queue.wait_list_first = queue.wait_list_first->next;
        /* TODO: add flag to the CondNode and assign it here */
        cnd_signal(&temp_2->cond);
        cnd_destroy(&temp_2->cond);
        free(temp_2);
    }
    queue.wait_list_end = NULL;
    mtx_unlock(&queue.queue_mutex);
    mtx_destroy(&queue.queue_mutex);
}

void enqueue(void *data) {
    mtx_lock(&queue.queue_mutex);

    if(queue.wait_list_first != NULL) {
        CondNode *temp = queue.wait_list_first;

        queue.wait_list_first = queue.wait_list_first->next;
        if(queue.wait_list_first == NULL) {
            queue.wait_list_end = NULL;
        }

        temp -> data = (void*)data;

        queue.waiting_count--;

        cnd_signal(&temp->cond);
    }

    else {
        Node *new_node = (Node*)malloc(sizeof(Node));
        new_node->data = (void*)data;
        new_node->next = NULL;

        if(queue.end == NULL) {
            queue.first = new_node;
            queue.end = new_node;
        } else {
            queue.end->next = new_node;
            queue.end = new_node;
        }

    }

    queue.queue_size++;

    mtx_unlock(&queue.queue_mutex);
}

void* dequeue(void) {
    CondNode *new_cond_node = NULL;
    void *data;

    mtx_lock(&queue.queue_mutex);

    if(queue.first == NULL) {
        /* There are no items in the queue from before */
        new_cond_node = (CondNode*)malloc(sizeof(CondNode));
        cnd_init(&new_cond_node->cond);
        new_cond_node->next = NULL;
        new_cond_node->data = NULL;


        if(queue.wait_list_first == NULL) {
            queue.wait_list_first = new_cond_node;
            queue.wait_list_end = new_cond_node;
        } else {
            queue.wait_list_end->next = new_cond_node;
            queue.wait_list_end = new_cond_node;
        }

        queue.waiting_count++;

        cnd_wait(&new_cond_node->cond, &queue.queue_mutex);
        /* TODO: check wakeup flag in the CondNode return NULL, if woke up from destroy. */
        data = new_cond_node->data;

        free(new_cond_node);
    }

    else {
        /* There are items in the queue from before */
        Node *temp = queue.first;
        data = temp->data;
        queue.first = queue.first->next;
        if(queue.first == NULL) {
            queue.end = NULL;
        }
        free(temp);

    }
    /* TODO: what is the definition of waiting ? */
    /* TODO: make sure what is the definition of size & visited */
    queue.queue_size--;
    queue.visited_count++;

    mtx_unlock(&queue.queue_mutex);

    return data;
}

bool tryDequeue(void **data) {
    bool dequeued = false;
    /* TODO: make sure correctness requirement 2 is relevant to tryDequeue as well */
    mtx_lock(&queue.queue_mutex);

    if(queue.first != NULL) {
        Node *temp = queue.first;
        *data = temp->data;
        queue.first = queue.first->next;
        if(queue.first == NULL) {
            queue.end = NULL;
        }
        free(temp);

        queue.queue_size--;
        queue.visited_count++;

        dequeued = true;
    }

    mtx_unlock(&queue.queue_mutex);

    return dequeued;
}

size_t size(void) {
    return atomic_load(&queue.queue_size);
}

size_t waiting(void) {
    size_t waiting_count;
    mtx_lock(&queue.queue_mutex);
    waiting_count = queue.waiting_count;
    mtx_unlock(&queue.queue_mutex);
    return waiting_count;
}

size_t visited(void) {
    return atomic_load(&queue.visited_count);
}