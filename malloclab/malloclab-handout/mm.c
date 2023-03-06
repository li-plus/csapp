/*
 * A dynamic memory allocator implemented in multiple algorithms.
 *
 * Free blocks could be organized by implicit/explicit/segregated free lists.
 * Define one of the below macros to enable the corresponding algorithm.
 * - MM_IMPLICIT: free blocks are linked implicitly by the size fields in the
 * headers.
 * - MM_EXPLICIT: uses an explicit doubly linked list to organize the free
 * blocks, by storing a prev and next pointer in each free block.
 * - MM_SEGREGATED: maintains multiple free lists where each list holds blocks
 * that are roughly the same size.
 *
 * The placement strategy could be first-fit or best-fit, enabled by the macros
 * below.
 * - MM_FIRST_FIT: searches the free list from the beginning and chooses the
 * first free block that fits.
 * - MM_BEST_FIT: examines every free block and chooses the free block with the
 * smallest size that fits.
 *
 * Splitting and coalescing are performed as long as possible.
 * - After placing a newly allocated block in a free block, the free block is
 * split into an allocated block and a free block, if the remainder is large
 * enough for a new block.
 * - After freeing an allocated block, adjacent free blocks always coalesce into
 * a single larger free block.
 *
 * The block format is shown below. An allocated block contains a header
 * followed by the user payload. The header is a size_t (32-bit) integer. The
 * first 29 bits encode the entire block size in double words. The P bit is 1
 * only if the previous block is allocated. The A bit is 1 only if this block is
 * allocated. Allocated blocks need no footer since they do not coalesce. A free
 * block contains both header and footer sections. The footer is always
 * identical to the header, providing block size information for the next block
 * when it needs to coalesce this block. When using explicit or segregated free
 * lists, each free block is also a node of an external doubly linked free list.
 * The previous and next pointers are stored in the payload section of each free
 * block.
 */
/* clang-format off */
/*
 *  31                                0            31                                0
 * +-----------------------------+-+-+-+          +-----------------------------+-+-+-+
 * |           Block Size        | |P|A|  Header  |           Block Size        | |P|A| P: Previous Allocated
 * +-----------------------------+-+-+-+ <- bp -> +-----------------------------+-+-+-+ A: Allocated
 * |                                   |          |    Prev Pointer (expl/segr only)  |
 * |                                   |          +-----------------------------------+
 * |             Payload               |          |    Next Pointer (expl/segr only)  |
 * |                                   |          +-----------------------------------+
 * |                                   |          |             Padding               |
 * |                                   |          +-----------------------------+-+-+-+
 * |                                   |  Footer  |           Block Size        | |P|A|
 * +-----------------------------------+          +-----------------------------+-+-+-+
 *            Allocated Block                                   Free Block
 */
/* clang-format on */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* Using segregated free list algorithm by default */
#if !defined(MM_IMPLICIT) && !defined(MM_EXPLICIT) && !defined(MM_SEGREGATED)
#define MM_SEGREGATED
#endif

/* Using best fit strategy by default */
#if !defined(MM_FIRST_FIT) && !defined(MM_BEST_FIT)
#define MM_BEST_FIT
#endif

/* Common utils */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void *block_head;
static void *block_tail;

static const size_t ALIGNMENT = 8;
static inline size_t align(size_t size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}
static inline void set_meta(void *p, size_t size, size_t prev_alloc,
                            size_t alloc) {
    *(size_t *)p = size | (prev_alloc << 1) | alloc;
}
static inline void set_size(void *p, size_t size) {
    *(size_t *)p = (*(size_t *)p & 0x7) | size;
}
static inline void set_prev_alloc(void *p, size_t prev_alloc) {
    *(size_t *)p = (*(size_t *)p & ~0x2) | (prev_alloc << 1);
}
static inline void set_alloc(void *p, size_t alloc) {
    *(size_t *)p = (*(size_t *)p & ~0x1) | alloc;
}
static inline size_t get_size(void *p) { return *(size_t *)p & ~0x7; }
static inline size_t get_prev_alloc(void *p) {
    return (*(size_t *)p >> 1) & 0x1;
}
static inline size_t get_alloc(void *p) { return *(size_t *)p & 0x1; }

