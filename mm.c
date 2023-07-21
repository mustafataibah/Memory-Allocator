#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


team_t team = {"mtaybah", "Mustafa Taibah", "mtaybah@bu.edu", "", ""};


#define ALIGNMENT 8												/* single word (4) or double word (8) alignment */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)			/* rounds up to the nearest multiple of ALIGNMENT */

/******* $begin mallocmacros **************/
/******* Basic constants and macros *******/
#define WSIZE 4 				/* Word and header/footer size (bytes) */
#define DSIZE 8					/* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) 	/* Extend heap by this amount (bytes) */
#define INITSIZE (1 << 6)       

#define LISTSIZE 20

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Stores a pointer to the previous or next free block */
#define STORE_BP(p, bp) (*(unsigned int *)(p) = (unsigned int)(bp))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val)) 
#define PUT_TAG(p, val) (*(unsigned int *)(p) = (val) | TAG(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define TAG(p) (GET(p) & 0x2)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))

/* address of next and previous free blocks */
#define PREV_FREE_BLKP(bp) ((char *)(bp))
#define NEXT_FREE_BLKP(bp) ((char *)(bp) + WSIZE)

/* address of next and previous free blocks in the segregated free list */
#define PREV_SEG_FREE_BLKP(bp) (*(char **)(bp))
#define NEXT_SEG_FREE_BLKP(bp) (*(char **)(NEXT_FREE_BLKP(bp)))
/******* $end mallocmacros *******/
 

/******* Global variables *******/
static char *heap_listp; 					/* Pointer to first block */
static void *segregated_listp[20];          /* Free Block List */

/******* Function prototypes for internal helper routines *******/
static void *extend_heap(size_t words);
static void *place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void node(void *bp, size_t size);
static void free_node(void *bp);

/******* Function prototypes for debugger helper routines *******/
// static void mm_check(int verbose);
// static void print_block(void *bp);



/************************************************************************/
/************************** MAIN FUNCTIONS ******************************/
/************************************************************************/



/* 
 * mm_init - initialize the malloc package. 
 * The program mm.c first calls mm_init to initialize the initial memory heap
 * along with the segregated free lists and other stuff needed to be initialized.
 * If mm_init fails or there any other errors such as extending the heap fails then 
 * -1 is returned, otherwise the initializations was succesufull and 0 is returned.
 */
int mm_init(void)													
{
    int i;        

    /* Initializes the segregated free list */
    for (i = 0; i < LISTSIZE; i++) {
        segregated_listp[i] = NULL;
    }
    
    /* Allocate Memory for the initial empty memory heap */
    if ((long)(heap_listp = mem_sbrk(4 * WSIZE)) == -1)
        return -1;
    
    PUT(heap_listp, 0);                                 /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));      /* Prologue header   */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));      /* Prologue footer   */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));          /* Epilogue header   */
    

    /* If extend_heap fails then error is returned */
    if (extend_heap(INITSIZE) == NULL)
        return -1;
    
    /* else the initializations is succesfull and 0 is returned */ 
    return 0;
}



/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 * mm_malloc returns a pointer to the allocated block payload in the memory heap,
 * The entire allocated block should lie within the heap region 
 * and should not overlap with any other allocated chunk.
 */
void *mm_malloc(size_t size)
{
    size_t asize;           /* adjusted size */
    size_t extendsize;      /* extend size */
    void *bp = NULL; 

    /* if allocating size is 0 simply return NULL */
    if (size == 0)
        return NULL;
    
    /* Align the block size by multiple of 8 bytes */
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = ALIGN(size+DSIZE);
    }
    
    int i = 0; 
    size_t helpersize = asize;

    /* Loop over the segregated free list until a free block is found,
    * if the free block is too small break the second while loop and continue
    * looking for a free block */
    while (i  < LISTSIZE) {
        if ((i == LISTSIZE - 1) || ((helpersize <= 1) && (segregated_listp[i] != NULL))) {
            bp = segregated_listp[i];
            while ((bp != NULL) && ((asize > GET_SIZE(HDRP(bp))) || (TAG(HDRP(bp)))))
            {
                bp = PREV_SEG_FREE_BLKP(bp);
            }
            if (bp != NULL)
                break;
        }
        helpersize >>= 1;
        i++;
    }
    
    /* if there is no available free block then extend the heap for more free block
    * if extend_heap fails return NULL */
    if (bp == NULL) {
        extendsize = MAX(asize, CHUNKSIZE);
        
        if ((bp = extend_heap(extendsize)) == NULL)
            return NULL;
    }

   /* free block found, call place function and place the block at the start of the free block and divide if necessary */
    bp = place(bp, asize);

    /* return the block pointer to the new allocated block initialized by mm_malloc */
    return bp;
}



