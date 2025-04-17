#ifndef SMARTLOADER_H
#define SMARTLOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

// ELF loader global variables
extern Elf32_Ehdr *ehdr;          // ELF header
extern Elf32_Phdr *phdr;          // Program headers
extern int fd;                    // File descriptor for ELF file
extern int page_faults;           // Number of page faults encountered
extern int pages_allocated;       // Number of pages allocated
extern void *virtual_mem;         // Pointer to virtual memory (if needed for future use)
extern Elf32_Addr entry_pt;       // Entry point address of ELF file
extern size_t page_size;          // System page size

// Function declarations
void load_and_run_elf(char *exe);                     // Loads and runs the ELF executable
void loader_cleanup();                                // Cleans up resources after execution
void segfault_handler(int signum, siginfo_t *info, void *context); // Custom signal handler for SIGSEGV
void load_ehdr();                                     // Loads the ELF header from the file
void load_phdr();                                     // Loads the program headers from the ELF file
int is_elf32(Elf32_Ehdr *ehdr);                      // Checks if the ELF header corresponds to a 32-bit ELF file
void find_entry_pt();                                 // Finds the entry point of the ELF program

#endif // SMARTLOADER_H
