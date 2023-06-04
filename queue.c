#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

#include "queue.h"

atomic_size_t size = 0;
atomic_size_t waiting_threads_num = 0;
atomic_size_t visited_elements_num = 0;
