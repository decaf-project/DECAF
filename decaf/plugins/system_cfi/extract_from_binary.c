#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peheaders.h"
#include "cfi.h"

#define uint32_t unsigned int
#define uint16_t unsigned short


BYTE *file_data_raw = 0;
uint32_t file_sz;

void* ptr_from_file(uint32_t raw_offset){
	if(raw_offset>=file_sz)return 0;
	void *ptr = file_data_raw + raw_offset;
	return ptr;
}

IMAGE_NT_HEADERS* ReadHeaders(char *file_loc)
{
    IMAGE_DOS_HEADER *DosHeader;
    IMAGE_NT_HEADERS *PeHeader;
	FILE *fp;

	if(!file_data_raw){
		fp = fopen(file_loc, "r");
		if(!fp){
			fprintf(stderr, "cannot open file %s\n", file_loc);
			return 0;
		}
		fseek(fp, 0, SEEK_END);
		file_sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		file_data_raw = (BYTE *)malloc(file_sz);
		fread(file_data_raw, 1, file_sz, fp);
		fclose(fp);
	}

	DosHeader = (IMAGE_DOS_HEADER *) file_data_raw;
    if (DosHeader->e_magic != (0x5a4d)) {
        fprintf(stderr, "Error -- Not a valid PE file!\n");
        return 0;
    }

	PeHeader = (IMAGE_NT_HEADERS*)(file_data_raw + DosHeader->e_lfanew);
	if(PeHeader->Signature != IMAGE_NT_SIGNATURE){
        fprintf(stderr, "Error -- Not a valid PE file!\n");
        return 0;
    }

	return PeHeader;
}

IMAGE_SECTION_HEADER *getSectionHeader(IMAGE_NT_HEADERS *PeHeader, int i){
	if(PeHeader->FileHeader.NumberOfSections <= i){
		return 0;
	}

	void* ret = (void*)(&(PeHeader->OptionalHeader)) +
			PeHeader->FileHeader.SizeOfOptionalHeader +
			i*sizeof(IMAGE_SECTION_HEADER);
	return (IMAGE_SECTION_HEADER*)ret;
}

IMAGE_SECTION_HEADER* locateSection(IMAGE_NT_HEADERS *PeHeader, DWORD va){
	IMAGE_SECTION_HEADER *fsh = getSectionHeader(PeHeader, 0);
	int i;
	for(i=0;i<PeHeader->FileHeader.NumberOfSections;i++){
		if(fsh[i].VirtualAddress <= va &&
			(fsh[i].VirtualAddress + fsh[i].Misc.VirtualSize) > va){
			return &(fsh[i]);
		}
	}
	return 0;
}

DWORD virtualAddressToRawOffset(IMAGE_NT_HEADERS *PeHeader, DWORD va){
	IMAGE_SECTION_HEADER *section = locateSection(PeHeader, va);
	if(!section)
		return 0;
	DWORD rawOffset = va - section->VirtualAddress + section->PointerToRawData;
	return rawOffset;
}

uint32_t getImageBase(IMAGE_NT_HEADERS *PeHeader){
	return PeHeader->OptionalHeader.ImageBase;
}

DWORD *getExportTable(IMAGE_NT_HEADERS *PeHeader, DWORD *numOfExport){
	DWORD edt_va, edt_raw_offset, *export_table, *ptr_to_table;
	IMAGE_EXPORT_DIRECTORY *pedt;

	edt_va = PeHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if(!edt_va){
		printf("this file does not have a export table\n");
		return 0;
	}
	
	edt_raw_offset = virtualAddressToRawOffset(PeHeader, edt_va);
	if(!edt_raw_offset)
		return 0;
	pedt = (IMAGE_EXPORT_DIRECTORY*)ptr_from_file(edt_raw_offset);
	ptr_to_table = (DWORD*)ptr_from_file(virtualAddressToRawOffset(PeHeader, pedt->AddressOfFunctions));
	(*numOfExport) = pedt->NumberOfFunctions;

	export_table = (DWORD*)malloc((*numOfExport)*sizeof(DWORD));
	memcpy(export_table, ptr_to_table, (*numOfExport)*sizeof(DWORD));

	DWORD imageBase = getImageBase(PeHeader);
	int i;
	for(i=0;i<(*numOfExport);i++){
		export_table[i] += imageBase;
	}

	return export_table;
}

IMAGE_SECTION_HEADER *getSectionHeaderByName(IMAGE_NT_HEADERS *PeHeader, char *name){
	int i;
	IMAGE_SECTION_HEADER *ish = getSectionHeader(PeHeader, 0);
	for(i=0;i<PeHeader->FileHeader.NumberOfSections;i++){
		if(!strcmp((const char*)ish[i].Name, name))
			return &(ish[i]);
	}
	return 0;
}

