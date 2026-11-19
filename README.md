# myalloc

A small malloc, free, calloc and realloc built on sbrk(), written in C for ICS 2305 Systems Programming (Question 2).

Name: Brooklyn Bruce Kidiga
Student ID: SCT212-0452/2024

The benchmark write-up is in REPORT.md.

## Building and running

    git clone git@github.com:ACESVAGE/malloc-SCT212-0452-2024.git
    cd malloc-SCT212-0452-2024
    make            # builds and runs the provided tests
    make bench      # builds and runs the benchmark for Task 8
    make clean

To check it with AddressSanitizer:

    gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
    ./t_asan

## What is done

All eight tasks are done.

1. Block header and helpers.
2. Growing the heap with sbrk.
3. my_malloc with first-fit.
4. Splitting blocks.
5. my_free, with coalescing of neighbouring free blocks.
6. my_calloc and my_realloc.
7. Best-fit, set_policy, heap_stats, heap_dump and heap_check.
8. The experiment and report (REPORT.md, bench.c).

## Test output

The provided tests give this at the end (the full output has T1 to T8):

    T8 randomized stress test
      PASS  no data corruption during 20000 operations
      PASS  everything freed
      PASS  fully coalesced (few free blocks remain)
      PASS  heap consistent

    Result: 29 passed, 0 failed

It compiles with -Wall -Wextra and gives no warnings, and the same 29 checks pass under AddressSanitizer.

## How it works

Every block is a header followed by its payload. The header holds the payload size, a pointer to the next block, a free flag and a magic number. It is 24 bytes but I round it up to 32, so a payload placed right after it stays 16-byte aligned, and every request is rounded up to a multiple of 16 with (n + 15) & ~15. my_malloc returns the address just after the header, and my_free gets back to the header by subtracting 32.

All blocks, free and used, sit in one linked list in address order, with head and tail pointers. When nothing fits, grow() asks sbrk for at least 64 KB, pads the start if the break is not aligned, and adds one free block at the end. If the last block is already free and touches the new memory, it is just made bigger.

my_malloc searches with first-fit by default, or best-fit after set_policy. The block it finds is split if the leftover can hold a header plus at least 16 bytes, and otherwise the caller gets the whole block.

Coalescing runs after every free. I walk the list, and whenever a free block is followed by a free block that starts exactly where the first one ends, I add the second to the first and unlink it. I compare the addresses because two blocks that are neighbours in the list do not always touch in memory. The C library's own malloc can move the break between my sbrk calls and leave a gap, and merging across a gap would claim memory that is not mine.

my_free checks that the pointer is inside the heap, 16-byte aligned and carries the right magic number, so a bad pointer or a double free prints a message and returns instead of crashing. my_realloc has four cases: shrink in place, grow in place by absorbing a free neighbour, move to a new block, and fail, which leaves the old block alone. heap_check walks the list and checks the headers, alignment, ordering, overlaps and that no two touching free blocks were left unmerged.

## Questions

1. Why must the header size be a multiple of 16 as well as the payload sizes?

The payload starts right after the header. If a block starts on a 16-byte boundary but the header is 24 bytes, the payload lands on an address that is 8 more than a multiple of 16, and the pointer I give the user is misaligned. If the header and the payload sizes are both multiples of 16, the next block and every payload also start on a 16-byte boundary.

2. Why can printf() inside my_malloc() cause infinite recursion?

printf can call malloc internally, for example to create the buffer for stdout the first time it is used. If that malloc were my_malloc, then my_malloc would call printf, which would call my_malloc again, and it would never stop. Using snprintf into a local array and then write() avoids this, because neither of them allocates.

3. Your my_free() is O(n). Which extra field would let you find a block's previous neighbour in O(1), and what is the name of the technique?

A footer at the end of each block that stores its size (or a prev_size field, or a prev pointer in a doubly linked list). The technique is called boundary tags. With it, free can read the footer just before its own header, jump straight to the previous block and merge it in constant time, without scanning the list.

4. Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.

Internal fragmentation is wasted space inside an allocated block. In mine it comes from rounding every request up to a multiple of 16, and from handing out a whole block when the leftover is too small to split. External fragmentation is when there is enough free memory in total but it is in many small pieces, so no single piece fits a request. In mine it comes from freeing blocks of different sizes in between blocks that are still in use, which leaves holes.

5. Why does sbrk() memory never get returned to the OS in your allocator?

I only ever call sbrk() with a positive number, and freed blocks just stay in the list to be reused. To give memory back, I could check after a free whether the last block is free and large, and if so call sbrk() with a negative amount to shrink the heap, keeping a little slack and fixing the tail pointer. This only works at the end of the heap, because the break is a single line that moves up or down.

6. Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?

Between two of my sbrk() calls, the C library's own malloc (used by printf in the tests) can move the break, so the second chunk starts later than the first one ended, and the gap belongs to libc. My list then has the two blocks next to each other, but there is foreign memory between them. Before merging, the coalescing code compares the end of a block with the address of the next one and only merges when they are equal.

## Limitations

- my_malloc, my_free and the coalescing pass walk the list, so they are O(n) and get slow with many blocks (see REPORT.md).
- Memory is never given back to the OS.
- It is not thread-safe. There are no locks.
- my_realloc checks the magic number but does not check that the pointer is inside the heap first, so a completely wild pointer could crash it. my_free does check.
- After a block has been merged into its left neighbour, freeing its old pointer prints "invalid pointer" and not "double free detected", because that header no longer exists.
- The pointer check relies on the magic number, so data inside a payload that happens to look like a header could fool it.
- The smallest amount taken from the OS is 64 KB, so a program that allocates a few bytes still takes 64 KB.

## Sources and AI declaration

- The assignment brief and the ICS 2305 course notes.
- Linux man pages for sbrk, brk, write, snprintf and clock_gettime.
- I used Claude, an AI assistant, to help plan the project and to draft the code, the benchmark, the report and this README, including the first draft of the design explanation and the answers above. I built and tested everything myself and went through the code so I could explain it.
