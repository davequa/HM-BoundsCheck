#include <noinstrument.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>

#define AMOUNT_OF_LOCKS 2

#define HASHSZ 4096

#define checkMemoryAccess NOINSTRUMENT(checkMemoryAccess)

#define Dlib_free NOINSTRUMENT(Dlib_free)
#define Dlib_malloc NOINSTRUMENT(Dlib_malloc)
#define Dlib_realloc NOINSTRUMENT(Dlib_realloc)
#define Dlib_calloc NOINSTRUMENT(Dlib_calloc)
#define Dlib_memalign NOINSTRUMENT(Dlib_memalign)

#define checkRegistration NOINSTRUMENT(checkRegistration)

//#define calcRZSize NOINSTRUMENT(calcRZSize)

//#define unloadLib NOINSTRUMENT(unloadLib)
#define initLib NOINSTRUMENT(initLib)

#define getRZAddrBucket NOINSTRUMENT(getRZAddrBucket)
#define removeAddrFromList NOINSTRUMENT(removeAddrFromList)
#define removeAddr NOINSTRUMENT(removeAddr)

#define addAddrToList NOINSTRUMENT(addAddrToList)
#define registerAddr NOINSTRUMENT(registerAddr)

#define insertRZPattern NOINSTRUMENT(insertRZPattern)

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

//Originally used for the size of the shadow memory. A left-over from the ASAN implementation, but now used to calculate
//the red-zone size dynamically. For scale value N, the red-zone size will be 2 ^ N (e.g., N = 3, 2 ^ 3 = 8 bytes).
//The minimum size of this scale value is 3, where the red-zone is 8 bytes. There is, in theory, no maximum size,
//but the largest (recommended) size is 10 (for a red-zone size of 1024 bytes).
static const size_t scale = 5;

//New redzone pattern, simply an unsigned character with a size 8 bits. The maximum value (0xFF) was chosen
//for this, but can be changed to anything (as long as it is 8 bits/1 byte long).
unsigned char redzone = 0x2A;

//Size of the red-zone, determined by the scale variable.
static size_t rz_sz = 32;

rzHashBucket hashTable[HASHSZ];

static const int hashexp = 12;

//Used to create a mapping from virtual memory addresses to the hash table buckets.
static unsigned long int pagesz = 0;

/*----------------Environment Variables----------------*/

//SET TO 0 TO ENABLE!
//SET TO 1 TO DISABLE!

//Variable for activating red-zone poison pattern checking. Deactivating this option will also disable all
//red-zone pattern insertion, meaning red-zones will no longer be explicitly poisoned. Only the 'slow' check will be used here,
//resulting in a performance decrease, but an increase in memory efficiency (since the red-zones will no longer be intialised).
static const int fastCheckInit = 0;

//Variable for enabling hash table registration. If disabled, registration will be turned off, and only the explicit poisoning of
//the red-zones will be used for the detection of memory errors. This will increase the amount of false-positives experienced,
//but will probably increase performance as well. 
static const int useRegistration = 0;

//This debug variable can be turned on to display error messages, or informative notes to show the user what things are going wrong.
//When turned off, runtime is significantly lower.
static const int debug = 1;

//OPTIONS:
//Both enabled (a fast check, using a hash table for the slow check).
//Only the hash table enabled (only a slow check, no explicitly poisoned red-zones).
//Only the explicit poisoning enabled (only a fast check, no hash table for checking pattern matches).

//Other combinations may result in a non-working framework/library and/or undefined behaviour.
/*------------------------------*/

/*----------------Initialisation Functions----------------*/
size_t calcRZSize(size_t scale){
	/* The minimum size currently is 32 bytes, where scale N = 5, and the standard size is 128 bytes,
	   with scale N = 7. */
	if(debug == 0 && scale == 0){
		return 0;
	}

	size_t rz_sz;
	rz_sz = pow(2, scale);

	return rz_sz;
}

