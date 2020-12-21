#include "xinu.h"

/*------------------------------------------------------------------------
 *  vmalloc  -  Allocate from private heap storage, first fit policy
 *------------------------------------------------------------------------
 */

void ffs_init()
{

	struct memblk *memptr; /* Ptr to memory block		*/

	/* Initialize the ffs list */

	memptr = ffs_list.mnext = (struct memblk *) FFS_START;
	
    /* initialize free memory list to one block */
    memptr->mnext = (struct memblk *)NULL;
    memptr->mlength = (uint32)( MAX_FFS_SIZE * PAGE_SIZE);
    ffs_list.mlength = (uint32)( MAX_FFS_SIZE * PAGE_SIZE);

    // kprintf("In ffs_init: setting total free size as %d\n", ffs_list.mlength);

	return;
}

char* get_next_free_ffs_page(uint32 nbytes) {
    struct memblk *prev, *curr, *leftover;

    prev = &ffs_list;
    curr = ffs_list.mnext;
    while (curr != NULL)
    { /* Search free list	*/

        if (curr->mlength == nbytes)
        { /* Block is exact match	*/
            prev->mnext = curr->mnext;
            ffs_list.mlength -= nbytes;
            return (char *)(curr);
        }
        else if (curr->mlength > nbytes)
        { /* Split big block	*/
            leftover = (struct memblk *)((uint32)curr +
                                         nbytes);
            prev->mnext = leftover;
            leftover->mnext = curr->mnext;
            leftover->mlength = curr->mlength - nbytes;
            ffs_list.mlength -= nbytes;
            return (char *)(curr);
        }
        else
        { /* Move to next block	*/
            prev = curr;
            curr = curr->mnext;
        }
    }
}

void	release_ffs_pages(
	  char		*blkaddr,	/* Pointer to memory block	*/
	  uint32	nbytes		/* Size of block in bytes	*/
	)
{
	struct	memblk	*next, *prev, *block;
	uint32	top;

	block = (struct memblk *)blkaddr;

	prev = &ffs_list;			/* Walk along free list	*/
	next = ffs_list.mnext;
	while ((next != NULL) && (next < block)) {
		prev = next;
		next = next->mnext;
	}

	if (prev == &ffs_list) {		/* Compute top of previous block*/
		top = (uint32) NULL;
	} else {
		top = (uint32) prev + prev->mlength;
	}

	/* Ensure new block does not overlap previous or next blocks	*/

	if (((prev != &ffs_list) && (uint32) block < top)
	    || ((next != NULL)	&& (uint32) block+nbytes>(uint32)next)) {
		kprintf("From free_ffs_list:: should not have come here!! new block overlaps previous or next block!!!!!!!!!!!\n");
		return SYSERR;
	}

	ffs_list.mlength += nbytes;

	/* Either coalesce with previous block or add to free list */

	if (top == (uint32) block) { 	/* Coalesce with previous block	*/
		prev->mlength += nbytes;
		block = prev;
	} else {			/* Link into list as new node	*/
		block->mnext = next;
		block->mlength = nbytes;
		prev->mnext = block;
	}

	/* Coalesce with next block if adjacent */

	if (((uint32) block + block->mlength) == (uint32) next) {
		block->mlength += next->mlength;
		block->mnext = next->mnext;
	}
	return OK;
}


uint32 get_free_virtual_heap_pages(uint32 npages) {
    
    uint32 numPages = 0;
    uint32 i = 0;
    uint32 finalPageNumber = 0;
    while (numPages != npages) {
        if (proctab[currpid].virtualPage[i] == 0) {
            numPages++;
        } else {
            numPages = 0;
        }
        i++;
    }
    finalPageNumber = i - npages;
    // kprintf("got req for %d, giving finalPN=%d\n", npages, finalPageNumber);
    for (uint32 j = finalPageNumber; j < finalPageNumber+npages; j++) {
        proctab[currpid].virtualPage[j] = 1;
    }
    
    
    // pd_t* pdbr = (pd_t*) proctab[currpid].pdbr;
    // uint32 numPages = npages;
    // uint32 pdOffset=0, ptOffset=0;
    // uint32 finalPDOffset=0, finalPTOffset=0;
    // while ( numPages != 0) {
    //     if(pdbr[pdOffset].pd_pres == 0) {
    //         if(numPages < 1024) {
    //             break;
    //         } else {
    //             numPages = numPages - 1024;
    //         }
    //     } else {
    //         if ()
    //         pt_t* ptbr = (pt_t*) (pdbr[pdOffset].pd_base << 12);
    //         ptOffset = 0;
    //         uint32 numPagesDone = 0;
    //         while(numPagesDone != numPages) {
    //             if (ptbr[ptOffset].pt_pres == 1) {
    //                 numPagesDone = 0;
    //             } else {
    //                 numPagesDone++;
    //                 break;
    //             }
    //             ptOffset++;
    //         }
    //     }
    //     pdOffset++;
    // }    
    
    // for(uint32 pdOffset = 9; pdOffset < 1024; pdOffset++) {
    //     uint32 ptToCheck;
    //     if(pdbr[pdOffset].pd_pres == 0) {
    //         if(npages < 1024) {
    //             break;
    //         } else {

    //         }
    //     } else {
    //         ptToCheck = pdbr[pdOffset].pd_base << 12 ;
    //     }
    // }


    //MAKE SURE NOTHING IN ANY PT OR PD IS MODIFIED IN THIS FUNCTION!!!!!!!!!!!!!!!!!!!
    return finalPageNumber * 0x1000 + XINU_PAGES*PAGE_SIZE;
}

