# Task 8: first-fit, best-fit and the C library

Name: Brooklyn Bruce Kidiga
Student ID: SCT212-0452/2024

## What I ran

bench.c does 50,000 random operations using srand(12345). There are 1,000 slots. Each step picks a slot at random. If the slot holds a block it is freed, and if it is empty a block of 1 to 4096 bytes is allocated into it. The same sequence is run with first-fit, with best-fit, and with the C library's malloc and free. Each run is in its own forked process so one run's heap does not affect the next. Build and run it with `make bench`.

Machine: AMD Ryzen 5 PRO 5650U (6 cores, 12 threads), Ubuntu on WSL2, gcc 15.2.0. Compiled with -O2.

## Results

| | First-fit | Best-fit |
|---|---|---|
| heap_bytes | 1,310,720 | 1,310,720 |
| free_bytes | 226,016 | 226,848 |
| free_blocks | 221 | 162 |
| largest_free | 84,112 | 66,960 |
| external fragmentation (1 - largest_free / free_bytes) | 0.628 | 0.705 |
| time for 50,000 operations | 0.250 s | 0.299 s |

The C library's malloc and free took 0.0071 s on the same workload.

The first five rows are the same on every run because the workload is seeded. The times change a little between runs, so I took the middle one of three runs. The ranges were 0.245 to 0.258 s for first-fit, 0.295 to 0.301 s for best-fit and 0.0070 to 0.0074 s for the C library. heap_check() returned 1 after both runs.

## Which policy I would choose

On this workload first-fit did better on both things I looked at. Its largest free block is bigger (84 KB against 67 KB), which gives lower external fragmentation, and it is faster (0.250 s against 0.299 s), because best-fit has to look at the whole list on every allocation while first-fit stops at the first block that fits. Best-fit did end with fewer free blocks, but they are smaller, and small blocks are the ones that turn into holes nobody can use. I expected best-fit to waste less memory, so this was a surprise, and it shows that a policy that sounds better does not always win on a particular workload. I would use first-fit, which is also what my allocator does by default. This is one seed and one range of sizes, so I would not say it holds for every program.

## Why mine is slower than the C library

My my_malloc and my_free both walk the list from the head, and my_free also scans the whole list to coalesce, so the cost grows with the number of blocks. The C library keeps free blocks in bins sorted by size and caches recently freed chunks, so most requests are answered without searching at all. That is why it is about 35 times faster than mine in my runs.
