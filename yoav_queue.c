#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "queue.h"

typedef struct Node {
    void *data;
    struct Node *next;
} Node;

typedef struct CondNode {
    thrd_t thread_id;
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
    thrd_t token;
    bool has_token;
} Queue;

Queue queue;

void initQueue(void) {
    queue.first = queue.end = NULL;
    queue.wait_list_first = queue.wait_list_end = NULL;
    mtx_init(&queue.queue_mutex, mtx_plain);
    queue.queue_size = 0;
    queue.waiting_count = 0;
    queue.visited_count = 0;
    queue.has_token = false;
}

void destroyQueue(void) {
    mtx_lock(&queue.queue_mutex);
    while(queue.first != NULL) {
        Node *temp = queue.first;
        queue.first = queue.first->next;
        free(temp);
    }
    queue.end = NULL;

    while(queue.wait_list_first != NULL) {
        CondNode *temp = queue.wait_list_first;
        queue.wait_list_first = queue.wait_list_first->next;
        cnd_destroy(&temp->cond);
        free(temp);
    }
    queue.wait_list_end = NULL;
    mtx_unlock(&queue.queue_mutex);
    mtx_destroy(&queue.queue_mutex);
}

void enqueue(const void *data) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    new_node->data = (void*)data;
    new_node->next = NULL;

    mtx_lock(&queue.queue_mutex);

    if(queue.end == NULL) {
        queue.first = new_node;
        queue.end = new_node;
    } else {
        queue.end->next = new_node;
        queue.end = new_node;
    }

    queue.queue_size++;

    if(queue.wait_list_first != NULL && !queue.has_token) {
        CondNode *temp = queue.wait_list_first;
        queue.wait_list_first = queue.wait_list_first->next;
        if(queue.wait_list_first == NULL) {
            queue.wait_list_end = NULL;
        }
        queue.waiting_count--;

        queue.token = temp->thread_id;
        queue.has_token = true;

        cnd_signal(&temp->cond);
        free(temp);
    }

    mtx_unlock(&queue.queue_mutex);
}

void* dequeue(void) {
    mtx_lock(&queue.queue_mutex);

    if(queue.first == NULL || (queue.has_token && !thrd_equal(queue.token, thrd_current()))) {
        CondNode *new_cond_node = (CondNode*)malloc(sizeof(CondNode));
        new_cond_node->thread_id = thrd_current();
        cnd_init(&new_cond_node->cond);
        new_cond_node->next = NULL;

        if(queue.wait_list_first == NULL) {
            queue.wait_list_first = new_cond_node;
            queue.wait_list_end = new_cond_node;
        } else {
            queue.wait_list_end->next = new_cond_node;
            queue.wait_list_end = new_cond_node;
        }

        queue.waiting_count++;

        cnd_wait(&new_cond_node->cond, &queue.queue_mutex);
    }

    Node *temp_1 = queue.first;
    void *data = temp_1->data;
    queue.first = queue.first->next;
    if(queue.first == NULL) {
        queue.end = NULL;
    }
    free(temp_1);

    queue.queue_size--;
    queue.visited_count++;

    if(queue.wait_list_first != NULL && queue.queue_size > 0) {
        CondNode *temp_2 = queue.wait_list_first;
        queue.wait_list_first = queue.wait_list_first->next;
        if(queue.wait_list_first == NULL) {
            queue.wait_list_end = NULL;
        }
        queue.waiting_count--;

        queue.token = temp_2->thread_id;
        queue.has_token = true;

        cnd_signal(&temp_2->cond);
        free(temp_2);
    } else {
        queue.has_token = false;
    }

    mtx_unlock(&queue.queue_mutex);

    return data;
}

bool tryDequeue(void **data) {
    bool dequeued = false;

    if(mtx_trylock(&queue.queue_mutex) == thrd_success) {
        if(queue.first != NULL && (!queue.has_token || thrd_equal(queue.token, thrd_current()))) {
            Node *temp = queue.first;
            *data = temp->data;
            queue.first = queue.first->next;
            if(queue.first == NULL) {
                queue.end = NULL;
            }
            free(temp);

            queue.queue_size--;
            queue.visited_count++;

            if(queue.wait_list_first != NULL) {
                queue.token = queue.wait_list_first->thread_id;
            } else {
                queue.has_token = false;
            }

            dequeued = true;
        }

        mtx_unlock(&queue.queue_mutex);
    }

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