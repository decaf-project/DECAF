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

#ifdef __cplusplus
};
#endif /* __cplusplus */





#define PACKAGE "your-program-name"
#define PACKAGE_VERSION "1.2.3"


#include "DECAF_main.h"
#include "DECAF_target.h"
#include "shared/vmi.h"
#include "hookapi.h"
#include "function_map.h"
#include "shared/utils/SimpleCallback.h"
#include "linux_readelf.h"

#if HOST_LONG_BITS == 64
/* Define BFD64 here, even if our default architecture is 32 bit ELF
   as this will allow us to read in and parse 64bit and 32bit ELF files.
   Only do this if we believe that the compiler can support a 64 bit
   data type.  For now we only rely on GCC being able to do this.  */
#define BFD64
#endif

#include "bfd.h"
#include "elf/common.h"
#include "elf/external.h"
#include "elf/internal.h"

   
/* The following headers use the elf/reloc-macros.h file to
   automatically generate relocation recognition functions
   such as elf_mips_reloc_type()  */
#define RELOC_MACROS_GEN_FUNC

#ifdef TARGET_I386
#include "elf/i386.h"
#elif defined(TARGET_ARM)
#include "elf/arm.h"
#elif defined(TARGET_X86_64)
#include "elf/x86-64.h"
#endif

#if TARGET_LONG_BITS == 64

typedef uint64_t elf_vma;
typedef Elf64_External_Rel Elf_External_Rel;
typedef Elf64_External_Rela Elf_External_Rela;
typedef Elf64_External_Sym Elf_External_Sym;
typedef Elf64_External_Phdr Elf_External_Phdr;
typedef Elf64_External_Shdr Elf_External_Shdr;
typedef Elf64_External_Ehdr Elf_External_Ehdr;
typedef Elf64_External_Dyn Elf_External_Dyn;

#else

typedef uint32_t elf_vma;
typedef Elf32_External_Rel Elf_External_Rel;
typedef Elf32_External_Rela Elf_External_Rela;
typedef Elf32_External_Sym Elf_External_Sym;
typedef Elf32_External_Phdr Elf_External_Phdr;
typedef Elf32_External_Shdr Elf_External_Shdr;
typedef Elf32_External_Ehdr Elf_External_Ehdr;
typedef Elf32_External_Dyn Elf_External_Dyn;

#endif

// define the elf data we need to retrive
typedef struct _ELFInfo {
	const uint32_t cr3;
	const char * mod_name;
	const target_ulong start_addr;	// elf memory start address
	const uint64_t size;	// elf memory region size

	Elf_Internal_Ehdr elf_header;
	std::vector<Elf_Internal_Sym> dynamic_symbols;
	std::vector<Elf_Internal_Shdr> section_headers;
	std::vector<Elf_Internal_Phdr> program_headers;
	std::vector<Elf_Internal_Dyn> dynamic_section;
	bfd_vma dynamic_info[DT_ENCODING];
	int symtab_shndx_hdr_index;
	char * string_table;
	size_t string_table_length;
	char * dynamic_strings;
	size_t dynamic_strings_length;
	target_ulong dynamic_addr;
	bfd_size_type dynamic_size;
	
	_ELFInfo(CPUState *_env, const uint32_t _cr3, const char * _mod_name, const target_ulong _start_addr, const uint64_t _size):
		env(_env), cr3(_cr3), 
		mod_name(_mod_name), start_addr(_start_addr), size(_size), 
		string_table(NULL), string_table_length(0),
		dynamic_strings(NULL), dynamic_strings_length(0),
		dynamic_addr(0), dynamic_size(0),
		symtab_shndx_hdr_index(-1)
	{
		cur_cr3 = DECAF_getPGD(env);
	}
	
	~_ELFInfo() {
		delete [] string_table;
		delete [] dynamic_strings;

		string_table = NULL;
		string_table_length = 0;

		dynamic_strings = NULL;
		dynamic_strings_length = 0;
	}

	// read memory using DECAF
	int read_mem( target_ulong offset, target_ulong length, void * buf ) {
		// one thing we can check here is the offset of target address, if the memory does not
		// fall into the shared library memory region, we consider the content of ELF may corrupt
		if ( offset + length >= start_addr + size )
			return (-1);
		if ( cur_cr3 == cr3 )
			return DECAF_read_mem( env, start_addr + offset, length, buf );
		else
			return DECAF_read_mem_with_pgd( env, cr3, start_addr + offset, length, buf );
	}

private:
	// context info
	CPUState *env;
	uint32_t cur_cr3;

} ELFInfo;

