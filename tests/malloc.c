/*
 * malloc and free implementations
 */

#include "tests/lib.h"

/* each memory segment allocated by malloc has a header
 * - last header has next NULL
 */
typedef struct segment_header_s {
    struct segment_header_s *next; 
    struct segment_header_s *prev; 
    int32_t size;
    uint32_t free;
} segment_header_t;

#define SEG_HEADER_SIZE (sizeof(segment_header_t))

/* heap information for malloc */
static struct {
    struct segment_header_s *start;
} heap;

static void
init_malloc(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    heap.start = syscall_memlimit(NULL);
    heap.start->next = NULL;
    heap.start->prev = NULL;
    heap.start->size = 0;
    heap.start->free = 1;
    return;
}

/* returns free segment of size size for allocation
 * the segment must be divided after this function
 * here only the linked list updated
 */
static segment_header_t *
next_free(segment_header_t *start, int size) {
    void *heap_end, *new_heap_end;
    segment_header_t *seg, *last_seg;
    /* find next free and large enough segment
     * - if not found return the last segment
     */
    seg = start;
    while (seg->next) {
        if ((seg->free) && (seg->size >= size))
            break;
        seg = seg->next;
    }
    if (!seg->free || seg->size < size) {
        /* seg is the last one and too small or in use!
         * get more heap
         */
        heap_end = syscall_memlimit(NULL);
        new_heap_end = syscall_memlimit((char *)heap_end + size + SEG_HEADER_SIZE);
        if (!new_heap_end)
            return NULL;
        /* update the last header */
        /* +1 since the address heap_end is used by the previously
         * last last_segment */
        last_seg = (segment_header_t *)(heap_end+1);
        seg->next = last_seg;
        last_seg->prev = seg;
        seg = last_seg;
    }
    return seg;
}

static void
divide_segment(segment_header_t *seg, int size) {
    segment_header_t *between;
    if (seg->next) {
        /* create a new seg after size bytes */
        between = (segment_header_t *)(seg + size + SEG_HEADER_SIZE);
        between->next = seg->next;
        between->prev = seg;
        between->size = seg->size - size - SEG_HEADER_SIZE;

        seg->next = between;
        seg->size = size;
    } else {
        /* last seg just needs size update */
        seg->size = size;
    }
    return;
}

static void *
data_vaddr(segment_header_t *header) {
    return (void *)((char *)header + SEG_HEADER_SIZE);
}

static segment_header_t *
find_header(void *segment_start) {
    return (void *)((char *)segment_start - SEG_HEADER_SIZE);
}

static void
merge_around(segment_header_t *freed_header) {
    freed_header = freed_header;
}

static int
last_free(segment_header_t *freed_header) {
    freed_header = freed_header;
    return 0;
}

void *
malloc(int size) {
    segment_header_t *seg;

    init_malloc();

    if (size <= 0) return NULL;

    seg = next_free(heap.start, size);
    if (!seg) return NULL;
    /* we found proper slot */
    seg->free = 0;
    divide_segment(seg, size);
    return data_vaddr(seg);
}

void
free(void *ptr) {
    if (!ptr) return;
    segment_header_t *header = find_header(ptr);
    header->free = 1;
    merge_around(header);
    if (last_free(header))
        syscall_memlimit(header);
}
