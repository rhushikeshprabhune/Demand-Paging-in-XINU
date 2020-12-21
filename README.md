# Demand-Paging-in-XINU
Built a virtual memory system for Xinu OS on x86 architecture to include user process creation and virtual heap allocation using
lazy allocation of physical addresses to optimize use of physical memory.

Incorporated a two-level hierarchical page table mechanism to enable virtual-to-physical memory translations and maintained a
free list of physical memory using linked list and allocated pages to processes through a first fit policy.