//get target addr for jump short
uint32_t getTargetAddrForEB(BYTE* data, DWORD CodeBase, uint32_t offset)
{
	uint32_t targetAddr = 0;

	if (data[offset+1] >> 7 == 0x0){
		targetAddr = data[offset+4] + CodeBase + offset + 2;
	}
	if (data[offset+1] >> 7 == 0x1){
		targetAddr = -(data[offset+1]^0xff) + CodeBase + offset + 1;
	}
	return targetAddr;
}

//get target addr for jump and call
uint32_t getTargetAddrForE8E9(BYTE* data, DWORD CodeBase, uint32_t offset)
{
	uint32_t targetAddr = 0;

	if (data[offset+4] >> 7 == 0x0){
		targetAddr = data[offset+4]*0x1000000 + data[offset+3]*0x10000 + 
			     data[offset+2]*0x100 + data[offset+1] + CodeBase + offset + 5;
	}
	if (data[offset+4] >> 7 == 0x1){
		targetAddr = -(data[offset+4]^0xff)*0x1000000 - (data[offset+3]^0xff)*0x10000 - 
			      (data[offset+2]^0xff)*0x100 - (data[offset+1]^0xff) + CodeBase + offset + 4;
	}
	return targetAddr;
}

uint32_t getTargetAddrFor55(BYTE* data, DWORD CodeBase, uint32_t offset)
{
	uint32_t targetAddr = 0;
	if (data[offset+1] == 0x8B && data[offset+2] == 0xEC)
	{
		targetAddr = offset + CodeBase;
	}
	return targetAddr;
}

//Add targetAddr into whitelist
void addToList(uint32_t targetAddr){
	printf("Target Address: %0x\n", targetAddr);
}

//Scan binary
uint32_t* checkBinary(BYTE* data, DWORD CodeBase, DWORD CodeSize, uint32_t *wl_numOfEntries, uint32_t imageBase)
{
	uint32_t targetAddr, i = 0;
	int size = 0;
	uint32_t *textTable = (DWORD*)malloc(CodeSize);
	for (i=0;i<CodeSize;i++)
	{		
		if(data[i]==0xeb || data[i]==0x70 || data[i]==0x71 || data[i]==0x72 || data[i]==0x73 || data[i]==0x74 || data[i]==0x75 || data[i]==0x76 || data[i]==0x77|| data[i]==0x78 || data[i]==0x79 || data[i]==0x7A || data[i]==0x7B || data[i]==0x7C || data[i]==0x7D || data[i]==0x7E || data[i]==0x7F)
		{
			targetAddr = getTargetAddrForEB(data, CodeBase, i);
		
			if ( targetAddr >= CodeBase && targetAddr < CodeBase + CodeSize)
			{
				textTable[size] = targetAddr + imageBase;
				size++;
			}
		}
		
		if(data[i]==0xe8 || data[i]==0xe9 )
		{
			targetAddr= getTargetAddrForE8E9(data, CodeBase, i);
			if ( targetAddr >= CodeBase && targetAddr < CodeBase + CodeSize)
			{
				textTable[size] = targetAddr + imageBase;
				size++;
			}
			
		}
		if(data[i]==0x55)
		{
			targetAddr = getTargetAddrFor55(data, CodeBase, i);
			if ( targetAddr >= CodeBase && targetAddr < CodeBase + CodeSize)
			{
				textTable[size] = targetAddr + imageBase;
				size++;
			}
		}
	}
	uint32_t *textTableFinal = (DWORD*)malloc(size*sizeof(DWORD));
	memcpy(textTableFinal,textTable,size*sizeof(DWORD));
	(*wl_numOfEntries) = size;
	return textTableFinal;
}

uint32_t* handleTextSection (IMAGE_NT_HEADERS *PeHeader, uint32_t *wl_numOfEntries){
	IMAGE_SECTION_HEADER *text_section;
	uint32_t text_sz, imageBase;
	BYTE *text_data;
	text_section = getSectionHeaderByName(PeHeader,".text");
	if(!text_section)
	{
		return 0;
	}
	imageBase = getImageBase(PeHeader);
	text_data = (BYTE*)ptr_from_file(text_section->PointerToRawData);
	DWORD CodeBase = PeHeader->OptionalHeader.BaseOfCode;
	DWORD CodeSize = PeHeader->OptionalHeader.SizeOfCode;
	
	return checkBinary(text_data, CodeBase, CodeSize, wl_numOfEntries, imageBase);

}

