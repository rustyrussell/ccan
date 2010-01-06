#include <ccan/block_pool/block_pool.h>
#include <ccan/block_pool/block_pool.c>
#include <ccan/tap/tap.h>

struct alloc_record {
	size_t size;
	char *ptr;
};

static int compar_alloc_record_by_ptr(const void *ap, const void *bp) {
	const struct alloc_record *a=ap, *b=bp;
	
	if (a->ptr < b->ptr)
		return -1;
	else if (a->ptr > b->ptr)
		return 1;
	else
		return 0;
}

static size_t random_block_size(void) {
	int scale = random() % 11;
	switch (scale) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4: return random() % 25;
		case 5:
		case 6:
		case 7: return random() % 100;
		case 8:
		case 9: return random() % 1000;
		case 10: return random() % 10000;
		default:
			fprintf(stderr, "random() %% 3 returned %d somehow!\n", scale);
			exit(EXIT_FAILURE);
	}
}

#define L(node) (node+node+1)
#define R(node) (node+node+2)
#define P(node) ((node-1)>>1)

#define V(node) (bp->block[node].remaining)

//used by test_block_pool to make sure the pool's block array is a max heap
//set node=0 to scan the whole heap (starting at the root)
//returns nonzero on success
static int check_heap(struct block_pool *bp, size_t node) {
	if (node < bp->count) {
		if (node) { //the root node need not be the max, but its subtrees must be valid
			if (L(node) < bp->count && V(L(node)) > V(node))
				return 0;
			if (R(node) < bp->count && V(R(node)) > V(node))
				return 0;
		}
		return check_heap(bp, L(node)) && check_heap(bp, R(node));
	} else
		return 1;
}

#undef L
#undef R
#undef P
#undef V

/* Performs a self-test of block_pool.
   Returns 1 on success, 0 on failure.
   If verify_heap is nonzero, the test will check the heap structure every
   single allocation, making test_block_pool take n^2 time. */
static int test_block_pool(size_t blocks_to_try, FILE *out, int verify_heap) {
	struct block_pool *bp = block_pool_new(NULL);
	struct alloc_record *record = malloc(sizeof(*record) * blocks_to_try);
	size_t i;
	size_t bytes_allocated = 0;
	#define print(...) do { \
			if (out) \
				printf(__VA_ARGS__); \
		} while(0)
	
	print("Allocating %zu blocks...\n", blocks_to_try);
	
	for (i=0; i<blocks_to_try; i++) {
		record[i].size = random_block_size();
		record[i].ptr = block_pool_alloc(bp, record[i].size);
		
		bytes_allocated += record[i].size;
		
		memset(record[i].ptr, 0x55, record[i].size);
		
		if (verify_heap && !check_heap(bp, 0)) {
			print("Block pool's max-heap is wrong (allocation %zu)\n", i);
			return 0;
		}
	}
	
	print("Finished allocating\n"
	       "    %zu blocks\n"
	       "    %zu bytes\n"
	       "    %zu pages\n",
		blocks_to_try, bytes_allocated, bp->count);
	
	qsort(record, blocks_to_try,
		sizeof(*record), compar_alloc_record_by_ptr);
	
	print("Making sure block ranges are unique...\n");
	//print("0: %p ... %p\n", record[0].ptr, record[0].ptr+record[0].size);
	for (i=1; i<blocks_to_try; i++) {
		struct alloc_record *a = &record[i-1];
		struct alloc_record *b = &record[i];
		
		//print("%zu: %p ... %p\n", i, b->ptr, b->ptr+b->size);
		
		if (a->ptr > b->ptr) {
			struct alloc_record *tmp = a;
			a = b;
			b = tmp;
		}
		
		if (a->ptr <= b->ptr && a->ptr+a->size <= b->ptr)
			continue;
		
		print("Allocations %zu and %zu overlap\n", i-1, i);
		return 0;
	}
	
	print("Checking heap structure...\n");
	if (!check_heap(bp, 0)) {
		print("Block pool's max-heap is wrong\n");
			return 0;
	}
	
	block_pool_free(bp);
	free(record);
	
	return 1;
	
	#undef print
}


int main(void)
{
	plan_tests(1);
	
	//test a few blocks with heap verification
	ok1(test_block_pool(10000, NULL, 1));
	
	return exit_status();
}
