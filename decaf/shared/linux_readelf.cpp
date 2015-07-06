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

/*
   linux_readelf.c
   Extract elf information from modules inside DECAF, based on readelf from binutils
   Different from the original readelf, we do not need to support both x86 and x64 at
   the same time, nor do we need to support differnt platform neither.  We assume the
   target platform's architecture is the one we are going to read.

   by Kevin Wang, Sep 2013
*/

#include <iostream>
#include <istream>
#include <streambuf>
#include <sstream>
#include <inttypes.h>
#include <string>
#include <list>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <queue>
#include <sys/time.h>
#include <math.h>
#include <glib.h>
#include <mcheck.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "cpu.h"
#include "config.h"
#include "hw/hw.h" // AWH

#include "block.h"

#ifdef __cplusplus
};
#endif /* __cplusplus */




#include "DECAF_main.h"
#include "DECAF_target.h"
#include "shared/vmi.h"
#include "hookapi.h"
#include "function_map.h"
#include "shared/utils/SimpleCallback.h"
#include "linux_readelf.h"
//#include <elfio/elfio.hpp>
#include "shared/elfio/elfio.hpp"

#include "shared/DECAF_fileio.h"


#if HOST_LONG_BITS == 64
/* Define BFD64 here, even if our default architecture is 32 bit ELF
   as this will allow us to read in and parse 64bit and 32bit ELF files.
   Only do this if we believe that the compiler can support a 64 bit
   data type.  For now we only rely on GCC being able to do this.  */
#define BFD64
#endif

#define PACKAGE "libgrive"

#include "bfd.h"


using namespace ELFIO;


/* Call back action for file_walk
 */
static TSK_WALK_RET_ENUM
write_action(TSK_FS_FILE * fs_file, TSK_OFF_T a_off, TSK_DADDR_T addr,
    char *buf, size_t size, TSK_FS_BLOCK_FLAG_ENUM flags, void *ptr)
{
    if (size == 0)
        return TSK_WALK_CONT;

	std::string *sp = static_cast<std::string*>(ptr);

	sp->append(buf,size);

    return TSK_WALK_CONT;
}



// register symbol to DECAF
static void register_symbol(const char * mod_name, const char * func_name,
		const target_ulong func_addr, uint32_t inode_number)
{
	funcmap_insert_function(mod_name, func_name, func_addr, inode_number);
}



/* Process one ELF object */
int read_elf_info(const char * mod_name, target_ulong start_addr, unsigned int inode_number) {

	FILE *fp;
	fp = fopen("exported_symbols.log","a");

	
	bool header_present;	
	TSK_FS_FILE *file_fs = tsk_fs_file_open_meta(disk_info_internal[0].fs, NULL, (TSK_INUM_T)inode_number);

	void *file_stream = static_cast<void*>(new std::string());
	std::string *local_copy = static_cast<std::string*>(file_stream);
	
	int ret = 0;
	ret = tsk_fs_file_walk(file_fs, TSK_FS_FILE_WALK_FLAG_NONE, write_action, file_stream);

	std::istringstream is(*local_copy);
	
	elfio reader;
	Elf64_Addr elf_entry;
	bool found = false;
	header_present = reader.load(is);
	
	
    Elf_Half seg_num = reader.segments.size();
    std::cout << "Number of segments: " << seg_num << std::endl;
    for ( int i = 0; i < seg_num; ++i ) {
        const segment* pseg = reader.segments[i];

		if(pseg->get_type() == PT_LOAD) {
			elf_entry = pseg->get_virtual_address();
			found = true;
		}
		if(found)
			break;
    }

	
	Elf_Half sec_num = reader.sections.size();
	for ( int i = 0; i < sec_num; ++i ) {
		section* psec = reader.sections[i];
		// Check section type
		if ( psec->get_type() == SHT_DYNSYM || psec->get_type() == SHT_SYMTAB) {

			const symbol_section_accessor symbols( reader, psec );
			for ( unsigned int j = 0; j < symbols.get_symbols_num(); ++j ) {
				std::string   name;
				Elf64_Addr	  value;
				Elf_Xword	  size;
				unsigned char bind;
				unsigned char type;
				Elf_Half	  section_index;
				unsigned char other;
				
				// Read symbol properties
				symbols.get_symbol( j, name, value, size, bind,
									   type, section_index, other );
				//if( size>0  &&  type == STT_FUNC  && ( bind == STB_GLOBAL || bind == STB_WEAK  ) ) {
				if(type == STT_FUNC ) {
					register_symbol(mod_name, name.c_str(), (value-elf_entry), inode_number);
					fprintf(fp, "mod_name=\"%s\" elf_name=\"%s\" base_addr=\"%x\" func_addr= \"%x\" \n",mod_name, name.c_str(),elf_entry ,value);
					fflush(fp);
				}
			}
		}
	}

	fclose(fp);
    return true;
}


