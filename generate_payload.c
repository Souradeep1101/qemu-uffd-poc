#include <fcntl.h>                  // file control op
#include <unistd.h>                 // POSIX API
#include <stdint.h>                 // uint8_t
#include <string.h>                 // memset()
#include <stdio.h>                  // perror()

#define PAGE_SIZE 4096
#define NUM_PAGES 1024
#define FILE_NAME "snapshot.bin"

int main(void) 
{
    int fd_snapshot = open(FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd_snapshot == -1) {
        perror("Error: Couldn't open file.");
        return 1;
    }

    uint8_t buffer[PAGE_SIZE];

    for (int i = 0; i < NUM_PAGES; i++) {
        int target_byte = i % 256;
        memset(buffer, target_byte, sizeof(buffer));
        ssize_t bytes_written = write(fd_snapshot, buffer, sizeof(buffer));
        
        if (bytes_written == -1 || bytes_written != PAGE_SIZE) {
            perror("Error: Failed to write to file.");
            close(fd_snapshot);
            return 1;
        }
    }

    if (fsync(fd_snapshot) == -1) {
        perror("Error: Fsync failed.");
    }

    if (close(fd_snapshot) == -1) {
        perror("Error: Close failed.");
    }

    printf("Success: Closing file.\n");

    return 0;
}