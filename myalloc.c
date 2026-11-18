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
