/* ELF support for BFD.
   Copyright 1991, 1992, 1993, 1994, 1995, 1997, 1998, 2000, 2001, 2002,
   2003, 2006, 2007, 2008, 2010, 2011 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support, from information published
   in "UNIX System V Release 4, Programmers Guide: ANSI C and
   Programming Support Tools".

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* This file is part of ELF support for BFD, and contains the portions
   that describe how ELF is represented internally in the BFD library.
   I.E. it describes the in-memory representation of ELF.  It requires
   the elf-common.h file which contains the portions that are common to
   both the internal and external representations.  */

/* NOTE that these structures are not kept in the same order as they appear
   in the object file.  In some cases they've been reordered for more optimal
   packing under various circumstances.  */

#ifndef _ELF_INTERNAL_H
#define _ELF_INTERNAL_H

/* Special section indices, which may show up in st_shndx fields, among
   other places.  */

#undef SHN_UNDEF
#undef SHN_LORESERVE
#undef SHN_LOPROC
#undef SHN_HIPROC
#undef SHN_LOOS
#undef SHN_HIOS
#undef SHN_ABS
#undef SHN_COMMON
#undef SHN_XINDEX
#undef SHN_HIRESERVE
#define SHN_UNDEF	0		/* Undefined section reference */
#define SHN_LORESERVE	(-0x100u)	/* Begin range of reserved indices */
#define SHN_LOPROC	(-0x100u)	/* Begin range of appl-specific */
#define SHN_HIPROC	(-0xE1u)	/* End range of appl-specific */
#define SHN_LOOS	(-0xE0u)	/* OS specific semantics, lo */
#define SHN_HIOS	(-0xC1u)	/* OS specific semantics, hi */
#define SHN_ABS		(-0xFu)		/* Associated symbol is absolute */
#define SHN_COMMON	(-0xEu)		/* Associated symbol is in common */
#define SHN_XINDEX	(-0x1u)		/* Section index is held elsewhere */
#define SHN_HIRESERVE	(-0x1u)		/* End range of reserved indices */
#define SHN_BAD		(-0x101u)	/* Used internally by bfd */

/* ELF Header */

#define EI_NIDENT	16		/* Size of e_ident[] */

typedef struct elf_internal_ehdr {
  unsigned char		e_ident[EI_NIDENT]; /* ELF "magic number" */
  bfd_vma		e_entry;	/* Entry point virtual address */
  bfd_size_type		e_phoff;	/* Program header table file offset */
  bfd_size_type		e_shoff;	/* Section header table file offset */
  unsigned long		e_version;	/* Identifies object file version */
  unsigned long		e_flags;	/* Processor-specific flags */
  unsigned short	e_type;		/* Identifies object file type */
  unsigned short	e_machine;	/* Specifies required architecture */
  unsigned int		e_ehsize;	/* ELF header size in bytes */
  unsigned int		e_phentsize;	/* Program header table entry size */
  unsigned int		e_phnum;	/* Program header table entry count */
  unsigned int		e_shentsize;	/* Section header table entry size */
  unsigned int		e_shnum;	/* Section header table entry count */
  unsigned int		e_shstrndx;	/* Section header string table index */
} Elf_Internal_Ehdr;

/* Program header */

typedef struct elf_internal_phdr {
  unsigned long	p_type;			/* Identifies program segment type */
  unsigned long	p_flags;		/* Segment flags */
  bfd_vma	p_offset;		/* Segment file offset */
  bfd_vma	p_vaddr;		/* Segment virtual address */
  bfd_vma	p_paddr;		/* Segment physical address */
  bfd_vma	p_filesz;		/* Segment size in file */
  bfd_vma	p_memsz;		/* Segment size in memory */
  bfd_vma	p_align;		/* Segment alignment, file & memory */
} Elf_Internal_Phdr;

/* Section header */

typedef struct elf_internal_shdr {
  unsigned int	sh_name;		/* Section name, index in string tbl */
  unsigned int	sh_type;		/* Type of section */
  bfd_vma	sh_flags;		/* Miscellaneous section attributes */
  bfd_vma	sh_addr;		/* Section virtual addr at execution */
  file_ptr	sh_offset;		/* Section file offset */
  bfd_size_type	sh_size;		/* Size of section in bytes */
  unsigned int	sh_link;		/* Index of another section */
  unsigned int	sh_info;		/* Additional section information */
  bfd_vma	sh_addralign;		/* Section alignment */
  bfd_size_type	sh_entsize;		/* Entry size if section holds table */

  /* The internal rep also has some cached info associated with it. */
  asection *	bfd_section;		/* Associated BFD section.  */
  unsigned char *contents;		/* Section contents.  */
} Elf_Internal_Shdr;

/* Symbol table entry */

typedef struct elf_internal_sym {
  bfd_vma	st_value;		/* Value of the symbol */
  bfd_vma	st_size;		/* Associated symbol size */
  unsigned long	st_name;		/* Symbol name, index in string tbl */
  unsigned char	st_info;		/* Type and binding attributes */
  unsigned char	st_other;		/* Visibilty, and target specific */
  unsigned char st_target_internal;	/* Internal-only information */
  unsigned int  st_shndx;		/* Associated section index */
} Elf_Internal_Sym;

/* Relocation Entries */

typedef struct elf_internal_rela {
  bfd_vma	r_offset;	/* Location at which to apply the action */
  bfd_vma	r_info;		/* Index and Type of relocation */
  bfd_vma	r_addend;	/* Constant addend used to compute value */
} Elf_Internal_Rela;

/* dynamic section structure */

typedef struct elf_internal_dyn {
  /* This needs to support 64-bit values in elf64.  */
  bfd_vma d_tag;		/* entry tag value */
  union {
    /* This needs to support 64-bit values in elf64.  */
    bfd_vma	d_val;
    bfd_vma	d_ptr;
  } d_un;
} Elf_Internal_Dyn;

/* Structure for syminfo section.  */
typedef struct
{
  unsigned short int 	si_boundto;
  unsigned short int	si_flags;
} Elf_Internal_Syminfo;


#endif /* _ELF_INTERNAL_H */
