/* Team Name: Freaks!
 * Member 1: Abhijit Patel (id: 201401432)
 * Member 2: Kartik Shah (id: 201401442)
 *
 * Simple, 32-bit and 64-bit clean allocator based on an implicit free list,
 * first fit placement, and boundary tag coalescing, as described in the
 * CS:APP2e text.  Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor, and 16-byte aligned
 * blocks on a 64-bit processor.  However, 16-byte alignment is stricter
 * than necessary; the assignment only requires 8-byte alignment.  The
 * minimum block size is four words.
 *
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"Freaks!",
	/* First member's full name */
	"Abhijit patel",
	/* First member's email address */
	"201401432@daiict.ac.in",
	/* Second member's full name (leave blank if none) */
	"Kartik Shah",
	/* Second member's email address (leave blank if none) */
	"201401442@daiict.ac.in"
};

/* Basic constants and macros: */
#define WSIZE      sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE      (2 * WSIZE)    /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)      /* Extend heap by this amount (bytes) */

#define MAX(x, y)  ((x) > (y) ? (x) : (y))  

/* Pack a size and allocated bit into a word. */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p. */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer. */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks. */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* pointers to previous and next block of free list */
#define GET_NEXT_FREEP(bp) (*(char **)(bp + WSIZE))
#define GET_PREV_FREEP(bp) (*(char **)(bp))

/* puts pointers in the next and previous block of free list */
#define SET_NEXT_FREEP(bp, qp) (GET_NEXT_FREEP(bp) = qp)
#define SET_PREV_FREEP(bp, qp) (GET_PREV_FREEP(bp) = qp)

/* Global variables: */
static char *heap_listp; /* Pointer to first block */  
static char *free_list_header;

/* Function prototypes for internal helper routines: */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


/* Function prototypes for mainting free_list */
static void insert_free_blk(void *bp);
static void remove_free_blk(void *bp);

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp); 

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Initialize the memory manager.  Returns 0 if the memory manager was
 *   successfully initialized and -1 otherwise.
 */
int
mm_init(void) 
{
	/* Create the initial empty heap. */
	if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void *)-1)
		return (-1);
	PUT(heap_listp, 0);                            /* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
	free_list_header = heap_listp + 2*WSIZE;           /* free list header points to first free block */

	/* Extend the empty heap with a free block of CHUNKSIZE bytes. */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return (-1);
	return (0);

}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 *   and NULL otherwise.
 */
void *
mm_malloc(size_t size) 
{
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void *bp;
	/* Ignore spurious requests. */
	if (size == 0)
		return (NULL);
	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
	/* Search the free list for a fit. */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return (bp);
	}

	/* No fit found.  Get more memory and place the block. */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL) { 
		return (NULL);
	}
	place(bp, asize);
	return (bp);
} 

/* 
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void
mm_free(void *bp)
{
	size_t size;
	/* Ignore spurious requests. */
	if (bp == NULL)
		return;

	/* Free and coalesce the block. */
	size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  If "size" is zero, frees the block
 *   "ptr" and returns NULL.  If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" may optionally be returned.
 *   Otherwise, a new block is allocated and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this new
 *   block if the allocation was successful and NULL otherwise.
 */
void *
mm_realloc(void *ptr, size_t size)
{
	/* If size == 0 then this is just free, and we return NULL. */
	if ((int)size< 0){
	return NULL;
	}
	else if (size == 0) {
		mm_free(ptr);
		return (NULL);
	}
	
	size_t prev_size = GET_SIZE(HDRP(ptr)); 
	size_t req_size = size + 2*WSIZE; // adding 2*wsize for the space of header and footer 
	// if size is less than previous block size then we just return same ptr
      	if(req_size <= prev_size){ 
		return ptr; 
      	}
	else{   
		// for new size is greater than old size
		bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
		size_t combine_size;
		// if checks for if next block of current block is free or not. if free and total size is large enough then we return same ptr 			and do the needful
		if(!next_alloc && (( combine_size = prev_size + GET_SIZE( HDRP(NEXT_BLKP(ptr)) ) ) >= req_size)) 
		{
			remove_free_blk(NEXT_BLKP(ptr));
			PUT(HDRP(ptr), PACK(combine_size, 1));
			PUT(FTRP(ptr), PACK(combine_size, 1));
		        return ptr;
		}
		else{
		// else we find new free block from free list which is large enough
			void *new_ptr = mm_malloc(req_size);
			place(new_ptr, req_size);
			memcpy(new_ptr, ptr, req_size);
			mm_free(ptr); 
		        return new_ptr;
		}
	}

		
		return NULL;
}

