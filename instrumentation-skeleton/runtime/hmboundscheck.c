#include <noinstrument.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>

#include <malloc.h>

#include <sys/mman.h>

#define checkMemoryAccess NOINSTRUMENT(checkMemoryAccess)

#define Dlib_free NOINSTRUMENT(Dlib_free)
#define Dlib_malloc NOINSTRUMENT(Dlib_malloc)
#define Dlib_realloc NOINSTRUMENT(Dlib_realloc)
#define Dlib_calloc NOINSTRUMENT(Dlib_calloc)
#define Dlib_memalign NOINSTRUMENT(Dlib_memalign)

#define checkRegistration NOINSTRUMENT(checkRegistration)

#define unmapShadowMemory NOINSTRUMENT(unmapShadowMemory)
#define initShadowMemory NOINSTRUMENT(initShadowMemory)

#define getShadowMemoryAddress NOINSTRUMENT(getShadowMemoryAddress)

#define getBlockFromFreeList NOINSTRUMENT(getBlockFromFreeList)
#define returnBlockToFreeList NOINSTRUMENT(returnBlockToFreeList)
#define createBlock NOINSTRUMENT(createBlock)
#define getFreeListArrayIndex NOINSTRUMENT(getFreeListArrayIndex)

#define checkFreeListArray NOINSTRUMENT(checkFreeListArray)
#define setFreeList NOINSTRUMENT(setFreeList)
#define allocateFreeList NOINSTRUMENT(allocateFreeList)

//#define calcRZSize NOINSTRUMENT(calcRZSize)

//#define unloadLib NOINSTRUMENT(unloadLib)
#define initLib NOINSTRUMENT(initLib)

#define insertRZPattern NOINSTRUMENT(insertRZPattern)

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

//IFDEF for big-endian change calc for address to () mem >> 7, and for little-endian 7 - () mem >> 7?

#define ADDRSPACE_BITS 47
#define SIZE ((1ULL<<ADDRSPACE_BITS) / 8)
#define LOC 0x6600000000ULL

//The following macro computes the offset into the byte (array).
#define BIT_OFFSET(bit) ((bit) / 8)
 
//The following computes the offset of the bit in the byte.
#define BIT_MASK(bit) (1 << ((bit) & 7))
 
//Used to set a particular bit in a byte (or set of bytes).
#define BIT_SET(byte, bit) (((unsigned char*)(byte))[BIT_OFFSET(bit)] |= BIT_MASK(bit))
 
//Used to clear a particular bit in the byte (or set of bytes).
#define BIT_CLR(byte, bit) (((unsigned char*)(byte))[BIT_OFFSET(bit)] &= ~BIT_MASK(bit))
 
//Used to test if a particular bit is set in the byte (or set of bytes).
#define BIT_TST(byte, bit) (((unsigned char*)(byte))[BIT_OFFSET(bit)] & BIT_MASK(bit))

//Offset used as a compiler constant for the mmap() operation of the shadow memory. This value has to be set to a 
//platform-specific value (depending on Linux, Windows, MacOS, etc). If the -fPIE/-pie flags etc are activated, use 0.
//size_t global_offset = 0;

//Essential global values for the management of the shadow memory.
void *shadowMemStart = NULL;

//Originally used for the size of the shadow memory. A left-over from the ASAN implementation, but now used to calculate
//the red-zone size dynamically. For scale value N, the red-zone size will be 2 ^ N (e.g., N = 3, 2 ^ 3 = 8 bytes).
//The minimum size of this scale value is 3, where the red-zone is 8 bytes. There is, in theory, no maximum size,
//but the largest (recommended) size is 10 (for a red-zone size of 1024 bytes).
static const size_t scale = 5;

//The magic redzone pattern, simply an unsigned character with a size 8 bits. The maximum value (0xFF) was chosen
//for this, but can be changed to anything (as long as it is 8 bits/1 byte long).
static const unsigned char redzone = 0x2A;

//Size of the red-zone, determined by the scale variable.
static const size_t rz_sz = 32;

/*----------------Environment Variables----------------*/

//SET TO 0 TO ENABLE!
//SET TO 1 TO DISABLE!

