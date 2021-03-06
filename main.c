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
2. page doesnt exist in physmem
3. theres no room in physmem


Not all accesses occur on page faults


*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

//Global variables
int nframes = 1;
int npages = 1;
int fifocounter = 0;
int pageFaultCounter = 0;
int diskReads = 0;
int diskWrites = 0;
int customCounter = 0;
//Pointers for various arrays
int *frameQueueP = 0;
struct disk *disk = NULL;
char *physmem = NULL;
char *replacement = NULL;

int *customPlacement = 0; //Pointer for array used in custom replacement algorithm

//Function to shuffle custom frame replacement array
void shuffle(int *array, size_t n) {

    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
          size_t j = i + lrand48() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }

}

//Function that returns a random replacement frame number
int randomReplacement() {
	return lrand48()%nframes;
}

//Function that returns a fifo replacement frame number
int fifoReplacement() {
	if(fifocounter >= nframes){
		fifocounter = 0;
	}
	return fifocounter;
}

//Function that returns a custom replacement frame number
int customReplacement() {

	if (customCounter < nframes) {
		customCounter++;
	} else {
		customCounter = 0;
	}
	return	customPlacement[customCounter];
}

// Function to handle page faults
void page_fault_handler( struct page_table *pt, int page )
{
	pageFaultCounter++;

    // Variables and associated pointers to keep track of set bits and frame
	int curframe = -1, curbits = -1;
	int *frameP = &curframe;
	int *bitsp = &curbits;
	int frame = 0, replacementFrame = 0;

    // Get page table entry
	page_table_get_entry(pt,page,frameP,bitsp);

    // For loop that fills pages in order until all have been full
	for (frame = 0; frame < nframes; frame++) {

		if (frameQueueP[frame] == -1) {
			frameQueueP[frame] = page;
			page_table_set_entry(pt,page,frame,PROT_READ);
			disk_read(disk,page,&physmem[frame*PAGE_SIZE]);
			diskReads++;
			return;
		}

        // Update to read and write if read is already set
		if (curbits == 1) {
			page_table_set_entry(pt,page,curframe,PROT_READ|PROT_WRITE);
			disk_read(disk,page,&physmem[curframe*PAGE_SIZE]);
			diskReads++;
			return;
		}

	}

    // If all of the pages are full
	if (curframe == 0 && curbits == 0) {
		// remove a page from physical memory
        // rand algorithm
		if(!strcmp(replacement,"rand")) {
			replacementFrame = randomReplacement();

        // fifo algorithm
		} else if(!strcmp(replacement,"fifo")) {
			replacementFrame = fifoReplacement();
			fifocounter++;

        //custom algorithm
		} else if(!strcmp(replacement,"custom")) {
			replacementFrame = customReplacement();

		} else {
			fprintf(stderr,"unknown replacement algorithm: %s\n",replacement);
			exit(1);
		}

		page_table_get_entry(pt,frameQueueP[replacementFrame],frameP,bitsp);

		if (curbits > 1) {
			disk_write(disk, frameQueueP[replacementFrame], &physmem[curframe*PAGE_SIZE]);
			diskWrites++;
		}

		disk_read(disk,page,&physmem[curframe*PAGE_SIZE]);
		diskReads++;
		page_table_set_entry(pt,page,curframe,PROT_READ);
		page_table_set_entry(pt,frameQueueP[replacementFrame],0,0);
		frameQueueP[replacementFrame] = page;
		return;
	}


}

// Main Execution
int main( int argc, char *argv[] )
{
    // Reading in input and performing basic error checking
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}
	srand(time(NULL));
	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	replacement = argv[3];
	frameQueueP = malloc(sizeof(int) * nframes);
	customPlacement = malloc(sizeof(int) * nframes);
	const char *program = argv[4];
	int i = 0;

    if (nframes < 3){
        nframes = 3;
    }

    if (npages < 3){
        npages = 3;
    }

	if (nframes > npages) {
		nframes = npages;
	}

    //Initializing arrays for the frameQueue and custom placement
	for (i = 0; i < nframes; i++) {
		frameQueueP[i] = -1;
		customPlacement[i] = i;
	}

	shuffle(customPlacement, nframes);

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

    //Output
    printf("=======================\nProgram completed with:\n%d Page Faults\n%d Disk Reads\n%d Disk Writes\n", pageFaultCounter, diskReads, diskWrites);
	//printf("%s,%s,%d,%d,%d,%d,%d\n", replacement, program, npages, nframes, diskReads, diskWrites, pageFaultCounter );

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