char *vmalloc(uint32 nbytes) /* Size of memory requested	*/
{
    intmask mask; /* Saved interrupt mask		*/
    uint32 bytesDemanded = nbytes;
    mask = disable();
    //switch to complete memory space
    write_cr3(system_page_directory & 0xFFFFF000 );

    if (nbytes == 0)
    {
        write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
        restore(mask);
        return (char *)SYSERR;
    }

    nbytes =  round_to_page(nbytes); /*round to page boundary	*/
    uint32 numPagesToMap = nbytes / PAGE_SIZE;

    if (numPagesToMap >= (1024*1024 - XINU_PAGES - 1))
    {
        write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
        restore(mask);
        return (char *)SYSERR;
    }

    uint32 vaddr = get_free_virtual_heap_pages(numPagesToMap);
    proctab[currpid].allocated_virtual_pages += numPagesToMap;
    pd_t* pdbr = (pd_t*) proctab[currpid].pdbr ;
    uint32 pdOffset = ( vaddr >> 22 );
    uint32 ptOffset = ( vaddr >> 12 ) & 0x3FF ;
    uint32 toMap = numPagesToMap;
    uint32 numPDEtoModify = (uint32)( ( numPagesToMap + 1023) / 1024);
    uint32 ptToModify;
    for(uint32 i =0; i < numPDEtoModify; i++) {
        if(pdbr[i+pdOffset].pd_pres == 0) {
            ptToModify = get_page_from_paging_area();
            pdbr[i+pdOffset].pd_base = (ptToModify >> 12);
        } else {
            ptToModify = pdbr[i+pdOffset].pd_base << 12 ;
        }
        pdbr[i+pdOffset].pd_pres = 1;
        pdbr[i+pdOffset].pd_valid = 1;

        pt_t* ptbr = (pt_t*)ptToModify;
        uint32 mapInThisLoop=0;
        if (toMap <= 1024) {
            mapInThisLoop = toMap;
            toMap = 0;
        } else {
            mapInThisLoop = 1024;
            toMap = toMap-1024;
        }
        for(uint32 j = 0; j < mapInThisLoop; j++) {
            ptbr[j+ptOffset].pt_valid = 1;
            ptbr[j+ptOffset].pt_pres = 0;
        }
    }

    // kprintf("In vmalloc: completed setting valid bits NEEDS A FIX TO THE j LOOP LIMIT!\n");
    write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
    restore(mask);
    return vaddr;
}







