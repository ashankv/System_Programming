#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct _block {
	size_t size;
	struct _block* next; // next fr block
	struct _block* prev; // previous fr block
} block;

typedef struct _block_sentinal {
	size_t size;
	struct _block* next; // next fr block
	struct _block* prev; // previous fr block
	char useless;
	size_t fake_tag;
} sentinal;

#define BLOCK_SIZE (sizeof(block))
#define TAG_SIZE (sizeof(size_t))

block* heap_begin = NULL;
char* heap_end = NULL;
block* head = NULL;
int first_time = 0;

sentinal head_sentenal;

sentinal tail_sentenal;

void initializer() {
	if (first_time == 0) head = NULL;
	first_time = 1;
	void* hptr = heap_begin = sbrk(BLOCK_SIZE+ 4 + TAG_SIZE);
	heap_end = (char*)hptr + BLOCK_SIZE+ 4 + TAG_SIZE;
	((block*)hptr)->size = 4;
	((block*)hptr)->next = head;
	char* tag = (char*)hptr + BLOCK_SIZE + 4;
	*((size_t*)tag) = 4;
	head = (block*)hptr;

	head->prev = (block*)&head_sentenal;
	head_sentenal.next = head;
	head_sentenal.prev = NULL;
	head->next = (block*)&tail_sentenal;
	tail_sentenal.prev = head;
	tail_sentenal.next = NULL;
	tail_sentenal.size = 1;
	tail_sentenal.fake_tag = 1;
	head_sentenal.size = 1;
	head_sentenal.fake_tag = 1;
}

block* find_block(size_t size) {
	block* ptr = head_sentenal.next;
	//int count = 0;
	while (ptr) {
		// fprintf(stderr, "count = %d\n",++count);
		if (~(ptr->size & 1) && ptr->size >= size) {
			return ptr;
		}
		ptr = ptr->next;

	}
	return NULL;
}

size_t* find_tag(block* ptr) {

	char* tt = (char*)ptr + BLOCK_SIZE + ((ptr->size & 1) ? (ptr->size & ~1) : ptr->size);
	return (size_t*)tt;
}

void print_list() {
	block* ptr = head_sentenal.next;
	while (ptr != (block*)&tail_sentenal) {
		size_t* tag = find_tag(ptr);
		fprintf(stderr,"ptr = %p, size = %lu, ptrprev = %p, tag_size = %lu \n",ptr,ptr->size,ptr->prev,*tag);
		ptr = ptr->next;
	}
	printf("\n");
}

void split(size_t align_size, block* find) {
	long d1 = align_size;
	long d2 = find->size - align_size - BLOCK_SIZE - TAG_SIZE;

	int using_now = find->size & 1;

	if (!using_now) find->size = (size_t)d1 | 1;


	size_t* tag = find_tag(find);
	if (!using_now) *tag = (size_t)d1 | 1;

	//remove it from list
	if (!using_now) {
		if (find != head_sentenal.next) {
			find->prev->next = find->next;
			find->next->prev = find->prev;
		} else {
			head_sentenal.next = find->next;
			find->next->prev = (block*)&head_sentenal;
			head = head_sentenal.next;
		}
	}
	// new block update
	block* new_block = (block*)((char*)tag + TAG_SIZE);

	new_block->next = (block*)&tail_sentenal;
	tail_sentenal.prev->next = new_block;
	new_block->prev = tail_sentenal.prev;
	tail_sentenal.prev = new_block;


	new_block->size = (size_t)d2;
	size_t* new_block_tag = find_tag(new_block);
	*new_block_tag = (size_t)d2;
}