#define BYTE_GET(field)		byte_get (field, sizeof (field))

#define NEW_INSTANCE(VAR, TYPE, SIZE)	\
	try {	\
		VAR = new TYPE[(size_t) (SIZE)];	\
	}	\
	catch (std::bad_alloc& ba) {	\
		delete [] VAR;	\
		VAR = NULL;	\
	}

#define DELETE(VAR)	\
	do {	\
		delete VAR;	\
		VAR = NULL;	\
	} while (0)

#define DELETE_ARR(VAR)	\
	do {	\
		delete [] VAR;	\
		VAR = NULL;	\
	} while (0)

// the following functions are used for translating endianess
#ifdef TARGET_WORDS_BIGENDIAN

elf_vma
byte_get (unsigned char *field, int size)
{
	switch (size)
	{
	case 1:
		return *field;

	case 2:
		return ((unsigned int) (field[1])) | (((int) (field[0])) << 8);

	case 3:
		return ((unsigned long) (field[2]))
			   |   (((unsigned long) (field[1])) << 8)
			   |   (((unsigned long) (field[0])) << 16);

	case 4:
		return ((unsigned long) (field[3]))
			   |   (((unsigned long) (field[2])) << 8)
			   |   (((unsigned long) (field[1])) << 16)
			   |   (((unsigned long) (field[0])) << 24);

	case 8:
		if (sizeof (elf_vma) == 8)
			return ((elf_vma) (field[7]))
				   |   (((elf_vma) (field[6])) << 8)
				   |   (((elf_vma) (field[5])) << 16)
				   |   (((elf_vma) (field[4])) << 24)
				   |   (((elf_vma) (field[3])) << 32)
				   |   (((elf_vma) (field[2])) << 40)
				   |   (((elf_vma) (field[1])) << 48)
				   |   (((elf_vma) (field[0])) << 56);
		else if (sizeof (elf_vma) == 4)
		{
			/* Although we are extracing data from an 8 byte wide field,
			   we are returning only 4 bytes of data.  */
			field += 4;
			return ((unsigned long) (field[3]))
				   |   (((unsigned long) (field[2])) << 8)
				   |   (((unsigned long) (field[1])) << 16)
				   |   (((unsigned long) (field[0])) << 24);
		}

	default:
		printf("Unhandled data length: %d\n", size);
		abort ();
	}
}

#else

elf_vma
byte_get (unsigned char *field, int size)
{
	switch (size)
	{
	case 1:
		return *field;

	case 2:
		return  ((unsigned int) (field[0]))
				|    (((unsigned int) (field[1])) << 8);

	case 3:
		return  ((unsigned long) (field[0]))
				|    (((unsigned long) (field[1])) << 8)
				|    (((unsigned long) (field[2])) << 16);

	case 4:
		return  ((unsigned long) (field[0]))
				|    (((unsigned long) (field[1])) << 8)
				|    (((unsigned long) (field[2])) << 16)
				|    (((unsigned long) (field[3])) << 24);

	case 8:
		if (sizeof (elf_vma) == 8)
			return  ((elf_vma) (field[0]))
					|    (((elf_vma) (field[1])) << 8)
					|    (((elf_vma) (field[2])) << 16)
					|    (((elf_vma) (field[3])) << 24)
					|    (((elf_vma) (field[4])) << 32)
					|    (((elf_vma) (field[5])) << 40)
					|    (((elf_vma) (field[6])) << 48)
					|    (((elf_vma) (field[7])) << 56);
		else if (sizeof (elf_vma) == 4)
			/* We want to extract data from an 8 byte wide field and
			   place it into a 4 byte wide field.  Since this is a little
			   endian source we can just use the 4 byte extraction code.  */
			return  ((unsigned long) (field[0]))
					|    (((unsigned long) (field[1])) << 8)
					|    (((unsigned long) (field[2])) << 16)
					|    (((unsigned long) (field[3])) << 24);

	default:
		printf("Unhandled data length: %d\n", size);
		abort ();
	}
}