static inline void *header_ptr(void *bp) { return (char *)bp - sizeof(size_t); }
static inline void *footer_ptr(void *bp) {
    return (char *)bp + get_size(header_ptr(bp)) - 2 * sizeof(size_t);
}
static inline void *prev_footer_ptr(void *bp) {
    return (char *)bp - 2 * sizeof(size_t);
}
static inline void *next_block(void *bp) {
    return (char *)bp + get_size(header_ptr(bp));
}
static inline void *prev_block(void *bp) {
    return (char *)bp - get_size(prev_footer_ptr(bp));
}
static inline void sync_footer(void *bp) {
    *(size_t *)footer_ptr(bp) = *(size_t *)header_ptr(bp);
}

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
} list_node_t;

static inline void list_init(list_node_t *head) {
    head->prev = head->next = head;
}
static inline void list_push_front(list_node_t *head, list_node_t *node) {
    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
}
static inline void list_erase(list_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}
static inline list_node_t *list_begin(list_node_t *head) { return head->next; }
static inline list_node_t *list_end(list_node_t *head) { return head; }

static inline void mm_print_heap() {
    printf("  HEAP: ");
    void *bp;
    for (bp = block_head; bp != block_tail; bp = next_block(bp)) {
        printf("[%d/%d/%d %p] -> ", get_size(header_ptr(bp)),
               get_prev_alloc(header_ptr(bp)), get_alloc(header_ptr(bp)), bp);
    }
    printf("[%d/%d/%d %p]\n", get_size(header_ptr(bp)),
           get_prev_alloc(header_ptr(bp)), get_alloc(header_ptr(bp)), bp);
}

static inline void mm_print_list(list_node_t *head) {
    for (list_node_t *fp = list_begin(head); fp != list_end(head);
         fp = fp->next) {
        printf("[%d/%d/%d %p] -> ", get_size(header_ptr(fp)),
               get_prev_alloc(header_ptr(fp)), get_alloc(header_ptr(fp)),
               (void *)fp);
    }
    printf("NULL\n");
}

static inline void mm_check_heap(size_t min_block_size) {
    void *bp;
    for (bp = block_head; bp != block_tail; bp = next_block(bp)) {
        if (bp != block_head) {
            assert(get_size(header_ptr(bp)) >= min_block_size &&
                   "block size is too small");
        } else {
            assert(get_size(header_ptr(bp)) == 2 * sizeof(size_t) &&
                   get_alloc(header_ptr(bp)) && "corrupted head block");
        }
        if (!get_alloc(header_ptr(bp))) {
            assert(*(size_t *)footer_ptr(bp) == *(size_t *)header_ptr(bp) &&
                   "header & footer must be the same for free block");
            assert(get_prev_alloc(header_ptr(bp)) &&
                   get_alloc(header_ptr(next_block(bp))) &&
                   "uncoalesced adjacent free blocks");
        }
        assert(get_alloc(header_ptr(bp)) ==
                   get_prev_alloc(header_ptr(next_block(bp))) &&
               "inconsistent alloc & prev_alloc");
    }
    assert(get_size(header_ptr(bp)) == 0 && get_alloc(header_ptr(bp)) == 1 &&
           "corrupted block tail");
}

#if defined(MM_IMPLICIT)
/*
 * Memory malloc using implicit free list.
 *
 * The simplest algorithm. All free and allocated blocks are implicitly linked
 * by the size field in block headers. On processing malloc requests, the
 * allocator traverses along the entire heap to find a free block.
 */

static const size_t IMPLICIT_MIN_BLOCK_SIZE = 2 * sizeof(size_t);

static inline void implicit_free_list_init() {}
static inline void implicit_free_list_insert(void *bp) {}
static inline void implicit_free_list_erase(void *bp) {}
static inline void implicit_free_list_update(void *bp) {}

static inline void implicit_mm_print() { mm_print_heap(); }

static inline void implicit_mm_check() {
    mm_check_heap(IMPLICIT_MIN_BLOCK_SIZE);
}

