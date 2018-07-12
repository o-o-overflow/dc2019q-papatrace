#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

int main()
{
	register unsigned long long i;
	register unsigned long long j;
	unsigned long long k;
	char secret[256];
	read(open("/flag", 0), secret, 256);

	read(0, &k, 8);
	i = k;
	puts("Checking input...");
	if ((unsigned char)i > 9)
	{
		puts("OOO is unwilling to reveal that much of the flag at this time.");
		exit(1);
	}

	for (j = 0; j < i % 256; j++)
	{
		printf("Flag byte %lld: %c\n", j, secret[j]);
	}
}
