#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>               // mmap
#include <sys/syscall.h>            // syscall
#include <linux/userfaultfd.h>      // userfaultfd
#include <sys/ioctl.h>              // ioctl

#define PAGE_SIZE 4096
#define NUM_PAGES 1024
#define FILE_NAME "snapshot.bin"

int main(void) 
{
    size_t total_size = (size_t)PAGE_SIZE * NUM_PAGES;
    uint8_t *guest_ram = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (guest_ram == MAP_FAILED) {
        perror("Error: mmap failed to allocate guest RAM");
        return 1;
    }

    printf("Success: Allocated %zu bytes at address %p\n", total_size, (void *)guest_ram);

    

    return 0;
}