//Variable for activating red-zone poison pattern checking. Deactivating this option will also disable all
//red-zone pattern insertion, meaning red-zones will no longer be explicitly poisoned. Only the 'slow' check will be used here,
//resulting in a performance decrease, but an increase in memory efficiency (since the red-zones will no longer be intialised).
static const int fastCheckInit = 0;

//Variable for activating the different method of keeping track of addressibility in the shadow memory. The first, which is
//activated by setting this to 0, uses the standard ASAN method. The second, non-standard, uses the flipping of bits to
//describe the addressability of bits (i.e., 0 for addressable, 1 for unaddressable, per byte).
static const int ASANCheckInit = 0;

//Variable for enabling the ASAN custom memory allocator. If disabled, the LBC-like allocator (standard) will be used. If enabled,
//an array of freelists exists to pre-allocate memory for instant and easy allocation on a memory allocation request. Beware, this
//slows down the framework, and disables explicit poisoning, and thus also the fast check.
static const int useFreeLists = 1;

//NOTE: The locking mechanism for the mutexes with the custom allocator appears to compromise its funcionality.
//This is incredibly strange, but since SPEC2006 is executed single threaded, just remove the mutexes.

//IFDEF (to not allocate space for it if unused?) (change mapping more efficiency? now quickly very large?)
#define MEMORY_LIST 48

//The array of freelists.
static freeList memoryArray[MEMORY_LIST];

//This is the amount of blocks that are pre-allocated whenever a new freelist is created.
static const int standardPreAllocSize = 10;

//Variable for enabling shadow memory. If disabled, shadow memory will be turned off, and only the explicit poisoning of
//the red-zones will be used for the detection of memory errors. This will increase the amount of false-positives experienced,
//but will probably increase performance as well. 
static const int useRegistration = 0;

//For debugging purposes. Increases runtime overhead by almost 100%.
static const int debug = 1;

//OPTIONS:
//All enabled (a fast check, using shadow memory for the slow check, and the ASAN method for checking values).

//All but ASANCheckInit enabled (a fast check, using shadow memory for the slow check, 
//and the bit-flip method for checking values).

//All but fastCheckInit enabled (only a slow check using shadow memory, and the standard ASAN method for checking values).

//The custom allocator function (useFreeLists) can be activated to ensure that the custom allocator from ASAN is used to
//pre-allocate blocks of memory using freelists. This allows for more control over allocated memory, but induces
//increased run-time overhead, and requires the use of mutexes to ensure the freelist does not induce read/write conflicts or
//undefined behaviour. This also means that the fast check is impossible (since explicit poisoning is 
//disabled automatically).

//Only useRegistration enabled (only a slow check using shadow memory, with the bit-flip method for checking values).
//Only the fastCheckInit enabled (only a fast check based on explicitly poisoned red-zones, without shadow memory).

//Other combinations may result in a non-working framework/library and/or undefined behaviour.
/*------------------------------*/

/*----------------Initialisation Functions----------------*/
int unmapShadowMemory(){
	/* Function is either unnecessary (because this happens automatically on process termination), or useful in
	   the case of a premature termination because of memory failure/etc. */
	if(debug == 0 && shadowMemStart == NULL){
		/* Function called without any shadow memory in-use. */
		return 0;
	}

	if(munmap(shadowMemStart, SIZE) == -1){
		/* Fatal error. */
		return 1;
	}

	return 0;
}

int initShadowMemory(){
	/* Deactivate ASLR on MacOS to make this work. On other platforms, this should work. */
	shadowMemStart = mmap((void*) LOC, SIZE, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED), -1, 0);
	if(shadowMemStart == MAP_FAILED){
		printf("ERROR: FAILED TO PRE-ALLOCATE SPACE IN VIRTUAL MEMORY ADDRESS SPACE.\n");
		return 1;
	}

	return 0;
}

size_t calcRZSize(size_t scale){
	/* The minimum size currently is 32 bytes, where scale N = 5, and the standard size is 128 bytes,
	   with scale N = 7. */
	size_t rz_sz;
	rz_sz = pow(2, scale);

	return rz_sz;
}

int unloadLib(){
	/* Function to destroy the mutexes, and to ensure the library cannot function properly any more. This is only necessary
	   on dynamic library unloads (probably). */
	if(unmapShadowMemory() == 1){
		printf("ERROR: FAILED TO UNMAP SHADOW MEMORY.\n");
		return 1;
	}

	return 0;
}