#endif

// register symbol to DECAF
static void register_symbol(const char * mod_name, const char * func_name,
		const target_ulong func_addr)
{
	funcmap_insert_function(mod_name, func_name, func_addr);
}

// get section's name from string table
static inline const std::string
get_section_name (const Elf_Internal_Shdr& section, ELFInfo & elf) {
	if (!elf.string_table)
		return "<no-name>";
	else if (section.sh_name >= elf.string_table_length)
		return "<corrupt>";
	else
		return std::string(elf.string_table + section.sh_name);
}

// Decode the data held in 'elf_header'
static bool
process_section_header (ELFInfo & elf)
{
	if (elf.section_headers.empty())
		return true;
		
	if (elf.elf_header.e_phnum == PN_XNUM && elf.section_headers[0].sh_info != 0)
		elf.elf_header.e_phnum = elf.section_headers[0].sh_info;
	if (elf.elf_header.e_shnum == SHN_UNDEF)
		elf.elf_header.e_shnum = elf.section_headers[0].sh_size;
	if (elf.elf_header.e_shstrndx == (SHN_XINDEX & 0xffff))
		elf.elf_header.e_shstrndx = elf.section_headers[0].sh_link;
	else if (elf.elf_header.e_shstrndx >= elf.elf_header.e_shnum)
		elf.elf_header.e_shstrndx = SHN_UNDEF;
		
	return true;
}

// get program header
static int
get_program_headers (ELFInfo & elf)
{
	if ( !elf.program_headers.empty() )	// avoid reloading the program headers
		return true;

	if ( elf.elf_header.e_phentsize != sizeof(Elf_External_Phdr) ) {
		return false;
	}

	Elf_External_Phdr * external;
	NEW_INSTANCE( external, Elf_External_Phdr, elf.elf_header.e_phnum );
	if (!external || elf.read_mem(elf.elf_header.e_phoff, sizeof(Elf_External_Phdr) * elf.elf_header.e_phnum, external) < 0) {
		DELETE_ARR( external );
		return false;
	}

	for (size_t i = 0; i < elf.elf_header.e_phnum; i++)
	{
		Elf_Internal_Phdr internal;

		internal.p_type   = BYTE_GET (external[i].p_type);
		internal.p_offset = BYTE_GET (external[i].p_offset);
		internal.p_vaddr  = BYTE_GET (external[i].p_vaddr);
		internal.p_paddr  = BYTE_GET (external[i].p_paddr);
		internal.p_filesz = BYTE_GET (external[i].p_filesz);
		internal.p_memsz  = BYTE_GET (external[i].p_memsz);
		internal.p_flags  = BYTE_GET (external[i].p_flags);
		internal.p_align  = BYTE_GET (external[i].p_align);

		elf.program_headers.push_back( internal );
	}

	DELETE_ARR( external );
	return true;
}

