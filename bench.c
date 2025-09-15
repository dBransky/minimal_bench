#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/swap.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>

#define ARRNAGE_BY_ROWS (1 << 19)
#define PAGE_SIZE 4096
struct pg{
    int values[PAGE_SIZE/sizeof(int)];
} typedef pg_t;
void start_vmstat(const char *test_name) {
    char command[256];
    snprintf(command, sizeof(command), "sudo vmstat -n 1 > %s_vmstat.txt &", test_name);
    system(command);
}
void stop_vmstat(const char *test_name) {
    char command[256];
    snprintf(command, sizeof(command), "sudo pkill vmstat");
    system(command);
}
void mkswap(const char *filename){
    char command[256];
    snprintf(command, sizeof(command), "mkswap %s", filename);
    system(command);
}
void enable_swap(const char *filename, int swap_flags) {
    // printf("Enabling swap on %s\n", filename);
    int ret = syscall(SYS_swapon, filename, swap_flags);
    if (ret < 0) {
        perror("swapon");
    }
}
void disable_swap(const char *filename) {
    // printf("Disabling swap on %s\n", filename);
    if (swapoff(filename) < 0) {
        perror("Failed to disable swap");
        exit(EXIT_FAILURE);
    }
}


void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s -n <test_name> -s <num_swapfiles> -b <buffer_size> -i <iterations> -r <arrange_by_rows?> -w <write?> -f <file_path>\n", prog_name);
}

int main(int argc, char *argv[]) {
    int n_swapfiles = 0;
    unsigned long long buffer_size = 0;
    int iterations = 0;
    int swap_flags=0;
    bool write = false;
    char* test_name = "default";
    char* file_path = NULL;
    int* mapped;
    int fd;

    int opt;
    while ((opt = getopt(argc, argv, "f:s:b:i:rwn")) != -1) {
        switch (opt) {
            case 'n':
                test_name = optarg;
                break;
            case 's':
                n_swapfiles = atoi(optarg);
                break;
            case 'b':
                buffer_size = strtoull(optarg, NULL, 10);
                break;
            case 'i':
                iterations = atoi(optarg);
                break;
            case 'r':
                swap_flags |= ARRNAGE_BY_ROWS;
                break;
            case 'w':
                write = true;
                break;
            case 'f':
                file_path = optarg;
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (n_swapfiles <= 0 || buffer_size == 0 || iterations <= 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    printf("swapfiles: %d\n", n_swapfiles);
    printf("buffer_size: %llu\n", buffer_size);
    printf("iterations: %d\n", iterations);
    printf("arrange_by_rows: %s\n", (swap_flags & ARRNAGE_BY_ROWS) ? "true" : "false");
    printf("write: %s\n", write ? "true" : "false");


    for (int i = 1; i <= n_swapfiles; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/scratch/vma_swaps/swapfile_%d.swap", i);
        mkswap(filename);
        enable_swap(filename,swap_flags);
    }
    if (file_path){
        fd = open(file_path,O_RDWR);
        if (fd < 0){
            perror("failed to open file");
            return EXIT_FAILURE;
        }
        mapped = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    else
        mapped = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) {
        perror("Failed to map memory");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < buffer_size/sizeof(int); i++) {
        mapped[i] = i+1;
    }
    unsigned long long *latency = malloc(sizeof(unsigned long long) * iterations);
    if (latency == NULL) {
        perror("Failed to allocate latency array");
        munmap(mapped, buffer_size);
        return EXIT_FAILURE;
    }
    start_vmstat(test_name);
    struct timespec start, end;
    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int j = 0; j < buffer_size / sizeof(int); j++) {
            if(write)
                mapped[i]++;
            else {
                volatile int temp = mapped[i];
                asm volatile("" : "+r"(temp)); // Prevent compiler optimization
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        latency[i] = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    }
    stop_vmstat(test_name);
    munmap(mapped, buffer_size);
       for (int i = 1; i <= n_swapfiles; i++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "/scratch/vma_swaps/swapfile_%d.swap", i);
        disable_swap(filename);
    }
    if (file_path)
        close(fd);
    // create file for test 
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.csv", test_name);
    FILE *csv_file = fopen(filename, "w");
    fprintf(csv_file,"iteration, latency_ns\n");
    for (int i = 0; i < iterations; i++) {
        fprintf(csv_file,"%d, %llu\n", i, latency[i]);
    }
    return EXIT_SUCCESS;
}