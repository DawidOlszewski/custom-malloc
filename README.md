# Custom Memory Manager (malloc)
*project made for uwr operating system classes and boilerplate was provided by lecturer (Krystian Bacławski)*
## Overview

This project is a custom memory management system written in C, providing dynamic memory allocation functions similar to standard `malloc`, `free`, `realloc`, and `calloc`. The memory manager is built using an **explicit free list**, **pointer compression**, and a **segregated fit** strategy to optimize memory allocation, reduce fragmentation, and improve performance.

## Features

- **Dynamic Memory Allocation**:
  - `mm_malloc(size_t size)`: Allocates memory dynamically.
  - `mm_free(void *ptr)`: Frees previously allocated memory.
  - `mm_realloc(void *ptr, size_t size)`: Resizes allocated memory blocks.
  - `mm_calloc(size_t nmemb, size_t size)`: Allocates and initializes memory.
- **Explicit Free Lists**: Maintains a doubly-linked list of free memory blocks for efficient allocation and deallocation.
- **Pointer Compression**: Reduces memory overhead by using compressed pointers for free list management.
- **Segregated Fit Allocation**:
  - Organizes free memory blocks into multiple buckets (bins) based on block sizes.
  - Improves performance by quickly finding the best-fit block for allocation.
- **Heap Consistency Checker**:
  - `mm_checkheap()`: Validates the internal state of the heap to detect memory leaks and corruption.
  
## Usage

### Compilation

To compile the memory manager, use the provided `Makefile`:

```bash
make
```

### Example Usage

Here's a basic example of using the custom memory manager:

```c
#include <stdio.h>
#include "mm.h"

int main() {
    mm_init(); // Initialize memory manager

    int *arr = (int *)mm_malloc(10 * sizeof(int));  // Allocate memory
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }

    arr = (int *)mm_realloc(arr, 20 * sizeof(int));  // Reallocate memory
    mm_free(arr);  // Free allocated memory

    mm_checkheap(1); // Check heap consistency
    return 0;
}
```

## Code Structure

- `mm.c`: Core implementation of memory allocation functions and memory management algorithms.
- `memlib.c`: Simulates the heap and provides low-level memory operations.
- `mm.h`: Header file defining the memory manager's interface.
- `Makefile`: Automates compilation and cleanup of the project.

## How It Works

1. **Explicit Free Lists**: 
   - Maintains a linked list of free memory blocks, allowing efficient insertion and removal during allocation and deallocation.
   
2. **Segregated Fit Allocation**:
   - Free blocks are divided into buckets based on their sizes, which speeds up finding suitable blocks for allocation requests.
   
3. **Pointer Compression**:
   - Optimizes memory usage by reducing the size of pointers used to link free blocks, improving performance and reducing overhead.

4. **Heap Consistency Check**:
   - Regular checks ensure the integrity of the memory structures, preventing fragmentation and corruption.

## Requirements

- GCC Compiler (C99 standard)
- Linux or Unix-based OS

## Testing

Run the following to test for memory leaks and ensure proper execution:

```bash
make grade
```