// Returns 1 if the program headers were loaded.
static bool
process_program_headers (ELFInfo & elf)
{
	if (elf.elf_header.e_phnum == 0 || !get_program_headers (elf) )
		return false;

	for (std::vector<Elf_Internal_Phdr >::iterator segment = elf.program_headers.begin();
			segment != elf.program_headers.end();
			segment++) {
		if (segment->p_type == PT_DYNAMIC) {

			/* By default, assume that the .dynamic section is the first
			   section in the DYNAMIC segment.  */
			elf.dynamic_addr = segment->p_offset;
			elf.dynamic_size = segment->p_filesz;

			/* Try to locate the .dynamic section. If there is
			   a section header table, we can easily locate it.  */
			if (!elf.section_headers.empty()) {
				std::vector<Elf_Internal_Shdr>::iterator sec = elf.section_headers.begin();
				for ( ; sec != elf.section_headers.end(); sec++) {
					if ( !get_section_name(*sec, elf).compare(".dynamic") )
						break;
				}

				if ( sec == elf.section_headers.end() || sec->sh_size == 0 )
					break;
				else if (sec->sh_type == SHT_NOBITS)
				{
					elf.dynamic_size = 0;
					break;
				}
				elf.dynamic_addr = sec->sh_offset;
				elf.dynamic_size = sec->sh_size;
			}
			break;
		}
	}

	return true;
}

/* Find the file offset corresponding to VMA by using the program headers.  */

static long
offset_from_vma (ELFInfo & elf, bfd_vma vma, bfd_size_type size)
{
	if ( !get_program_headers (elf))
	{
		// Cannot interpret virtual addresses without program headers
		return (long) vma;
	}

	for ( std::vector<Elf_Internal_Phdr>::iterator seg = elf.program_headers.begin();
			seg != elf.program_headers.end();
			seg++)
	{
		if (seg->p_type != PT_LOAD)
			continue;

		if (vma >= (seg->p_vaddr & -seg->p_align)
				&& vma + size <= seg->p_vaddr + seg->p_filesz)
			return vma - seg->p_vaddr + seg->p_offset;
	}

	return (long) vma;
}

// read specific number of elf header sections
static bool
read_elf_section_headers ( ELFInfo & elf, size_t readNum )
{
	Elf_External_Shdr * shdrs;
	NEW_INSTANCE(shdrs, Elf_External_Shdr, readNum);
	if (!shdrs || elf.read_mem(elf.elf_header.e_shoff, sizeof(Elf_External_Shdr) * readNum, shdrs) < 0) {
		DELETE_ARR( shdrs );
		return false;
	}

	if ( !elf.section_headers.empty() )	// read all the section headers again
		elf.section_headers.erase( elf.section_headers.begin(), elf.section_headers.end() );

	bool readError = false;
	for (size_t i = 0; !readError && i < readNum; i++)
	{
		Elf_Internal_Shdr internal;

		internal.sh_name      = BYTE_GET (shdrs[i].sh_name);
		internal.sh_type      = BYTE_GET (shdrs[i].sh_type);
		internal.sh_flags     = BYTE_GET (shdrs[i].sh_flags);
		internal.sh_addr      = BYTE_GET (shdrs[i].sh_addr);
		internal.sh_offset    = BYTE_GET (shdrs[i].sh_offset);
		internal.sh_size      = BYTE_GET (shdrs[i].sh_size);
		internal.sh_link      = BYTE_GET (shdrs[i].sh_link);
		internal.sh_info      = BYTE_GET (shdrs[i].sh_info);
		internal.sh_addralign = BYTE_GET (shdrs[i].sh_addralign);
		internal.sh_entsize   = BYTE_GET (shdrs[i].sh_entsize);
		
		readError = (
			(internal.sh_type != SHT_NULL && internal.sh_size == 0)	// invalid section header
			|| (internal.sh_offset + internal.sh_size >= elf.size)	// address out of bound
			);

		elf.section_headers.push_back(internal);
	}

	DELETE_ARR(shdrs);
	
	return !readError;
}

