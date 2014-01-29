/* Implements malloc, free, realloc using an explicit free list of segmented fits.
   The explicit free list is a doubly linked list of blocks.  Each free block uses the 8 bytes 
 * between the header and footer to hold a pointer to the next free block and the previous free block. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "cam",
    /* First member's full name */
    "C.M.",
    /* First member's email address */
    "cam@ut.edu",
    /* Second member's full name (leave blank if none) */
     "", 
    /* Second member's email address (leave blank if none) */
     ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* Basic constants and macros */
 #define WSIZE 4 /* Word and header/footer size (bytes) */
 #define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
 
/* Free list constants */
#define LSIZE 64    /* segmented width */
#define NLISTS 100 /* 100 free lists */

/* Global variables */
static char *heap_listp = NULL;   /* points to the prologue block */
static int min_list = 0;          /* first list with free blocks */    


/* Private helper functions */
static unsigned int max(unsigned int, unsigned int);
static size_t pack(size_t, size_t);
static unsigned int get(void *p);
static void put(void *p, size_t val);
static char *hdrp(void *bp);
static char *ftrp(void *bp);
static void *extend_heap(size_t words);
static void align(size_t *asize, size_t size);
static void place(void *bp, size_t asize);
static char *next_blkp(char *bp);
static char *prev_blkp(char *bp);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static size_t  get_size(void *p);
static void add_free_block(void *bp);
static void remove_free_block(void *bp);
static int get_min_list(size_t asize);
static void update_min_list(int minlist);
static size_t  get_alloc(void *p);


/* 
 * Before calling mm malloc mm realloc or mm free, the application program (i.e.,
 * the trace-driven driver program that you will use to evaluate your implementation) calls mm init to
 * perform any necessary initializations, such as allocating the initial heap area. The return value should
 * be -1 if there was a problem in performing the initialization, 0 otherwise.  Initializes 100 words in the 
 *  prologue block as pointers for our array of free lists.  Sets all of the pointers to 0 signifying the free list is empty.  
 */