void *mlc(size_t size) {
	if (first_time == 0) {
		initializer();
		first_time = 1;
	}

	size_t align_size = size / 4 * 4  == size ? size : (size / 4 * 4 + 4);
	block* find = find_block(align_size);
	//print_list();
 	//fprintf(stderr, "FINDBLOCK = %p\n",find);
	// fprintf(stderr, "find = %p\n",find);

	//find = NULL;
	if (find == NULL) {
		block* ptr = sbrk(BLOCK_SIZE + align_size + TAG_SIZE); // open new space
		heap_end = (char*)ptr + BLOCK_SIZE + align_size + TAG_SIZE;
		ptr->size = align_size | 1; // label as used

		size_t* tag = find_tag(ptr);


		*tag = align_size | 1; // new need to put into the list

		return ptr + 1;
	} else {


		long d2 = find->size - align_size - BLOCK_SIZE - TAG_SIZE;

		if (d2 > 0 && d2 >= 64) {
			split(align_size,find);
/*			find->size = (size_t)d1 + 1;
			size_t* tag = find_tag(find);
			*tag = (size_t)d1 + 1;

		 	//remove it from list
			if (find != head_sentenal.next) {
				find->prev->next = find->next;
				find->next->prev = find->prev;
			} else {
				head_sentenal.next = find->next;
				find->next->prev = (block*)&head_sentenal;
				head = head_sentenal.next;
			}

			// new block update
			block* new_block = (block*)((char*)tag + TAG_SIZE);

			new_block->next = (block*)&tail_sentenal;
			tail_sentenal.prev->next = new_block;
			new_block->prev = tail_sentenal.prev;
			tail_sentenal.prev = new_block;


			new_block->size = (size_t)d2;
			size_t* new_block_tag = find_tag(new_block);
			*new_block_tag = (size_t)d2;*/
		} else {

			find->size |= 1;
			size_t* tag = find_tag(find);
			*tag |= 1;
			if (find != head_sentenal.next) {
				find->prev->next = find->next;
				find->next->prev = find->prev;
			} else {
				head_sentenal.next = find->next;
				find->next->prev = (block*)&head_sentenal;
				head = head_sentenal.next;
			}
		}
		return find + 1;

	}

}

block* prev_blk(block* ptr) {
	if (ptr == heap_begin) return NULL;
	size_t size = ((size_t*)ptr)[-1];
	if (size & 1) return NULL;
	return (block*)((char*)ptr - TAG_SIZE - size - BLOCK_SIZE);
}

block* next_blk(block* ptr) {

	size_t size = ptr->size;
	char* ret = (char*)ptr;
	ret += BLOCK_SIZE + size + TAG_SIZE;

	//fprintf(stderr,"next_blk_phy is %p\n",ret);

	if (ret - (char*)heap_end >= 0) return NULL;



	if (((block*)ret)->size & 1) return NULL;


	return (block*)ret;

}

void try_merge(block* ptr) {
	// in liked list:



	if (ptr->prev == (block*)&head_sentenal && ptr->next == (block*)&tail_sentenal) return;

	block* phy_prev = prev_blk(ptr);
	block* phy_next = next_blk(ptr);

	// fprintf(stderr,"curr = %p\n",ptr);
	// fprintf(stderr,"phy_prev = %p\n",phy_prev);
	// fprintf(stderr,"phy_next = %p\n",phy_next);


	// (Mprev //Dprev T Mcurr Dprev// T)
	if (phy_prev && !phy_next) {
		phy_prev->size = phy_prev->size + TAG_SIZE + ptr->size + BLOCK_SIZE;
		*find_tag(ptr) = phy_prev->size;
		if (ptr != (block*)&head_sentenal.next) {
			// fprintf(stderr,"head is  %p sure>_< %p \n",(block*)&head_sentenal.next,head);
			// print_list();
			ptr->prev->next = ptr->next;
			ptr->next->prev = ptr->prev;
			// fprintf(stderr,"sinish merg \n");
		} else {
			//fprintf(stderr, "TELL ME WHY\n" );
			head_sentenal.next = ptr->next;
			ptr->next->prev = (block*)&head_sentenal;
		}

	} else if (!phy_prev && phy_next) {
		// (Mcurr //Dcurr Tcurr Mnext Dnext// Tnext)
		ptr->size = ptr->size + TAG_SIZE + phy_next->size + BLOCK_SIZE;
		*find_tag(phy_next) = ptr->size;
		if (phy_next != (block*)&head_sentenal.next) {
			phy_next->prev->next = phy_next->next;
			phy_next->next->prev = phy_next->prev;
		} else {
			head_sentenal.next = phy_next->next;
			phy_next->next->prev = (block*)&head_sentenal;
		}

	} else if (phy_next && phy_prev) {
		// Mprev (D T Mcurr D T Mnext D) T
		phy_prev->size = phy_prev->size + 2 * TAG_SIZE + ptr->size + 2 * BLOCK_SIZE + phy_next->size;
		*find_tag(phy_next) = phy_prev->size;

		// eat the next
		if (phy_next != (block*)&head_sentenal.next) {
			phy_next->prev->next = phy_next->next;
			phy_next->next->prev = phy_next->prev;
		} else {
			head_sentenal.next = phy_next->next;
			phy_next->next->prev = (block*)&head_sentenal;
		}

		//shit off curr
		if (ptr != (block*)&head_sentenal.next) {
			ptr->prev->next = ptr->next;
			ptr->next->prev = ptr->prev;
		} else {
			head_sentenal.next = ptr->next;
			ptr->next->prev = (block*)&head_sentenal;
		}
	}
}



