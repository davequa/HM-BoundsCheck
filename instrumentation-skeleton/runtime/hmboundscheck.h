#ifndef _DLIB_H_
#define _DLIB_H_

#include <dlfcn.h>
#include <stddef.h>

typedef struct memSeg{
	struct memSeg *next;
	size_t allocsz;

	void *startAddrRegion;
	void *startAddrL;
	void *startAddrR;
}memSeg;

typedef struct freeList{
	struct memSeg *first;
	int counter;
	size_t size;
}freeList;

//Essential values for shadow memory management, including the starting address of the shadow memory.
extern void *shadowMemStart;

//The scale variable used for determining the red-zone size.
extern size_t scale;

//Used for the red-zone of 1-byte. Easier for 'slow' and 'fast' checks, and will not result
//in any alignment issues for the red-zone pattern (unlike the 8-byte pattern).
extern unsigned char redzone;

/*----------------Initialisation Functions----------------*/
//Unmap the shadow memory whenever necessary for security/program safety reasons.
int unmapShadowMemory();

//Set up the shadow memory for the run-time library.
int initShadowMemory();

//Calculates the size of the red-zone. Should be done once at startup, result saved in a constant.
size_t calcRZSize(size_t sz);

//Destroys mutexes, unmaps the shadow memory, and sets the initialisation variable to 0 to ensure the
//library crashes/exhibits undefined behaviour if improperly unloaded.
int unload();

//Initialises some needed variables/data/etc.
int initLib();
/*--------------------------------*/

/*----------------General Functions----------------*/
//Examine which lock must be acquired which is to be locked/unlocked by the caller.
int getLock(size_t sz);

//Unlock a lock by using a size, which functions as an indicator of the index in the freelist array.
int unlock(size_t sz);

//Acquire a lock by using a size, which functions as an indicator of the index in the freelist array.
int lock(size_t sz);

//Method for getting the actual address with respect to an object memory address given.
void *getShadowMemoryAddress(void *mem);

//Check the shadow memory on a 'slow' path (check). The accessSize parameter is un-used now, but exists to offer
//compatibility for future work with 1-, 2-, 4-, and 8-byte accesses in the shadow memory (currently, only 1-byte
//accesses are supported).
int checkRegistration(void *mem, int accessSize);

//Check if the selected memory (for access) is addressable through a 'fast' check, and otherwise opt for a 'slow' check.
int checkMemoryAccess(void *mem, int accessSize);

//A method to reverse all bits in the shadow memory to a not-in-use state.
int removeAddr(void *memL, void *memR);

//A method for setting all the bits in the shadow memory to represent an allocated/addressable state.
int registerAddr(void *memL, void *memR);

//Insert the red-zone memory pattern into the red-zones of the specified memory region.
//An extra input variable is used for the size here, to allow for stack, static, and global objects
//to have different sizes for red-zones.
int insertRZPattern(void *mem, size_t size);
/*--------------------------------*/

/*----------------Custom Allocator Functions----------------*/
//Calculates the array element for the specific size of an allocated memory region. (NOT CORRECT, YET)
int getFreeListArrayIndex(size_t sz);

//Creates a register entry for the free-list items.
memSeg *createBlock(void *start, size_t originalsz);

//Returns a block of memory that was in-use to the free-list, signalling that it is ready to be re-used for new allocations.
int returnBlockToFreeList(void *mem, int startAdd);

//Retrieves the address of a block from the free-list, in turn removing it from the free-list registration.
void *getBlockFromFreeList(size_t sz);

//Checks if a free list for a certain region size already exists.
int checkFreeListArray(size_t sz);

//Readies the list for use by dividing the pre-allocated block into chunks and calling the poisoning functionality.
int setFreeList(void *blockStart, int index, size_t sz);

//Pre-allocates a new memory region, ready for use by the application.
int allocateFreeList(size_t sz);
/*--------------------------------*/

//Wrapped free. Initiates the additional deallocation checks.
void Dlib_free(void *mem);

//Wrapped malloc, initiates the pattern insertion and allocator data retrieval.
void *Dlib_malloc(size_t sz);

//Wrapped realloc.
void *Dlib_realloc(void *mem, size_t nsz);

//Wrapped calloc.
void *Dlib_calloc(size_t num, size_t sz);

//Wrapped memalign. Still fix for posix_memalign?
void *Dlib_memalign(size_t alignment, size_t sz);

#endif