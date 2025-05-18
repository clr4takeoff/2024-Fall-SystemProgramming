/*
 * mm-explicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 202302627 
 * @name : 최소연
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define HDRSIZE 4
#define FTRSIZE 4
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define OVERHEAD 8

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y)? (x) : (y))

#define PACK(size, alloc) ((unsigned)((size) | (alloc)))

#define GET(p) (*(unsigned *)(p))
#define PUT(p, val) (*(unsigned *)(p) = (unsigned)(val))
#define GET8(p) (*(unsigned long *)(p))
#define PUT8(p, val) (*(unsigned long *)(p) = (unsigned long)(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_FREEP(bp) ((char*)(bp))
#define PREV_FREEP(bp) ((char*)(bp) + DSIZE)

#define NEXT_FREE_BLKP(bp) ((char*)GET8((char*)(bp)))
#define PREV_FREE_BLKP(bp) ((char*)GET8((char*)(bp)+DSIZE))

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

static char *fbp;
static char *h_ptr;
void *extend_heap(size_t size);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
void unlink_block(void *bp);
void link_block(void *first, void *second);


int mm_init(void) {
	if ((h_ptr=mem_sbrk(DSIZE*2))==NULL)
		return -1;

    fbp = NULL;
    PUT(h_ptr, 0);
    PUT(h_ptr + WSIZE, PACK(OVERHEAD, 1));
    PUT(h_ptr + DSIZE, PACK(OVERHEAD, 1));
    PUT(h_ptr + WSIZE + OVERHEAD, PACK(0, 1));

//	void *bp=extend_heap(CHUNKSIZE/WSIZE);
//	if (bp==NULL) {
//		return -1;
//	}

	return 0;
}

void *extend_heap(size_t words) {
    char *bp;
	size_t size;

	size = (words%2) ? (words+1) * WSIZE : words*WSIZE;

    if ((long)(bp=mem_sbrk(size))<0)
		return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}

void *malloc(size_t size) {
    char *bp;
    size_t asize;
    
    if (size<=0) {
        return NULL;
    }
    
    asize = MAX(ALIGN(size+OVERHEAD), DSIZE*3);
    
    if ((bp=find_fit(asize))!=NULL) {
        place(bp,asize);
        return bp;
    }
    
    if ((bp=extend_heap(asize/WSIZE))==NULL)
        return NULL;
    
    place(bp,asize);
    return bp;
}

void free(void *ptr) {
    if (!ptr) return;
    size_t csize = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(csize, 0));
    PUT(FTRP(ptr), PACK(csize, 0));
	
	coalesce(ptr);
}

void *realloc(void *oldptr, size_t size) {
    if (size == 0) {
        free(oldptr);
        return 0;
    }
    if (oldptr == NULL) {
        return malloc(size);
    }
    void *newptr = malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    size_t oldsize = GET_SIZE(HDRP(oldptr));
    if (size < oldsize)
        oldsize = size;
    memcpy(newptr, oldptr, oldsize);
    free(oldptr);
    return newptr;
}

void *calloc(size_t nmemb, size_t size) {
    return NULL;
}

static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

void mm_checkheap(int verbose) {
}

static void place(void *bp, size_t asize) {
    size_t block_size = GET_SIZE(HDRP(bp));
    char *prev_bp = PREV_FREE_BLKP(bp);
    size_t nbsize = block_size - asize;
    char *next_bp = NEXT_FREE_BLKP(bp);
    size_t default_size = 3 * OVERHEAD;

	if (nbsize >= default_size) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        char *rp = NEXT_BLKP(bp);
        PUT8(PREV_FREEP(rp), NULL);
        PUT8(NEXT_FREEP(rp), NULL);
        if (next_bp != NULL) {
            PUT8(NEXT_FREEP(rp), next_bp);
            PUT8(PREV_FREEP(next_bp), rp);
        }
        if (prev_bp != NULL) {
            PUT8(NEXT_FREEP(prev_bp), rp);
            PUT8(PREV_FREEP(rp), prev_bp);
        }
        if (bp == fbp) {
            fbp = rp;
        }
        PUT(HDRP(rp), PACK(nbsize, 0));
        PUT(FTRP(rp), PACK(nbsize, 0));
        return;
    } 
    PUT(HDRP(bp), PACK(block_size, 1));
    PUT(FTRP(bp), PACK(block_size, 1));
    if (prev_bp == NULL && next_bp == NULL) {
        fbp = NULL;
        return;
    }

    if (prev_bp == NULL && next_bp != NULL) {
        PUT8(PREV_FREEP(next_bp), NULL);
        fbp = next_bp;
        return;
    }
    if (prev_bp != NULL && next_bp == NULL) {
        PUT8(NEXT_FREEP(prev_bp), NULL
        );
        return;
    }
    PUT8(NEXT_FREEP(prev_bp), next_bp);
    PUT8(PREV_FREEP(next_bp), prev_bp);
}

static void *find_fit(size_t asize) {
	/* best fit */ 
	void *bp=h_ptr;
	void *best_fit = NULL;
	size_t min_diff=(size_t)-1;

	if (fbp == NULL) {
		return NULL;
	}

	for (bp=fbp; bp!=NULL; bp=NEXT_FREE_BLKP(bp)) {
		size_t bsize=GET_SIZE(HDRP(bp));
		if (asize <= bsize && bsize - asize <min_diff) {
			best_fit=bp;
			min_diff=bsize-asize;
		}
	}

	return best_fit;
}