void fr(void* ptr) {
	if (ptr == NULL) return;

	block* find = ptr;
	find = find - 1;

	find->size &= ~1;
	size_t* tag = find_tag(find);

	*tag &= ~1;


	// fprintf(stderr, "MERGE START\n" );


	//fprintf(stderr,"merge ok\n");

	find->next = head_sentenal.next;
	head_sentenal.next->prev = find;
	head_sentenal.next = find;
	find->prev = (block*)&head_sentenal;
	head = head_sentenal.next;

	try_merge(find);
}

void *clc(size_t num, size_t size) {
    // implement calloc!
    block* a = mlc(num*size);
    memset(a,0,num*size);
    return a;
}


void *rlc(void *ptr, size_t size) {
	block* blk = ptr;
	blk = blk - 1;
	size_t align_size = size / 4 * 4  == size ? size : (size / 4 * 4 + 4);

	long old_size = blk->size & ~1;

	if ((unsigned)old_size >= size) {

		if ((unsigned)old_size - align_size > (unsigned)old_size || (unsigned)old_size / 3 == size ) {
			size_t* tag = find_tag(blk);
			blk->size = align_size + 1;
			char* pp = (char*)ptr + align_size;
			*((size_t*)pp) = align_size + 1;
			pp += TAG_SIZE;

			block* new_block = (block*)pp;
			new_block->size = old_size - align_size - TAG_SIZE - BLOCK_SIZE;
			*tag = new_block->size;
			new_block->next = (block*)&tail_sentenal;
			new_block->prev = tail_sentenal.prev;
			tail_sentenal.prev->next = new_block;
			tail_sentenal.prev = new_block;

			try_merge(new_block);
			//print_list();
			// fprintf(stderr, "SEG SEND\n");

		}

		return ptr;
	}
// // M DDDDD T
// //M1 D1 T1 M2 D2 T2
// 	if (old_size >= size) {
// 		// no need for new mlc, just split || not split
// 		if (old_size - size < 64) {
// 			// don't split
// 		} else {
// 			//split plz
// 			split(old_size,blk);
//
// 		}
// 		return ptr;
// 	}

	char* a = mlc(size);
	long i;
	long len = old_size > (long)size ? (long)size : old_size;
	for (i = 0; i < len; i++) {
		a[i] = ((char*)ptr)[i];
	}

	fr(ptr);

	return a;
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    // implement calloc!

    return clc(num,size);
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */


void *malloc(size_t size) {
    // implement malloc!
    return mlc(size);
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    // implement free!
    fr(ptr);
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    // implement realloc!
    return rlc(ptr,size);
}
