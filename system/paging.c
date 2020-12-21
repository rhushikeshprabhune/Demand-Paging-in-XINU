#include <xinu.h>

#define PT_AREA_START 0x2000000

uint32 free_page = PT_AREA_START;

bool8 used_page_tracker[MAX_PT_SIZE];

uint32 get_page_from_paging_area() {


    uint32 entryNumber = 0;
    while(entryNumber != MAX_PT_SIZE) {
        if(!used_page_tracker[entryNumber]) {
            break;
        }
        entryNumber += 1;
    }
    used_page_tracker[entryNumber] = 1;
    return PT_AREA_START + entryNumber * PAGE_SIZE ;

    //OLD ALLOCATOR
    // uint32 to_return ;
    // to_return = free_page;
    // free_page = free_page + 0x1000;

    // if( (free_page - PT_AREA_START)  > 0x400000) {
    //     //out of pages in page table area
    //     kprintf("\n\n**************ERROR, OUT OF PAGE TABLE AREA!!!!******************\n\n\n");
    // }
    // return to_return;


}

uint32 free_page_directory_from_paging_area(uint32 pd) {
    pd_t* pd_entries = (pd_t*) pd;
    for(uint32 i = 0; i < 1024; i++) {
        
        //now mark eache PTE for each of the pages pointed to by the page directory
        if(pd_entries[i].pd_pres == 1) {
            uint32 pt_start_addr = (pd_entries[i].pd_base << 12);
            pt_t* pt_entries = (pt_t*) pt_start_addr;
            // kprintf("freeing PTE for PT starting at %x\n", pt_start_addr);
            for(uint32 j=0; j < 1024; j++) {
                pt_entries[j].pt_valid = 0;
                pt_entries[j].pt_pres = 0;
                pt_entries[j].pt_write = 0;
                pt_entries[j].pt_base = 0;
            }
            uint32 toFree = pt_start_addr >> 12 ;
            used_page_tracker[ toFree & 0x3FF] = 0;
        }
        pd_entries[i].pd_valid = 0;
        pd_entries[i].pd_pres = 0;
        pd_entries[i].pd_write = 0;
        pd_entries[i].pd_base = 0;
    }
    // kprintf("Freeing entry number %d in free_page_tracker!\n", (pd >> 12) & 0x000003FF);
    used_page_tracker[ (pd >> 12) & 0x000003FF] = 0;
}

void map_pages(uint32 pd, uint32 num_of_pages){
    pd_t* pd_entry = (pd_t*) pd;

    //map XINU pages
	for (uint32 i = 0; i < num_of_pages; i++) {
		uint32 new_page_from_pt_area = get_page_from_paging_area();
		//initialise directory entry to point to new PT
		// kprintf("setting PD entry at %x : base set as %x\n", &pd_entry[i], (new_page_from_pt_area >> 12));
		
		pd_entry[i].pd_base = (new_page_from_pt_area >> 12);			//set base
		pd_entry[i].pd_pres = 1;										//set present
		pd_entry[i].pd_write = 1;										//set writeable
        pd_entry[i].pd_valid = 1;										//set valid

		pt_t* new_pt = (pt_t*) new_page_from_pt_area;
		for (uint32 j=0; j < 1024; j++) {
			//initialise each entry in page table - flat mapping should be created
			// if (j < 5|| j > 1018) {
			// 	kprintf("PT entry at %x : base set as %x\n", &new_pt[j], ((i<<10) | j));
			// }
			new_pt[j].pt_base =  (i<<10) | j ;								//set base
			new_pt[j].pt_pres = 1;											//set present
			new_pt[j].pt_write = 1;											//set writeable
			new_pt[j].pt_valid = 1;											//set valid
		}
	}

    //map PT area
   
}