static void *coalesce(void *bp) {
    char *prev_bp = PREV_BLKP(bp);
    size_t prev_size = GET_SIZE(HDRP(prev_bp));
   	int prev_alloc = GET_ALLOC(HDRP(prev_bp));
    
	size_t csize = GET_SIZE(HDRP(bp));

    char *next_bp = NEXT_BLKP(bp);
    size_t next_size = GET_SIZE(HDRP(next_bp));
    int next_alloc = GET_ALLOC(HDRP(next_bp));

	if (prev_alloc && next_alloc) {
        PUT8(PREV_FREEP(bp), NULL);
        PUT8(NEXT_FREEP(bp), NULL);
        if (fbp != NULL) {
            PUT8(PREV_FREEP(fbp), bp);
            PUT8(NEXT_FREEP(bp), fbp);
        }
        fbp = bp;
        return fbp;
    }

    if (!prev_alloc && next_alloc) {
        csize += prev_size;
        unlink_block(prev_bp);
        PUT8(PREV_FREEP(prev_bp), NULL);
        if (prev_bp != fbp) {
            PUT8(NEXT_FREEP(prev_bp), fbp);
            PUT8(PREV_FREEP(fbp), prev_bp);
        }
        PUT(HDRP(prev_bp), PACK(csize, 0));
        PUT(FTRP(prev_bp), PACK(csize, 0));
        fbp = prev_bp;
        return fbp;
    }

    if (prev_alloc && !next_alloc) {
        csize += next_size;
        unlink_block(next_bp);
        PUT8(PREV_FREEP(bp), NULL);
        if (next_bp == fbp) {
            PUT8(NEXT_FREEP(bp), NEXT_FREE_BLKP(next_bp));
            if (NEXT_FREE_BLKP(next_bp) != NULL) {
                PUT8(PREV_FREEP(NEXT_FREE_BLKP(next_bp)), bp);
            }
        }
        if (next_bp != fbp) {
            PUT8(NEXT_FREEP(bp), fbp);
            PUT8(PREV_FREEP(fbp), bp);
        }
        PUT(HDRP(bp), PACK(csize, 0));
        PUT(FTRP(bp), PACK(csize, 0));
        fbp = bp;
        return fbp;
    }
    if (!prev_alloc && !next_alloc) {
        csize += prev_size + next_size;
     
		if (prev_bp != fbp && next_bp != fbp) {
            unlink_block(prev_bp);
            unlink_block(next_bp);
            PUT8(PREV_FREEP(prev_bp), NULL);
            link_block(prev_bp, fbp);
            PUT(HDRP(prev_bp), PACK(csize, 0));
            PUT(FTRP(prev_bp), PACK(csize, 0));
            fbp = prev_bp;
            return fbp;
        }
        
		if (next_bp == fbp && prev_bp == NEXT_FREE_BLKP(next_bp)) {
            void *left_next = NEXT_FREE_BLKP(prev_bp);
            PUT8(PREV_FREEP(prev_bp), NULL);
            link_block(prev_bp, left_next);
            PUT(HDRP(prev_bp), PACK(csize, 0));
            PUT(FTRP(prev_bp), PACK(csize, 0));
            fbp = prev_bp;
            return fbp;
        }
        
		if (next_bp == fbp && prev_bp != NEXT_FREE_BLKP(next_bp)) {
            unlink_block(prev_bp);
            PUT8(PREV_FREEP(prev_bp), NULL);
            link_block(prev_bp, NEXT_FREE_BLKP(next_bp));
            PUT(HDRP(prev_bp), PACK(csize, 0));
            PUT(FTRP(prev_bp), PACK(csize, 0));
            fbp = prev_bp;
            return fbp;
        }
        
		if (prev_bp == fbp && next_bp == NEXT_FREE_BLKP(prev_bp)) {
            void *right_next = NEXT_FREE_BLKP(next_bp);
            PUT8(PREV_FREEP(prev_bp), NULL);
            link_block(prev_bp, right_next);
            PUT(HDRP(prev_bp), PACK(csize, 0));
            PUT(FTRP(prev_bp), PACK(csize, 0));
            fbp = prev_bp;
            return fbp;
        }
        if (prev_bp == fbp && next_bp != NEXT_FREE_BLKP(prev_bp)) {
            void *left_next = NEXT_FREE_BLKP(prev_bp);
            unlink_block(next_bp);
            PUT8(PREV_FREEP(prev_bp), NULL);
            link_block(prev_bp, left_next);
            PUT(HDRP(prev_bp), PACK(csize, 0));
            PUT(FTRP(prev_bp), PACK(csize, 0));
            fbp = prev_bp;
            return fbp;
        }
    }
    return NULL;
}

void unlink_block(void *bp) {
    char *prev_bp = PREV_FREE_BLKP(bp);
    char *next_bp = NEXT_FREE_BLKP(bp);
    if ((prev_bp) == NULL) {
        return;
    }
    PUT8(NEXT_FREEP(prev_bp), next_bp);
    if (next_bp != NULL) {
        PUT8(PREV_FREEP(next_bp), prev_bp);
    }
}

void link_block(void *x, void *y) {
    PUT8(NEXT_FREEP(x), y);
    if (y != NULL)
        PUT8(PREV_FREEP(y), x);
}
