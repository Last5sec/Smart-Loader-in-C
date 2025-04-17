# ELF Loader with Demand Paging
A minimal ELF loader implementing demand paging with SIGSEGV handling for memory allocation. Measures page faults and internal fragmentation during execution.

# Key Features 
🖥️ On-demand page allocation via mmap
⚡ SIGSEGV handler for transparent page fault handling
📊 Tracks performance metrics:
Page fault counts
Memory allocations
Internal fragmentation
🔍 Supports 32-bit ELF executables
