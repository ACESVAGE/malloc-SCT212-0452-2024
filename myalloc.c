/*
 * myalloc.c - a small malloc/free built on sbrk()
 *
 * Every piece of the heap is a block: a header followed by the payload.
 * All blocks (free and used) sit in one singly linked list in address order.
 */
#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdio.h>      /* snprintf only; never printf */
#include <string.h>
#include <unistd.h>

#include "myalloc.h"

#define ALIGNMENT   16
#define MAGIC       0xB10C
#define CHUNK       (64 * 1024)   /* minimum we ask the OS for */
#define MIN_PAYLOAD 16

/* ---------- Task 1: header and helpers ---------- */

typedef struct block {
    size_t        size;    /* payload size in bytes             */
    struct block *next;    /* next block, in increasing address */
    int           free;    /* 1 = free, 0 = in use              */
    int           magic;   /* detects bad pointers              */
} block_t;

static size_t align_up(size_t n)
{
    /* add 15, then clear the low 4 bits: rounds up to a multiple of 16 */
    return (n + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1);
}

#define HDR align_up(sizeof(block_t))

static void *payload(block_t *b)
{
    return (char *)b + HDR;
}

static block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

/* where the block's payload ends (and where a neighbour's header would start) */
static char *block_end(block_t *b)
{
    return (char *)b + HDR + b->size;
}

/* ---------- globals ---------- */

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t heap_total = 0;            /* bytes obtained from the OS */
static fit_policy_t policy = FIT_FIRST;

/* ---------- small output helper (no stdio output allowed here) ---------- */

static void say(int fd, const char *s)
{
    size_t len = strlen(s);
    ssize_t r = write(fd, s, len);
    (void)r;
}

/* ---------- Task 2: growing the heap ---------- */

/* Gets at least `need` bytes of payload (and never less than CHUNK in total)
 * from the OS and links it in as a free block at the end of the list. */
static block_t *grow(size_t need)
{
    if (need > SIZE_MAX / 2)
        return NULL;

    size_t total = align_up(HDR + need);
    if (total < CHUNK)
        total = CHUNK;

    /* the break may not be 16-byte aligned, so pad up to the next boundary */
    void *cur = sbrk(0);
    if (cur == (void *)-1)
        return NULL;
    size_t pad = align_up((size_t)cur) - (size_t)cur;

    void *p = sbrk((intptr_t)(pad + total));
    if (p == (void *)-1)
        return NULL;

    /* libc's malloc could have moved the break between the two calls, so
     * align what sbrk actually returned rather than trusting our padding */
    char *start = (char *)align_up((size_t)p);
    char *end = (char *)p + pad + total;
    size_t usable = (size_t)(end - start) & ~(size_t)(ALIGNMENT - 1);
    heap_total += pad + total;

    /* if the old last block is free and touches the new memory, extend it
     * rather than creating a second neighbour that would never be merged */
    if (tail && tail->free && block_end(tail) == start) {
        tail->size += usable;
        return tail;
    }

    block_t *b = (block_t *)start;
    b->size = usable - HDR;
    b->next = NULL;
    b->free = 1;
    b->magic = MAGIC;

    if (tail)
        tail->next = b;
    else
        head = b;
    tail = b;
    return b;
}

/* ---------- Task 3: finding a block ---------- */

/* First-fit: the first free block that is big enough. */
static block_t *find_first(size_t need)
{
    for (block_t *b = head; b; b = b->next)
        if (b->free && b->size >= need)
            return b;
    return NULL;
}

/* ---------- Task 4: splitting ---------- */

/* Cut b so its payload is exactly `need` bytes, if the rest is big enough
 * to become a block of its own (a header plus at least MIN_PAYLOAD bytes). */
static void split_block(block_t *b, size_t need)
{
    if (b->size < need + HDR + MIN_PAYLOAD)
        return;                          /* leftover too small: keep it whole */

    block_t *rest = (block_t *)((char *)b + HDR + need);   /* char * math */
    rest->size = b->size - need - HDR;
    rest->free = 1;
    rest->magic = MAGIC;
    rest->next = b->next;

    b->size = need;
    b->next = rest;
    if (tail == b)
        tail = rest;
}

/* ---------- Task 3: my_malloc ---------- */

void *my_malloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX - 2 * HDR)
        return NULL;

    size_t need = align_up(size);

    block_t *b = find_first(need);
    if (b == NULL) {
        /* nothing fits: get more memory, then the new block must fit */
        if (grow(need) == NULL)
            return NULL;
        b = find_first(need);
        if (b == NULL)
            return NULL;
    }

    split_block(b, need);
    b->free = 0;
    return payload(b);
}

/* ---------- Task 5: coalescing ---------- */

/* Merge every run of free blocks that are also touching in memory. */
static void coalesce(void)
{
    block_t *b = head;
    while (b) {
        block_t *n = b->next;
        if (b->free && n && n->free && block_end(b) == (char *)n) {
            b->size += HDR + n->size;
            b->next = n->next;
            if (tail == n)
                tail = b;
            n->magic = 0;           /* that header no longer exists */
            continue;               /* b may now touch another free block */
        }
        b = b->next;
    }
}

/* ---------- Task 5: my_free ---------- */

void my_free(void *ptr)
{
    if (ptr == NULL)
        return;

    /* reject anything that cannot possibly be one of our payloads before
     * we read the header in front of it */
    char *lo = (char *)head;
    char *hi = tail ? block_end(tail) : NULL;
    if (head == NULL || (char *)ptr < lo + HDR || (char *)ptr >= hi ||
        ((size_t)ptr % ALIGNMENT) != 0) {
        say(2, "my_free: invalid pointer\n");
        return;
    }

    block_t *b = header(ptr);
    if (b->magic != MAGIC) {
        say(2, "my_free: invalid pointer\n");
        return;
    }
    if (b->free) {
        say(2, "my_free: double free detected\n");
        return;
    }

    b->free = 1;
    coalesce();
}

/* ---------- Task 6: my_calloc ---------- */

void *my_calloc(size_t n, size_t size)
{
    if (n != 0 && size > SIZE_MAX / n)
        return NULL;                 /* n * size would overflow */

    size_t total = n * size;
    void *p = my_malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}