static bool
get_elf_symbols ( ELFInfo & elf, const Elf_Internal_Shdr & section, std::vector<Elf_Internal_Sym> & symbols )
{
	Elf_External_Sym * esyms = NULL;
	Elf_External_Sym_Shndx * shndx = NULL;

	/* Run some sanity checks first.  */
	if ( section.sh_size == 0 || sizeof (Elf_External_Sym) != section.sh_entsize )
	{
		// there may be some error in the memory read by DECAF
		return false;
	}

	size_t number = section.sh_size / sizeof (Elf_External_Sym);

	if (number * sizeof (Elf_External_Sym) != section.sh_size)
	{
		return false;
	}

	NEW_INSTANCE(esyms, Elf_External_Sym, number);
	if (!esyms || elf.read_mem(section.sh_offset, number * sizeof (Elf_External_Sym), esyms) < 0) {
		DELETE_ARR( esyms );
		return false;
	}

	if ( elf.symtab_shndx_hdr_index > -1
			&& ( (int) elf.section_headers[elf.symtab_shndx_hdr_index].sh_link == elf.symtab_shndx_hdr_index ) )
	{
		if (number * sizeof(Elf_External_Sym_Shndx) < elf.section_headers[elf.symtab_shndx_hdr_index].sh_size) {
			DELETE_ARR( esyms );
			return false;
		}
		NEW_INSTANCE(shndx, Elf_External_Sym_Shndx, number);
		if (!shndx || elf.read_mem(elf.section_headers[elf.symtab_shndx_hdr_index].sh_offset, elf.section_headers[elf.symtab_shndx_hdr_index].sh_size, shndx) < 0) {
			DELETE_ARR( esyms );
			DELETE_ARR( shndx );
			return false;
		}
	}

	if (!symbols.empty())
		symbols.erase( symbols.begin(), symbols.end() );

	for (size_t i = 0; i < number; i++)
	{
		Elf_Internal_Sym tmp;
		tmp.st_name  = BYTE_GET (esyms[i].st_name);
		tmp.st_value = BYTE_GET (esyms[i].st_value);
		tmp.st_size  = BYTE_GET (esyms[i].st_size);
		tmp.st_shndx = BYTE_GET (esyms[i].st_shndx);
		if (tmp.st_shndx == (SHN_XINDEX & 0xffff) && shndx != NULL)
			tmp.st_shndx
				= byte_get ((unsigned char *) &shndx[i], sizeof (shndx[i]));
		else if (tmp.st_shndx >= (SHN_LORESERVE & 0xffff))
			tmp.st_shndx += SHN_LORESERVE - (SHN_LORESERVE & 0xffff);
		tmp.st_info  = BYTE_GET (esyms[i].st_info);
		tmp.st_other = BYTE_GET (esyms[i].st_other);
		
		symbols.push_back(tmp);
	}

	DELETE_ARR( esyms );
	DELETE_ARR( shndx );
	return true;
}