syscall vfree(char* vaddr, uint32 nbytes) {
    intmask mask; /* Saved interrupt mask		*/
    uint32 bytesDemanded = nbytes;
    mask = disable();
    //switch to complete memory space
    write_cr3(system_page_directory & 0xFFFFF000 );

    if (nbytes == 0)
    {
        restore(mask);
        return (char *)SYSERR;
    }

    nbytes =  round_to_page(nbytes); /*round to page boundary	*/
    uint32 numPagesToUnmap = nbytes / PAGE_SIZE;
    pd_t* pdbr = (pd_t*) proctab[currpid].pdbr ;
    uint32 pdOffset = ( (uint32)vaddr >> 22 );
    uint32 ptOffset = ( (uint32)vaddr >> 12 ) & 0x3FF ;
    uint32 numPDEtoModify = (uint32)( ( numPagesToUnmap + 1023) / 1024); //8
    uint32 ptToModify;
    uint32 contiguousFreePTEs = 0;
    bool8 illegalCall = 0;
    uint32 toUnmap = numPagesToUnmap; //8192
    for(uint32 i=0; i < numPDEtoModify; i++) {
        if(pdbr[i+pdOffset].pd_pres == 0) {
            // kprintf("In vfree:: Illegal Call, PDE not pres!!!\n");
            illegalCall = 1;
            break;
        } else {
            ptToModify = pdbr[i+pdOffset].pd_base << 12 ;
        }

        pt_t* ptbr = (pt_t*)ptToModify;
        uint32 unmapInThisLoop=0;
        if (toUnmap <= 1024) {
            unmapInThisLoop = toUnmap;
            toUnmap = 0;
        } else {
            unmapInThisLoop = 1024;
            toUnmap = toUnmap-1024;
        }

        for(uint32 j = 0; j < unmapInThisLoop; j++) {
            
            if (ptbr[j+ptOffset].pt_valid == 0) {
                // kprintf("In vfree:: Illegal Call, case 2!\n");
                illegalCall = 1;
                break;
            }
        }
    }



    if(illegalCall == 1) {
        write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
        restore(mask);
        return SYSERR;
    } else {
        proctab[currpid].allocated_virtual_pages -= numPagesToUnmap;
        
        uint32 ptToModify;
        uint32 toUnmap = numPagesToUnmap;
        for(uint32 i=0; i < numPDEtoModify; i++) {
            
            ptToModify = pdbr[i+pdOffset].pd_base << 12 ;
            pt_t* ptbr = (pt_t*)ptToModify;
            uint32 unmapInThisLoop=0;
            if (toUnmap <= 1024) {
                unmapInThisLoop = toUnmap;
                toUnmap = 0;
            } else {
                unmapInThisLoop = 1024;
                toUnmap = toUnmap-1024;
            }

            // kprintf("In vfree: releasing %d in the inner for!\n", unmapInThisLoop);
            for(uint32 j = 0; j < unmapInThisLoop; j++) {
                uint32 addrToUnset = ((pdOffset+i) << 22) + ( ((j+ptOffset) & 0x3FF) << 12) - XINU_PAGES*PAGE_SIZE;
                uint32 pageToUnsetInArray = addrToUnset >> 12;
                ///unset free bit in per process virtual heap free tracker array
                proctab[currpid].virtualPage[pageToUnsetInArray] = 0;
                //invalidate entries in PTEs
                if (ptbr[j+ptOffset].pt_pres == 1) {
                    uint32 ffs_addr_to_free = (ptbr[j+ptOffset].pt_base << 12 );
                    release_ffs_pages(ffs_addr_to_free, PAGE_SIZE);
                    // kprintf("rlsng\n");
                    proctab[currpid].used_virtual_pages--;
                }
                ptbr[j+ptOffset].pt_valid = 0;
                ptbr[j+ptOffset].pt_pres = 0;
            }
        }
    }

    
    // kprintf("In vfree: completed unsetting valid & pres bits!\n");


    write_cr3(proctab[currpid].pdbr & 0xFFFFF000);
    restore(mask);
    return OK;
}



uint32 free_ffs_pages() {
    return ( ffs_list.mlength / PAGE_SIZE ) ;
}

uint32 allocated_virtual_pages(pid32 pid) {
    return XINU_PAGES + proctab[pid].allocated_virtual_pages;
}

uint32 used_ffs_frames(pid32 pid) {
    return proctab[pid].used_virtual_pages;
}









// char *vmallocOLD(uint32 nbytes) /* Size of memory requested	*/
// {
//     intmask mask; /* Saved interrupt mask		*/
//     uint32 bytesDemanded = nbytes;
//     mask = disable();
//     //switch to complete memory space
//     write_cr3(system_page_directory & 0xFFFFF000 );

//     if (nbytes == 0)
//     {
//         restore(mask);
//         return (char *)SYSERR;
//     }

//     nbytes =  round_to_page(nbytes); /*round to page boundary	*/
//     char* ffs_addr = get_next_free_ffs_page(nbytes) ;

