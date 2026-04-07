# Fast Snapshot Load: Userfaultfd Proof-of-Concept

This repository contains a standalone C proof-of-concept validating the core local fault resolution architecture for my QEMU GSoC 2026 proposal ("Fast Snapshot Load using mapped-ram and userfaultfd").

## Objective
To prove that `userfaultfd` can intercept unmapped guest memory accesses and resolve them by reading directly from a local snapshot file descriptor ([simulating QEMU's `mapped-ram` feature](https://wiki.qemu.org/Google_Summer_of_Code_2026)), bypassing the need for network-based message queues used in standard postcopy.

## Architecture

1. **Synthetic Snapshot Generation (`generate_payload.c`)**: Generates a deterministic 4MB dummy snapshot file (`snapshot.bin`). Every 4KB page is filled with its own page index to verify perfect byte-offset calculations.
2. **Hardware Fault Induction (`uffd_poc.c` - Main Thread)**: Allocates an anonymous memory block, registers it with `UFFDIO_REGISTER_MODE_MISSING`, and deliberately attempts a read to trigger a hardware page fault.
3. **Page Fault Handling (`uffd_poc.c` - Listener Thread)**: 
    * A detached `pthread` polls the `userfaultfd` file descriptor (`O_NONBLOCK`).
    * Upon catching `UFFD_EVENT_PAGEFAULT`, it calculates the exact file offset using `fault_addr - ram_base`.
    * It performs a lockless `pread()` from the local `snapshot.bin` file.
    * The missing page is atomically injected into the guest RAM via `UFFDIO_COPY`, instantly waking the stalled main thread.

## Prerequisites

* **OS:** Linux (Tested on Arch Linux, Kernel `6.19.10-arch1-1`, `x86_64`). *Note: This POC assumes a standard 4KB page size.*
* **Dependencies:** `gcc` or `clang`, `make`, and standard Linux kernel headers (`<linux/userfaultfd.h>`).
* **Kernel Permissions:** Many modern Linux distributions restrict `userfaultfd` to privileged users by default. To run this POC without `sudo`, you must enable unprivileged `userfaultfd`.

**Enable Unprivileged Userfaultfd (Temporary):**
```bash
sudo sysctl -w vm.unprivileged_userfaultfd=1
```
*(For a permanent fix, add `vm.unprivileged_userfaultfd=1` to `/etc/sysctl.d/99-userfaultfd.conf` and run `sudo sysctl --system`)*

## Build and Execute

```bash
# Compile the binaries
make all

# 1. Generate the 4MB snapshot payload
./generate_payload

# 2. Execute the userfaultfd trap and injection
./uffd_poc