int mm_init(void)
{

    if ((heap_listp = mem_sbrk(104*WSIZE)) == (void *)-1)
  	return -1;
    
    put(heap_listp, 0); /* Alignment padding */
    put(heap_listp + (1*WSIZE), pack(102*WSIZE, 1)); /* Prologue header */

    int i;
    for (i = 2; i < 102; i++)     /* initialize 100 empty free lists */
      put(heap_listp + (i * WSIZE), 0);
    
    put(heap_listp + (102*WSIZE), pack(102*WSIZE, 1)); /* Prologue footer */
    put(heap_listp + (103*WSIZE), pack(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE); /* point to first free list */


    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
  	return -1;

    return 0;

  
}


/*Function to extend heap by size bytes.  Creates new free block at the end of list. */
 static void *extend_heap(size_t words)
 {
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	put(hdrp(bp), pack(size, 0)); /* Free block header */
	put(ftrp(bp), pack(size, 0)); /* Free block footer */
	put(hdrp(next_blkp(bp)), pack(0, 1)); /* New epilogue header */

	/* Coalesce to combine neighboring free blocks if possible */
	bp = coalesce(bp);
	return bp;
 }
 
 
/* 
 *   Allocate a block by finding a large enough block on the free list (extending heap if needed) and placing it in the free block.   
 *   Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize; /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	char *bp;

	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	align(&asize, size);

	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = max(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	
	place(bp, asize);
	return bp;

}


/* 
 * search free list for large enough block to place malloc request.  Return null if no fit fount.  
 */
 static void *find_fit(size_t asize)
 {
 	/* First fit search */
 	void *bp;
 	int minlist = get_min_list(asize);
 	if(minlist < min_list)
 		minlist = min_list;
 	
 	/* Loop through the free lists starting at min list */
 	for(; minlist < NLISTS; minlist++){
 		int i = 0;
		for (bp = (char *)get(heap_listp + (minlist * WSIZE)); (int)bp != 0 && get_size(hdrp(bp)) > 0; bp = (char *)get(bp+WSIZE)) {
			if (!get_alloc(hdrp(bp)) && (asize <= get_size(hdrp(bp)))) {
				return bp;
			}
			i++;
		}
 	}
 	return NULL; /* no fit found */
 }
 
/* 
 *      place - place a block of given size in a block of given pointer.    
 *  	splits given block if the block is large enough for allocated space and new free block  
 *	removes block from the free list. 
 */ 
 static void place(void *bp, size_t asize)
 {
    void *next_bp;
    size_t csize = get_size(hdrp(bp));

    if ((csize - asize) >= (2*DSIZE)) {
      	remove_free_block(bp); /* Remove block from free list */
        put(hdrp(bp), pack(asize, 1));
    	put(ftrp(bp), pack(asize, 1));
 
    	next_bp = next_blkp(bp);
    	put(hdrp(next_bp), pack(csize-asize, 0));
    	put(ftrp(next_bp), pack(csize-asize, 0));
    	add_free_block(next_bp);  /* Add leftover split block to free list */
    }
    else {
    	put(hdrp(bp), pack(csize, 1));
    	put(ftrp(bp), pack(csize, 1));
    	remove_free_block(bp); /* Remove block from free list */
    }
 }



/* 
 *      remove_free_block - removes a given pointer from the free list.     
 */  
 static void remove_free_block(void *bp)
 {	
   int size = get_size(hdrp(bp));
   int minlist = get_min_list(size);

 	/* Case 1: no remaining free blocks on list */
   if(get(bp) == 0 && get(bp + WSIZE) == 0) {
     
 		put(heap_listp+(minlist * WSIZE), 0);
		update_min_list(minlist);
   }
 	/* Case 2: previous empty and next not empty */
 	else if (get(bp) == 0 && get(bp + WSIZE) != 0){

 		put(heap_listp+(minlist * WSIZE), get(bp + WSIZE));
 		put((char *)get(bp + WSIZE), 0);
 	}
 	/* Case 3: previous not empty and next empty */	
 	else if (get(bp) != 0 && get(bp + WSIZE) == 0) 
 		put(((char *)get(bp) + WSIZE), 0);

 	/* Case 4: both are not empty */	 		
 	else {
 		put(((char *)get(bp) + WSIZE), get(bp + WSIZE));	
 		put(((char *)get(bp + WSIZE)), get(bp));	
 	}

 }
 
 
 /* 
 * adds a free block to the free list.       
 */  
 static void add_free_block(void *bp)
 {
 	
 	int size = get_size(hdrp(bp));
 	int minlist = get_min_list(size);
	
	/* pointer to first block on the min list free list */
	void *cur = (char *)get(heap_listp + (minlist * WSIZE));
	
	/* Case 1: the free list is empty */
	if(!cur){
	        /* set free list pointer to point to this block */
		put(heap_listp + (minlist * WSIZE), (int)bp);
		put(bp, 0);
		put(bp+WSIZE, 0); /* always set the next block */
	}
	
	/* Case 2: this free list isn't free */
	else {
	
        	 void *prev = (char *) cur;
		 	
		 /* find place to insert new free block */
		for (; (int)cur != 0 && get_size(hdrp(cur)) < size; cur = (char *)get(cur+WSIZE))
			prev = cur;
		
		cur = prev;
		void *next = (char *)get(cur + WSIZE);
		
		/* Insert in between two blocks */
		put(cur + WSIZE, (int)bp);
		if((int)next != 0)
			put(next, (int)bp);
		put(bp, (int)cur);
		put(bp+WSIZE, (int)next);
		
		}

 }


/*
 *  Frees block pointed to by given pointer.
 */
void mm_free(void *bp)
{
	//mm_check();
	size_t size = get_size(hdrp(bp));
	/* Set allocated bit to zero */
	put(hdrp(bp), pack(size, 0));
	put(ftrp(bp), pack(size, 0));
	
        /* Combine with adjacent blocks and add to free list */
	coalesce(bp);
}

 /* 
 * combines newly created free block with neighboring free blocks if possible       
 * adds given block to the free list. 
 */  
 static void *coalesce(void *bp)
 {
    size_t prev_alloc = get_alloc(ftrp(prev_blkp(bp)));
    size_t next_alloc = get_alloc(hdrp(next_blkp(bp)));
    size_t size = get_size(hdrp(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
      add_free_block(bp);
	return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        remove_free_block(next_blkp(bp));
	size += get_size(hdrp(next_blkp(bp)));
	put(hdrp(bp), pack(size, 0));
	put(ftrp(bp), pack(size,0));
	add_free_block(bp);
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        remove_free_block(prev_blkp(bp));
	size += get_size(hdrp(prev_blkp(bp)));
	put(ftrp(bp), pack(size, 0));
	put(hdrp(prev_blkp(bp)), pack(size, 0));
	bp = prev_blkp(bp);
	add_free_block(bp);
    }

    else {                                     /* Case 4 */
       remove_free_block(prev_blkp(bp));
       remove_free_block(next_blkp(bp));
	size += get_size(hdrp(prev_blkp(bp))) +
	    get_size(ftrp(next_blkp(bp)));
	put(hdrp(prev_blkp(bp)), pack(size, 0));
	put(ftrp(next_blkp(bp)), pack(size, 0));
	bp = prev_blkp(bp);
	add_free_block(bp);
    }

     return bp; 

 }

/*
 * *not yet implemented with an explicit free list.  needs implementation*
 * resize a given block to a given size and return pointer to the new resize block.  
 */
void *mm_realloc(void *ptr, size_t size)
{
      /* If oldptr is NULL, then malloc. */
    if(ptr == NULL) 
	return mm_malloc(size);
    
    /* If size == 0 then free, and return NULL. */
    if(size == 0) {
	mm_free(ptr);
	return NULL;
    }

    void *newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) 
	return NULL;
    
    /* Copy the old data. */
    size_t oldsize = get_size(hdrp(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;

}

/***** Helper functions *****/

static unsigned int max(unsigned int x, unsigned int y){
  return x > y ? x : y;
}


/* Pack a size and allocated bit into a word */
static size_t pack(size_t size, size_t alloc){
 return size | alloc ; 
}

/* Read and write a word at address p */
static unsigned int get(void *p){
  return *(unsigned int *)p;
}

static void put(void *p, size_t val){
  *(unsigned int *)p = val;
}

/* Read the size and allocated fields from address p */
static size_t  get_size(void *p){
  return (get(p) & ~0x7);
}

static size_t  get_alloc(void *p){
   return (get(p) & 0x1);
}

/* Given block ptr bp, compute address of its header and footer */
static char *hdrp(void *bp){
  return ((char *)(bp) - WSIZE);                    
}

static char *ftrp(void *bp){
  return ((char *)(bp) + get_size(hdrp(bp)) - DSIZE);
}

static char *next_blkp(char *bp){
  return ((char *)(bp) + get_size((char *)(bp) - WSIZE)); 
}

static char *prev_blkp(char *bp){
  return ((char *)(bp) - get_size((char *)(bp) - DSIZE));
} 


static int get_min_list(size_t asize){
 int minlist  = asize / LSIZE;
	
 /* minlist should be [0, 99] if its greater, reset to 99 */
 if(minlist > LSIZE-1)
    minlist = LSIZE -1; 

 return minlist;

}

/* If allocating a block, first adjust its size.*/
static void align(size_t *asize, size_t size){

  if(size <= DSIZE)
    *asize = 2*DSIZE; /*Minimum 16 byte block size*/
  
  else 
    *asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);  /* Round up byte */

}

static void update_min_list(int minlist){
  int i = minlist;
  for (; get(heap_listp+(i * WSIZE)) == 0; i++){
  }
  minlist = i;
}