uint32_t* getRelocTable(IMAGE_NT_HEADERS *PeHeader, uint32_t *wl_numOfEntries){	

	IMAGE_SECTION_HEADER *reloc_section;
	char *reloc_data;
	uint32_t reloc_sz, i;

	reloc_section = getSectionHeaderByName(PeHeader, ".reloc");
	if(!reloc_section){
		return handleTextSection(PeHeader,wl_numOfEntries);
	}
	reloc_sz = reloc_section->SizeOfRawData;
	reloc_data = (char *)ptr_from_file(reloc_section->PointerToRawData);

//	if(!reloc_data)
//		return handleTextSection(PeHeader,wl_numOfEntries);

	uint32_t block_base, block_sz;
	uint32_t *reloc_table = 0;
	uint32_t table_sz = 0;
	uint32_t count = 0;

	i = 0;
	while(i+8<reloc_sz){
		block_base = *(int*)(reloc_data+i);
		block_sz = *(int*)(reloc_data+i+4);
		if(!block_sz)break;
		//if(!block_base)break;
		//printf("block_base:%x block_sz:%x old_sz:%x realloc_sz:%x\n", block_base, block_sz, table_sz, table_sz+block_sz-8);
		reloc_table = realloc(reloc_table, (table_sz+block_sz-8)*2);
		int old_table_sz = table_sz;
		table_sz+=block_sz-8;
		
		int j;
		for(j=8;j<block_sz;j+=2){
			uint16_t tmp = *(uint16_t *)(reloc_data+i+j);
			if((tmp >> 12)!=3){
				continue;
			}
			reloc_table[count] = block_base + (tmp & 0x0fff);
			//printf("%d 0x%08x --> ", count, reloc_table[count]);
			uint32_t raw_offset = virtualAddressToRawOffset(PeHeader, reloc_table[count]);
			reloc_table[count] = *(uint32_t*)ptr_from_file(raw_offset);
			//printf("0x%08x\n", reloc_table[count]);
			count++;
		}
		i+=block_sz;
	}
	(*wl_numOfEntries) = count;

	return (uint32_t*)reloc_table;
}

void WL_cleanUp()
{
	if(file_data_raw){
		free(file_data_raw);
		file_data_raw = 0;
	}
}

int WL_Extract (
		char *file_name,
		uint32_t* entries,
		uint32_t *code_base,
		struct bin_file *file )
{
	uint32_t numOfReloc, numOfExport, imageBase = 0, currentSize = 0;
	uint32_t *relocTable, *exportTable, *mergeTable = 0;
	IMAGE_NT_HEADERS *PeHeader;

	//cleanup just in case
	WL_cleanUp();
	PeHeader = ReadHeaders(file_name);
	if(!PeHeader){
		WL_cleanUp();
		return -1;
	}
	printf("extracting from file %s...\n", file_name);
	file->reloc_tbl  = getRelocTable(PeHeader, &(file->reloc_tbl_count));
	file->exp_tbl = getExportTable(PeHeader, &(file->exp_tbl_count));
	file->image_base = imageBase = getImageBase(PeHeader);

	return file->reloc_tbl_count + file->exp_tbl_count;
}

uint32_t* WL_ExportTableFromBinary(char *file, uint32_t* entries){
	if(file_data_raw)
		WL_cleanUp();
	IMAGE_NT_HEADERS *PeHeader = ReadHeaders(file);
	uint32_t numOfReloc, numOfExport;
	//printf("ImageBase: %08x\n", getImageBase(PeHeader));
	printf("extracting from file %s...\n", file);
	uint32_t* relocTable = getRelocTable(PeHeader, &numOfReloc);
	uint32_t* exportTable = getExportTable(PeHeader, &numOfExport);
	
	if(!exportTable)
		return 0;

	(*entries) = numOfExport;
	return exportTable;

	int i;
	for(i=0;i<numOfExport;i++){
		printf("E:0x%08x\n", exportTable[i]);
	}
	for(i=0;i<numOfReloc;i++){
		printf("R:0x%08x\n", relocTable[i]);
	}
}

int main(int argc, char **argv){
	uint32_t et_num, new_addr, image_base;
	struct bin_file file;
	uint32_t *et = WL_Extract(argv[1], &et_num, &image_base, &file);
	int i;
	if(et){
		for(i=0;i<et_num;i++){
			new_addr = et[i];
			printf("M%d: 0x%08x\n", i, new_addr);
		}
		WL_cleanUp();
	}
	return 0;
}


