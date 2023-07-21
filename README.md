# Memory Allocator

This memory allocator was implemented in the C programming language and utilizes the concept of segregated storage to maintain multiple free lists to reduce the allocation time.

## Description

This implementation is a special case of segregated fits; the buddy system. Each allocated new block is rounded to the nearest power of 2. Upon allocation, it finds the first available block of size 2^j where k <= j <= m. If j = k there is no need to split the block, otherwise, the block is split in half until j = k. The other half is placed on the appropriate free list. Coalescing stops when the other half (buddy) is allocated.

This approach is efficient because it approximates a best-fit search of the entire heap. The major advantage of this approach is fast searching and coalescing. Every block in the allocator has a header and footer that contain the information for the size of the block, and the allocation information.

The allocator uses tags in the header and footer to determine where to place each free block and whether the block was allocated or not.

## Resources used :

1. IIT College of Science: [Link](https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf)
2. Daniel Stenberg best-fit : [Link](https://daniel.haxx.se/projects/dbestfit/thoughts.html)

## Main Files

- `mm.{c,h}` : Your solution malloc package. `mm.c` is the file that you will be handing in, and is the only file you should modify.
- `mdriver.c` : The malloc driver that tests your `mm.c` file
- `short{1,2}-bal.rep` : Two tiny tracefiles to help you get started.
- `Makefile` : Builds the driver

## Other support files for the driver

- `config.h` : Configures the malloc lab driver
- `fsecs.{c,h}` : Wrapper function for the different timer packages
- `clock.{c,h}` : Routines for accessing the Pentium and Alpha cycle counters
- `fcyc.{c,h}` : Timer functions based on cycle counters
- `ftimer.{c,h}` : Timer functions based on interval timers and gettimeofday()
- `memlib.{c,h}` : Models the heap and sbrk function

## Building and running the driver

To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

```sh
unix> mdriver -V -f short1-bal.rep

## Main Functions

1. `mm_init` : Initializes the initial memory heap and the segregated free lists.
2. `mm_malloc` : Allocates a block by incrementing the brk pointer. Always allocate a block whose size is a multiple of the alignment.
3. `mm_free` : Freeing a block. Only works if the pointer was not freed earlier by a call to `mm_malloc` or `mm_realloc`.
4. `mm_realloc` : Implemented simply in terms of `mm_malloc` and `mm_free`.

## Helper Functions

1. `coalesce` : Implements Boundary tag coalescing. Returns pointer to coalesced block based on the textbook CSAPA.
2. `extend_heap` : Extends heap with a free block and returns its block pointer.
3. `place` : Places a block of asize bytes at start of free block bp and splits if remainder would be at least minimum block size.
4. `node` : Adds information to the header and footer of the block about size allocation info and sets the pointer for the previous and next block in the segregated list.
5. `free_node` : Frees all block header and footer info in the free list and segregated list.
```

#####################################################################

# CS:APP Malloc Lab

# Handout files for students

#

# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.

# May not be used, modified, or copied without permission.

#

######################################################################