static inline void *implicit_find_first_fit(size_t alloc_size) {
    for (void *bp = next_block(block_head); bp != block_tail;
         bp = next_block(bp)) {
        if (!get_alloc(header_ptr(bp)) &&
            get_size(header_ptr(bp)) >= alloc_size) {
            return bp;
        }
    }
    return NULL;
}

static inline void *implicit_find_best_fit(size_t alloc_size) {
    void *best_bp = NULL;
    size_t best_size = SIZE_MAX;
    for (void *bp = next_block(block_head); bp != block_tail;
         bp = next_block(bp)) {
        size_t block_size = get_size(header_ptr(bp));
        if (!get_alloc(header_ptr(bp)) && block_size >= alloc_size &&
            block_size < best_size) {
            best_bp = bp;
            best_size = block_size;
        }
    }
    return best_bp;
}

#define MIN_BLOCK_SIZE IMPLICIT_MIN_BLOCK_SIZE
#define free_list_init implicit_free_list_init
#define free_list_insert implicit_free_list_insert
#define free_list_erase implicit_free_list_erase
#define free_list_update implicit_free_list_update
#define do_mm_check implicit_mm_check
#define do_mm_print implicit_mm_print

#if defined(MM_FIRST_FIT)
#define find_fit implicit_find_first_fit
#elif defined(MM_BEST_FIT)
#define find_fit implicit_find_best_fit
#else
#error "must define either MM_FIRST_FIT or MM_BEST_FIT"
#endif

#elif defined(MM_EXPLICIT)
/*
 * Memory malloc using explicit free list.
 *
 * Free blocks are managed by an explicit doubly linked list. A block is
 * inserted into the free list once it is freed, and is removed from the list
 * when allocated. On processing malloc requests, the allocator traverses along
 * the free list to find a proper free block.
 */

static list_node_t explicit_free_list;

static const size_t EXPLICIT_MIN_BLOCK_SIZE =
    2 * sizeof(size_t) + 2 * sizeof(list_node_t *);

static inline void explicit_free_list_init() { list_init(&explicit_free_list); }
static inline void explicit_free_list_insert(void *bp) {
    list_push_front(&explicit_free_list, bp);
}
static inline void explicit_free_list_erase(void *bp) { list_erase(bp); }
static inline void explicit_free_list_update(void *bp) {}

static inline void explicit_mm_print() {
    mm_print_heap();

    printf("  FREE: ");
    mm_print_list(&explicit_free_list);
}

static inline void explicit_mm_check() {
    mm_check_heap(EXPLICIT_MIN_BLOCK_SIZE);

    for (list_node_t *fp = list_begin(&explicit_free_list);
         fp != list_end(&explicit_free_list); fp = fp->next) {
        assert(!get_alloc(header_ptr(fp)) &&
               "blocks in free list must be unallocated");
        assert(fp->prev->next == fp && fp->next->prev == fp &&
               "corrupted free list");
    }
}

static inline void *explicit_find_first_fit(size_t alloc_size) {
    for (list_node_t *fp = list_begin(&explicit_free_list);
         fp != list_end(&explicit_free_list); fp = fp->next) {
        if (get_size(header_ptr(fp)) >= alloc_size) {
            return fp;
        }
    }
    return NULL;
}

static inline void *explicit_find_best_fit(size_t alloc_size) {
    void *best_bp = NULL;
    size_t best_size = SIZE_MAX;
    for (list_node_t *fp = list_begin(&explicit_free_list);
         fp != list_end(&explicit_free_list); fp = fp->next) {
        size_t block_size = get_size(header_ptr(fp));
        if (block_size >= alloc_size && block_size < best_size) {
            best_size = block_size;
            best_bp = fp;
        }
    }
    return best_bp;
}

#define MIN_BLOCK_SIZE EXPLICIT_MIN_BLOCK_SIZE
#define free_list_init explicit_free_list_init
#define free_list_insert explicit_free_list_insert
#define free_list_erase explicit_free_list_erase
#define free_list_update explicit_free_list_update
#define do_mm_check explicit_mm_check
#define do_mm_print explicit_mm_print

