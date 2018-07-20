#include "DlibHash.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <malloc/malloc.h>

/* A simple program for multiple tests to be launched.*/

int test21(){
	char *buffer, *bufferb, *bufferc;
	size_t sz = 2048;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	bufferb = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	bufferc = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	Dlib_free(bufferc);

	bufferc = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	Dlib_free(buffer);
	Dlib_free(bufferb);
	Dlib_free(bufferc);

	return 0;
}

int test20(){
	char *buffer;
	size_t sz = 16;

	buffer = NULL;

	buffer = Dlib_realloc(buffer, sz);

	if(buffer == NULL){
		return 1;
	}else{
		Dlib_free(buffer);
		return 0;
	}
}

int test19(){
	char *buffer;
	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test;

	test = buffer;

	if(checkMemoryAccess(test, 2) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 7;

	if(checkMemoryAccess(test, 2) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	if(checkMemoryAccess(test, 8) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 12;

	if(checkMemoryAccess(test, 2) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 14;

	if(checkMemoryAccess(test, 2) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	if(checkMemoryAccess(test, 4) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 15;

	if(checkMemoryAccess(test, 2) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 13;

	if(checkMemoryAccess(test, 8) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test18(){
	char *buffer;
	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test;

	test = buffer;

	if(checkMemoryAccess(test, 8) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 14;

	if(checkMemoryAccess(test, 4) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test17(){
	char *buffer;
	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test;

	test = buffer - 153;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test16(){
	char *buffer;
	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test;

	test = buffer;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 1;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 15;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 18;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 16;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 129;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 128;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS DETECTED AS IN-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 153;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test15(){
	char *buffer;
	buffer = Dlib_malloc(2);

	char *test;
	test = buffer - 8;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 2;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 4;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test14(){
	size_t sz;
	sz = 1;

	char *buffer;
	buffer = Dlib_malloc(1);

	char *test = buffer;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 1;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test13(){
	size_t sz;
	sz = 64;

	char *buffer;
	buffer = NULL;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char* test;
	test = buffer;

	printf("CHECK: MEMORY REGION START.\n");
	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer - 1;

	printf("CHECK: LEFT BORDER OF LEFT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer;

	printf("CHECK: BORDER OF LEFT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 0) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 1;

	printf("CHECK: RIGHT BORDER OF LEFT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 63;

	printf("CHECK: LEFT BORDER OF RIGHT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 0) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64;

	printf("CHECK: BORDER OF RIGHT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 65;

	printf("CHECK: RIGHT BORDER OF RIGHT RED-ZONE FOR UNDERFLOW DETECTION.\n");
	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 100;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 127;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 128;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 129;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer - 136;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer - 144;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 39;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 25;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 11;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 8;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 3;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 63;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64 + 127;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64 + 128;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64 + 136;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 64 + 144;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test12(){
	char *buffer;
	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test;
	test = buffer - 16;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 153;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 16;

	printf("EDGE OF RED-ZONE DETECTION?\n");
	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 32;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + sz + 1;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + sz;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + sz - 1;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer - 1;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 127;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 128;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer - 129;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 15;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = test + 1;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 15;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	test = buffer + 16;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 16 + 127;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
		return 1;
	}

	test = buffer + 16 + 128;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: TOO FAR OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
	}else{
		printf("INCORRECT: TOO FAR OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test11(){
	char *buffer;
	size_t sz = 64;

	buffer = Dlib_memalign(32, sz);
	if(buffer == NULL){
		return 1;
	}

	char *test = buffer;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 63;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 32;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 48;

	if(checkMemoryAccess(test, 1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer - 32;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = buffer + 65;

	if(checkMemoryAccess(test, 1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test10(){
	char *buffer;
	size_t sz = 10000;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test = buffer;
	test = test + 10000 - 64;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = test + 64;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	test = test + 32;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test9(){
	//Have to set to same bucket to test well for a bit.
	char *buffer, *buffera, *bufferb, *bufferc, *bufferd;
	size_t sz = 64;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	buffera = Dlib_malloc(sz);
	if(buffera == NULL){
		return 1;
	}

	bufferb = Dlib_malloc(sz);
	if(bufferb == NULL){
		return 1;
	}

	Dlib_free(buffera);

	bufferc = Dlib_malloc(sz);
	if(bufferc == NULL){
		return 1;
	}

	Dlib_free(buffer);
	Dlib_free(bufferb);

	bufferd = Dlib_malloc(sz);
	if(bufferd == NULL){
		return 1;
	}

	Dlib_free(bufferc);
	Dlib_free(bufferd);

	buffer = Dlib_malloc(1024);
	if(buffer == NULL){
		return 1;
	}

	char *buffere = Dlib_malloc(512);
	if(buffer == NULL){
		return 1;
	}

	char *buffero = Dlib_malloc(2048);
	if(buffero == NULL){
		return 1;
	}

	Dlib_free(buffer);
	Dlib_free(buffero);
	Dlib_free(buffere);

	return 0;

}

int test8(){
	char *buffer;
	buffer = NULL;

	size_t sz = 64;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	char *test = buffer;
	test = test + 32;

	if(checkMemoryAccess(test, -1) == 1){
		printf("INCORRECT: IN-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}else{
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED AS IN-BOUNDS.\n");
	}

	test = test + 64;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		return 1;
	}

	test = test + 200;

	if(checkMemoryAccess(test, -1) == 1){
		printf("INCORRECT: FAR-OUT-OF-BOUNDS ACCESS DETECTED AS OUT-OF-BOUNDS.\n");
		return 1;
	}else{
		printf("CORRECT: MEMORY TOO FAR OUT-OF-BOUNDS NOT DETECTED.\n");
	}

	test = test - 200 - 64 - 32 - 80;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test7(){
	void *buffer;
	buffer = NULL;

	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	void *check;
	check = NULL;

	//changed to 1-byte pattern for additional checking.
	check = memmove(buffer, &redzone, sizeof(redzone));
	if(check == NULL){
		Dlib_free(buffer);
		return 1;
	}

	printf("WARNING: MUST PERFORM SLOW-CHECK, RED-ZONE PATTERN MANUALLY PLACED IN ADDRESSABLE MEMORY.\n");
	if(checkMemoryAccess(buffer, -1) == 0){
		printf("CORRECT: ADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test6(){
	void *buffer;
	buffer = NULL;

	size_t sz = 16;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	void *check;
	check = NULL;

	check = memmove(buffer, &redzone, sizeof(redzone));
	if(check == NULL){
		Dlib_free(buffer);
		return 1;
	}

	if(checkMemoryAccess(buffer, -1) == 0){
		printf("CORRECT: ADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int test5(){
	size_t sz;
	sz = 32;

	int *buffer;
	buffer = NULL;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	if(checkMemoryAccess(buffer, -1) == 0){
		printf("CORRECT: ADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	buffer = buffer - 32;

	if(checkMemoryAccess(buffer, -1) == 1){
		printf("CORRECT: UNADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	buffer = buffer + 32;

	buffer = buffer + 32;

	if(checkMemoryAccess(buffer, -1) == 1){
		printf("CORRECT: UNADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	buffer = buffer - 32;

	Dlib_free(buffer);

	return 0;
}

int test4(){
	size_t num;
	num = 4;

	size_t sz;
	sz = 32;

	char *buffer;
	buffer = NULL;

	buffer = Dlib_calloc(num, sz);
	if(buffer == NULL){
		return 1;
	}

	buffer[0] = 'x';
	buffer[25] = 'y';
	buffer[sz - 1] = 'z';

	Dlib_free(buffer);

	return 0;
}

int test3(){
	//shrinking Dlib_realloc
	size_t sz;
	sz = 32;

	size_t nsz;
	nsz = sz - 16;

	char *buffer;
	buffer = NULL;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	buffer[0] = 'x';
	buffer[25] = 'y';
	buffer[sz - 1] = 'z';

	char *new;
	new = NULL;

	new = Dlib_realloc(buffer, nsz);

	char *test = new;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	test = new - 32;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	test = new + 16;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	Dlib_free(new);

//	Dlib_free(buffer);

	return 0;
}

int test2(){
	//growing Dlib_realloc
	size_t sz;
	sz = 32;

	size_t nsz;
	nsz = sz + 16;

	char *buffer;
	buffer = NULL;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	buffer[0] = 'x';
	buffer[25] = 'y';
	buffer[sz - 1] = 'z';

	char *new;
	new = NULL;

	new = Dlib_realloc(buffer, nsz);

	char *test = new;

	if(checkMemoryAccess(test, -1) == 0){
		printf("CORRECT: IN-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	test = new - 32;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	test = new + 48;

	if(checkMemoryAccess(test, -1) == 1){
		printf("CORRECT: OUT-OF-BOUNDS ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: OUT-OF-BOUNDS ACCESS NOT DETECTED.\n");
		Dlib_free(new);
		return 1;
	}

	Dlib_free(new);

	return 0;
}

int test1(){
	size_t sz;
	sz = 32;

	char *buffer;
	buffer = NULL;

	buffer = Dlib_malloc(sz);
	if(buffer == NULL){
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int hashTest(){
	void *buffer = Dlib_malloc(32);

	void *ex = buffer - 64;
	if(checkMemoryAccess(ex, -1) == 1){
		printf("CORRECT: UNADDRESSABLE MEMORY ACCESS DETECTED.\n");
	}else{
		printf("INCORRECT: UNADDRESSABLE MEMORY ACCESS NOT DETECTED.\n");
		Dlib_free(buffer);
		return 1;
	}

	ex = ex - 128;
	if(checkMemoryAccess(ex, -1) == 0){
		printf("CORRECT: ATTEMPING TO ACCESS MEMORY TOO FAR OUT-OF-BOUNDS, NO DETECTION.\n");
	}else{
		printf("INCORRECT: FAR OUT-OF-BOUNDS MEMORY DETECTED AS UNADDRESSABLE.\n");
		Dlib_free(buffer);
		return 1;
	}

	Dlib_free(buffer);

	return 0;
}

int main(int argc, char **argv){
	printf("Executing tests.\n");

	printf("\n");

	printf("Test 0: BASIC HASH TABLE AND MEMORY CHECKING TEST\n");
	if(hashTest() == 0){
		printf("Test 0: COMPLETED.\n");
	}else{
		printf("Test 0: FAILED.\n");
	}

	printf("\n");

	printf("Test 1: BASIC Dlib_malloc\n");
	if(test1() == 0){
		printf("Test 1: COMPLETED.\n");
	}else{
		printf("Test 1: FAILED.\n");
	}

	printf("\n");

	printf("Test 2: BASIC GROWING Dlib_realloc\n");
	if(test2() == 0){
		printf("Test 2: COMPLETED.\n");
	}else{
		printf("Test 2: FAILED.\n");
	}

	printf("\n");


	printf("Test 3: BASIC SHRINKING Dlib_realloc\n");
	if(test3() == 0){
		printf("Test 3: COMPLETED.\n");
	}else{
		printf("Test 3: FAILED.\n");
	}

	printf("\n");

	printf("Test 4: BASIC Dlib_calloc\n");
	if(test4() == 0){
		printf("Test 4: COMPLETED.\n");
	}else{
		printf("Test 4: FAILED.\n");
	}

	printf("\n");

	printf("Test 5: REDZONE OVER- AND UNDERFLOW CHECKING\n");
	if(test5() == 0){
		printf("Test 5: COMPLETED.\n");
	}else{
		printf("Test 5: FAILED.\n");
	}

	printf("\n");

	printf("Test 6: GENERAL REDZONE MEMORY CHECK 1\n");
	if(test6() == 0){
		printf("Test 6: COMPLETED.\n");
	}else{
		printf("Test 6: FAILED.\n");
	}

	printf("\n");

	printf("Test 7: GENERAL REDZONE MEMORY CHECK 2\n");
	if(test7() == 0){
		printf("Test 7: COMPLETED.\n");
	}else{
		printf("Test 7: FAILED.\n");
	}

	printf("\n");

	printf("Test 8: IN- AND OUT-OF-BOUNDS ACCESS CHECK\n");
	if(test8() == 0){
		printf("Test 8: COMPLETED.\n");
	}else{
		printf("Test 8: FAILED.\n");
	}

	printf("\n");

	printf("Test 9: Dlib_malloc (SMALL) STRESS TEST\n");
	if(test9() == 0){
		printf("Test 9: COMPLETED.\n");
	}else{
		printf("Test 9: FAILED.\n");
	}

	printf("\n");

	printf("Test 10: CROSS-PAGE ALLOCATION TEST\n");
	if(test10() == 0){
		printf("Test 10: COMPLETED.\n");
	}else{
		printf("Test 10: FAILED.\n");
	}

	printf("\n");

	printf("Test 11: BASIC MEMALIGN TEST\n");
	if(test11() == 0){
		printf("Test 11: COMPLETED.\n");
	}else{
		printf("Test 11: FAILED.\n");
	}

	printf("\n");

	printf("Test 12: BASIC RED-ZONE EDGE TEST\n");
	if(test12() == 0){
		printf("Test 12: COMPLETED.\n");
	}else{
		printf("Test 12: FAILED.\n");
	}

	printf("\n");

	printf("Test 13: ADVANCED RED-ZONE CONDITIONING TEST\n");
	if(test13() == 0){
		printf("Test 13: COMPLETED.\n");
	}else{
		printf("Test 13: FAILED.\n");
	}

	printf("\n");

	printf("Test 14: ULTRA-SMALL ALLOCATION TEST\n");
	if(test14() == 0){
		printf("Test 14: COMPLETED.\n");
	}else{
		printf("Test 14: FAILED.\n");
	}

	printf("\n");

	printf("Test 15: 1-BYTE MEMORY TEST\n");
	if(test15() == 0){
		printf("Test 15: COMPLETED.\n");
	}else{
		printf("Test 15: FAILED.\n");
	}

	printf("\n");

	printf("Test 16: ADDITIONAL TEST FOR REPORTED SHADOW MEMORY BUG\n");
	if(test16() == 0){
		printf("Test 16: COMPLETED.\n");
	}else{
		printf("Test 16: FAILED.\n");
	}

	printf("\n");

	printf("Test 17: PROBLEMATIC/STRANGE BUG (FIXED)\n");
	if(test17() == 0){
		printf("Test 17: COMPLETED.\n");
	}else{
		printf("Test 17: FAILED.\n");
	}

	printf("\n");

	printf("Test 18: LARGER ACCESS SIZE TEST\n");
	if(test18() == 0){
		printf("Test 18: COMPLETED.\n");
	}else{
		printf("Test 18: FAILED.\n");
	}

	printf("\n");

	printf("Test 19: ADVANCED LARGER ACCESS SIZE TEST\n");
	if(test19() == 0){
		printf("Test 19: COMPLETED.\n");
	}else{
		printf("Test 19: FAILED.\n");
	}

	printf("\n");

	printf("Test 20: REALLOC BUG TEST\n");
	if(test20() == 0){
		printf("Test 20: COMPLETED.\n");
	}else{
		printf("Test 20: FAILED.\n");
	}

	printf("\n");

	printf("Test 21: LIST ARITHMETIC BUG TEST\n");
	if(test21() == 0){
		printf("Test 21: COMPLETED.\n");
	}else{
		printf("Test 21: FAILED.\n");
	}

	return 0;
}