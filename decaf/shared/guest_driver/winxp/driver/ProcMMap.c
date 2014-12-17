#include "pch.h"
#include "ProcMMap.h"

#pragma warning(disable:4995)

/* Size for buffers used for names and output lines. Some
   mangled symbols can be large, so we print them without
   using a temporary buffer. If you are tempted to increase
   this, keep in mind that stack of a Windows kernel thread
   is only 12k. */
#define NAME_BUFFER_SIZE 512

FAST_MUTEX qemu_output_lock;

NTSTATUS NTAPI
ZwQuerySystemInformation (DWORD  SystemInformationClass,
                          PVOID  SystemInformation,
                          DWORD  SystemInformationLength,
                          PDWORD ReturnLength);

#define SystemModuleInformation 11 // SYSTEMINFOCLASS


NTSYSAPI
NTSTATUS
NTAPI
PsLookupProcessByProcessId (
IN ULONG ProcessId,
OUT PEPROCESS *Process
);



LIST_ENTRY ModuleListHead;

static int get_basename(char *path)
{
	int len = strlen(path);
	int i;
	for(i=len-1; i>=0; i--)
		if(path[i] == '\\') break;
	return i+1;
}


static unsigned long read_cr3() 
{ 
	unsigned long __dummy; 
	_asm { 
		push eax
		mov eax, cr3	
		mov __dummy, eax 
		pop eax
	}	
	return  __dummy; 
}


static void QemuOutputString(char *fmt, ...)
{
    char __tmp;
	va_list vl;
    int i;
	char buf[NAME_BUFFER_SIZE], *p;

	va_start(vl, fmt);
	if (fmt[0] == '%' && fmt[1] == 's' && fmt[2] == '\0') {
		/* Use the single string directly */
		p = va_arg(vl, char *);
	} else {
		RtlStringCbVPrintfA(buf, sizeof(buf), fmt, vl);
		p = buf;
	}
	va_end(vl);

	ExAcquireFastMutex(&qemu_output_lock);
	for(i=0; (__tmp=p[i])!=0; i++)
	{
		__asm {
			push eax
			mov al, __tmp
			out 0x68, al
			pop eax
		}
		if (__tmp == '\n') {
			/* Terminate the message after a newline */
			__asm {
				push eax
				mov al, 0
				out 0x68, al
				pop eax
			}
		}
	}
	ExReleaseFastMutex(&qemu_output_lock);
//	DbgPrint(buf);
}



static
NTSTATUS
InsertModuleInfo(
		PLIST_ENTRY		ModuleListHead,
		CHAR			ImageName[],
		ULONG ImageBase,
		ULONG ImageSize
		)
{
	PSYSTNT_MODULE_INFO pModInfo;

	pModInfo = (PSYSTNT_MODULE_INFO)ExAllocatePool(PagedPool, sizeof(SYSTNT_MODULE_INFO));
	if(!pModInfo) return STATUS_NO_MEMORY;

	RtlStringCbCopyA(pModInfo->ImageName, sizeof(pModInfo->ImageName), ImageName);
	pModInfo->ImageBase = ImageBase;
	pModInfo->ImageSize = ImageSize;
	
	InsertTailList(ModuleListHead, &pModInfo->ModListEntry); //TODO: sort it for better search

	return STATUS_SUCCESS;
}


static
PSYSTNT_MODULE_INFO
FindModulebyName(
		CHAR		FullImageName[],
		PLIST_ENTRY	ModuleListHead
		)
{
	PLIST_ENTRY entry;
	PSYSTNT_MODULE_INFO pmi;
	STRING str1, str2;

	for(entry=ModuleListHead->Flink; entry!=ModuleListHead; entry=entry->Flink) {
		pmi = CONTAINING_RECORD(entry, SYSTNT_MODULE_INFO, ModListEntry);
		RtlInitString(&str1, FullImageName);
		RtlInitString(&str2, pmi->ImageName);
		if(!RtlCompareString(&str1, &str2, FALSE))
			return pmi;
	}
	return NULL;
}




static
int
InsertModuleList(
		char			ImageName[],
		ULONG			ImageBase,
		ULONG			ImageSize
		)
{
	PSYSTNT_MODULE_INFO pmi;
	NTSTATUS status = STATUS_SUCCESS;

	pmi = FindModulebyName(ImageName, &ModuleListHead);
	if(pmi) return 0;

	status = InsertModuleInfo(&ModuleListHead, ImageName, ImageBase, ImageSize);
	
	return (status == STATUS_SUCCESS);
}