static bool
process_section_headers ( ELFInfo & elf )
{
	std::vector<Elf_Internal_Shdr>::iterator section;

	if (elf.elf_header.e_shnum == 0 && elf.elf_header.e_shoff != 0) {
		//possibly corrupt ELF file header - it has a non-zero section header offset, but no section headers
		return false;
	}
	else if ( elf.section_headers.empty() )
		return true;

	if ( !read_elf_section_headers ( elf, elf.elf_header.e_shnum ) )
		return false;

	/* Read in the string table, so that we have names to display.  */
	if (elf.elf_header.e_shstrndx != SHN_UNDEF && elf.elf_header.e_shstrndx < elf.elf_header.e_shnum) {
		section = elf.section_headers.begin() + elf.elf_header.e_shstrndx;

		if (section->sh_type == SHT_STRTAB && section->sh_size > 0) {
			NEW_INSTANCE(elf.string_table, char, section->sh_size + 1);	// NOTICE: one extra char to avoid buffer over flow
			if (elf.string_table) {
				elf.string_table[section->sh_size] = '\0';
				if (elf.read_mem(section->sh_offset, elf.string_table_length = section->sh_size, elf.string_table) < 0) {
					DELETE_ARR( elf.string_table );
					elf.string_table_length = 0;
					return false;
				}
			}
			else {
				return false;
			}
		}
		else {	// consider current module as not-fully loaded.  We cannot extract any symbol from a null string table
			return false;
		}
	}

	/* Scan the sections for the dynamic symbol table
	   and dynamic string table and debug sections.  */
	for (section = elf.section_headers.begin(); section != elf.section_headers.end(); section++) {
		const std::string name = get_section_name( *section, elf );
		if (section->sh_type == SHT_DYNSYM) {
			if ( !elf.dynamic_symbols.empty() ) {
				monitor_printf ( default_mon, "File contains multiple dynamic symbol tables\n");
				continue;
			}

			if (section->sh_entsize != sizeof(Elf_External_Sym)) {
				return false;
			}
			if ( !get_elf_symbols ( elf, *section, elf.dynamic_symbols ) )
				return false;
		}
		else if (section->sh_type == SHT_STRTAB && !name.compare(".dynstr")) {
			if (elf.dynamic_strings) {
				continue;
			}
			NEW_INSTANCE(elf.dynamic_strings, char, section->sh_size + 1);	// NOTICE: one extra char to avoid buffer over flow
			if (elf.dynamic_strings) {
				elf.dynamic_strings[section->sh_size] = '\0';
				if (elf.read_mem(section->sh_offset, elf.dynamic_strings_length = section->sh_size, elf.dynamic_strings) < 0) {
					DELETE_ARR( elf.dynamic_strings );
					elf.dynamic_strings_length = 0;
					return false;
				}
			}
			else {
				return false;
			}
		}
		else if (section->sh_type == SHT_SYMTAB_SHNDX) {
			if (elf.symtab_shndx_hdr_index > -1) {
				continue;
			}
			elf.symtab_shndx_hdr_index = section - elf.section_headers.begin();	// compute symbol table index

		}
		else if (section->sh_type == SHT_SYMTAB && section->sh_entsize != sizeof(Elf_External_Sym)) {
			return false;
		}
	}

	return true;
}

#if 0

// get the dynamic section from ELF file
static bool
get_dynamic_section (ELFInfo & elf)
{
	Elf_External_Dyn * edyn;

	size_t section_count = elf.dynamic_size / sizeof(Elf_External_Dyn);
	NEW_INSTANCE( edyn, Elf_External_Dyn, section_count );
	if (!edyn || elf.read_mem(elf.dynamic_addr, sizeof(Elf_External_Dyn) * section_count, edyn) < 0) {
		DELETE_ARR( edyn );
		return false;
	}

	// normally sizeof(Elf_External_Dyn) * section_count should be equal to elf.dynamic_size, but just in case..
	for (size_t i = 0; i < section_count; i++) {
		Elf_Internal_Dyn entry;
		entry.d_tag = BYTE_GET ( (edyn+i)->d_tag );
		entry.d_un.d_val = BYTE_GET ( (edyn+i)->d_un.d_val );
		
		elf.dynamic_section.push_back( entry );

		if ( entry.d_tag == DT_NULL )
			break;
	}

	DELETE_ARR( edyn );
	return true;
}