__attribute__((constructor))
int initLib(){
	/* Function to set up some necessary variables/values. Called upon the use of any (callable) function. */
	if(useRegistration == 0){
		if(initShadowMemory() == 1){
			/* Something went wrong while pre-allocating the virtual memory space required for the
		  	 shadow memory. The program will exit. */
			printf("ERROR: FAILED TO PRE-ALLOCATE SHADOW MEMORY.\n");
			return 1;
		}
	}

	return 0;
}
/*------------------------------*/

/*----------------General Functions----------------*/
void *getShadowMemoryAddress(void *mem){
	if(debug == 0 && mem == NULL){
		return NULL;
	}

	void *shadow;
	shadow = NULL;

	unsigned char* shadowAddrLong;
	shadowAddrLong = (unsigned char*) (((unsigned long long int) mem >> 3) + LOC);
	
	shadow = (void*) shadowAddrLong;

	return shadow;
}

int checkRegistration(void *mem, int accessSize){
	void *shadowAddr;
	shadowAddr = getShadowMemoryAddress(mem);

	unsigned char var;
	var = 0;

	if(debug == 0 && shadowAddr == NULL){
		return -1;
	}

	if(ASANCheckInit == 0){
			/* Perform a check if the value of the shadow byte is 0. If so, the entire 8-byte memory is accessible.
	   		   If the value N is 0 < N < 8, then the first N bytes referenced in the shadow memory are accessible. */
		var = (unsigned char) (*(unsigned int*) shadowAddr);

		if(var != 0){
			/* If the access is not 0, it either means that N bytes are accessible in the 8-byte word referenced to, or
		       that the shadow memory is inaccessible, or that we are checking shadow memory that is unitiliased (and not relevant
		       to our application memory region, i.e., too far out-of-bounds). */
			//	printf("hey: %llu.\n", ((unsigned long long int) mem & 7) + accessSize);
			if(((unsigned long long int) mem & 7) + accessSize > var){
				return 1;
			}else if(var == 255){
				return 1;
			}else if (var == 64){
				/* Previously poisoned shadow memory. Report as being too far out-of-bounds. */
				return 1;
			}else{
				/* If no usable value was read, it means the shadow memory contained usable input, and the access was legal. Therefore, we simply treat this
				   access as an access that was legal. */
				return 0;
			}
		}else{
			if(accessSize > 1){
				void* addedMem;
				addedMem = mem + (size_t) (accessSize - 1);

				var = (unsigned char) (*(unsigned int*) getShadowMemoryAddress(addedMem));

				if(var != 0){
					if(var == 255){
						return 1;
					}else if(var == 64){
						return 1;
					}else{
						if(((unsigned long long int) addedMem & 7) + accessSize > var){
							return 1;
						}
					}
				}				
			}

			/* The value matches 0, meaning the bytes are all addressable. */
			return 0;
		}
	}else{

		if(BIT_TST(shadowAddr, (7 - (unsigned long long int) mem & 7)) != 0){
			return 1;
		}else{
			if(accessSize > 1){
				void* newAccess;
				newAccess = mem + (size_t) (accessSize - 1);

				unsigned char* maxAccess;
				maxAccess = getShadowMemoryAddress(newAccess);
				if(BIT_TST(maxAccess, (7 - (unsigned long long int) newAccess & 7)) != 0){
					return 1;
				}else{
					return 0;
				}
			}else{
				return 0;
			}
		}
	}

	return -1;
}

