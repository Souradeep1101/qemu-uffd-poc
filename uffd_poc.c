#define _GNU_SOURCE                 // reveal O_CLOEXEC

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>               // mmap
#include <sys/syscall.h>            // syscall
#include <linux/userfaultfd.h>      // userfaultfd
#include <sys/ioctl.h>              // ioctl
#include <fcntl.h>                  // O_CLOEXEC
#include <pthread.h>                // thread
#include <poll.h>                   // monitoring
#include <errno.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 1024
#define FILE_NAME "snapshot.bin"

pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
int is_thread_ready = 0;

void *fault_handler_thread(void *arg) 
{
    int uffd = (int)(uintptr_t)arg;
    struct pollfd pollfd;
    pollfd.fd = uffd;
    pollfd.events = POLLIN;

    printf("Background thread started, waiting for page faults...\n");
    fflush(stdout);

    // Signal main thread
    pthread_mutex_lock(&ready_mutex);
    is_thread_ready = 1;
    pthread_cond_signal(&ready_cond);
    pthread_mutex_unlock(&ready_mutex);

    while (1) {
        if (poll(&pollfd, 1, -1) == -1) {
            perror("Error: poll() failed in fault_handler_thread");
            pthread_exit(NULL);
        }

        // printf("Debug: poll() woke up! revents = %d\n", pollfd.revents);
        // fflush(stdout);

        if (pollfd.revents & POLLIN) {
            struct uffd_msg msg;
            ssize_t nread = read(uffd, &msg, sizeof(msg));

            if (nread != sizeof(struct uffd_msg)) {
                if (nread == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue; 
                } else if (nread == 0) {
                    fprintf(stderr, "Error: EOF on userfaultfd\n");
                } else {
                    perror("Error: read(uffd) failed");
                }
                pthread_exit(NULL);
            }

            if (msg.event != UFFD_EVENT_PAGEFAULT) {
                printf("Listener: Unexpected event received.\n");
                continue;
            }

            uint64_t fault_addr = msg.arg.pagefault.address;

            printf("Listener: CAUGHT PAGE FAULT!\n");
            printf("Listener: The main thread is stalled trying to access address: 0x%lx\n", 
                   (unsigned long)fault_addr);
            fflush(stdout);
        }
    }
    return NULL;
}

int main(void) 
{
    size_t total_size = (size_t)PAGE_SIZE * NUM_PAGES;
    uint8_t *guest_ram = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (guest_ram == MAP_FAILED) {
        perror("Error: mmap failed to allocate guest RAM");
        return 1;
    }

    printf("Success: Allocated %zu bytes at address %p\n", total_size, 
        (void *)guest_ram);
    
    /*
     * NOTE: If this syscall fails with EPERM (Operation not permitted),
     * you must enable unprivileged userfaultfd in the Linux kernel.
     *
     * Permanent fix (Arch/Debian/Fedora):
     * echo "vm.unprivileged_userfaultfd=1" | sudo tee /etc/sysctl.d/10-userfaultfd.conf
     * sudo sysctl --system
     *
     * Temporary fix:
     * sudo sysctl -w vm.unprivileged_userfaultfd=1
     */
    int uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK);

    if (uffd < 0) {
        perror("Error: userfaultfd syscall failed.");
        return 1;
    }

    struct uffdio_api api_req;
    api_req.api = UFFD_API;
    api_req.features = 0;

    if (ioctl(uffd, UFFDIO_API, &api_req) == -1) {
        perror("Error: ioctl(UFFDIO_API) failed");
        return 1;
    }

    if (api_req.api != UFFD_API) {
        fprintf(stderr, "Error: Unsupported userfaultfd API version\n");
        return 1;
    }

    struct uffdio_register reg_req;
    reg_req.range.start = (uint64_t)guest_ram;
    reg_req.range.len = total_size;
    reg_req.mode = UFFDIO_REGISTER_MODE_MISSING;

    if (ioctl(uffd, UFFDIO_REGISTER, &reg_req) == -1) {
        perror("Error: ioctl(UFFDIO_REGISTER) failed");
        return 1;
    }

    if (!(reg_req.ioctls & (1 << _UFFDIO_COPY))) {
        fprintf(stderr, "Error: UFFDIO_COPY not supported\n");
        return 1;
    }

    printf("Success: Memory successfully registered with userfaultfd.\n");
    
    pthread_t fault_thread;

    if (pthread_create(&fault_thread, NULL, fault_handler_thread, (void *)(uintptr_t)uffd) != 0) {
        perror("Error: pthread_create failed");
        return 1;
    }

    // Wait for the background thread to be ready
    pthread_mutex_lock(&ready_mutex);
    while (!is_thread_ready) {
        pthread_cond_wait(&ready_cond, &ready_mutex);
    }
    pthread_mutex_unlock(&ready_mutex);

    printf("Warning: Attempting to read from unmapped memory. The program should hang now...\n");

    // volatile uint8_t dummy_read;
    // dummy_read = guest_ram[0];
    // (void)dummy_read;

    // guest_ram[0] = 0xAA;

    volatile uint8_t *trap_ptr = (volatile uint8_t *)guest_ram;
    trap_ptr[0] = 0xAA;

    printf("CRITICAL FAILURE: If you see this text, the page fault DID NOT HAPPEN!\n");
    fflush(stdout);

    pthread_join(fault_thread, NULL);

    return 0;
}