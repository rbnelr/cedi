
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "windows.h"

typedef double				f64;
typedef unsigned long long	u64;

#define LEN 1000*80
char arr[LEN];

static void delete_random_char () {
	int i = rand() % LEN;
	int len = LEN -1 -i;
	memmove(&arr[i], &arr[i +1], len);
}

u64 qpc_freq;

int main (int argc, char** argv) {
	
	srand(time(NULL));
	
	QueryPerformanceFrequency((LARGE_INTEGER*)&qpc_freq);
	printf("freq %llu\n", qpc_freq);
	
	for (int i=0; i<LEN; ++i) {
		arr[i] = 'a' +(rand() % 27);
	}
	
	int avg_count = 2000;
	
	for (;;) {
		f64 total_t = 0;
		
		u64 dt;
		u64 last_end;
		QueryPerformanceCounter((LARGE_INTEGER*)&last_end);
		
		for (int i=0; i<avg_count; ++i) {
			
			delete_random_char();
			
			{
				u64 now;
				QueryPerformanceCounter((LARGE_INTEGER*)&now);
				dt = now -last_end;
				last_end = now;
			}
			
			total_t += dt;
		}
		
		printf(">>> avg dt: %f ms\n", (f64)total_t/(f64)avg_count/(f64)qpc_freq*1000*1000);
	}
	
	return 0;
}
