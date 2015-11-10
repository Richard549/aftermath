#include <stdio.h>

int main(int argc, char** argv)
{
	for(int j = 0; j < 10; j++) {
		#pragma omp parallel for
		for(int i = 0; i < 10; i++)
			printf("Loop 1, iteration %d\n", i);

		#pragma omp parallel for
		for(int i = 0; i < 10; i++)
			printf("Loop 2, iteration %d\n", i);
	}

	return 0;
}