// =================================================================
// MEMORY ACCESS FUNCTIONS
// =================================================================

BOOL SpyMemoryPageEntry (PVOID           pVirtual,
                         PSPY_PAGE_ENTRY pspe)
    {
    SPY_PAGE_ENTRY spe;
    BOOL           fOk = FALSE;

    spe.pe       = X86_PDE_ARRAY [X86_PDI (pVirtual)];
    spe.dSize    = X86_PAGE_4M;
    spe.fPresent = FALSE;

    if (spe.pe.pde4M.P)
        {
        if (spe.pe.pde4M.PS)
            {
            fOk = spe.fPresent = TRUE;
            }
        else
            {
            spe.pe    = X86_PTE_ARRAY [X86_PAGE (pVirtual)];
            spe.dSize = X86_PAGE_4K;

            if (spe.pe.pte4K.P)
                {
                fOk = spe.fPresent = TRUE;
                }
            else
                {
                fOk = (spe.pe.pnpe.PageFile != 0);
                }
            }
        }
    if (pspe != NULL) *pspe = spe;
    return fOk;
    }

// -----------------------------------------------------------------

BOOL SpyMemoryTestAddress (PVOID pVirtual)
    {
    return SpyMemoryPageEntry (pVirtual, NULL);
    }

// -----------------------------------------------------------------

BOOL SpyMemoryTestBlock (PVOID pVirtual,
                         DWORD dBytes)
    {
    PBYTE pbData;
    DWORD dData;
    BOOL  fOk = TRUE;

    if (dBytes)
        {
        pbData = (PBYTE) ((DWORD_PTR) pVirtual & X86_PAGE_MASK);
        dData  = (((dBytes + X86_OFFSET_4K (pVirtual) - 1)
                   / PAGE_SIZE) + 1) * PAGE_SIZE;
        do  {
            fOk = SpyMemoryTestAddress (pbData);

            pbData += PAGE_SIZE;
            dData  -= PAGE_SIZE;
            }
        while (fOk && dData);
        }
    return fOk;
    }



NTSTATUS EnumExportedSymbol(
				 PVOID ImageBase, 
				 CHAR  ImageName[]
				 )
{
	PIMAGE_NT_HEADERS		pinh = NULL;
	PIMAGE_EXPORT_DIRECTORY pied = NULL;
	PIMAGE_DATA_DIRECTORY	pidd;
    PDWORD                  pdNames, pdFunctions;
    PWORD                   pwOrdinals;
	DWORD					i;
	WORD					j;
	PVOID pAddress;
	PCHAR szName;
	PHYSICAL_ADDRESS		pa;
	CHAR					OrdinalName[128];
	BYTE					*bVisited;

	//Aravind: Fixing a bug to work on win 7.
	if(/*(DWORD)ImageBase >= 0x80000000 &&  */ //Bug fix to work on Win 7
		!MmIsAddressValid(ImageBase) )
		return STATUS_DATA_ERROR;

	if((pinh = RtlImageNtHeader(ImageBase)) == NULL)
		return STATUS_INVALID_IMAGE_FORMAT;

	pidd = pinh->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT;
	if(!pidd->VirtualAddress || pidd->Size < IMAGE_EXPORT_DIRECTORY_)
		return STATUS_DATA_ERROR;

	pied = (PIMAGE_EXPORT_DIRECTORY)PTR_ADD (ImageBase, pidd->VirtualAddress);

	pdNames     = (PDWORD)PTR_ADD (ImageBase, pied->AddressOfNames);
        pdFunctions = (PDWORD)PTR_ADD (ImageBase, pied->AddressOfFunctions);
	pwOrdinals  = (PWORD)PTR_ADD (ImageBase, pied->AddressOfNameOrdinals);

	if(pied->NumberOfFunctions == 0)
		return STATUS_SUCCESS;

	bVisited = (BYTE*)ExAllocatePoolWithTag(PagedPool, pied->NumberOfFunctions * sizeof(BYTE), 'TEMU');
	if(bVisited == NULL)
		return STATUS_NO_MEMORY;

	memset(bVisited, 0, pied->NumberOfFunctions * sizeof(BYTE));

	for(i = 0; i < pied->NumberOfNames; i++)
    {
		j = pwOrdinals[i];
        if (j >= pied->NumberOfFunctions || j < 0) 
			continue;
        pAddress = PTR_ADD(ImageBase, pdFunctions[j]);
		szName = (PBYTE)PTR_ADD(ImageBase, pdNames[i]);
		QemuOutputString("F %s ", ImageName);
		QemuOutputString("%s", szName);
		QemuOutputString(" %08x \n", (PBYTE)pAddress-(PBYTE)ImageBase);
		bVisited[j] = 1;
	}

	for(i = 0; i < pied->NumberOfFunctions; i++)
    {
		if (bVisited[i]) 
			continue;

        pAddress = PTR_ADD(ImageBase, pdFunctions[i]);
		if(pAddress == ImageBase)
			continue;

		RtlStringCbPrintfA(OrdinalName, sizeof(OrdinalName), "Ordinal_%d", (DWORD)i + pied->Base);
		szName = (PBYTE)OrdinalName;
		QemuOutputString("F %s %s %08x \n", ImageName, szName, (PBYTE)pAddress-(PBYTE)ImageBase);
	}

	ExFreePool(bVisited);
	return STATUS_SUCCESS;
}



