#ifndef _DLIB_H_
#define _DLIB_H_

#include <dlfcn.h>
#include <stddef.h>

#define HASHSZ 4096

typedef struct rzAddr{
	void *startAddrL;
	void*startAddrR;
	struct rzAddr *next;
}rzAddr;

typedef struct rzHashBucket{
	unsigned int counter;
	rzAddr *first;
	rzAddr *last;
}rzHashBucket;

//The scale variable used for determining the red-zone size.
extern size_t scale;

//Used for the red-zone of 1-byte. Easier for 'slow' and 'fast' checks, and will not result
//in any alignment issues for the red-zone pattern (unlike the 8-byte pattern).
extern unsigned char redzone;

/*----------------Initialisation Functions----------------*/
//Calculates the size of the red-zone. Should be done once at startup, result saved in a constant.
size_t calcRZSize(size_t sz);

//Destroys mutexes, and sets the initialisation variable to 0 to ensure the library crashes/exhibits undefined
//behaviour if improperly unloaded.
int unloadLib();

//Initialises some needed variables/data/etc.
int initLib();
/*--------------------------------*/

/*----------------General Functions----------------*/
//Examine which lock must be acquired which is to be locked by the caller.
int getLock(int bucket);

//Unlock a lock by using a memory address, which functions as an indicator of the bucket in the hash table.
int unlock(void *mem);

//Acquire a lock by using a memory address, which functions as an indicator of the bucket in the hash table.
int lock(void *mem);

//Calculate hash bucket for given red-zone memory address.
int getRZAddrBucket(void *mem);

//Check if the selected memory (for access) is addressable, or poisoned (red-zone), through the hash table memory registration.
//This is also defined as the 'slow' check, performed after a 'fast' check determines the red-zone pattern is available.
//The parameter accessSize is un-used, but exists for compatibility with the shadow memory variant of this library.
int checkRegistration(void *mem, int accessSize);

//Check if the selected memory (for access) is addressable through a 'fast' check, and otherwise opt for a 'slow' check.
//The accessSize parameter is un-used in this implementation, but exists here for compatibility with the shadow memory
//variant of this library.
int checkMemoryAccess(void *mem, int accessSize);

//Remove registered red-zone address from its respective bucket in the hash table.
rzAddr *removeAddrFromList(rzAddr* toRemove, int rzBucket);

//Remove red-zone address from red-zone hash table on deallocation.
int removeAddr(void *memL, void *memR);

//Add registered red-zone address to its respective bucket in the hash table.
int addAddrToList(rzAddr *toAdd, int rzBucket);

//Register red-zone address in red-zone hash table.
int registerAddr(void *memL, void *memR);

//Insert the red-zone memory pattern into the red-zones of the specified memory region.
//An extra input variable is used for the size here, to allow for stack, static, and global objects
//to have different sizes for red-zones.
int insertRZPattern(void *mem, size_t size);
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
