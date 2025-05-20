#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#define PAGE_SIZE 4096
#define SWAPFILE_SIZE (PAGE_SIZE * 256*1024) // 1GiB
struct pg{
    int values[PAGE_SIZE/sizeof(int)];
} typedef pg_t;

int main(){
    pg_t *mapped = mmap(NULL, SWAPFILE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) {
        perror("Failed to map memory");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < SWAPFILE_SIZE/PAGE_SIZE; i++) {
        mapped[i].values[0] = i;
    }
    for (int i = 0; i < 10 ; i++) {
        for(int j = 0; j < SWAPFILE_SIZE/PAGE_SIZE; j++) {
            assert(mapped[j].values[0] == j+i);
            mapped[j].values[0]++;
        }
    }
    munmap(mapped, PAGE_SIZE);
    return 0;
    

}