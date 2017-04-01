/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

/*
Page fault:
1. permissions
2. virtual address space values
3.

*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

int nframes = 1;
int npages = 1;
int fifocounter = 0;
int *frameQueueP = 0;
int *frameModification = 0;
struct disk *disk = NULL;
char *physmem = NULL;
char *replacement = NULL;

int randomReplacement() {
	return rand()%nframes;
}

int fifoReplacement() {
	if(fifocounter >= nframes){
		fifocounter = 0;
	}
	return fifocounter;
}

int NRUReplacement() {
	return rand()%nframes;
}


void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);

	int curframe = -1, curbits = -1;
	int *frameP = &curframe;
	int *bitsp = &curbits;
	int frame = 0, replacementFrame = 0;

	page_table_get_entry(pt,page,frameP,bitsp);
	frameModification[curframe]++;

	for (frame = 0; frame < nframes; frame++) {

		if (frameQueueP[frame] == -1) {
			frameQueueP[frame] = page;
			page_table_set_entry(pt,page,frame,PROT_READ);
			frameModification[frame]++;
			disk_read(disk,page,&physmem[frame*PAGE_SIZE]);
			return;
		}

		if (curbits == 1) {
			page_table_set_entry(pt,page,*frameP,PROT_READ|PROT_WRITE);
			frameModification[curframe]++;
			disk_read(disk,page,&physmem[*frameP*PAGE_SIZE]);
			return;
		}

	}

	if (curframe == 0 && curbits == 0) {
		// remove a page from physical memory
		if(!strcmp(replacement,"rand")) {
			replacementFrame = randomReplacement();

		} else if(!strcmp(replacement,"fifo")) {
			replacementFrame = fifoReplacement();
			fifocounter++;

		} else if(!strcmp(replacement,"custom")) {
			replacementFrame = customReplacement();

		} else {
			fprintf(stderr,"unknown replacement algorithm: %s\n",replacement);
			exit(1);
		}

		page_table_get_entry(pt,frameQueueP[replacementFrame],frameP,bitsp);
		frameModification[curframe]++;

		if (curbits > 1) {
			disk_write(disk, frameQueueP[replacementFrame], &physmem[curframe*PAGE_SIZE]);
		}

		disk_read(disk,page,&physmem[curframe*PAGE_SIZE]);
		page_table_set_entry(pt,page,curframe,PROT_READ);
		page_table_set_entry(pt,frameQueueP[replacementFrame],0,0);
		frameQueueP[replacementFrame] = page;
		return;
	}

	// page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
	//disk_read(disk,2,&physmem[3*frame_size]);

}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
		return 1;
	}
	srand(time(NULL));
	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	replacement = argv[3];
	frameQueueP = malloc(sizeof(int) * nframes);
	frameModification = malloc(sizeof(int) * nframes);
	const char *program = argv[4];
	int i = 0;

	if (nframes > npages) {
		nframes = npages;
	}

	for (i = 0; i < nframes; i++) {
		frameQueueP[i] = -1;
		frameModification[i] = 0;
	}

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}


	page_table_print(pt);
	page_table_delete(pt);
	disk_close(disk);
	//free(frameQueueP);

	return 0;
}
