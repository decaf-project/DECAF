/*
 Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

 DECAF is based on QEMU, a whole-system emulator. You can redistribute
 and modify it under the terms of the GNU GPL, version 3 or later,
 but it is made available WITHOUT ANY WARRANTY. See the top-level
 README file for more details.

 For more information about DECAF and other softwares, see our
 web site at:
 http://sycurelab.ecs.syr.edu/

 If you have any questions about DECAF,please post it on
 http://code.google.com/p/decaf-platform/
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "config.h"
//#include "TEMU_lib.h"
#include "DECAF_main.h"
#include "DECAF_callback.h"
#include "DECAF_target.h"

typedef struct {
  uint64_t bitmap; 	/*!<one bit for each byte. */
  uint8_t records[0]; 	
}mark_entry_t;

typedef struct {
  mark_entry_t *entry[64];
}mark_page_t;

mark_page_t *mark_mem_space[2<<20];


int mem_mark_init()
{
	bzero(mark_mem_space, sizeof(mark_mem_space));
	return 0;
}

int mem_mark_cleanup()
{
	int i, j;
	
	for(i=0; i<sizeof(mark_mem_space)/4; i++) {
		mark_page_t *page = mark_mem_space[i];
		if(!page) continue;
		
		for(j=0; j<64; j++) {
			if(page->entry[j]) free(page->entry[j]);
		}
		
		free(page);
		mark_mem_space[i] = NULL;
	
	}
	return 0;
}

#define min(a,b) ((a>b)? b:a)
static inline uint64_t size_to_ones(int size)
{
	return (size<64)?(1ULL<<size)-1: (uint64_t)(-1);
}

int set_mem_mark(uint32_t vaddr, uint32_t size, uint64_t mark_bitmap)
{
	uint32_t i, j, len, addr2;
	
	for(i=0, addr2 = vaddr; i<size; i+=len, addr2+=len) {
		uint32_t page_index = addr2>>12;
		uint32_t entry_index = (addr2&(TARGET_PAGE_SIZE-1))>>6;
		uint32_t offset = addr2&63;
		len = min(size-i, 64-offset);
		uint64_t bitmap2 = (mark_bitmap>>i) & size_to_ones(len);
		
		mark_page_t * page;
		if(!(page = mark_mem_space[page_index])) {
			if(!bitmap2) continue;
		
			page = (mark_page_t *)malloc(sizeof(mark_page_t));
			if(!page) return 0;
		
		    bzero(page, sizeof(mark_page_t));
			mark_mem_space[page_index] = page;
		}
		mark_entry_t *entry = page->entry[entry_index];
		if(!entry) {
			if(!bitmap2) continue;
			
			entry = (mark_entry_t *)malloc(sizeof(mark_entry_t));
			if(!entry) return 0;
			
			bzero(entry, sizeof(mark_entry_t));
			page->entry[entry_index] = entry;
			entry->bitmap = bitmap2<<offset;
		} else {
			entry->bitmap &= ~(size_to_ones(len)<<offset);
			entry->bitmap |= bitmap2 <<offset;
			if(entry->bitmap == 0) {
				free(entry);
				page->entry[entry_index] = NULL;
				for(j=0; j<64; j++) 
					if((page->entry[j])) break;
				if(j>=64) {
					free(page);
					mark_mem_space[page_index] = NULL;
				}
			}
		}
	}
	
	return 0;
}

/*
 * size<=64;
 * return value: a bitmap
 */
uint64_t check_mem_mark(uint32_t vaddr, uint32_t size)
{
	uint32_t i, len, addr2;
	uint64_t mark = 0;
	
	for(i=0, addr2 = vaddr; i<size; i+=len, addr2+=len) {
		uint32_t page_index = addr2>>12;
		uint32_t entry_index = (addr2&(TARGET_PAGE_SIZE-1))>>6;
		uint32_t offset = addr2&63;
		len = min(size-i, 64-offset);
		
		mark_page_t * page;
		if(!(page = mark_mem_space[page_index])) 
			continue;
		mark_entry_t *entry = page->entry[entry_index];
		if(!entry) continue;
		mark |= ((entry->bitmap>>offset) & size_to_ones(len))<<i;
	}
	return mark;
}