//     //create mapping in current processes PD for the new found malloc'ed area
//     uint32 numPagesToMap = nbytes / PAGE_SIZE;
//     proctab[currpid].allocated_virtual_pages += numPagesToMap;
//     uint32 vaddr = get_next_free_virtual_heap_page(nbytes);
//     pd_t* pdbr = (pd_t*) proctab[currpid].pdbr ;
//     uint32 pdOffset = ( vaddr >> 22 );
//     uint32 ptOffset = ( vaddr >> 12 ) & 0x3FF ;
//     //Modify one PDE and correct number of PTE's in the first table
//     uint32 numPTEtoModifyInFirstPT;
//     uint32 numPTEtoModifyInFurtherPTs;
//     bool8 furtherModification = 0;
//     if( (1024 - ptOffset) >= numPagesToMap) {
//         numPTEtoModifyInFirstPT = numPagesToMap;
//         furtherModification = 0;
//         numPTEtoModifyInFurtherPTs = 0;
//     } else {
//         numPTEtoModifyInFirstPT = 1024 - ptOffset;
//         furtherModification = 1;
//         numPTEtoModifyInFurtherPTs = numPagesToMap - numPTEtoModifyInFirstPT;
//     }
//     uint32 ptToModify;
//     //check if pdbr[i+pdOffset] already points to a table, if yes, do not grab a new table, else do grab one
//     if(pdbr[pdOffset].pd_pres == 0) {
//         ptToModify = get_page_from_paging_area();
//         pdbr[pdOffset].pd_base = (ptToModify >> 12);
//     } else {
//         ptToModify = pdbr[pdOffset].pd_base << 12 ;
//     }

//     pdbr[pdOffset].pd_pres = 1;					    //SET PD PRESENT HERE, BUT NOT PRESENTS IN PTE, LAZY ALLOCATION NEEDED
//     pdbr[pdOffset].pd_write = 1;					//set writeable
//     pdbr[pdOffset].pd_valid = 1;					//set valid

//     //iterate and fill in first PTE
//     kprintf("In vmalloc: about to modify %d PTE's for (first PT) PT at: %x\n", numPTEtoModifyInFirstPT, ptToModify);
//     pt_t* ptbr = (pt_t*) ptToModify;
//     for (uint32 j = 0 ; j < numPTEtoModifyInFirstPT; j++) {
//         ptbr[j+ptOffset].pt_base = ((uint32)ffs_addr >> 12) + j ; 
//         ptbr[j+ptOffset].pt_valid = 1;
//         ptbr[j+ptOffset].pt_write = 1;
//     }

//     if(furtherModification == 1) {
//         kprintf("Further modification needed!!\n");
//     }




//     kprintf("In vmalloc: completed adding mappings!\n");
//     write_cr3(proctab[currpid].pdbr & 0xFFFFF000);

//     restore(mask);
//     return vaddr;
// }








// char *vmalloc(uint32 nbytes) /* Size of memory requested	*/
// {
//     intmask mask; /* Saved interrupt mask		*/
//     uint32 bytesDemanded = nbytes;

//     mask = disable();

//     //switch to complete memory space
//     write_cr3(system_page_directory & 0xFFFFF000 );

//     if (nbytes == 0)
//     {
//         restore(mask);
//         return (char *)SYSERR;
//     }

//     nbytes =  round_to_page(nbytes); /*round to page boundary	*/
//     char* addr = get_next_free_ffs_page(nbytes) ;