#if defined(MM_FIRST_FIT)
#define find_fit explicit_find_first_fit
#elif defined(MM_BEST_FIT)
#define find_fit explicit_find_best_fit
#else
#error "must define either MM_FIRST_FIT or MM_BEST_FIT"
#endif

#elif defined(MM_SEGREGATED)
/*
 * Memory malloc using segregated free list.
 *
 * Block sizes are divided into multiple size classes. Here we define 10 size
 * classes: {16}, {17-32}, {33-64}, ..., {4097-inf}. For each size class, we
 * maintain a free list that only holds free blocks belonging to this size
 * class. When a block is freed, we first determine its size class, and insert
 * it into the free list of this size class. When a block is allocated, it is
 * removed from the list of its size class. When a free block is coalesced with
 * other blocks, its size class may have changed and it needs to be removed and
 * re-inserted into the correct free list.
 */

static const size_t SEGREGATED_MIN_BLOCK_SIZE =
    2 * sizeof(size_t) + 2 * sizeof(list_node_t *);

#define SEGREGATED_NUM_LISTS 10
static list_node_t segregated_free_lists[SEGREGATED_NUM_LISTS];

static inline void segregated_free_list_init() {
    for (size_t i = 0; i < SEGREGATED_NUM_LISTS; i++) {
        list_init(&segregated_free_lists[i]);
    }
}
static inline size_t segregated_free_list_min_size(size_t index) {
    return index ? ((SEGREGATED_MIN_BLOCK_SIZE << (index - 1)) + 1)
                 : SEGREGATED_MIN_BLOCK_SIZE;
}
static inline size_t segregated_free_list_max_size(size_t index) {
    return (index < SEGREGATED_NUM_LISTS - 1)
               ? (SEGREGATED_MIN_BLOCK_SIZE << index)
               : SIZE_MAX;
}
static inline size_t segregated_free_list_lower_bound(size_t size) {
    assert(size >= SEGREGATED_MIN_BLOCK_SIZE && "block is too small");
    size_t index = 0;
    for (size = (size - 1) / SEGREGATED_MIN_BLOCK_SIZE; size; size >>= 1) {
        index++;
    }
    return MIN(index, SEGREGATED_NUM_LISTS - 1);
}
static inline void segregated_free_list_insert(void *bp) {
    size_t size = get_size(header_ptr(bp));
    size_t idx = segregated_free_list_lower_bound(size);
    list_push_front(&segregated_free_lists[idx], bp);
}
static inline void segregated_free_list_erase(void *bp) { list_erase(bp); }
static inline void segregated_free_list_update(void *bp) {
    segregated_free_list_erase(bp);
    segregated_free_list_insert(bp);
}

static inline void segregated_mm_print() {
    mm_print_heap();

    for (size_t i = 0; i < SEGREGATED_NUM_LISTS; i++) {
        printf("  FREE[%d] [%zu,%zu]: ", i, segregated_free_list_min_size(i),
               segregated_free_list_max_size(i));
        mm_print_list(&segregated_free_lists[i]);
    }
}

static inline void segregated_mm_check() {
    mm_check_heap(SEGREGATED_MIN_BLOCK_SIZE);

    for (size_t i = 0; i < SEGREGATED_NUM_LISTS; i++) {
        list_node_t *list = &segregated_free_lists[i];
        size_t min_size = segregated_free_list_min_size(i);
        size_t max_size = segregated_free_list_max_size(i);
        for (list_node_t *fp = list_begin(list); fp != list_end(list);
             fp = fp->next) {
            size_t size = get_size(header_ptr(fp));
            assert(min_size <= size && size <= max_size &&
                   "invalid block size");
            assert(!get_alloc(header_ptr(fp)) &&
                   "blocks in free list must be unallocated");
            assert(fp->prev->next == fp && fp->next->prev == fp &&
                   "corrupted free list");
        }
    }
}

static inline void *segregated_find_first_fit(size_t alloc_size) {
    for (size_t i = segregated_free_list_lower_bound(alloc_size);
         i < SEGREGATED_NUM_LISTS; i++) {
        list_node_t *list = &segregated_free_lists[i];
        for (list_node_t *fp = list_begin(list); fp != list_end(list);
             fp = fp->next) {
            if (get_size(header_ptr(fp)) >= alloc_size) {
                return fp;
            }
        }
    }
    return NULL;
}

