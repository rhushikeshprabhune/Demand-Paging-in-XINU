#include "xinu.h"

void pagefault_handler() {

    write_cr3(system_page_directory & 0xFFFFF000 );

    uint32 faultyAddress = read_cr2();
    
    uint32 pdOffset = ( faultyAddress >> 22 );
    uint32 ptOffset = ( faultyAddress >> 12 ) & 0x3FF ;
    pd_t* pdbr = (pd_t*) proctab[currpid].pdbr ;
    pt_t* ptbr = (pt_t*) (pdbr[pdOffset].pd_base << 12);
    // kprintf("P%d:: SEGMENTATION FAULT due to access to %x(pres:%d)\n", currpid, faultyAddress, ptbr[ptOffset].pt_pres);

    if (ptbr[ptOffset].pt_valid == 1) {
        //create mapping, i.e.update pt_base
        char* ffs_addr = get_next_free_ffs_page(PAGE_SIZE) ;
        ptbr[ptOffset].pt_base = ((uint32)ffs_addr >> 12);
        ptbr[ptOffset].pt_pres = 1;
        proctab[currpid].used_virtual_pages++;
    } else {
        kprintf("P%d:: SEGMENTATION_FAULT\n", currpid);
        kill(currpid);
    }

    write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
}