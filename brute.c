#include <openssl/md5.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>

#define TOP_COUNT 0x1000000LL //33554432LL // 1073741824LL // 4294967296LL // 1024LL*1024LL*1024LL*4LL
#define TOP_SIZE  0x8000000LL // 8589934592LL //TOP_COUNT*sizeof(uintptr_t)
#define BOTTOM_COUNT 340//1024/sizeof(uint64_t)
#define BOTTOM_SIZE 2720 //BOTTOM_COUNT * 8
unsigned long long **TOP_LEVEL;
__thread unsigned long long *CUR_PAGE = 0;
__thread unsigned long long CUR_IDX = 0xfffffffffff00000LL;

unsigned long long calced = 0;
unsigned long long full_buckets = 0;

//char fmt[] = "%016llx";
__thread unsigned char fmt[] = { 128, 4, 149, 43, 0, 0, 0, 0, 0, 0, 0, 40, 140, 3, 66, 86, 86, 148, 138, 8, 88, 88, 88, 88, 88, 88, 88, 88, 75, 64, 134, 148, 140, 2, 54, 52, 148, 138, 6, 248, 246, 70, 162, 24, 121, 137, 74, 115, 211, 53, 0, 116, 148, 46 };
#define OFFSET 20
#define LENGTH 54

unsigned long long get_digest(unsigned long long i)
{
	unsigned long long digest[2];
	//sprintf(string, fmt, i);
	*(unsigned long long *)(fmt+OFFSET) = i;
	MD5((unsigned char*)fmt, LENGTH, (unsigned char*)&digest);
	return digest[0];
}

void do_one(unsigned long long i)
{
	unsigned long long our_hash = get_digest(i);
	unsigned long long idx = our_hash % TOP_COUNT;
	unsigned long long *page = TOP_LEVEL[idx];
	if (!page)
	{
		if (CUR_IDX + BOTTOM_SIZE >= TOP_SIZE)
		{
			CUR_PAGE = mmap(0, TOP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, 0, 0);
			printf("Mapped: %p\n", CUR_PAGE);
			assert(CUR_PAGE != (void *)-1);
			assert(CUR_PAGE != 0);
			CUR_IDX = 0;
		}
		page = TOP_LEVEL[idx] = (unsigned long long*)((uintptr_t)CUR_PAGE + CUR_IDX);
		assert(page != 0);
		CUR_IDX += BOTTOM_SIZE;
		//printf("Allocated page %llx (of %lld) for digest %llx, at %p\n", idx, TOP_COUNT, our_hash, page);
	}

	int page_idx;
	for (page_idx = 0; page_idx < BOTTOM_COUNT && page[page_idx]; page_idx++)
	{
		//printf("PI: %d\n", page_idx);
		unsigned long long their_hash = get_digest(page[page_idx]);
		//printf("I: %#llx, H: %#llx, PI: %d, PP: %#llx, TH: %#llx\n", i, our_hash, page_idx, page[page_idx], their_hash);
		if (their_hash == our_hash)
		{
			printf("COLLISION!!!!! %llx %llx %llx\n", i, page[page_idx], their_hash);
			//exit(0);
		}
	}
	//printf("I: %#llx, H: %#llx, PI: %d\n", i, our_hash, page_idx);
	if (page_idx == BOTTOM_COUNT)
	{
		//printf("Bucket %llx is full...\n", idx);
		full_buckets++;
	}
	else page[page_idx] = i;
	calced++;
	
}

int do_loop(unsigned long long i)
{
	while (1)
	{
		do_one(i);
		i++;
	}
}

void *thread_loop(void *i) { do_loop((unsigned long long)i); return NULL; }

int main(int argc, char **argv)
{
	TOP_LEVEL = mmap(0, TOP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, 0, 0);
	printf("Storage: %p\n", TOP_LEVEL);
	assert(TOP_LEVEL != (void *)-1);
	assert(TOP_LEVEL != 0);

	unsigned long long sanity = get_digest(6365935209750747224);
	assert(sanity == 927387844087402805);
	printf("Sanity passed!\n");

	//thread_loop((void *)0x100000000000);

	unsigned int thread_num;
	if (argc == 1) thread_num = get_nprocs();
	else thread_num = atoi(argv[1]);
	printf("Threads: %d\n", thread_num);

	unsigned long long start = 0;
	pthread_t threads[thread_num];
	read(open("/dev/urandom", 0), &start, 7);
	for (unsigned int t = 0; t < thread_num; t++)
	{
		pthread_create(&threads[t], NULL, thread_loop, (void *)start);
		start += 0x0100000000000000;
	}

	while (1)
	{
		sleep(1);
		printf("Calced: %lld (%lldM %lldG) (%lld full buckets)\n", calced, calced/1024/1024, calced/1024/1024/1024, full_buckets);
		if (calced > 4294967296)
			break;
	}

	return 1;
}