static inline void *segregated_find_best_fit(size_t alloc_size) {
    for (size_t i = segregated_free_list_lower_bound(alloc_size);
         i < SEGREGATED_NUM_LISTS; i++) {
        list_node_t *list = &segregated_free_lists[i];
        void *best_bp = NULL;
        size_t best_size = SIZE_MAX;
        for (list_node_t *fp = list_begin(list); fp != list_end(list);
             fp = fp->next) {
            size_t block_size = get_size(header_ptr(fp));
            if (block_size >= alloc_size && block_size < best_size) {
                best_bp = fp;
                best_size = block_size;
            }
        }
        if (best_bp != NULL) {
            return best_bp;
        }
    }
    return NULL;
}

#define MIN_BLOCK_SIZE SEGREGATED_MIN_BLOCK_SIZE
#define free_list_init segregated_free_list_init
#define free_list_insert segregated_free_list_insert
#define free_list_erase segregated_free_list_erase
#define free_list_update segregated_free_list_update
#define do_mm_print segregated_mm_print
#define do_mm_check segregated_mm_check

#if defined(MM_FIRST_FIT)
#define find_fit segregated_find_first_fit
#elif defined(MM_BEST_FIT)
#define find_fit segregated_find_best_fit
#else
#error "must define either MM_FIRST_FIT or MM_BEST_FIT"
#endif

#else
#error "must define one of MM_IMPLICIT, MM_EXPLICIT or MM_SEGREGATED"
#endif

static void place(void *bp, size_t alloc_size) {
    free_list_erase(bp);
    size_t block_size = get_size(header_ptr(bp));
    if (block_size < alloc_size + MIN_BLOCK_SIZE) {
        set_meta(header_ptr(bp), block_size, 1, 1);
        set_prev_alloc(header_ptr(next_block(bp)), 1);
    } else {
        // splitting
        set_meta(header_ptr(bp), alloc_size, 1, 1);
        void *next_bp = next_block(bp);
        set_meta(header_ptr(next_bp), block_size - alloc_size, 1, 0);
        sync_footer(next_bp);
        free_list_insert(next_bp);
    }
}

static void *coalesce(void *bp) {
    size_t size = get_size(header_ptr(bp));

    void *next_bp = next_block(bp);
    size_t next_alloc = get_alloc(header_ptr(next_bp));
    size_t next_size = get_size(header_ptr(next_bp));

    size_t prev_alloc = get_prev_alloc(header_ptr(bp));

    if (!prev_alloc) {
        void *prev_bp = prev_block(bp);
        size_t prev_size = get_size(prev_footer_ptr(bp));
        if (!next_alloc) {
            // coalesce previous & next blocks
            set_meta(header_ptr(prev_bp), prev_size + size + next_size, 1, 0);
            sync_footer(prev_bp);
            free_list_erase(bp);
            free_list_erase(next_bp);
        } else {
            // coalesce previous block
            set_meta(header_ptr(prev_bp), prev_size + size, 1, 0);
            sync_footer(prev_bp);
            free_list_erase(bp);
        }
        bp = prev_bp;
    } else {
        if (!next_alloc) {
            // coalesce next block
            set_meta(header_ptr(bp), size + next_size, 1, 0);
            sync_footer(bp);
            free_list_erase(next_bp);
        }
    }
    // block size may have changed, update the block location in free list
    free_list_update(bp);
    return bp;
}