__attribute__((always_inline, used))
int checkMemoryAccess(void *mem, int accessSize){
	/* Returns a negative integer if the address was invalid, 0 if there was no match, and a positive integer
    if the patterns are equal. */
	if(debug == 0 && mem == NULL){
		return -1;
	}

	int check;
	check = 0;

	if(useRegistration == 0){
		if(fastCheckInit == 0 && useFreeLists == 1){
			/* Perform a fast check. If the pattern matches, do a slow check to see if a pattern match was not simply
	   		   random chance. */
			if(accessSize > 1){
				void *accessMem;
				accessMem = mem + (size_t) (accessSize - 1);

				if((memcmp(mem, &redzone, sizeof(redzone)) != 0) && memcmp(accessMem, &redzone, sizeof(redzone)) != 0){
					return 0;
				}else{
					check = checkRegistration(mem, accessSize);
					if(debug == 0 && check == 1){
						/* A red-zone was indeed accessed. */
						return 1;
					}else if(debug == 0 && check == -1){
						/* Something went wrong with the shadow memory. Probably exit the program? */
						printf("ERROR: SOMETHING WENT WRONG.\n");
						return -1;
					}else{
						/* No red-zone was accessed. */
						return 0;
					}
				}
			}else{
				if(memcmp(mem, &redzone, sizeof(redzone)) != 0){
					return 0;
				}else{
					check = checkRegistration(mem, accessSize);
					if(debug == 0 && check == 1){
						/* A red-zone was indeed accessed. */
						return 1;
					}else if(debug == 0 && check == -1){
						/* Something went wrong with the shadow memory. Probably exit the program? */
						printf("ERROR: SOMETHING WENT WRONG.\n");
						return -1;
					}else{
						/* No red-zone was accessed. */
						return 0;
					}
				}
			}
		}else{
			check = checkRegistration(mem, accessSize);
			if(check == 1){
				/* A red-zone was indeed accessed. */
				return 1;
			}else if(check == -1){
				/* Something went wrong with the shadow memory. Probaby exit the program? */
				printf("ERROR: SOMETHING WENT WRONG.\n");
				return -1;
			}else{
				/* No red-zone was accessed. */
				return 0;
			}
		}
	}else{
		/* In this mode, use only the red-zone pattern as a detection mechanism. False-positives will increase,
		   but so will performance (decrease of run-time overhead). */
		if(accessSize > 1){
			void *accessMem;
			accessMem = mem + (size_t) (accessSize - 1);

			if((memcmp(mem, &redzone, sizeof(redzone)) != 0) && memcmp(accessMem, &redzone, sizeof(redzone)) != 0){
				return 0;
			}
		}else{
			if(memcmp(mem, &redzone, sizeof(redzone)) != 0){
				return 0;
			}

			return 1;
		}
	}

	printf("ERROR: SOMETHING WENT WRONG.\n");
	return -1;
}

int removeAddr(void *memL, void *memR){
	/* Re-set the values of the shadow memory corresponding to the freed memory. */
	void *shadowAddr;
	shadowAddr = getShadowMemoryAddress(memL);

	/* The address of the right red-zone of an object is unrecoverable in most cases for heap allocations, so it is not
	   used in this case. For stack allocations, it can be used. */
	if(debug == 0 && (shadowAddr == NULL || memL == NULL)){
		return 1;
	}

	size_t sz;
	sz = 0;

	void *check;
	check = NULL;

	if(useFreeLists == 0){
		if(debug == 0 && memR == NULL){
			return 1;
		}

		sz = (memR + rz_sz) - memL;

		if(debug == 0 && sz <= 0){
			return 1;
		}
	}else{
		/* NOTE: The function malloc_size() possibly returns more than the original allocation request, due to padding
		   and alignment operations automatically performed by malloc(), etc. In this context, this is no problem, since this
		   memory will simply also be 'poisoned' without any ill effect. */

		/* NOTE: The function malloc_size() cannot be used for stack allocations. Whenever this framework is altered to support
	  	   stack instrumentation, provide some kind of control variable to check for stack allocations. */
		
		sz = malloc_usable_size(memL);
		if(debug == 0 && sz == 0){
			return 1;
		}
	}

	int var;
	var = sz % 8;

	size_t toWriteRZ;
	toWriteRZ = rz_sz / 8;

	size_t toWriteAM;
	toWriteAM = ((sz - 2 * rz_sz) - var) / 8;

	if(var != 0){
		toWriteAM = toWriteAM + 1;
	}

	/* Poison the entire shadow memory. This will be undone if a new memory allocation will overwrite this
	   poisoning. The way we poison memory in both the standard ASAN and alternative way of registration is the same,
	   we set it to -1 (which, unsigned, is 255) which flips all bits to 1. */
	check = memset((unsigned char*) shadowAddr, 64, ((2 * toWriteRZ) + toWriteAM));	
	if(debug == 0 && check == NULL){
		return 1;
	}

	return 0;
}

