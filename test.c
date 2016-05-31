#include <stdio.h>

#include <sys/syscall.h>

#ifndef __NR_mem_usage
#define __NR_mem_usage 353
#endif

int main() {
	int result = syscall(__NR_mem_usage);
	printf("%d\n", result);
	return result;
}