// Parse and display the contents of the dynamic section.
static bool
process_dynamic_section ( ELFInfo & elf )
{
	if (elf.dynamic_size == 0) // There is no dynamic section
		return true;

	if ( !get_dynamic_section (elf) )
		return false;

	/* Find the appropriate symbol table.  */
	if ( elf.dynamic_symbols.empty() ) {
		for (std::vector<Elf_Internal_Dyn>::iterator it = elf.dynamic_section.begin();
				it != elf.dynamic_section.end();
				++it) {

			Elf_Internal_Shdr section;
			if (it->d_tag != DT_SYMTAB)
				continue;

			elf.dynamic_info[DT_SYMTAB] = it->d_un.d_val;
			section.sh_offset = offset_from_vma (elf, it->d_un.d_val, 0);
			section.sh_size = elf.size + elf.start_addr - section.sh_offset;
			section.sh_entsize = sizeof (Elf_External_Sym);
			if ( !get_elf_symbols ( elf, section, elf.dynamic_symbols ) )
				return false;
			if ( elf.dynamic_symbols.empty() )
			{
				monitor_printf ( default_mon, "Unable to determine the number of symbols to load\n");
				continue;
			}
		}
	}

	/* Similarly find a string table.  */
	if ( elf.dynamic_strings == NULL )
	{
		for (std::vector<Elf_Internal_Dyn>::iterator it = elf.dynamic_section.begin();
				it != elf.dynamic_section.end();
				++it) {
			unsigned long offset;
			long str_tab_len;

			if (it->d_tag != DT_STRTAB)
				continue;

			elf.dynamic_info[DT_STRTAB] = it->d_un.d_val;
			offset = offset_from_vma (elf, it->d_un.d_val, 0);
			str_tab_len = elf.size + elf.start_addr - offset;

			if (str_tab_len < 1)
			{
				monitor_printf ( default_mon, "Unable to determine the length of the dynamic string table\n" );
				continue;
			}

			NEW_INSTANCE(elf.dynamic_strings, char, str_tab_len + 1);	// NOTICE: one extra char to avoid buffer over flow
			if (elf.dynamic_strings) {
				elf.dynamic_strings[str_tab_len] = '\0';
				if (elf.read_mem(offset, elf.dynamic_strings_length = str_tab_len, elf.dynamic_strings) < 0) {
					DELETE_ARR( elf.dynamic_strings );
					elf.dynamic_strings_length = 0;
					return false;
				}
			}
			else {
				return false;
			}
			break;
		}
	}

	return true;
}
#endif

/* Dump the symbol table.  */
static bool
process_symbol_table ( ELFInfo  & elf )
{
	for ( std::vector<Elf_Internal_Shdr>::iterator section = elf.section_headers.begin();
			section != elf.section_headers.end();
			section++)
	{

		// only deal with symbol table or dynamic symbol table
		if (section->sh_type != SHT_SYMTAB && section->sh_type != SHT_DYNSYM)
			continue;

		//monitor_printf( default_mon, "type %u size %u \n", section->sh_type, section->sh_size );
		char * strtab = NULL;
		size_t strtab_size = 0;
		std::vector<Elf_Internal_Sym> symtab;

		if (section->sh_entsize == 0)
		{
			// sh_entsize is zero
			continue;
		}

#if 0
		if ( !strcmp( "libc.so.6", elf.mod_name ) )
		monitor_printf ( default_mon, "[%s] symbol table '%s' contains %lu entries: (size %u, entsize %u)\n",
				elf.mod_name, get_section_name (*section, elf).c_str() ,
				(unsigned long) (section->sh_size / section->sh_entsize),
				section->sh_size, section->sh_entsize);
#endif

		// symtab will be cleaned inside the function
		if ( !get_elf_symbols ( elf, *section, symtab ) )
			return false;
		if ( symtab.empty() )
			continue;

		if ( section->sh_link == elf.elf_header.e_shstrndx )
		{
			strtab = elf.string_table;
			strtab_size = elf.string_table_length;
		}
		else if ( section->sh_link < elf.elf_header.e_shnum )
		{
			Elf_Internal_Shdr string_sec = elf.section_headers[section->sh_link];
			
			NEW_INSTANCE( strtab, char, string_sec.sh_size + 1);	// NOTICE: one extra char to avoid buffer over flow
			if ( strtab ) {
				strtab[string_sec.sh_size] = '\0';
				if ( elf.read_mem(string_sec.sh_offset, strtab_size = string_sec.sh_size, strtab) < 0 ) {
					DELETE_ARR( strtab );
					strtab_size = 0;
					return false;
				}
			}
			else {
				return false;
			}
		}

		if (!strtab)	// can't do anything with empty string table
			continue;

		size_t sym_count = 0;
		// finally, we can present it to DECAF
		for ( std::vector<Elf_Internal_Sym>::iterator it = symtab.begin(); it != symtab.end(); it++ )
		{
			if ( !it->st_value )	// when offset is zero, it is most likely system function
				continue;
			// we only care about a couple types of symbols
			if ( ELF_ST_TYPE (it->st_info) == STT_FUNC ) {	// function symbols
				char * symbol_name = it->st_name < strtab_size ? strtab + it->st_name : NULL;
				if (!symbol_name) {	// there is no meaning having a corrupted symbol name
					if ( strtab != elf.string_table )
						DELETE_ARR ( strtab );
					return false;
				}

				sym_count++;

				register_symbol ( elf.mod_name, symbol_name, it->st_value );
#if 0
				//if ( !strcmp( "libc.so.6", elf.mod_name ) )
					monitor_printf( default_mon, "symbol from [%s] [%s]@[%08x] \n", elf.mod_name, symbol_name, it->st_value );
#endif
			}
		}
#if 0
		//if ( !strcmp( "libc.so.6", elf.mod_name ) )
			monitor_printf( default_mon, " %u symbols are extracted from [%s] \n", sym_count, elf.mod_name );
#endif
		
		if ( strtab != elf.string_table )
			DELETE_ARR ( strtab );
	}

	return true;
}

