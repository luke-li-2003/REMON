#ifndef REMON_ALLOCATOR_MACROS_H
#define REMON_ALLOCATOR_MACROS_H

#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (WSIZE<<1)            /* doubleword size (bytes) */
#define QSIZE       (WSIZE<<2)
#define OVERHEAD    DSIZE
// #define MAX_INTRA_POD_SIZE POD_SIZE - QSIZE

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))
#define POINT(p)        (uintptr_t*)(p)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_BLOCK_SIZE(bp))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

/* For free blocks we store prev and next in the bp and bp + WSIZE location */
#define NEXT_FREE(bp) ((char*)(bp))
#define PREV_FREE(bp) ((char*)(bp) + WSIZE)

#define NEXT_FREE_VAL(bp) GET(((char*)(bp)))
#define PREV_FREE_VAL(bp) GET(((char*)(bp) + WSIZE))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_BLOCK_SIZE(bp) (GET_SIZE(HDRP(bp)))
#define SET_BLOCK_SIZE(p, val)     PUT(HDRP(p), val); PUT(FTRP(p), val)
#define GET_ALLOC(p)    (GET(p) & 0x1)

//#define SET_ALLOC(bp) GET(HDRP(bp)) = (GET(HDRP(bp)) | 0x1);GET(FTRP(bp)) = GET(HDRP(bp))
//#define SET_FREE(bp) GET(HDRP(bp)) = (GET(HDRP(bp)) & (~0x1));GET(FTRP(bp)) = GET(HDRP(bp))

#define SET_ALLOC(bp) GET(HDRP(bp)) = (GET(HDRP(bp)) | 0x1);GET(FTRP(bp)) = GET(HDRP(bp))
#define SET_FREE(bp) GET(HDRP(bp)) = (GET(HDRP(bp)) & (~0x1));GET(FTRP(bp)) = GET(HDRP(bp))


#endif //REMON_ALLOCATOR_MACROS_H