/*
 * mm_free - Freeing a block returns nothing.
 * function only works if the pointer was not free'd earlier by a call to mm_malloc or mm_realloc
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
 
    PUT_TAG(HDRP(bp), PACK(size, 0));
    PUT_TAG(FTRP(bp), PACK(size, 0));
    node(bp, size);
    coalesce(bp);
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free (unefficient)
 */
void *mm_realloc(void *ptr, size_t size)
{
	size_t oldsize;
	void *newptr;

	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0)
	{
		mm_free(ptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL)
	{
		return mm_malloc(size);
	}

	newptr = mm_malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if (!newptr)
	{
		return 0;
	}

	/* Copy the old data. */
	oldsize = GET_SIZE(HDRP(ptr));
	if (size < oldsize)
		oldsize = size;
	memcpy(newptr, ptr, oldsize);

	/* Free the old block. */
	mm_free(ptr);

	return newptr;
}


/************************************************************************/
/*************************** HELPER FUNCTIONS ***************************/
/************************************************************************/


/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block based on the textbook CSAPA
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {    
        /* Case 1; Previous and next are allocated */                     
        return bp;
    }

    else if (prev_alloc && !next_alloc) {     
        /* Case 2; Previous allocated and next free */             
        free_node(bp);
        free_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT_TAG(HDRP(bp), PACK(size, 0));
        PUT_TAG(FTRP(bp), PACK(size, 0));
    } 

	else if (!prev_alloc && next_alloc) {    
        /* Case 3; Previous free and next allocated */             
        free_node(bp);
        free_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT_TAG(FTRP(bp), PACK(size, 0));
        PUT_TAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 

	else {           
        /* Case 4; next and previous free*/                                    
        free_node(bp);
        free_node(PREV_BLKP(bp));
        free_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT_TAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT_TAG(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    node(bp, size);
    return bp;
}



/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    void *bp;                   
    size_t size;                                /* size to extend heap with */             
    
    size = ALIGN(words);                        /* Align block with multiple of 8 bytes */
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    
    /* Initializes the headers and footers for the new free block */
    PUT(HDRP(bp), PACK(size, 0));  
    PUT(FTRP(bp), PACK(size, 0));   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 
    node(bp, size);

    return coalesce(bp);    
}


/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 *         
 */
static void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remainder = csize - asize;
    
    free_node(bp);
    
    if (remainder <= DSIZE * 2) {                    /* checks if remainder is at least minimum block size, if yes then split */           
        PUT_TAG(HDRP(bp), PACK(csize, 1)); 
        PUT_TAG(FTRP(bp), PACK(csize, 1)); 
    }
	
    else if (asize >= 100) {                        /* does not split the block */
        PUT_TAG(HDRP(bp), PACK(remainder, 0));
        PUT_TAG(FTRP(bp), PACK(remainder, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        node(bp, remainder);
        return NEXT_BLKP(bp);
        
    }
    else {                                          /* also does not split the block */
        PUT_TAG(HDRP(bp), PACK(asize, 1)); 
        PUT_TAG(FTRP(bp), PACK(asize, 1)); 
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remainder, 0)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remainder, 0)); 
        node(NEXT_BLKP(bp), remainder);
    }
    return bp;
}