/*
 * The following routines are internal helper routines.
 */

/*
 * Requires:
 *   "bp" is the address of a newly freed block.
 *
 * Effects:
 *   Perform boundary tag coalescing.  Returns the address of the coalesced
 *   block.
 */
static void *
coalesce(void *bp) 
{
	//checking condition for the if prev block is NULL if it is NULL then the flag is set.
	bool prev_alloc = GET_ALLOC(  FTRP(PREV_BLKP(bp))  ) || PREV_BLKP(bp) == bp ;
	bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	if (prev_alloc && !next_alloc) {         /* Case of next block is not allocated */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		remove_free_blk(NEXT_BLKP(bp));	//removing next free block from free list to combine both free block
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	} else if (!prev_alloc && next_alloc) {         /* Case of previous block is not allocated */
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		bp = PREV_BLKP(bp);
		remove_free_blk((bp));			//removing previous free block from free list to combine both free block
		PUT(HDRP((bp)), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	} else if(!prev_alloc && !next_alloc)	{         /* Case of prevoius and next both blocks are not allocated */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
		    GET_SIZE(FTRP(NEXT_BLKP(bp)));
		remove_free_blk(PREV_BLKP(bp)); 	//removing next and prevoius free block from free list to combine all blocks
		remove_free_blk(NEXT_BLKP(bp));
		bp = PREV_BLKP(bp);
		PUT(HDRP((bp)), PACK(size, 0));
		PUT(FTRP((bp)), PACK(size, 0));
	}
	insert_free_blk(bp);				//inserting combined free block to the free list
	return (bp);
}

/*
 * Inserts the free block in the free list
 */
static void insert_free_blk(void *bp){
	// inserting free block to the starting of the free list
	SET_NEXT_FREEP(bp, free_list_header);
	SET_PREV_FREEP(free_list_header, bp);
	SET_PREV_FREEP(bp, NULL);
	free_list_header = bp;

}

/*
  Removes the free block from the free list
 */
static void remove_free_blk(void *bp){
	//removing the block pointed by bp
	if(GET_PREV_FREEP(bp) == NULL){
		free_list_header = GET_NEXT_FREEP(bp);
	}
	else{
		SET_NEXT_FREEP(GET_PREV_FREEP(bp), GET_NEXT_FREEP(bp));
	}
	SET_PREV_FREEP(GET_NEXT_FREEP(bp), GET_PREV_FREEP(bp));
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Extend the heap with a free block and return that block's address.
 */
static void *
extend_heap(size_t words) 
{
	void *bp;
	size_t size;
	/* Allocate an even number of words to maintain alignment. */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	// if size < 16 then we make it 16 as our size must be multiple of 8 and sizze of header and footer 
	 if (size < 16){
    		size = 16;
  	}
	if ((bp = mem_sbrk(size)) == (void *)-1)  
		return (NULL);

	/* Initialize free block header/footer and the epilogue header. */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
	/* Coalesce if the previous block was free. */
	return (coalesce(bp));
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Find a fit for a block with "asize" bytes.  Returns that block's address
 *   or NULL if no suitable block was found. 
 */
static void *
find_fit(size_t asize)
{
	void *bp;
	/* Search for the first fit in the free list. */
	for (bp = free_list_header; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_FREEP(bp)) {
		if (asize <= GET_SIZE(HDRP(bp)))
			return (bp);
	}
	/* No fit was found. */
	return (NULL);
}

/* 
 * Requires:
 *   "bp" is the address of a free block that is at least "asize" bytes.
 *
 * Effects:
 *   Place a block of "asize" bytes at the start of the free block "bp" and
 *   split that block if the remainder would be at least the minimum block
 *   size. 
 */
static void
place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));   
	if ((csize - asize) >= (2 * DSIZE)) { 
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		remove_free_blk(bp);			// deleting allocated block from free list
		void *qp = NEXT_BLKP(bp);
		PUT(HDRP(qp), PACK(csize - asize, 0));
		PUT(FTRP(qp), PACK(csize - asize, 0));
		insert_free_blk(qp);			// inserting splitted free block to free list
	} else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		remove_free_blk(bp);			// deleting allocated block from free list
	}
}
/* 
 * The remaining routines are heap consistency checker routines. 
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void
checkblock(void *bp) 
{

	if ((uintptr_t)bp % DSIZE)
		printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Error: header does not match footer\n");
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a minimal check of the heap for consistency. 
 */
