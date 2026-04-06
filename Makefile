CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O0
LDFLAGS = -pthread

all: generate_payload uffd_poc

generate_payload: generate_payload.c
	$(CC) $(CFLAGS) -o generate_payload generate_payload.c

uffd_poc: uffd_poc.c
	$(CC) $(CFLAGS) -o uffd_poc uffd_poc.c $(LDFLAGS)

clean:
	rm -f generate_payload uffd_poc snapshot.bin

.PHONY: all clean