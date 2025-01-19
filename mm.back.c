/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  Blocks are never coalesced or reused.  The size of
 * a block is found at the first aligned word before the block (we need
 * it for realloc).
 *
 * This code is correct and blazingly fast, but very bad usage-wise since
 * it never frees anything.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
#define debug(...) printf(_VA_ARGS_)
#else
#define debug(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


#define FIELD_SIZE(type, field) sizeof(((type *)0)->field)

typedef struct blck_t blck_t;

struct blck_t {
  // len % 16 | (prev_used << 1) | curr_used;
  int32_t info; // TODO: zrób strukture header
  blck_t* prev; // TODO: zmniejszenie do 32bajtów czyli adresowanie względne
  blck_t* next;
  /*
   * We don't know what the size of the payload will be, so we will
   * declare it as a zero-length array.  This allow us to obtain a
   * pointer to the start of the payload.
   */
  uint8_t payload[];
} ;




static const uint32_t blck_metadata_size = offsetof(blck_t, payload) + FIELD_SIZE(blck_t, info);

// typedef struct {
//   blck_t* queue;
//   blck_t* fst_blck;
//   uint8_t sentinel[blck_metadata_size];
//   uint8_t payload[];
// } malloc_t;

enum {
  USED = 1,
  PREV_USED = 2,
} BLCK_INFO;

static blck_t* sentinel;
static blck_t* queue; /* Address of the sentinel, because what if q1 is equal 0*/
static blck_t* heap_fst_blck; /* Address of the first block in heap*/
static void* heap_start;   /* Address of the first byte of heap */
static blck_t* heap_end;   /* Address past last byte of last block */

static inline int32_t get_size(int32_t info){
  return info & ~(USED | PREV_USED);
}

static inline int32_t get_blck_size(blck_t* blck){
  return get_size(blck->info);
}

// uwaga zwróci pozytywną wartość, a nie 1;
static inline int32_t is_left_used(blck_t *blck){ //TODO: name it - can i look at left?
  return blck->info & PREV_USED;
}

static inline blck_t* get_left(blck_t * blck){
  assert(!is_left_used(blck));
  int32_t* info = (((int32_t*)blck)-1);
  blck_t* left =  (blck_t*)((uint8_t*)blck - get_size(*info));
  return left;
}

static inline blck_t* get_right(blck_t * blck){
  blck_t* right = (blck_t*)((uint8_t*)blck + (get_size(blck->info)));
  return right;
}

static inline int32_t is_last(blck_t* blck){
  return get_right(blck) == heap_end;
}

static inline int32_t is_fst(blck_t* blck){
  return blck == heap_fst_blck;
}

static inline int32_t is_used(blck_t *blck){
  return blck->info & 1; // TODO: do it by enums; 
}

static inline blck_t* get_next(blck_t * blck){
  return blck->next;
}

static inline blck_t* get_prev(blck_t * blck){
  return blck->prev;
}

// static inline void clear_is_used(blck_t * blck){
//   blck->info &= ~1;
// }

static inline void clear_is_prev_used(blck_t * blck){
  blck->info &= ~2;
}

// static inline void set_is_used(blck_t * blck){
//   blck->info |= 1;
// }

// static inline void set_is_prev_used(blck_t * blck){
//   blck->info |= 2;
// }

/*
  wszędzie size odnosi sie do romiary calej struktury nie tylko zawartego w niej payloadu
*/
// be careful now the blck prev and next are incorrect and shouldn't be used
static void dtch_blck(blck_t* blck){
  assert(get_blck_size(blck) != 0);
  blck_t* prev_blck = blck->prev;
  blck_t* next_blck = blck->next;
  prev_blck->next = blck->next;
  next_blck->prev = blck->prev;
}
//TODO: różne tryby np fifo lifo oraz być może poźniej wskazńik na strukturę danych koljeke
static void attch_blck(blck_t* blck){
  // add to the begining of queue 
  blck_t* sent = queue;
  blck_t* past_fst = queue->next;
  blck->prev = sent;
  blck->next = past_fst;
  sent->next = blck;
  past_fst->prev = blck;
}

//TODO: zrób z to_coupied enuma
static void init_blck(blck_t* blck, uint32_t size, int to_used){
  assert(size % 16 == 0);
  uint32_t info = size | to_used;
  blck->info = info;
  *(uint32_t*)((uint8_t*)get_right(blck)-1) = info;
}

static blck_t* search_free_blck(uint32_t size, uint32_t* fnd_size){
  // first fit
  for(blck_t* free_blck = queue->next; get_size(free_blck->info) != 0; free_blck = free_blck->next){
    if(size <= get_blck_size(free_blck)){
        *fnd_size = get_blck_size(free_blck);
        return free_blck;
    }
  }
  return NULL;
}
static blck_t* increase_heap(uint32_t size){
  assert(size % 16 == 0);
  blck_t *block = mem_sbrk(size);
  if ((long)block < 0){
    return NULL;
  }
  heap_end = (blck_t*)(((uint8_t*)heap_end) + size); 
  return block;
}

