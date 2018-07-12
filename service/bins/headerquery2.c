#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

int main()
{
	int i;
	int j;
	char secret[256];
	read(open("/flag", 0), secret, 256);

	read(0, &i, 4);
	if (i >= 256)
	{
		puts("Overread attempt detected!");
		exit(1);
	}

	puts("Checking input...");
	if (i < 3)
	{
		printf("Flag header byte: %c\n", secret[i]);
	}
	else
	{
		printf("Nope.");
	}
}