void
checkheap(bool verbose) 
{
	void *bp;
	int count=0,count1=0;

	if (verbose)
		printf("Heap (%p):\n", heap_listp);

	if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
	    !GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose)
			printblock(bp);
		checkblock(bp);
	}

	if (verbose)
		printblock(bp);
	if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
		printf("Bad epilogue header\n");
	
        for(bp=free_list_header; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_FREEP(bp)){
		if(GET_ALLOC(HDRP(bp)))		//if block is not in the free list
			printf("Allocated block is marked as free, hence pointers in the free list do not point to valid free block\n");
		count++;		//block is in the free list
		if(GET_ALLOC(HDRP(GET_NEXT_FREEP(bp))) == 0 ){
			if(!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp))))	//check if two blocks are coleased
				printf("Some block has escaped coalescing\n");
			
		}	
	}
	
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
		if(!GET_ALLOC(HDRP(bp)))
			count1++;
		if ((HDRP(bp) < (char *)mem_heap_lo()) || (HDRP(bp) > (char *)mem_heap_hi()) || (FTRP(bp) < (char *)mem_heap_lo()) || (FTRP(bp) > (char *)mem_heap_hi())) {		//pointer in the heap block not pointing to valid address
			printf("Some pointer in heap block do not point to valid heap addresses\n");		
		}
		if(bp!=heap_listp){
			char *prev_hdrp=HDRP(PREV_BLKP(bp));
			char *prev_ftrp=FTRP(PREV_BLKP(bp));
			char *curr_hdrp=HDRP(bp);
			char *curr_ftrp=FTRP(bp);
			if ((curr_hdrp >= prev_hdrp && curr_hdrp <= prev_ftrp) || 
			    (curr_ftrp >= prev_hdrp && curr_ftrp <= prev_ftrp)) 	//check if block is overlapping or not
				printf("Some allocated blocks are overlapping\n");
		}		
	}
	if(count!=count1)	//checking any free block not present in the free list
		printf("All free blocks are not covered in free list\n");
	
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 */
static void
printblock(void *bp) 
{
	bool halloc, falloc;
	size_t hsize, fsize;

	checkheap(false);
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));  
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));  

	if (hsize == 0) {
		printf("%p: end of heap\n", bp);
		return;
	}

	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, 
	    hsize, (halloc ? 'a' : 'f'), 
	    fsize, (falloc ? 'a' : 'f'));
}

/*
 * The last lines of this file configures the behavior of the "Tab" key in
 * emacs.  Emacs has a rudimentary understanding of C syntax and style.  In
 * particular, depressing the "Tab" key once at the start of a new line will
 * insert as many tabs and/or spaces as are needed for proper indentation.
 */

/* Local Variables: */
/* mode: c */
/* c-default-style: "bsd" */
/* c-basic-offset: 8 */
/* c-continued-statement-offset: 4 */
/* indent-tabs-mode: t */
/* End: */