int registerAddr(void *memL, void *memR){
	/* Set the values of the shadow memory corresponding to the new allocation. */
	void *shadowAddrL;
	shadowAddrL = getShadowMemoryAddress(memL);

	/* In principle, the right address is never needed, but this exists solely for compatibility with
	   the hash table library. */
	void *shadowAddrR;
	shadowAddrR = getShadowMemoryAddress(memR);

	void *shadowAddrRegion;
	shadowAddrRegion = getShadowMemoryAddress(memL + rz_sz);

	void *check;
	check = NULL;

	if(debug == 0 && (shadowAddrL == NULL || shadowAddrR == NULL || shadowAddrRegion == NULL)){
		return 1;
	}

	size_t sz;
	sz = (size_t) (((unsigned int) memR - (unsigned int) memL) - rz_sz);
	if(debug == 0 && sz <= 0){
		return 1;
	}

	size_t sz_rem;
	sz_rem = 0;

	int var;
	var = sz % 8;
	if(var != 0){
		sz_rem = (size_t) var;
	}

	size_t toWriteRZ;
	toWriteRZ = rz_sz / 8;

	size_t toWriteAM;
	toWriteAM = (sz - sz_rem) / 8;

	/* Write the shadow memory of the left red-zone. */
	check = memset((unsigned char*) shadowAddrL, (unsigned char) 0xFF, toWriteRZ);
	if(debug == 0 && check == NULL){
		return 1;
	}

	check = NULL;

	/* Write the shadow memory of the right red-zone. */
	check = memset((unsigned char*) shadowAddrR, (unsigned char) 0xFF, toWriteRZ);
	if(debug == 0 && check == NULL){
		return 1;
	}

	check = NULL;

	/* Write the shadow memory of the addressable memory region. */
	check = memset((unsigned char*) shadowAddrRegion, (unsigned char) 0, toWriteAM);
	if(debug == 0 && check == NULL){
		return 1;
	}

	check = NULL;

	if(ASANCheckInit == 0){
		if(sz_rem != 0){
			check = memset((unsigned char*) (shadowAddrRegion + toWriteAM), (unsigned char) sz_rem, 1);
			if(debug == 0 && check == NULL){
				return 1;
			}
		}
	}else{
		if(sz_rem != 0){
			/* Calculate the required value to be in memory for checking the set/non-set bits. */
			unsigned char avar;
			avar = 0xFF >> (sz_rem);

			/* Write the shadow memory of the last shadow byte to the maximal value (i.e., all bits set). */
			check = memset((unsigned char*) (shadowAddrRegion + toWriteAM), avar, 1);
			if(debug == 0 && check == NULL){
				return 1;
			}
		}
	}

	return 0;
}

int insertRZPattern(void *mem, size_t size){
	/* Use a custom size for global, stack, or static objects (specified, customisable). If the size
	   is either 0 (standard whenever not using a custom size) or not a multiple of 8, also use
	   the standard size (red-zone size, defined by scale). */
	int amount;
	if(size == 0 || size % 8 != 0){
		amount = rz_sz;
	}else{
		amount = size;
	}

	void *check;
	check = NULL;

	check = memset(mem, redzone, amount);
	if(debug == 0 && check == NULL){
		return 1;
	}

	return 0;
}
/*--------------------------------*/

/*----------------Custom Allocator Functions----------------*/
int getFreeListArrayIndex(size_t sz){
	int index;
	index = -1;

	int var;
	var = sz % 8;
	if(var != 0){
		sz = sz + (8 - var);
	}

	/* We subtract 3 from the index because the sizes 1, 2, and 4 all are rounded up to 8, and require no
	   separate freelists. This means that a freelist for memory regions with a size of 8 bytes is stored
	   on the first index of the array (index 0). */
	index = (log(sz) / log(2)) - 3;

	return index;
}

memSeg *createBlock(void *start, size_t originalsz){
	memSeg *new;
	new = NULL;

	new = (memSeg*) malloc(sizeof(struct memSeg));
	if(debug == 0 && new == NULL){
		return NULL;
	}

	new->next = NULL;
	new->allocsz = originalsz;

	new->startAddrRegion = start;
	new->startAddrL = start - rz_sz;
	new->startAddrR = start + originalsz;

	return new;
}