NTSTATUS 
SysTntInsertModuleInfo(
				 ULONG ProcessId, 
				 PUNICODE_STRING FullImageName, 
				 ULONG ImageBase, 
				 ULONG ImageSize
				 )
{
	char	name[NAME_BUFFER_SIZE];
	char    *basename;
	DWORD	cr3;

	RtlStringCbPrintfA(name, sizeof(name), "%wZ", FullImageName);
	cr3 = read_cr3();
	basename = name+get_basename(name);

	if(InsertModuleList(name, ImageBase, ImageSize))
		EnumExportedSymbol((PVOID)ImageBase, basename);

	QemuOutputString("M %d %08x \"%s\" %08x %08x \"%s\" \n", ProcessId, cr3, basename, ImageBase, ImageSize, name);


	return STATUS_SUCCESS;

}



void
CreateProcMMap(
		ULONG ProcessId,
		ULONG ParentId
		)
{
	QemuOutputString("P + %d %d \n", ProcessId, ParentId); 
}


void
RemoveProcMMap(
		ULONG ProcessId
		)
{
/*	PLIST_ENTRY entry;
	PSYSTNT_PROCESS_MMAP pmmap;

	for(entry=ProcessMMapListHead.Flink; entry!=&ProcessMMapListHead; entry=entry->Flink)
	{
		pmmap = CONTAINING_RECORD(entry, SYSTNT_PROCESS_MMAP, ProcListEntry);
		if(pmmap->ProcessId == ProcessId)
		{
			RemoveEntryList(&pmmap->ProcListEntry);
			ExFreePool(pmmap);
			QemuOutputString("P - %d\n", ProcessId);
			break;
		}
	}*/
	QemuOutputString("P - %d \n", ProcessId);

}



static 
PMODULE_LIST 
EnumSystemModule(
		PDWORD    pdData,
		PNTSTATUS pns)
{
    DWORD        dSize;
    DWORD        dData = 0;
    NTSTATUS     ns    = STATUS_INVALID_PARAMETER;
    PMODULE_LIST pml   = NULL;

    for (dSize = PAGE_SIZE; (pml == NULL) && dSize; dSize <<= 1)
	{
        pml = (PMODULE_LIST)ExAllocatePool(PagedPool, dSize);
		if(!pml)
		{
            ns = STATUS_NO_MEMORY;
            break;
		}
        ns = ZwQuerySystemInformation (SystemModuleInformation,
                                       pml, dSize, &dData);
        if (ns != STATUS_SUCCESS)
		{
            ExFreePool(pml);
			pml = NULL;
            dData = 0;

            if (ns != STATUS_INFO_LENGTH_MISMATCH) break;
        }
    }
    if (pdData != NULL) *pdData = dData;
    if (pns    != NULL) *pns    = ns;
    return pml;
}



void
UpdateSysModuleList(void)
{
	PMODULE_LIST pml;
	NTSTATUS	 status = STATUS_SUCCESS;
	DWORD		i, cr3;
	char		*ImageName;
	
	cr3 = read_cr3();

	if((pml = EnumSystemModule(NULL, &status)) != NULL)
	{
		for(i=0; i<pml->dModules; i++) {
			ImageName = (char *)pml->aModules[i].abPath+pml->aModules[i].wNameOffset;

			if(InsertModuleList(ImageName, (DWORD)pml->aModules[i].pBase, pml->aModules[i].dSize))
				EnumExportedSymbol(pml->aModules[i].pBase, ImageName);

			QemuOutputString("M 0 %08x \"%s\" %08x %08x \n", cr3, ImageName, 
				(DWORD)pml->aModules[i].pBase, pml->aModules[i].dSize);
		}
	}
	ExFreePool(pml);
}