int unloadLib(){
	/* Function to destroy the mutexes, and to ensure the library cannot function properly any more. This is only necessary
	   on dynamic library unloads (probably). */
	return 0;
}

__attribute__((constructor))
int initLib(){
	/* Function to set up some necessary variables/values. Called upon the use of any (callable) function. */
	if(useRegistration == 0){
		pagesz = sysconf(_SC_PAGESIZE);
		if(pagesz == 0){
			return 1;
		}
	}

	if(debug == 0){
		if(fastCheckInit == 0){
			printf("Fast Check: ACTIVATED\n");		
		}else{
			printf("Fast Check: DEACTIVATED\n");
		}

		if(useRegistration == 0){
			printf("Memory Registration: ACTIVATED\n");
		}else{
			printf("Memory Registration: DEACTIVATED\n");
		}
	}

	return 0;
}


int getRZAddrBucket(void *mem){
	/* Get most significant n bits from the address. */
	unsigned long int pageNum;
	pageNum = ((unsigned long int) mem >> hashexp);

	int bucket;
	bucket = (((pageNum) ^ ((pageNum) >> 8) ^ ((pageNum) >> 16) ^ ((pageNum) >> 24)) & (HASHSZ -1));

	return bucket;
}

int checkRegistration(void *mem, int accessSize){
	int rzBucket;
	rzBucket = getRZAddrBucket(mem);

	if(hashTable[rzBucket].counter == 0){
		/* No red-zones registered, so addressable memory. */
		return 0;
	}

	/* If memory is not at all in the range between the first and last entries, it means the address
	   is nowhere in the list (we can do this because the list is ordered). */
	if(hashTable[rzBucket].first->startAddrL > mem){
		/* The memory address given is smaller than the left-most red-zone. */
		return 0;
	}else if((hashTable[rzBucket].last->startAddrR + rz_sz) < mem){
		/* The memory address given is greater than the right-most red-zone. */
		return 0;
	}

	void *addedMem;
	addedMem = 0;

	if(accessSize > 1){
		addedMem = mem + (size_t) (accessSize - 1);
	}else{
		addedMem = mem;
	}

	for(rzAddr *current = hashTable[rzBucket].first; current != (rzAddr*) NULL; current = current->next){
		if((current->startAddrL <= mem && mem < current->startAddrL + rz_sz) || (current->startAddrR <= mem && mem < current->startAddrR + rz_sz) ||
			(current->startAddrL <= addedMem && addedMem < current->startAddrL + rz_sz) || (current->startAddrR <= addedMem && addedMem < current->startAddrR + rz_sz)){
			printf("ERROR: ATTEMPTING TO ACCESS UNADDRESSABLE (REDZONE) MEMORY.\n");
			return 1;
		}else{
			continue;
		}
	}

	/* Memory address was not found in hash table, meaning it is addressable (or there is a very
	   bad memory problem). */
	return 0;
}
/*--------------------------------*/
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
		if(fastCheckInit == 0){
			/* Perform a fast check. If the pattern matches, do a slow check to see if a pattern match was not simply
	   		   random chance. */
			if(accessSize > 1){
				void *accessMem;
				accessMem = mem + (size_t) (accessSize - 1);

				if((memcmp(mem, &redzone, 1) != 0) && memcmp(accessMem, &redzone, 1) != 0){
					return 0;
				}else{
					check = checkRegistration(mem, accessSize);
					if(check == 1){
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
				if(memcmp(mem, &redzone, 1) != 0){
					return 0;
				}else{
					check = checkRegistration(mem, accessSize);
					if(check == 1){
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
			}else if(debug == 0 && check == -1){
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

			if((memcmp(mem, &redzone, 1) != 0) && memcmp(accessMem, &redzone, 1) != 0){
				return 0;
			}

			return 1;
		}else{
			if(memcmp(mem, &redzone, 1) != 0){
				return 0;
			}

			return 1;
		}
	}

	if(debug == 0){
		printf("ERROR: SOMETHING WENT WRONG WITH THE MEMORY CHECK.\n");
	}

	return -1;
}

rzAddr *removeAddrFromList(rzAddr *toRemove, int rzBucket){
	int found;
	found = -1;

	rzAddr *current;
	current = NULL;

	rzAddr *prev;
	prev = NULL;

	if(hashTable[rzBucket].counter == 0){
		printf("ERROR: NO REDZONES REGISTERED.\n");
		return NULL;
	}else{
		for(current = hashTable[rzBucket].first; current != (rzAddr*) NULL; current = current->next){
			if((current->startAddrL == toRemove->startAddrL) || (current->startAddrR == toRemove->startAddrR)){
				found = 0;

				if(current->next == NULL){
					if(prev == NULL){
						hashTable[rzBucket].first = NULL;
						hashTable[rzBucket].last = NULL;

						break;
					}else{
						prev->next = NULL;
						hashTable[rzBucket].last = prev;

						break;
					}
				}else if(prev == NULL){
					hashTable[rzBucket].first = current->next;

					break;
				}else{
					prev->next = current->next;

					break;
				}
			}

			/* Save the current entry, for list arithmetic. */
			prev = current;
		}

		hashTable[rzBucket].counter--;
	}

	
	if(found != 0){
		printf("ERROR: NO REDZONE FOUND FOR REMOVAL.\n");
		return NULL;
	}else{
		return current;
	}
}

int removeAddr(void *memL, void *memR){
	rzAddr *current;
	current = NULL;

	rzAddr *crossCurrent;
	crossCurrent = NULL;

	int lrzBucket;
	lrzBucket = -1;

	int rrzBucket;
	rrzBucket = -1;

	int addrLock;
	addrLock = -1;

	int standardCrossAlloc;
	standardCrossAlloc = -1;

	int memRUnknown;
	memRUnknown = -1;

	int firstAddrLock;
	firstAddrLock = 0;

	int secondAddrLock;
	secondAddrLock = 0;

	if(debug == 0 && memL == NULL){
		/* We always require the left red-zone address to perform a remove. The address of the right red-zone is
		   optional. */
		printf("ERROR: INVALID STARTING ADDRESS PROVIDED.\n");
		return 1;
	}else if(memR == NULL){
		/* This means that the right red-zone address is unknown on deallocation. This is possible for any free
		   since we cannot extract the right red-zone address directly from the pointer given to free(). */
		lrzBucket = getRZAddrBucket(memL);
		memRUnknown= 0;
	}else{
		lrzBucket = getRZAddrBucket(memL);
		rrzBucket = getRZAddrBucket(memR);
		if(lrzBucket != rrzBucket){
			/* A cross-page allocation occurred, meaning that we require this entry to be saved in different buckets
			   at the same time. */
			standardCrossAlloc = 0;
		}
	}
	
	/* The very first bucket is 0, the very last bucket is 255. */
	if(debug == 0 && (!(lrzBucket >= 0 && lrzBucket < HASHSZ))){
		return 1;
	}

	if(memRUnknown != 0){
		if(debug == 0 && (!(rrzBucket >= 0 && rrzBucket < HASHSZ))){
			return 1;
		}
	}

	/* Used to find list entry, and to remove and deallocate it from the list and memory. */
	rzAddr toRemove;
	toRemove.startAddrL = memL;
	toRemove.startAddrR = memR;

	if(memRUnknown == 0){
		current = removeAddrFromList(&toRemove, lrzBucket);
		if(debug == 0 && current == NULL){
			return 1;
		}

		/* Recover the right red-zone address from the left red-zone entry (from the known bucket), and
		   use it to now find the other entry in the different bucket (if a cross-page allocation occurred). */
		if(debug == 0 && current->startAddrR == NULL){
			/* The memory was recorded as only having a left red-zone. Therefore, we perform no additional steps.
			   This, technically, is possible, but should not happen. We error, but do not exit, since it
			   is not a fatal error. */

			printf("ERROR: NO RIGHT RED-ZONE RECORDED.\n");
			free(current);
			return 1;
		}else{
			toRemove.startAddrR = current->startAddrR;
		}
		
		rrzBucket = getRZAddrBucket(toRemove.startAddrR);

		if(debug == 0 && (!(rrzBucket >= 0 && rrzBucket < HASHSZ))){
			free(current);

			return 1;
		}

		/* If the buckets are different, this means a cross-page allocation was performed. Then, the
		   entry must be removed from that specific bucket as well. If they are equal, the entry can simply be deallocated,
		   meaning that the removal was successful. */
		if(rrzBucket != lrzBucket){
			crossCurrent = removeAddrFromList(&toRemove, rrzBucket);
			if(debug == 0 && crossCurrent == NULL){
				free(current);

				return 1;
			}

			/* Free the cross-allocated entry which originates from a different bucket. */
			free(crossCurrent);
		}
	}else if(standardCrossAlloc == 0){
		current = removeAddrFromList(&toRemove, lrzBucket);
		if(debug == 0 && current == NULL){
			return 1;
		}

		crossCurrent = removeAddrFromList(&toRemove, rrzBucket);
		if(debug == 0 && crossCurrent == NULL){
			free(current);

			return 1;
		}

		/* Free the cross-allocated entry which originates from a different bucket. */
		free(crossCurrent);
	}else{
		/* This branch is used if and only if the left and right red-zone buckets match, meaning only the
		   left bucket can be used for removing the entry (since the list referred to by both buckets is the same). */
		current = removeAddrFromList(&toRemove, lrzBucket);
		if(debug == 0 && current == NULL){
			return 1;
		}
	}

	/* Free the removed entry from the hash table list. */
	free(current);

	return 0;
}

int addAddrToList(rzAddr *toAdd, int rzBucket){
	rzAddr *current;
	current = NULL;

	rzAddr *prev;
	prev = NULL;

	if(hashTable[rzBucket].counter == 0 && hashTable[rzBucket].first == NULL){
		/* The new entry will become the first. */
		hashTable[rzBucket].first = toAdd;
		hashTable[rzBucket].last = toAdd;
	}else if(hashTable[rzBucket].last->startAddrR < toAdd->startAddrL){
		/* It was observed that addresses are often allocated increasingly (in size). Therefore, starting at the end with
		   adding is more efficient (with this additional branch). */
		hashTable[rzBucket].last->next = toAdd;
		hashTable[rzBucket].last = toAdd;
	}else{
		for(current = hashTable[rzBucket].first; current != (rzAddr*) NULL; current = current->next){
			/* Compare the red-zone entry. */
			if((current->startAddrL == toAdd->startAddrL) || (current->startAddrR == toAdd->startAddrR)){
				/* Red-zone (or part of it) already in the table. */
				printf("ERROR: REDZONE ALREADY REGISTERED.\n");
				return 1;
			}else if(toAdd->startAddrR < current->startAddrL){
				if(prev == NULL){
					hashTable[rzBucket].first = toAdd;
					toAdd->next = current;

					break;
				}else{
					prev->next = toAdd;
					toAdd->next = current;

					break;
				}
			}

			/* Save the previous entry. */
			prev = current;
		}
	}

	hashTable[rzBucket].counter++;

	return 0;
}

int registerAddr(void *memL, void *memR){
	if(memL == NULL && memR == NULL){
		return 1;
	}

	rzAddr *toAdd;
	toAdd = NULL;

	/* Only exists for cross-page allocations. If the same pointer is used in different lists, list arithmetic
	   cannot be trusted. */
	rzAddr *crossToAdd;
	toAdd = NULL;

	int lrzBucket;
	lrzBucket = -1;

	int rrzBucket;
	rrzBucket = -1;

	int crossAlloc;
	crossAlloc = -1;

	int check;
	check = -1;

	if(debug == 0 && (memL == NULL || memR == NULL)){
		/* Requires two valid red-zone addresses for a register. */
		return 1;
	}

	lrzBucket = getRZAddrBucket(memL);
	rrzBucket = getRZAddrBucket(memR);

	if(lrzBucket != rrzBucket){
		/* A cross-page allocation occurred, meaning that we require this entry to be saved in different buckets
		   at the same time. */
		crossAlloc = 0;
	}
	
	/* The very first bucket is 0, the very last bucket is 255. Check if the selected bucket(s) are valid. */
	if(debug == 0 && (!(lrzBucket >= 0 && lrzBucket < HASHSZ))){
		return 1;
	}

	if(debug == 0 && (!(rrzBucket >= 0 && rrzBucket < HASHSZ))){
		return 1;
	}

	toAdd = (rzAddr*) malloc(sizeof(struct rzAddr));
	if(toAdd == NULL){
		return 1;
	}

	toAdd->startAddrL = memL;
	toAdd->startAddrR = memR;
	toAdd->next = NULL;

	if(crossAlloc == 0){
		check = addAddrToList(toAdd, lrzBucket);
		if(debug == 0 && (check == 1 || check == -1)){
			free(toAdd);
			return 1;
		}

		check = -1;

		crossToAdd = (rzAddr*) malloc(sizeof(struct rzAddr));
		if(debug == 0 && crossToAdd == NULL){
			return 1;
		}

		crossToAdd->startAddrL = memL;
		crossToAdd->startAddrR = memR;
		crossToAdd->next = NULL;

		check = addAddrToList(crossToAdd, rrzBucket);		
		if(debug == 0 && (check == 1 || check == -1)){
			/* If this occurs, everything will break. */
			free(crossToAdd);
			
			if(removeAddr(memL, memR) == 1){
				/* Fatal memory error. */
			}

			return 1;
		}
	}else{
		check = addAddrToList(toAdd, lrzBucket);
		if(debug == 0 && (check == 1 || check == -1)){
			free(toAdd);

			return 1;
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

	if(memset(mem, redzone, amount) == NULL){
		return 1;
	}

	return 0;
}

/* This function assumes alignment is in order when freeing memory. */
__attribute__((used))
void Dlib_free(void *mem){
	if(mem == NULL){
		return;
	}

	/* Return the pointer to the actual start of the contiguous memory region. */
	mem = mem - rz_sz;

	/* Remove the left and right red-zone entry from the red-zone hash table register. The right red-zone address is not
	   explicitily recoverable. However, the left red-zone address suffices, since all blocks of red-zone entries 
	   keep both the left and right starting addresses of its respective red-zones. */
	
	if(useRegistration == 0){
		if(removeAddr(mem, NULL) == 1){
			printf("ERROR: FAILED TO REMOVE ADDRESS FROM HASH TABLE.\n");
			free(mem);
			return;
		}
	}

	free(mem);

	return;
}

__attribute__((used))
void *Dlib_malloc(size_t sz){
	/* Defined behaviour, just return NULL on a 0-byte allocation. It can also return a (valid) pointer to
	   memory with 0 bytes (which is impossible to dereference), but this placeholder is fine for now. */
	if(sz <= 0){
		return NULL;
	}

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
		}
	}

	/* Re-align pointer to the address where the actual application memory starts. */
	mem = mem + rz_sz;

	return mem;
}

__attribute__((used))
void *Dlib_realloc(void *mem, size_t nsz){
	if(debug == 0 && mem == NULL && nsz <= 0){
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

		if(debug == 0 && mem == NULL){
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

		oldsz = malloc_usable_size(mem);
		oldsz = oldsz - (2 * rz_sz);

		if(debug == 0 && oldsz <= 0){
			Dlib_free(newmem);
			return NULL;
		}

		/* This is the POSIX-Standard. Could provide undefined behaviour in some cases. */
		if(oldsz < nsz){
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
		}
	}

	/* Re-align pointer to the address where the actual application memory starts. */
	mem = mem + rz_sz;

	return mem;
}
