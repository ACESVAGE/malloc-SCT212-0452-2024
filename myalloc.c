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