/*
1. detaches free neighbours
1. coalesce block that can be coalesced
2. clears is_prev_used on right if right is used
3. returns addres of coalesed blck: left or current
*/
static blck_t* free_n_coalesce(blck_t* blck, uint32_t* size){
  blck_t* coalesed_blck = blck;
  *size = get_blck_size(blck);
  if(!is_last(blck)){
    blck_t* right = get_right(blck);
    if(!is_used(right)){
      //if(get_blck_size(right) == 0) fprintf(stderr, "right: %lu\n", right);
      dtch_blck(right);
      *size += get_blck_size(right);
    }else{
      clear_is_prev_used(right);
    }
  }
  if(!is_fst(blck)){
    blck_t* left = get_left(blck);
    if(!is_used(left)){
      if(get_blck_size(left) == 0) fprintf(stderr, "lefft\n");
      dtch_blck(left);
      *size += get_blck_size(left);
      coalesed_blck = left;
    }
  }
  // left used and right free
  return coalesed_blck;
}

static size_t round_up(size_t size) { //TODO: cehck
  return (size + ALIGNMENT - 1) & -ALIGNMENT;
}

// static size_t get_size(blck_t *block) {
//   return block->info & -3;
// }



// static void set_header(blck_t *block, size_t size, bool is_allocated) {
//   block->info = size | is_allocated;
// }

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void) {
  /* Pad heap start so first payload is at ALIGNMENT. */
  // fprintf(stderr, "%lu\n", ((uintptr_t)mem_heap_hi() + 1) % 16);


  assert(((uintptr_t)mem_heap_hi() + 1) % 16 == 0);
  heap_start = (blck_t*)mem_sbrk(2*ALIGNMENT - offsetof(blck_t, payload));
  heap_end = (blck_t*)((uint8_t*)mem_heap_hi() + 1);
  if ((long)heap_start < 0){
    // fprintf(stderr, "dupa\n");
    return -1;
  }
  sentinel = heap_start;
  sentinel->prev=sentinel;
  sentinel->next=sentinel;
  sentinel->info = 0;
  queue = sentinel;
  return 0;

  
}


/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t payload_size) {
  uint32_t alloced_blck_size = 0;
  uint32_t min_blck_size = round_up(payload_size + blck_metadata_size);
  uint32_t fnd_blck_size = 0;
  blck_t* fnd_blck = search_free_blck(min_blck_size, &fnd_blck_size);
  if(fnd_blck == NULL){
    fnd_blck = increase_heap(min_blck_size);
  }else{
    if(get_blck_size(fnd_blck) == 0) fprintf(stderr, "malloc\n");
    dtch_blck(fnd_blck);
  }

  if(fnd_blck == NULL){
    return NULL;
  }

  uint32_t reminder_size = fnd_blck_size - min_blck_size;
  if(reminder_size >  blck_metadata_size){ 
    blck_t* reminder_blck = (blck_t*)(((uint8_t*)fnd_blck) + min_blck_size);
    init_blck(reminder_blck, fnd_blck_size - min_blck_size, 0); 
    attch_blck(reminder_blck);
    alloced_blck_size = min_blck_size;
  }else{
    alloced_blck_size = fnd_blck_size;
  }
  init_blck(fnd_blck, alloced_blck_size, 1);
  return fnd_blck -> payload;

  // blck_t *block = mem_sbrk(masize);
  // if ((long)block < 0)
  //   return NULL;

  // set_header(block, size, true);
  // return block->payload;
}


void free(void *ptr) {
  blck_t* fnd_blck = ptr - (offsetof(blck_t, payload));
  
  uint32_t coalesced_blck_size;
  blck_t *coalesced_blck = free_n_coalesce(fnd_blck, &coalesced_blck_size);
  init_blck(coalesced_blck,coalesced_blck_size, 0);
  attch_blck(coalesced_blck);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.
 **/
void *realloc(void *old_ptr, size_t size) {
  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0) {
    free(old_ptr);
    return NULL;
  }

  /* If old_ptr is NULL, then this is just malloc. */
  if (!old_ptr)
    return malloc(size);

  void *new_ptr = malloc(size);

  /* If malloc() fails, the original block is left untouched. */
  if (!new_ptr)
    return NULL;

  /* Copy the old data. */
  blck_t *block = old_ptr - offsetof(blck_t, payload);
  size_t old_size = get_blck_size(block);
  if (size < old_size)
    old_size = size;
  memcpy(new_ptr, old_ptr, old_size);

  /* Free the old block. */
  free(old_ptr);

  return new_ptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc(size_t nmemb, size_t size) {
  size_t bytes = nmemb * size;
  void *new_ptr = malloc(bytes);

  /* If malloc() fails, skip zeroing out the memory. */
  if (new_ptr)
    memset(new_ptr, 0, bytes);

  return new_ptr;
}


/* TODO: opis*/
static void assert_heap(){
  for(blck_t* blck = heap_fst_blck; blck  < heap_end; blck = get_right(blck)){
    assert(((uintptr_t)blck % 16) == 0);
    blck_t* next = get_right(blck);
    if(next >= heap_end){
      assert(next == heap_end);
    }
    assert((next >= heap_end) || is_used(blck) || is_used(next));
    if(blck != heap_fst_blck){
      assert(is_used(blck) || is_left_used(blck));
    }
    assert(get_blck_size(blck) % ALIGNMENT == 0);
    assert(get_blck_size(blck) > 0);
  }
  assert(heap_end == (blck_t*)((uint8_t*)mem_heap_hi() + 1));
}



void mm_checkheap(int verbose) {
  assert_heap();
}