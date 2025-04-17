#include "smartloader.h"
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

Elf32_Ehdr *ehdr = NULL;
Elf32_Phdr *phdr = NULL;
int fd;
int i;
void *virtual_mem = NULL;
Elf32_Addr entry_pt = 0;
int page_faults = 0;
int pages_allocated = 0;
size_t page_size = 4096;

// Cleans up resources allocated by the loader
void loader_cleanup() {
    printf("\n");
    printf("Starting cleaning resources...\n");
    if (ehdr) free(ehdr);
    if (phdr) free(phdr);
    if (fd != -1) close(fd);
    printf("All resources cleaned up.\n");
}

// Checks if the ELF header corresponds to a 32-bit ELF file
int is_elf32(Elf32_Ehdr *ehdr) {
    return (ehdr->e_ident[EI_CLASS] == ELFCLASS32);
}

// Loads program headers from the ELF file
void load_phdr() {
    size_t sizeofphdr = sizeof(Elf32_Phdr);
    phdr = (Elf32_Phdr*)malloc(sizeofphdr * ehdr->e_phnum);
    if (!phdr) {
        printf("Failed to allocate memory for program headers.\n");
        exit(1);
    }
    printf("Allocated memory for program headers.\n");

    lseek(fd, ehdr->e_phoff, SEEK_SET);
    if (read(fd, phdr, sizeofphdr * ehdr->e_phnum) != sizeofphdr * ehdr->e_phnum) {
        printf("Failed to load program headers.\n");
        exit(1);
    }
}

// Loads ELF header
void load_ehdr() {
    size_t sizeofehdr = sizeof(Elf32_Ehdr);
    ehdr = (Elf32_Ehdr*)malloc(sizeofehdr);
    if (!ehdr) {
        printf("Failed to allocate memory for ELF header.\n");
        exit(1);
    }
    printf("Allocated memory for ELF header.\n");

    lseek(fd, 0, SEEK_SET);
    if (read(fd, ehdr, sizeofehdr) != sizeofehdr) {
        printf("Failed to read ELF header.\n");
        exit(1);
    }
    printf("Successfully read ELF header.\n");

    if (!is_elf32(ehdr)) {
        free(ehdr);
        exit(1);
    }
}

// Finds the entry point of the program
void find_entry_pt() {
    int min_entrypoint = 0;
    int min = 0xFFFFFFFF;

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (min > ehdr->e_entry - phdr[i].p_vaddr) {
                min = ehdr->e_entry - phdr[i].p_vaddr;
                min_entrypoint = i;
            }
        }
    }
    i = min_entrypoint;
    entry_pt = phdr[i].p_vaddr;
}

// Load and run the ELF file
void load_and_run_elf(char* exe) {
    printf("\n");
    printf("Loading and running ELF file: %s\n", exe);
    fd = open(exe, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open ELF file\n");
        exit(1);
    }
    printf("Opening ELF file: %s\n", exe);
    printf("Opened the ELF file successfully.\n");

    load_ehdr();
    load_phdr();
    find_entry_pt();

    // Calculate final report values for fragmentation
    size_t segment_size = phdr[i].p_memsz;
    size_t num_pages = (segment_size + page_size - 1) / page_size;
    size_t fragmentation = num_pages * page_size - segment_size;

    // Try to execute the entry point (_start)
    Elf32_Addr offset = ehdr->e_entry - entry_pt;
    void *entry_virtual = (void *)(entry_pt + offset);
    int (*_start)() = (int(*)())entry_virtual;
    int result = _start();

    // Final report
    printf("\n");
    printf("Return value of _start: %d\n", result);
    printf("Final values--->\n");
    printf("Pages used: %d\n", pages_allocated);
    printf("Page faults: %d\n", page_faults);
    printf("Total Fragmentation (in KB): %.2f KB\n", fragmentation / 1024.0);
    printf("\n");
}

// Signal handler for segmentation faults, treating them as page faults for unallocated memory
void segfault_handler(int signum, siginfo_t *info, void *context) {
    printf("\n");
    printf("Page fault caught (SIGSEGV) --> Invoking custom handler\n");
    printf("Address Causing SegFault: %p\n", info->si_addr);

    // Check if the fault address is within any loadable segment
    Elf32_Addr fault_addr = (Elf32_Addr)info->si_addr;
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD &&
            fault_addr >= phdr[i].p_vaddr &&
            fault_addr < phdr[i].p_vaddr + phdr[i].p_memsz) {

            // Segment info and memory allocation details
            size_t segment_size = phdr[i].p_memsz;
            size_t num_pages = (segment_size + page_size - 1) / page_size;
            size_t fragmentation = num_pages * page_size - segment_size;

            printf("Size of the segment: %zu bytes\n", segment_size);
            printf("Number of pages: %zu\n", num_pages);

            // Calculate page-aligned address for allocation
            void *page_start = (void *)((uintptr_t)fault_addr & ~(page_size - 1));
            void *segment_addr = (void *)(phdr[i].p_vaddr & ~(page_size - 1));
            size_t offset = (uintptr_t)page_start - (uintptr_t)segment_addr;

            // Allocate memory for this page only if it hasn't been allocated yet
            void *mapped_page = mmap(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                     MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped_page == MAP_FAILED) {
                perror("Error in mmap allocation");
                exit(1);
            }

            // Copy data into the newly mapped page
            lseek(fd, phdr[i].p_offset + offset, SEEK_SET);
            read(fd, mapped_page, page_size);

            printf("Memory allocated at: %p\n", mapped_page);
            printf("Fragmentation: %zu bytes\n", fragmentation);

            page_faults++;
            pages_allocated++;
            return;
        }
    }

    // If we reach here, it means we have an invalid access, so we exit
    printf("Segmentation fault at invalid address, exiting...\n");
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segfault_handler;
    sigaction(SIGSEGV, &sa, NULL);

    load_and_run_elf(argv[1]);
    loader_cleanup();

    return 0;
}