static bool
read_elf_header (ELFInfo & elf)
{
	Elf_External_Ehdr ehdr;

	// verify the magic bytes of ELF header
	if (
		elf.read_mem( 0, sizeof(ehdr), &ehdr ) < 0
		|| ehdr.e_ident[EI_MAG0] != ELFMAG0
		|| ehdr.e_ident[EI_MAG1] != ELFMAG1
		|| ehdr.e_ident[EI_MAG2] != ELFMAG2
		|| ehdr.e_ident[EI_MAG3] != ELFMAG3
	) {
		return false;
	}

	elf.elf_header.e_type      = BYTE_GET (ehdr.e_type);
	elf.elf_header.e_version   = BYTE_GET (ehdr.e_version);
	elf.elf_header.e_entry     = BYTE_GET (ehdr.e_entry);
	elf.elf_header.e_phoff     = BYTE_GET (ehdr.e_phoff);
	elf.elf_header.e_shoff     = BYTE_GET (ehdr.e_shoff);		// section header offset
	elf.elf_header.e_flags     = BYTE_GET (ehdr.e_flags);
	elf.elf_header.e_ehsize    = BYTE_GET (ehdr.e_ehsize);
	elf.elf_header.e_phentsize = BYTE_GET (ehdr.e_phentsize);
	elf.elf_header.e_phnum     = BYTE_GET (ehdr.e_phnum);
	elf.elf_header.e_shentsize = BYTE_GET (ehdr.e_shentsize);	// section header size
	elf.elf_header.e_shnum     = BYTE_GET (ehdr.e_shnum);		// section header number
	elf.elf_header.e_shstrndx  = BYTE_GET (ehdr.e_shstrndx);

	if (elf.elf_header.e_shentsize != sizeof(Elf_External_Shdr)) {
		//monitor_printf(default_mon, "elf_header.e_shentsize not correct. \n");
		return false;
	}

	if ((elf.elf_header.e_shoff > 0 && elf.elf_header.e_shnum == 0) || !read_elf_section_headers ( elf, 1 )) {
		//monitor_printf(default_mon, "ELF object section headers corrupted. \n");
		return false;
	}

	//if ( !strcmp( "libc.so.6", elf.mod_name ) )
	//	monitor_printf( default_mon, " %u symbol table index, section number %u \n", elf.elf_header.e_shstrndx, elf.elf_header.e_shnum );

	return true;
}

/* Process one ELF object */
int read_elf_info(CPUState *env, uint32_t cr3, const char * mod_name, target_ulong start_addr, uint64_t size) {
	ELFInfo elf ( env, cr3, mod_name, start_addr, size );

	int res = (cr3 != -1U)
		&& read_elf_header (elf)
		&& process_section_header (elf)
		&& process_section_headers (elf)
		&& process_program_headers (elf)
		//&& process_dynamic_section (elf)
		&& process_symbol_table (elf)
		;

	return res;
}