int returnBlockToFreeList(void *mem, int startAdd){
	size_t allocation_sz;
	allocation_sz = 0;

	int index;
	index = 0;
	
	int check;
	check = -1;

	memSeg *new;
	new = NULL;

	size_t *szgetter;
	szgetter = (size_t *) mem;

	szgetter--;
	allocation_sz = *(size_t*) szgetter;

	index = getFreeListArrayIndex(allocation_sz);

	new = createBlock(mem, allocation_sz);
	if(debug == 0 && new == NULL){
		return 1;
	}

	if(memoryArray[index].counter == 0 && memoryArray[index].first == NULL){
		memoryArray[index].first = new;
	}else{
		new->next = memoryArray[index].first;
		memoryArray[index].first = new;
	}

	memoryArray[index].counter++;

	if(startAdd == 0){
		check = removeAddr(new->startAddrL, new->startAddrR);
		if(debug == 0 && check == 1){
			free(new);
			return -1;
		}
	}

	return 0;
}

void *getBlockFromFreeList(size_t sz){
	int index;
	index = getFreeListArrayIndex(sz);

	void *shadowMem;
	shadowMem = NULL;

	int check;
	check = -1;

	/* Check if the free-list contains memory blocks large enough for the memory allocation. */
	if(debug == 0 && (!(memoryArray[index].size >= sz))){
		return NULL;
	}

	memSeg *toReturn;
	toReturn = memoryArray[index].first;

	if(debug == 0 && toReturn == NULL){
		/* The list was empty, and was incorrectly called for a new block. */
		return NULL;
	}

	void *mem;
	mem = NULL;
	
	if(!(toReturn->next == NULL)){
		/* The list is non-empty. */
		memoryArray[index].first = toReturn->next;
	}else{
		/* The list is now empty. */
		memoryArray[index].first = NULL;
	}

	memoryArray[index].counter--;

	/* Save the start of the actual memory region. */
	mem = toReturn->startAddrRegion;

	/* We would insert the red-zone poison patterns. This is normally done here, to ensure that not in-use
	   memory that has been pre-allocated is not in-use, but due to allocation data being required, we 
	   cannot use the fast check. */

	check = registerAddr(toReturn->startAddrL, toReturn->startAddrR);
	if(debug == 0 && check == 1){
		free(toReturn);
		return NULL;
	}

	/* Free the data structure used to keep track of the entry in the free-list, since it will now be in-use. */
	free(toReturn);

	return mem;
}

int checkFreeListArray(size_t sz){
	int index;
	index = getFreeListArrayIndex(sz);

	if(debug == 0 && (!(index >= 0 && index <= MEMORY_LIST))){
		return 1;
	}

	//What if list not yet created, counter not initialised yet? Therefore, counter not taken into account.
	if(memoryArray[index].first == NULL){
		/* List is empty, either because all memory is in use, or no memory was pre-allocated yet. */
		return 2;
	}

	return 0;
}

int setFreeList(void *blockStart, int index, size_t sz){
	void *ptr;
	ptr = blockStart + rz_sz;

	memSeg *current;
	current = NULL;

	memSeg *prev;
	prev = NULL;

	size_t counter;
	counter = 0;

	int check;
	check = -1;

	void *secCheck;
	secCheck = NULL;

	size_t *szsetter;
	szsetter = NULL;

	/*Build in functionality to free entire list if stuff goes wrong. Like a function that loops over list and frees everything.	*/
	for(size_t x = standardPreAllocSize; x > 0; x--){
		szsetter = (size_t*) ptr;
		szsetter--;

		/* The size of the size_t type is assumed to be 8 here for compiler optimisations. */
		secCheck = memmove(szsetter, &sz, 8);
		if(debug == 0 && secCheck == NULL){
			return 1;
		}

		szsetter = NULL;

		check = returnBlockToFreeList(ptr, 1);
		if(check == 1){
			return 1;
		}

		check = -1;
		ptr = ptr + memoryArray[index].size + rz_sz;
	}

	return 0;
}

