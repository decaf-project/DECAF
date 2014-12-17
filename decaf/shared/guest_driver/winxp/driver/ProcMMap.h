
extern FAST_MUTEX qemu_output_lock;

typedef struct _SYSTNT_MODULE_INFO
{
	CHAR			ImageName[MAXIMUM_FILENAME_LENGTH];
	ULONG			ImageBase;
	ULONG			ImageSize;
	LIST_ENTRY		ModListEntry;
}SYSTNT_MODULE_INFO, *PSYSTNT_MODULE_INFO;


/*
typedef struct _SYSTNT_PROCESS_MMAP
{
//	ULONG			cr3;
	BOOL			bInit;
	ULONG			ProcessId;
	CHAR			ProcessName[MAXIMUM_FILENAME_LENGTH];
//	LIST_ENTRY		ModuleListHead;	
	LIST_ENTRY		ProcListEntry;
}SYSTNT_PROCESS_MMAP, *PSYSTNT_PROCESS_MMAP;
*/

	
void
CreateProcMMap(
		ULONG ProcessId,
		ULONG ParentId
		);

void
RemoveProcMMap(
		ULONG ProcessId
		);

NTSTATUS 
SysTntInsertModuleInfo(
				 ULONG ProcessId, 
				 PUNICODE_STRING FullImageName, 
				 ULONG ImageBase, 
				 ULONG ImageSize
				 );

void
UpdateSysModuleList(void);

