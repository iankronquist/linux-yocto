#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/syscall.h>

#ifndef __NR_mem_usage
#define __NR_mem_usage 353
#endif
#ifndef __NR_mem_size
#define __NR_mem_size 354
#endif

int main(int argc, char **argv) {
	FILE *f;
	long num_secs, usage, total, i;
	time_t now, start;
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file> <seconds>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	f = fopen(argv[1], "w");
	assert(f != NULL);
	num_secs = strtol(argv[2], NULL, 10);
	start = time(NULL);
	for (i = 0; i < num_secs; ++i) {
		usage = syscall(__NR_mem_usage);
		total = syscall(__NR_mem_size);
		now = time(NULL);
		fprintf(f, "%lu, %lu\n", now, 1-usage/total);
		sleep(1);
	}
	fclose(f);
	return 0;
}
