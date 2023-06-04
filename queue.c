#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

#include "queue.h"

atomic_size_t size = 0;
atomic_size_t num_of_waiting_threads = 0;
