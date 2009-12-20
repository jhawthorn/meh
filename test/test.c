#include "stdio.h"
#include "stdlib.h"
#include "sys/time.h"

#include "../src/meh.h"

#define TESTRUNS 20
#define STARTTEST(name) int test_##name(){ testname = #name; testsrun++; do
#define ENDTEST while(0); testspassed++; return 0;}
#define STARTTIME struct timeval tvs[2];int _i, _j; for(_i = 0; _i < 2; gettimeofday(&tvs[_i], NULL), _i++){for(_j = 0; _j < (_i ? TESTRUNS : 3); _j++)
#define ENDTIME }long int mselapsed = (tvs[1].tv_sec - tvs[0].tv_sec) * 1000000L + (tvs[1].tv_usec - tvs[0].tv_usec); printf("%s: average time: %li.%.3li ms\n", testname, mselapsed/1000/TESTRUNS, (mselapsed/TESTRUNS)%1000);
#define assert(x) if(!(x)){fprintf(stderr, "test \"%s\" failed\n   assert(%s) was false\n   at %s:%i\n\n", testname, #x, __FILE__ ,__LINE__);return -1;}

char *testname = NULL;
int testsrun = 0, testspassed = 0;

typedef int (*test_t)();

STARTTEST(scale){
	struct image img;
	img.bufwidth = 1280*2;
	img.bufheight = 1024*2;
	img.buf = malloc(img.bufwidth*img.bufheight*4);
	char *to = malloc(1280*1024*4);
	STARTTIME{
		scale(&img, 1280, 1024, 1280, to);
	}ENDTIME
	free(to);
	free(img.buf);
}ENDTEST

STARTTEST(nearestscale){
	struct image img;
	img.bufwidth = 1280*2;
	img.bufheight = 1024*2;
	img.buf = malloc(img.bufwidth*img.bufheight*4);
	char *to = malloc(1280*1024*4);
	STARTTIME{
		nearestscale(&img, 1280, 1024, 1280, to);
	}ENDTIME
	free(to);
	free(img.buf);
}ENDTEST

test_t tests[] = {
	test_scale,
	test_nearestscale,
	NULL
};

void summary(){
	printf("%i tests run: %i passed  %i failed\n", testsrun, testspassed, testsrun - testspassed);
}

int main(){
	test_t *test = tests;
	for(; *test; test++)
		(*test)();
	summary();
	return 0;
}

