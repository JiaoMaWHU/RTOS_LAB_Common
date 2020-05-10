// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.

// Jacob Egner 2008-07-31
// modified 8/31/08 Jonathan Valvano for style
// modified 12/16/11 Jonathan Valvano for 32-bit machine
// modified August 10, 2014 for C99 syntax

/* This example accompanies the book
   "Embedded Systems: Real Time Operating Systems for ARM Cortex M Microcontrollers",
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains

 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include <stdint.h>
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/OS.h"
#include <string.h>
#include <stdlib.h> 

static int32_t heap[HEAP_SIZE];
heap_stats_t heap_stats;
int32_t* heapP = heap;
extern group groupArray[];

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Heap_Init(void){
	// init heap array
	memset(heap, 0, sizeof(heap));
	heap[0] = -(HEAP_SIZE-2);
	heap[HEAP_SIZE-1] = -(HEAP_SIZE-2);
	
	// init heap_stats
	heap_stats.free = (HEAP_SIZE-2)*sizeof(int32_t);
	heap_stats.size = (HEAP_SIZE)*sizeof(int32_t);
	heap_stats.used = 2*sizeof(int32_t);
  return 0;   // replace
}

int32_t Heap_group_Init(void){
	memset(heap, 0, sizeof(heap));
	// heap 1
	heap[0] = -(HEAP_SIZE/2-64-2);
	heap[HEAP_SIZE/2-64-1] = -(HEAP_SIZE/2-64-2);
	groupArray[1].heapAddress = heap;
	
	// heap 2
	heap[HEAP_SIZE/2] = -(HEAP_SIZE/2-2);
	heap[HEAP_SIZE-1] = -(HEAP_SIZE/2-2);
	groupArray[2].heapAddress = heap + HEAP_SIZE/2;
	
	// init heap_stats
	heap_stats.free = (HEAP_SIZE-4)*sizeof(int32_t);
	heap_stats.size = (HEAP_SIZE)*sizeof(int32_t);
	heap_stats.used = 4*sizeof(int32_t);
  return 0;   // replace
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes){
	// search from the first enntry
	// first fit
	if(desiredBytes<=0){
		return 0;
	}
	int i = 0;
	while(i<HEAP_SIZE-1){
		if(heap[i]<0){
			if(abs(heap[i])*sizeof(int32_t)>=desiredBytes){
				int old_size = abs(heap[i]);
				int num_alloc_entries = (desiredBytes+sizeof(int32_t)-1)/sizeof(int32_t);
				if((old_size-num_alloc_entries-2)<1){
					// no free space left after allocation
					// use the entire block
					heap[i] = old_size;
					heap[i+old_size+1] = old_size;
					
					// change stats
					heap_stats.free -= old_size*sizeof(int32_t);
					heap_stats.used += old_size*sizeof(int32_t);
				}else{
					// split the block
					heap[i] = num_alloc_entries;
					heap[i+num_alloc_entries+1] = num_alloc_entries;
					heap[i+num_alloc_entries+2] = -(old_size-num_alloc_entries-2);
					heap[i+old_size+1] = -(old_size-num_alloc_entries-2);
					
					// change stats
					heap_stats.free -= (num_alloc_entries+2)*sizeof(int32_t);
					heap_stats.used += (num_alloc_entries+2)*sizeof(int32_t);
				}
				return (void *)(&heap[i+1]);
			}
		}
		i = i + abs(heap[i]) + 2;
	}
  return 0;   // NULL
}

void* Heap_Group_Malloc(int32_t desiredBytes, uint16_t groupid){
	// search from the first enntry
	// first fit
	int32_t* gHeap = groupArray[groupid].heapAddress;
	if(desiredBytes<=0){
		return 0;
	}
	int i = 0;
	while(i<HEAP_SIZE/2-1){
		if(gHeap[i]<0){
			if(abs(gHeap[i])*sizeof(int32_t)>=desiredBytes){
				int old_size = abs(gHeap[i]);
				int num_alloc_entries = (desiredBytes+sizeof(int32_t)-1)/sizeof(int32_t);
				if((old_size-num_alloc_entries-2)<1){
					// no free space left after allocation
					// use the entire block
					gHeap[i] = old_size;
					gHeap[i+old_size+1] = old_size;
					
					// change stats
					heap_stats.free -= old_size*sizeof(int32_t);
					heap_stats.used += old_size*sizeof(int32_t);
				}else{
					// split the block
					gHeap[i] = num_alloc_entries;
					gHeap[i+num_alloc_entries+1] = num_alloc_entries;
					gHeap[i+num_alloc_entries+2] = -(old_size-num_alloc_entries-2);
					gHeap[i+old_size+1] = -(old_size-num_alloc_entries-2);
					
					// change stats
					heap_stats.free -= (num_alloc_entries+2)*sizeof(int32_t);
					heap_stats.used += (num_alloc_entries+2)*sizeof(int32_t);
				}
				return (void *)(&gHeap[i+1]);
			}
		}
		i = i + abs(gHeap[i]) + 2;
	}
  return 0;   // NULL
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){
	void* pt = Heap_Malloc(desiredBytes);
	if(pt==NULL){
		return 0;
	}
	memset(pt, 0, *((int*)pt-1)*sizeof(int32_t));
  return pt;   // NULL
}

void* Heap_Group_Calloc(int32_t desiredBytes, uint16_t groupid){
	void* pt = Heap_Group_Malloc(desiredBytes, groupid);
	if(pt==NULL){
		return 0;
	}
	memset(pt, 0, *((int*)pt-1)*sizeof(int32_t));
  return pt;   // NULL
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	int old_size = abs(*((int*)oldBlock-1));
	int desired_more_block = (desiredBytes+sizeof(int32_t)-1)/sizeof(int32_t) - old_size;
	if(desired_more_block<=0){
		return 0; // no need for more spaces
	}
	
	// see if top and bot block statisfy this requirement
	int32_t* top_self_counter = (int32_t*)oldBlock-1;
	int32_t* bot_self_counter = (int32_t*)oldBlock + old_size;
	int32_t* top_merge_counter = NULL;
	int32_t* bottom_merge_counter = NULL;
	int free_block = 0;
	
	if(top_self_counter-1>=heap && *((int32_t*)oldBlock-2)<0){
		int top_merge_size = abs(*(top_self_counter-1));
		free_block += top_merge_size; // merge used entries
		free_block += 2; // also used size counter for free space
		top_merge_counter = top_self_counter-1-top_merge_size-1;
	}
	
	if((bot_self_counter+1)<(heap+HEAP_SIZE) && *(bot_self_counter+1)<0){
		int bot_merge_size = abs(*(bot_self_counter+1));
		free_block += bot_merge_size;
		free_block += 2;
		bottom_merge_counter = bot_self_counter+1+bot_merge_size+1;
	}
	
	// statisfy
	if(free_block>=desired_more_block){
		if(top_merge_counter==NULL){
			top_merge_counter = top_self_counter;
		}else{
			heap_stats.free += (2)*sizeof(int32_t);
			heap_stats.used -= (2)*sizeof(int32_t);
		}
		if(bottom_merge_counter==NULL){
			bottom_merge_counter = bot_self_counter;
		}else{
			heap_stats.free += (2)*sizeof(int32_t);
			heap_stats.used -= (2)*sizeof(int32_t);
		}
    // use memmove for may overlap
		memmove(top_merge_counter+1, oldBlock, old_size*sizeof(int32_t));
		old_size += free_block;
		*top_merge_counter = old_size;
		*bottom_merge_counter = old_size;
		return top_merge_counter+1;
	}else{
		// no then, allocate new space
		// if no space, return null
		// else alloc, cpy, free old block
		void* res_pt = Heap_Malloc(desiredBytes);
		if(res_pt!=NULL){
			memcpy((int *)res_pt, oldBlock, old_size*sizeof(int32_t));
			Heap_Free(oldBlock);
            return res_pt;
		}
	}
  return 0;   // NULL
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Heap_Free(void* pointer){
	if(pointer==NULL || *((int32_t*)pointer-1)<=0){
		return 1;
	}
	int old_size = abs(*((int32_t*)pointer-1)); // top size counter
	int32_t* top_self_counter = (int32_t*)pointer-1;
	int32_t* bot_self_counter = (int32_t*)pointer + old_size;
	
	int32_t* top_merge_counter = NULL;
	int32_t* bottom_merge_counter = NULL;
  
	heap_stats.free += (old_size)*sizeof(int32_t);
	heap_stats.used -= (old_size)*sizeof(int32_t);
	
	if(top_self_counter-1>=heap && *((int32_t*)pointer-2)<0){
		int top_merge_size = abs(*(top_self_counter-1));
		old_size += top_merge_size; // merge used entries
		old_size += 2; // also used size counter for free space
		top_merge_counter = top_self_counter-1-top_merge_size-1;
		heap_stats.free += (2)*sizeof(int32_t);
		heap_stats.used -= (2)*sizeof(int32_t);
	}
	if((bot_self_counter+1)<(heap+HEAP_SIZE) && *(bot_self_counter+1)<0){
		int bot_merge_size = abs(*(bot_self_counter+1));
		old_size += bot_merge_size;
		old_size += 2;
		bottom_merge_counter = bot_self_counter+1+bot_merge_size+1;
		heap_stats.free += (2)*sizeof(int32_t);
		heap_stats.used -= (2)*sizeof(int32_t);
	}
	if(top_merge_counter==NULL){
		top_merge_counter = top_self_counter;
	}
	if(bottom_merge_counter==NULL){
		bottom_merge_counter = bot_self_counter;
	}
	*(top_merge_counter) = - old_size;
	*(bottom_merge_counter) = - old_size;
	memset(top_merge_counter+1, 0, old_size*sizeof(int32_t));
  return 0;   // replace
}

int32_t Heap_Group_Free(void* pointer, uint16_t groupid){
	int32_t* gHeap = groupArray[groupid].heapAddress;
	if(pointer==NULL || *((int32_t*)pointer-1)<=0){
		return 1;
	}
	int old_size = abs(*((int32_t*)pointer-1)); // top size counter
	int32_t* top_self_counter = (int32_t*)pointer-1;
	int32_t* bot_self_counter = (int32_t*)pointer + old_size;
	
	int32_t* top_merge_counter = NULL;
	int32_t* bottom_merge_counter = NULL;
  
	heap_stats.free += (old_size)*sizeof(int32_t);
	heap_stats.used -= (old_size)*sizeof(int32_t);
	
	if(top_self_counter-1>=gHeap && *((int32_t*)pointer-2)<0){
		int top_merge_size = abs(*(top_self_counter-1));
		old_size += top_merge_size; // merge used entries
		old_size += 2; // also used size counter for free space
		top_merge_counter = top_self_counter-1-top_merge_size-1;
		heap_stats.free += (2)*sizeof(int32_t);
		heap_stats.used -= (2)*sizeof(int32_t);
	}
	if((bot_self_counter+1)<(gHeap+HEAP_SIZE) && *(bot_self_counter+1)<0){
		int bot_merge_size = abs(*(bot_self_counter+1));
		old_size += bot_merge_size;
		old_size += 2;
		bottom_merge_counter = bot_self_counter+1+bot_merge_size+1;
		heap_stats.free += (2)*sizeof(int32_t);
		heap_stats.used -= (2)*sizeof(int32_t);
	}
	if(top_merge_counter==NULL){
		top_merge_counter = top_self_counter;
	}
	if(bottom_merge_counter==NULL){
		bottom_merge_counter = bot_self_counter;
	}
	*(top_merge_counter) = - old_size;
	*(bottom_merge_counter) = - old_size;
	memset(top_merge_counter+1, 0, old_size*sizeof(int32_t));
  return 0;   // replace
}


//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Heap_Stats(heap_stats_t *stats){
	(*stats).free = heap_stats.free;
	(*stats).size = heap_stats.size;
	(*stats).used = heap_stats.used;
  return 0;   // replace
}