//     //create mapping in current processes PD for the new found malloc'ed area
//     uint32 numPagesToMap = nbytes / PAGE_SIZE;
//     proctab[currpid].allocated_virtual_pages += numPagesToMap;
//     uint32 addrToMap = addr;
//     pd_t* pd_entries = (pd_t*) proctab[currpid].pdbr ;
//     uint32 pdOffset = ( addrToMap >> 22);
//     uint32 ptOffset = (addrToMap >> 12) & 0x3FF ;
//     uint32 numPTEtoModifyInFirstPT;
//     bool8 furtherModification = 0;
//     if( (1024 - ptOffset) >= numPagesToMap) {
//         numPTEtoModifyInFirstPT = numPagesToMap;
//         furtherModification = 0;
//     } else {
//         numPTEtoModifyInFirstPT = 1024 - ptOffset;
//         furtherModification = 1;
//     }

    
    // //Modify one PDE and correct number of PTE's in the first table
    // uint32 ptToModify;
    // //check if pd_entries[i+pdOffset] already points to a table, if yes, do not grab a new table, else do grab one
    // if(pd_entries[pdOffset].pd_pres == 0) {
    //     ptToModify = get_page_from_paging_area();
    //     pd_entries[pdOffset].pd_base = (ptToModify >> 12);
    // } else {
    //     ptToModify = pd_entries[pdOffset].pd_base << 12 ;
    // }
            
    // pd_entries[pdOffset].pd_pres = 1;					//SET PD PRESENT HERE, BUT NOT PRESENTS IN PTE, LAZY ALLOCATION NEEDED
    // pd_entries[pdOffset].pd_write = 1;					//set writeable
    // pd_entries[pdOffset].pd_valid = 1;					//set valid

    // //iterate over each page table and fill in PTEs
    
    // kprintf("In vmalloc: about to modify %d PTE's for PT at: %x\n", numPTEtoModifyInFirstPT, ptToModify);
    // pt_t* ptbr = (pt_t*) ptToModify;
    // for (uint32 j = 0 ; j < numPTEtoModifyInFirstPT; j++) {
    //     ptbr[j+ptOffset].pt_base = (addrToMap >> 12) + j ; 
    //     ptbr[j+ptOffset].pt_valid = 1;
    //     ptbr[j+ptOffset].pt_write = 1;
    // }





    //Modify PTEs in rest of the tables as required


    // uint32 numPDEtoModify = (uint32)( ( numPagesToMap + 1023) / 1024);
    // kprintf("In vmalloc: about to modify %d PDE's starting from offset: %d\n", numPDEtoModify, pdOffset);
    // uint32 pteToModifyForCurrentTable;
    // uint32 i = 0;
    // while()
    //     uint32 ptToModify;
    //     //check if pd_entries[i+pdOffset] already points to a table, if yes, do not grab a new table, else do grab one
    //     if(pd_entries[i+pdOffset].pd_pres == 0) {
    //         ptToModify = get_page_from_paging_area();
    //         pd_entries[i + pdOffset].pd_base = (ptToModify >> 12);
    //     } else {
    //         ptToModify = pd_entries[i+pdOffset].pd_base << 12 ;
    //     }
                
    //     pd_entries[i+pdOffset].pd_pres = 1;					    //SET PD PRESENT HERE, BUT NOT PRESENTS IN PTE, LAZY ALLOCATION NEEDED
	// 	pd_entries[i+pdOffset].pd_write = 1;					//set writeable
    //     pd_entries[i+pdOffset].pd_valid = 1;					//set valid

    //     //iterate over each page table and fill in PTEs
    //     if(numPagesToMap > 1024) {
    //         pteToModifyForCurrentTable = 1024;
    //     } else {
    //         pteToModifyForCurrentTable = numPagesToMap;
    //     }
    //     kprintf("In vmalloc: about to modify %d PTE's for PT at: %x\n", pteToModifyForCurrentTable, ptToModify);
    //     pt_t* ptbr = (pt_t*) ptToModify;
    //     for (uint32 j = 0 ; j < pteToModifyForCurrentTable; j++) {
    //         ptbr[j+ptOffset].pt_base = (addrToMap >> 12) + j ; 
    //         ptbr[j+ptOffset].pt_valid = 1;
    //         ptbr[j+ptOffset].pt_write = 1;
    //     }
    //     numPagesToMap = numPagesToMap - 1024;
    // }
    
    // for (uint32 i = 0 ; i < numPDEtoModify; i++ ) {
    //     uint32 ptToModify;
    //     //check if pd_entries[i+pdOffset] already points to a table, if yes, do not grab a new table, else do grab one
    //     if(pd_entries[i+pdOffset].pd_pres == 0) {
    //         ptToModify = get_page_from_paging_area();
    //         pd_entries[i + pdOffset].pd_base = (ptToModify >> 12);
    //     } else {
    //         ptToModify = pd_entries[i+pdOffset].pd_base << 12 ;
    //     }
                
    //     pd_entries[i+pdOffset].pd_pres = 1;					    //SET PD PRESENT HERE, BUT NOT PRESENTS IN PTE, LAZY ALLOCATION NEEDED
	// 	pd_entries[i+pdOffset].pd_write = 1;					//set writeable
    //     pd_entries[i+pdOffset].pd_valid = 1;					//set valid

    //     //iterate over each page table and fill in PTEs
    //     if(numPagesToMap > 1024) {
    //         pteToModifyForCurrentTable = 1024;
    //     } else {
    //         pteToModifyForCurrentTable = numPagesToMap;
    //     }
    //     kprintf("In vmalloc: about to modify %d PTE's for PT at: %x\n", pteToModifyForCurrentTable, ptToModify);
    //     pt_t* ptbr = (pt_t*) ptToModify;
    //     for (uint32 j = 0 ; j < pteToModifyForCurrentTable; j++) {
    //         ptbr[j+ptOffset].pt_base = (addrToMap >> 12) + j ; 
    //         ptbr[j+ptOffset].pt_valid = 1;
    //         ptbr[j+ptOffset].pt_write = 1;
    //     }
    //     numPagesToMap = numPagesToMap - 1024;
    // }
//     kprintf("In vmalloc: completed adding mappings!\n");


//     write_cr3(proctab[currpid].pdbr & 0xFFFFF000);

//     restore(mask);
//     return addr;
// }