/* 
* node - adds information to the header and footer of the block about size allocation info
* also sets the pointer for the previous and next block in the segregated list
*/
static void node(void *bp, size_t size) {
    int i = 0;
    void *scan_bp = bp;
    void *node_bp = NULL;
    
    /* adjusts size to equal the size of the segregated list */
    while((i < LISTSIZE - 1) && (size > 1)) {
        size >>= 1;
    	i++;
    }

    scan_bp = segregated_listp[i];

    while((scan_bp != NULL) && (size > GET_SIZE(HDRP(scan_bp)))) {
        node_bp = scan_bp;
        scan_bp = PREV_SEG_FREE_BLKP(scan_bp);
    }

    /* stores the pointer for the previous and next block in the segregated list using the defined macro SET_BP */
    if(scan_bp != NULL) {
        if (node_bp != NULL) {
            STORE_BP(PREV_FREE_BLKP(bp), scan_bp);
            STORE_BP(NEXT_FREE_BLKP(scan_bp), bp);
            STORE_BP(NEXT_FREE_BLKP(bp), node_bp);
            STORE_BP(PREV_FREE_BLKP(node_bp), bp);
        } 
		else {
            STORE_BP(PREV_FREE_BLKP(bp), scan_bp);
            STORE_BP(NEXT_FREE_BLKP(scan_bp), bp);
            STORE_BP(NEXT_FREE_BLKP(bp), NULL);
            segregated_listp[i] = bp;
        }
    } 
	else {
        if(node_bp != NULL) {
            STORE_BP(PREV_FREE_BLKP(bp), NULL);
            STORE_BP(NEXT_FREE_BLKP(bp), node_bp);
            STORE_BP(PREV_FREE_BLKP(node_bp), bp);
        } 
		else {
            STORE_BP(PREV_FREE_BLKP(bp), NULL);
            STORE_BP(NEXT_FREE_BLKP(bp), NULL);
            segregated_listp[i] = bp;
        }
    }
}

/* 
 * free_node - frees all block header and footer info in the free list and segregated list
 */
static void free_node(void *bp) {
    int i = 0;
    size_t size = GET_SIZE(HDRP(bp));
    
    /* adjusts size to equal the size of the segregated list */
    while ((i < LISTSIZE - 1) && (size > 1)) {
        size >>= 1;
        i++;
    }
    
    /* frees the pointer for the previous and next block in the segregated free list and adjusts as needed */
    if (PREV_SEG_FREE_BLKP(bp) != NULL) {
        if (NEXT_SEG_FREE_BLKP(bp) != NULL) {
            STORE_BP(NEXT_FREE_BLKP(PREV_SEG_FREE_BLKP(bp)), NEXT_SEG_FREE_BLKP(bp));
            STORE_BP(PREV_FREE_BLKP(NEXT_SEG_FREE_BLKP(bp)), PREV_SEG_FREE_BLKP(bp));
        } 
		else {
            STORE_BP(NEXT_FREE_BLKP(PREV_SEG_FREE_BLKP(bp)), NULL);
            segregated_listp[i] = PREV_SEG_FREE_BLKP(bp);
        }
    } 
	else {
        if (NEXT_SEG_FREE_BLKP(bp) != NULL) {
            STORE_BP(PREV_FREE_BLKP(NEXT_SEG_FREE_BLKP(bp)), NULL);
        } 
		else {
            segregated_listp[i] = NULL;
        }
    }
}



/************************************************************************/
/************************* DEBUGGER FUNCTIONS ***************************/
/************************************************************************/



// /*
//  * mm_check - checks the heap for inconsistencies
//  */
// static void mm_check(int verbose) {
//     char *bp = heap_listp;

//     /* checks if the header and footer match */
//     if(GET(HDRP(bp)) != GET(FTRP(bp))) {
//         printf("Header and footer do not match");
//     }

//     /* checks if the block is aligned */
//     if((size_t)bp % 8) {
//         printf("%p is not doubleword aligned\n", bp);
//     }

//     /* checks if the pointer is in the heap */
//     if (!(bp  <= mem_heap_hi() && bp >= mem_heap_lo())) {
//         printf("pointer not in heap");
//     }

//     if (verbose) {
//         printf("Heap (%p):\n", heap_listp);
//     }

//     /* checks header prologue */
//     if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
//         printf("Bad prologue header\n");
//     } 

//     /* checks header epilogue */
//     if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
//         printf("Bad epilogue header\n");
//     }   

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
// 	    if (verbose) {
//             print_block(bp);
//         }
//     }
// }


// /* 
//  * printblock - prints the block 
//  */

// static void print_block(void *bp) 
// {
//     size_t header_size, header_alloc, footer_size, footer_alloc;

//     checkheap(0);
//     header_size = GET_SIZE(HDRP(bp));
//     header_alloc = GET_ALLOC(HDRP(bp));  
//     footer_size = GET_SIZE(FTRP(bp));
//     footer_alloc = GET_ALLOC(FTRP(bp));  

//     if (header_size == 0) {
// 	    printf("%p: EOL\n", bp);
//         return;
//     }

//     printf("%p: header: [%p:%c] footer: [%p:%c]\n", bp, header_size, (header_alloc ? 'a' : 'f'), footer_size, (footer_alloc ? 'a' : 'f')); 
// }