int allocateFreeList(size_t sz){
	int index;
	index = 0;

	int check;
	check = -1;

	size_t newsz;
	newsz = 0;

	void *blockStart;
	blockStart = NULL;

	/* Get index that corresponds to the given (padded) size. */
	index = getFreeListArrayIndex(sz);

	if(debug == 0 && (!(index >= 0 && index <= MEMORY_LIST))){
		return 1;
	}

	/* Set all information for this free-list. */
	memoryArray[index].first = NULL;
	memoryArray[index].counter = 0;
	memoryArray[index].size = sz;

	/* The complete block will n times the allocated size, with n + 1 times the red-zone with it. This is
	   done because every block will have a left and right red-zone, but a right red-zone for memory region n - 1, will be
	   the left red-zone of a memory region n, and so forth. */
	newsz = (sz * standardPreAllocSize) + (rz_sz * (standardPreAllocSize + 1));

	/* The mmap() function allocates a contiguous piece of memory of a certain size starting on a certain address. The shadow memory
	   addresses must be determined as well, and must be mapped to it. */
	blockStart = mmap(NULL, newsz, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
	if(blockStart == MAP_FAILED){
		printf("ERROR: CANNOT PRE-ALLOCATE SPACE FOR FREELIST IN VIRTUAL MEMORY ADDRESS SPACE.\n");
		return 1;
	}

	/* Ready the free-list for actual use. */
	check = setFreeList(blockStart, index, sz);
	if(debug == 0 && (check == 1 || check == -1)){
		return 1;
	}

	return 0;
}
/*--------------------------------*/
/* This function assumes alignment is in order when freeing memory. */
__attribute__((used))
void Dlib_free(void *mem){
	if(debug == 0 && !mem){
		return;
	}

	if(useFreeLists == 0){
		if(returnBlockToFreeList(mem, 0) == 1){
			/* Memory was corrupted, and should be removed manually. */
			/* This, however, is not very possible at this moment. How to do this? */
			
			printf("ERROR: MEMORY CORRUPTION OCCURRED. MEMORY REGION LOST.");
			return;
		}
	}else{
		/* Return the pointer to the actual start of the contiguous memory region. */
		mem = mem - rz_sz;

		/* Remove the red-zones and object memory region from the shadow memory. The right red-zone in this case is
	  	   not useful, but only exists for compatibility purposes. */
		if(removeAddr(mem, NULL) == 1){
			free(mem);
			return;
		}

		free(mem);
	}

	return;
}

__attribute__((used))
void *Dlib_malloc(size_t sz){
	/* Defined behaviour, just return NULL on a 0-byte allocation. It can also return a (valid) pointer to
	   memory with 0 bytes (which is impossible to dereference), but this placeholder is fine for now. */
	if(sz <= 0){
		return NULL;
	}

	if(useFreeLists == 0 && useRegistration == 0){
		void *toReturn;
		toReturn = NULL;

		int check;
		check = -1;

		int var;
		var = 0;

		/* Add padding bytes to ensure all memory is a multiple of 8 (to ensure shadow mapping). */
		var = sz % 8;
		if(var != 0){
			sz = sz + (8 - var);
		}

		if(!((sz != 0) && ((sz & (sz - 1)) == 0))){
			sz = (unsigned) pow(2, ceil(log(sz) / log(2)));
		}

		check = checkFreeListArray(sz);
		if(check == 2){
			if(allocateFreeList(sz) == 1){
				printf("ERROR: FAILED TO PRE-ALLOCATE NEW FREELIST.\n");
				return NULL;
			}
		}else if(debug == 0 && check == 1){
			/* Something went wrong while checking the array. */
			return NULL;
		}

		toReturn = getBlockFromFreeList(sz);
		if(debug == 0 && toReturn == NULL){
			printf("ERROR: FAILED TO RETRIEVE BLOCK FROM FREELIST.\n");
			return NULL;
		}

		return toReturn;
	}else{
		size_t ac_sz;
		ac_sz = sz + (2 * rz_sz);

		void *mem;
		mem = NULL;

		mem = malloc(ac_sz);
		if(mem == NULL){
			return NULL;
		}

		if(fastCheckInit == 0){
			/* Insert pattern (i.e., poison values) into the left red-zone. */
			if(insertRZPattern(mem, 0) == 1){
				free(mem);
				return NULL;
			}

			/* Insert pattern into the right red-zone. */
			if(insertRZPattern(mem + rz_sz + sz, 0) == 1){
				free(mem);
				return NULL;
			}
		}

		if(useRegistration == 0){
			/* Put the red-zone address into red-zone table. */
			if(registerAddr(mem, mem + rz_sz + sz) == 1){
				printf("ERROR: REGISTRATION DENIED.\n");
				free(mem);
				return NULL;
			}
		}

		/* Re-align pointer to the address where the actual application memory starts. */
		mem = mem + rz_sz;

		return mem;
	}

	return NULL;
}

__attribute__((used))
void *Dlib_realloc(void *mem, size_t nsz){
	if(mem == NULL && nsz <= 0){
		/* Naturally undefined behaviour, so solve by returning NULL for now. */
		return NULL;
	}

	void *newmem;
	newmem = NULL;

	if(mem == NULL && nsz > 0){
		newmem = Dlib_malloc(nsz);
		if(newmem == NULL){
			return NULL;
		}

		return newmem;
	}else if(mem != NULL && nsz > 0){
		/* Move pointer back to original malloc() address. */
		mem = mem - rz_sz;

		if(mem == NULL){
			return NULL;
		}

		/* If fails, it does not free the old memory region provided (otherwise this would result in data loss). */
		newmem = Dlib_malloc(nsz);
		if(newmem == NULL){
			return NULL;
		}

		size_t oldsz;
		oldsz = 0;

		size_t actual;
		actual = 0;

		if(useFreeLists == 0){
			size_t *getsz;
			getsz = (size_t*) (mem + rz_sz);

			getsz--;
			oldsz = *getsz;
		}else{
			oldsz = malloc_usable_size(mem);
			oldsz = oldsz - (2 * rz_sz);
		}

		if(oldsz == 0){
			Dlib_free(newmem);
			return NULL;
		}

		/* This is the POSIX-Standard. Could provide undefined behaviour in some cases. */
		if(oldsz <= nsz){
			actual = oldsz;
		}else{
			actual = nsz;
		}

		/* If a memory move fails, free the new region and keep the old one to prevent data loss. */
		if(memmove(newmem, (mem + rz_sz), actual) == NULL){
			Dlib_free(newmem);
			return NULL;
		}

		mem = mem + rz_sz;
		Dlib_free(mem);
	}

	return newmem;
}

__attribute__((used))
void *Dlib_calloc(size_t num, size_t sz){
	if(sz <= 0){
		return NULL;
	}

	size_t size;
	size = num * sz;

	void *mem;
	mem = NULL;

	mem = Dlib_malloc(size);
	if(mem == NULL){
		return NULL;
	}

	/* Set all bytes to contain 0 in the memory region. */
	if(memset(mem, 0, size) == NULL){
		Dlib_free(mem);
		return NULL;
	}

	return mem;
}

__attribute__((used))
void *Dlib_memalign(size_t alignment, size_t sz){
	if(useFreeLists == 0){
		printf("WARNING: FUNCTION 'memalign' NOT INSTRUMENTED.\n");
		printf("ERROR: DEACTIVATE INSTRUMENTATION FOR FUNCTION 'memalign' WHEN USING THE CUSTOM MEMORY ALLOCATOR.\n");
		return NULL;
	}

	void *mem;
	mem = NULL;

	size_t newsz;
	newsz = sz + (2 * rz_sz);

	if(posix_memalign(&mem, alignment, newsz) != 0){
		return NULL;
	}else if(mem == NULL){
		return NULL;
	}

	if(fastCheckInit == 0){
		/* Insert pattern (i.e., poison values) into the left red-zone. */
		if(insertRZPattern(mem, 0) == 1){
			free(mem);
			return NULL;
		}

		/* Insert pattern into the right red-zone. */
		if(insertRZPattern(mem + rz_sz + sz, 0) == 1){
			free(mem);
			return NULL;
		}
	}

	if(useRegistration == 0){
		/* Put the red-zone address into red-zone table. */
		if(registerAddr(mem, mem + rz_sz + sz) == 1){
			printf("ERROR: REGISTRATION DENIED.\n");
			free(mem);
			return NULL;
		}
	}

	/* Re-align pointer to the address where the actual application memory starts. */
	mem = mem + rz_sz;

	return mem;
}