static void *extend_heap(size_t size) {
    size = MAX(align(size), MIN_BLOCK_SIZE);
    void *old_tail;
    if ((old_tail = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }
    block_tail = (char *)old_tail + size;
    size_t prev_alloc = get_prev_alloc(header_ptr(old_tail));
    set_meta(header_ptr(old_tail), size, prev_alloc, 0);
    sync_footer(old_tail);
    free_list_insert(old_tail);
    set_meta(header_ptr(block_tail), 0, 0, 1);
    return coalesce(old_tail);
}

static int do_mm_init(void) {
    free_list_init();
    if ((block_head = mem_sbrk(4 * sizeof(size_t))) == (void *)-1) {
        return -1;
    }
    block_head = (char *)block_head + 2 * sizeof(size_t);
    block_tail = (char *)block_head + 2 * sizeof(size_t);
    set_meta(header_ptr(block_head), 2 * sizeof(size_t), 1, 1);
    set_meta(header_ptr(block_tail), 0, 1, 1);
    return 0;
}

static void *do_mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size_t alloc_size = MAX(align(size + sizeof(size_t)), MIN_BLOCK_SIZE);

    void *bp;
    if ((bp = find_fit(alloc_size)) != NULL) {
        place(bp, alloc_size);
        return bp;
    }

    size_t extend_size = alloc_size;
    if (!get_prev_alloc(header_ptr(block_tail))) {
        extend_size -= get_size(prev_footer_ptr(block_tail));
    }

    if ((bp = extend_heap(extend_size)) == NULL) {
        return NULL;
    }
    place(bp, alloc_size);
    return bp;
}

static void do_mm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(get_alloc(header_ptr(ptr)) && "double free or corruption");
    set_alloc(header_ptr(ptr), 0);
    sync_footer(ptr);
    free_list_insert(ptr);
    set_prev_alloc(header_ptr(next_block(ptr)), 0);
    coalesce(ptr);
}

static void *do_mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return do_mm_malloc(size);
    }
    if (size == 0) {
        do_mm_free(ptr);
        return NULL;
    }
    size_t old_size = get_size(header_ptr(ptr)) - sizeof(size_t);

    size_t alloc_size = MAX(align(size + sizeof(size_t)), MIN_BLOCK_SIZE);

    void *next_bp = next_block(ptr);
    if (next_bp == block_tail || (!get_alloc(header_ptr(next_bp)) &&
                                  next_block(next_bp) == block_tail)) {
        // extend heap if needed
        size_t merged_size =
            get_size(header_ptr(ptr)) + get_size(header_ptr(next_bp));
        if (merged_size < alloc_size) {
            if (extend_heap(alloc_size - merged_size) == NULL) {
                return NULL;
            }
        }
    }

    if (!get_alloc(header_ptr(next_bp))) {
        // merge next free block
        free_list_erase(next_bp);
        set_size(header_ptr(ptr),
                 get_size(header_ptr(ptr)) + get_size(header_ptr(next_bp)));
        set_prev_alloc(header_ptr(next_block(ptr)), 1);
    }

    size_t this_size = get_size(header_ptr(ptr));
    if (alloc_size <= this_size) {
        // in-place realloc
        if (alloc_size + MIN_BLOCK_SIZE <= this_size) {
            // shrink to fit
            set_size(header_ptr(ptr), alloc_size);
            void *next_bp = next_block(ptr);
            set_meta(header_ptr(next_bp), this_size - alloc_size, 1, 1);
            do_mm_free(next_bp);
        }
        return ptr;
    }

    void *new_ptr;
    if ((new_ptr = do_mm_malloc(size)) == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, MIN(size, old_size));
    do_mm_free(ptr);
    return new_ptr;
}

int mm_init(void) {
    int ret = do_mm_init();
#ifdef MM_CHECK
    do_mm_check();
#endif
#ifdef MM_VERBOSE
    printf("\nINIT:\n");
    do_mm_print();
#endif
    return ret;
}

void *mm_malloc(size_t size) {
    void *ptr = do_mm_malloc(size);
#ifdef MM_CHECK
    do_mm_check();
#endif
#ifdef MM_VERBOSE
    printf("MALLOC %d:\n", size);
    do_mm_print();
#endif
    return ptr;
}

void mm_free(void *ptr) {
    do_mm_free(ptr);
#ifdef MM_CHECK
    do_mm_check();
#endif
#ifdef MM_VERBOSE
    printf("FREE %p:\n", ptr);
    do_mm_print();
#endif
}

void *mm_realloc(void *ptr, size_t size) {
    void *p = do_mm_realloc(ptr, size);
#ifdef MM_CHECK
    do_mm_check();
#endif
#ifdef MM_VERBOSE
    printf("REALLOC %d at %p:\n", size, ptr);
    do_mm_print();
#endif
    return p;
}
