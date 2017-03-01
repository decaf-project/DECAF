/*
 * recon.c
 *
 *  Created on: Jun 8, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 * add process list: Jul 24
 */

#include "TEMU_main.h"
#include "qemu-timer.h"
#include "../shared/hooks/function_map.h"
#include "../shared/procmod.h"
#include "../shared/hookapi.h"
#include "recon.h"


QLIST_HEAD (loadedlist_head, service_entry) loadedlist;
QLIST_HEAD (processlist_head, process_entry) processlist;
QLIST_HEAD (threadlist_head, thread_entry) threadlist;
QLIST_HEAD (filelist_head, file_entry) filelist;
GHashTable *cr3_hashtable = NULL;
GHashTable *eproc_ht = NULL;

struct process_entry *system_proc = NULL;

uint32_t gkpcr;
uint32_t GuestOS_index;
uintptr_t insn_handle = 0;
uintptr_t block_handle = 0;
uint32_t system_cr3 = 0;
BYTE *recon_file_data_raw = 0;
uint32_t file_sz;

/* Timer to check for proc exits */
static QEMUTimer *recon_timer = NULL;

////////////////////////////////////////////////

struct cr3_info{
	uint32_t value;
	GHashTable *vaddr_tbl;
	GHashTable *modules_tbl;
};

int find_in_modulelist(uint32_t modules,uint32_t pid){
	struct pe_entry* pe = NULL;
	struct process_entry* proc = NULL;

	if(!QLIST_EMPTY(&processlist)){
		QLIST_FOREACH(proc, &processlist, loadedlist_entry){
			if(pid == proc->process_id){
				if(!QLIST_EMPTY(&proc->modlist_head)){
					QLIST_FOREACH(pe, &proc->modlist_head, loadedlist_entry){
						if(modules == pe->base){
							return 1;
						}
					}
				}
			}
		}
	}
	return 0;
}

void insert_in_modulelist(struct pe_entry* pe, uint32_t pid){

	struct process_entry* proc = NULL;
	if(!QLIST_EMPTY(&processlist)){
		QLIST_FOREACH(proc, &processlist, loadedlist_entry){
			if(pid == proc->process_id){
				QLIST_INSERT_HEAD(&proc->modlist_head, pe, loadedlist_entry);
			}
		}
	}
}

struct process_entry* proc_exit_check(CPUState* env,uint32_t cr3){
	struct process_entry* proc = NULL;
	uint32_t exit_time;

	if(!QLIST_EMPTY(&processlist)){
		QLIST_FOREACH(proc, &processlist, loadedlist_entry){
			if(cr3 == proc->cr3){
				uint32_t curr_proc_base = proc->EPROC_base_addr;
				cpu_memory_rw_debug(env, curr_proc_base +0x78 , (uint8_t *) &exit_time, 4, 0);
				if(exit_time != 0x00){
					return proc;
				}
			}
		}
	}
	return NULL;
}

/* This is stop gap arrangement to utilize the existing infrastructure.
 * TODO: The message has to be done away with.
 */
void message_p(struct process_entry* proc, int operation){
	char proc_mod_msg[1024]= {'\0'};
	if(operation){
		monitor_printf(default_mon,"P + %d %d %08x %s\n", proc->process_id, proc->ppid, proc->cr3, proc->name);
		sprintf(proc_mod_msg, "P + %d %d %08x %s\n", proc->process_id, proc->ppid, proc->cr3, proc->name);
	}else{
		monitor_printf(default_mon,"P - %d %d %08x %s\n", proc->process_id, proc->ppid, proc->cr3, proc->name);
		sprintf(proc_mod_msg, "P - %d %d %08x %s\n", proc->process_id, proc->ppid, proc->cr3, proc->name);
	}
	handle_guest_message(proc_mod_msg);
}

void message_m(uint32_t pid, uint32_t cr3, struct pe_entry* pe){
	char proc_mod_msg[2048]= {'\0'};
	char api_msg[2048] = {'\0'};
	struct api_entry *api = NULL, *next = NULL;

	if(strlen(pe->name) == 0)
		return;

	monitor_printf(default_mon,"M %d %08x \"%s\" %08x %08x \"%s\"\n", pid, cr3, pe->name, pe->base, pe->size,pe->fullname);
	sprintf(proc_mod_msg, "M %d %08x \"%s\" %08x %08x \"%s\"\n", pid, cr3, pe->name, pe->base, pe->size,pe->fullname);
	update_api_with_pe(cr3, pe, ((pid == 0 || pid == 4)? 0 : 1));
	if(!QLIST_EMPTY(&pe->apilist_head)){
		QLIST_FOREACH_SAFE(api, &pe->apilist_head, loadedlist_entry, next){
			sprintf(api_msg, "F %s %s %08x\n", pe->name, api->name, api->base);
			handle_guest_message(api_msg);
			QLIST_REMOVE(api, loadedlist_entry);
			free(api);
		}
	}
	handle_guest_message(proc_mod_msg);
}

struct pe_entry* update_loaded_user_mods_with_peb(uint32_t cr3,uint32_t peb, target_ulong vaddr,uint32_t pid, struct cr3_info* cr3i)
{
	uint32_t ldr, memlist, first_dll, curr_dll;
	struct pe_entry *curr_entry = NULL;

	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	int ret = 0, flag = 0;

	if(peb == 0x00)
		return NULL;

	TEMU_memory_rw_with_cr3 (cr3, peb+0xc, (void *)&ldr, 4, 0);
	memlist = ldr + 0xc;
	TEMU_memory_rw_with_cr3 (cr3, memlist, (void *) &first_dll, 4, 0);

	if(first_dll == 0)
		return NULL;

	curr_dll = first_dll;

	do {
		curr_entry = (struct pe_entry *) malloc (sizeof(*curr_entry));
		memset(curr_entry, 0, sizeof(*curr_entry));

		TEMU_memory_rw_with_cr3 (cr3, curr_dll+ 0x18, (void *) &(curr_entry->base), 4, 0);
		if(curr_entry->base == 0x0 && flag == 0){
			flag = 1;
			TEMU_memory_rw_with_cr3 (cr3, curr_dll, (void *) &curr_dll, 4, 0);
			continue;
		}
		TEMU_memory_rw_with_cr3 (cr3, curr_dll+ 0x20, (void *) &(curr_entry->size), 4, 0);
		readustr_with_cr3(curr_dll + 0x24, cr3, curr_entry->fullname, env);
		readustr_with_cr3(curr_dll + 0x2c, cr3, curr_entry->name, env);
		uint32_t modules = curr_entry->base;

		if(modules >0x00300000 && !g_hash_table_lookup(cr3i->modules_tbl, modules)){
				message_m(pid, cr3, curr_entry);
				g_hash_table_insert(cr3i->modules_tbl,(gpointer)modules, (gpointer)1);
			}
		free(curr_entry);
		TEMU_memory_rw_with_cr3 (cr3, curr_dll, (void *) &curr_dll, 4, 0);
	} while (curr_dll != 0 && curr_dll != first_dll);

done:
	return ret;
}

void update_kernel_modules(uint32_t cr3, target_ulong vaddr, uint32_t pid, struct cr3_info * cr3i)
{
	uint32_t kdvb, psLM, curr_mod, next_mod;
	uint32_t base, size, holder;
	//char name[512], fullname[2048];
	CPUState *env;
	struct pe_entry *curr_entry = NULL;
	if (gkpcr == 0 || cr3i == NULL)
			return;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
			if (env->cpu_index == 0) {
					break;
			}
	}

	cpu_memory_rw_debug(env, gkpcr + KDVB_OFFSET, (uint8_t *) &kdvb, 4, 0);
	cpu_memory_rw_debug(env, kdvb + PSLM_OFFSET, (uint8_t *) &psLM, 4, 0);
	cpu_memory_rw_debug(env, psLM, (uint8_t *) &curr_mod, 4, 0);

	while (curr_mod != 0 && curr_mod != psLM) {
		curr_entry = (struct pe_entry *) malloc(sizeof(*curr_entry));

		memset(curr_entry, 0, sizeof(*curr_entry));
		QLIST_INIT(&curr_entry->apilist_head);

		cpu_memory_rw_debug(env, curr_mod + handle_funds[GuestOS_index].offset->DLLBASE_OFFSET, (uint8_t *) &(curr_entry->base), 4, 0); // dllbase  DLLBASE_OFFSET
		cpu_memory_rw_debug(env, curr_mod + handle_funds[GuestOS_index].offset->SIZE_OFFSET, (uint8_t *) &(curr_entry->size), 4, 0);           // dllsize  SIZE_OFFSET
		holder = readustr(curr_mod + handle_funds[GuestOS_index].offset->DLLNAME_OFFSET, (curr_entry->name), env);
		readustr(curr_mod+0x24, curr_entry->fullname, env);
		if(!g_hash_table_lookup(cr3i->modules_tbl, curr_entry->base)) {
			update_api_with_pe(cr3, curr_entry, 0);
			message_m(pid, cr3, curr_entry);
			g_hash_table_insert(cr3i->modules_tbl, (gpointer)(curr_entry->base), (gpointer)1);
		}
		free(curr_entry);
		cpu_memory_rw_debug(env, curr_mod, (uint8_t *)&next_mod, 4, 0);
		cpu_memory_rw_debug(env, next_mod+4, (uint8_t *) &holder, 4, 0);
		if(holder != curr_mod) {
				monitor_printf(default_mon, "Something is wrong. Next->prev != curr. curr_mod = 0x%08x\n",
						curr_mod);
				break;
		}
		curr_mod = next_mod;
	}
}

target_ulong get_new_modules(CPUState* env, uint32_t cr3, target_ulong vaddr, struct cr3_info* cr3i){

	uint32_t base = 0, self =0, pid = 0;
	if(cr3 == system_cr3) {
		//Need to load system module here.
		pid = 4; //TODO: Fix this.
		update_kernel_modules(cr3, vaddr, pid, cr3i);
	} else {
		base =  env->segs[R_FS].base;
		cpu_memory_rw_debug(env, base + 0x18,(uint8_t *)&self, 4, 0);

		if(base !=0 && base == self){
			uint32_t pid_addr = base+0x20;
			cpu_memory_rw_debug(env,pid_addr,(uint8_t *)&pid,4,0);
			uint32_t peb_addr = base+0x30;
			uint32_t peb,ldr;
			cpu_memory_rw_debug(env,peb_addr,(uint8_t *)&peb, 4, 0);
			update_loaded_user_mods_with_peb(cr3, peb, vaddr, pid, cr3i);
		}
	}
	return 0;
}

static void print_ghash_table(uint32_t cr3, GHashTable *ht)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, ht);
	monitor_printf(default_mon, "Printing known addresses for cr3:%08x... %d elements\n", cr3, g_hash_table_size (ht));
	while (g_hash_table_iter_next (&iter, &key, &value))
	  {
		monitor_printf(default_mon, "0x%08x, ", (uint32_t)key);
	  }
	monitor_printf(default_mon, "\n");
}

void print_cr3_known_vaddrs(uint32_t cr3)
{
	struct cr3_info* cr3i = NULL;
	cr3i = g_hash_table_lookup(cr3_hashtable, cr3);
	if(!cr3i) {
		monitor_printf(default_mon, "Invalid cr3...\n");
		goto done;
	}
	print_ghash_table(cr3, cr3i->vaddr_tbl);
done:
	return;
}

struct process_entry *get_system_process()
{
        struct process_entry *pe = NULL;
        handle_funds[GuestOS_index].update_processlist();
        QLIST_FOREACH(pe, &processlist, loadedlist_entry)
        {
                if(strcmp(pe->name, "System") == 0)
                        break;
        }
        return pe;
}

uint32_t present_in_vtable = 0;
uint32_t adding_to_vtable = 0;
uint32_t getting_new_mods = 0;
void tlb_call_back(CPUState* env, target_ulong vaddr)
{
        struct cr3_info* cr3i = NULL;
        struct process_entry* procptr = NULL;
        int flag = 0;
        int new = 0;
        char proc_mod_msg[1024] = {'\0'};
        target_ulong modules;
        uint32_t exit_page = 0;
        uint32_t cr3 = env->cr[3];

		cr3i = g_hash_table_lookup(cr3_hashtable, cr3);
		if(!TEMU_is_in_kernel()) {
				if(!cr3i){ // new cr3'
					new = 1;
					if(system_proc == NULL) { //get the system proc first. This should be automatic.
							cr3i = (struct cr3_info *) malloc (sizeof(*cr3i));
							cr3i->value = system_cr3;
							cr3i->vaddr_tbl = g_hash_table_new(0,0);
							cr3i->modules_tbl = g_hash_table_new(0,0);
							g_hash_table_insert(cr3_hashtable, (gpointer)cr3, (gpointer) cr3i);
							procptr = get_system_process();
							if(!procptr) {
									monitor_printf(default_mon, "System proc is null. shouldn't be. Stopping vm...\n");
									vm_stop(0);
							}
							system_proc = procptr;
							message_p(procptr,1); // 1 for addition, 0 for remove
							update_kernel_modules(system_cr3, vaddr, procptr->process_id, cr3i);
							exit_page = (((procptr->EPROC_base_addr)+0x78) >> 3) << 3;
							g_hash_table_insert(eproc_ht, (gpointer)(exit_page), (gpointer)1);
							QLIST_INIT(&procptr->modlist_head);
					}

					cr3i  = (struct cr3_info*)malloc(sizeof(*cr3i));
					cr3i->value = cr3;
					cr3i->vaddr_tbl = g_hash_table_new(0, 0);
					cr3i->modules_tbl = g_hash_table_new(0, 0);
					g_hash_table_insert(cr3i->vaddr_tbl, (gpointer)vaddr, (gpointer)1);
					g_hash_table_insert(cr3_hashtable, (gpointer)cr3, (gpointer) cr3i);

					procptr = get_new_process();
					message_p(procptr,1); // 1 for addition, 0 for remove

					exit_page = (((procptr->EPROC_base_addr)+0x78) >> 3) << 3;
					g_hash_table_insert(eproc_ht, (gpointer)(exit_page), (gpointer)1);
					QLIST_INIT(&procptr->modlist_head);

					if(g_hash_table_size(cr3_hashtable) == 2)
							startup_registrations();
				}
		} else if(!cr3i) {
			goto done;
		}

        if(!new) { // not a new cr3
				if(g_hash_table_lookup(cr3i->vaddr_tbl, (gpointer)vaddr)) {
						present_in_vtable++;
						goto done;
				}
				g_hash_table_insert(cr3i->vaddr_tbl, (gpointer) vaddr, (gpointer)1);
				adding_to_vtable++;
		}

        getting_new_mods++;
        get_new_modules(env, cr3, vaddr, cr3i);

        done:
                return;
}


//tlb call back for write
void tlb_write_call_back(CPUState* env, target_ulong vaddr, uint32_t cr3){
	struct cr3_info* cr3e = NULL;
	struct process_entry* procptr = NULL;
	int flag = 0;
	char porc_mod_msg[1024] = {'\0'};
	uint32_t exit_time[2] = {0};
	target_ulong modules;

	if(!eproc_ht || !TEMU_is_in_kernel()) {
		return;
	}
	//monitor_printf(default_mon,"here it is!");
	struct process_entry *proc = g_hash_table_lookup(eproc_ht, vaddr);
	if(!proc)
		return;

	cpu_memory_rw_debug(env, (proc->EPROC_base_addr)+0x78, &exit_time, 8, 0);
	if(exit_time == 0){
		monitor_printf(default_mon,"here it is!");
		return;
	}
	message_p(proc, 0);
//
//	if(cr3 != 0 && cr3_hashtable != NULL){
//		cr3e = g_hash_table_lookup(cr3_hashtable, cr3);
//		if(cr3e){
//			flag = g_hash_table_lookup(cr3e->vaddr_tbl, (gpointer)vaddr);
//			if(flag && proc_exit_check(env, cr3)){
//				procptr = proc_exit_check(env, cr3);
//				message_p(procptr, 0);
//			}
//		}
//	}
}
////////////////////////////////////////////////
unsigned long long insn_counter = 0;

void* recon_ptr_from_file(uint32_t raw_offset){
	if(raw_offset>=file_sz)return 0;
	void *ptr = recon_file_data_raw + raw_offset;
	return ptr;
}
///////////////////////////////////////////////////////
void get_result(void *opaque){
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	monitor_printf(default_mon,"HIT!: 0x%08x\n", env->eip);
}
void hook_test(){
	int pid = find_pid_by_name("smss.exe");
	monitor_printf(default_mon,"pid: %d\n", pid);
	uint32_t cr3 = find_cr3(pid);
	monitor_printf(default_mon,"cr3: 0x%08x\n", cr3);
	hookapi_hook_function_byname("ntdll.dll", "ZwLoadDriver",1, cr3 , get_result, NULL, 0);
}
///////////////////////////////////////////////////////
//get nt_header from image
//spaceType 1 for user space and 0 for kernal space
uint32_t NTHDR_from_image(uint32_t base, IMAGE_NT_HEADERS *PeHeader, uint32_t cr3, uint32_t spaceType)
{
	IMAGE_DOS_HEADER DosHeader;
	CPUState* env;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
				if (env->cpu_index == 0) {
					break;
				}
			}

	if(spaceType){
		TEMU_memory_rw_with_cr3(cr3, base, (void *) &DosHeader, sizeof(DosHeader), 0);
	}else{
		cpu_memory_rw_debug(env, base, (uint8_t *) &DosHeader, sizeof(DosHeader), 0);
	}

	if (DosHeader.e_magic != (0x5a4d)) {  // get first dos signature
		fprintf(stderr, "e_magic Error -- Not a valid PE file!\n");
		return -1;
		}

	if(spaceType){
		TEMU_memory_rw_with_cr3(cr3, base + DosHeader.e_lfanew, (void *)PeHeader, sizeof(*PeHeader), 0);
	}else{
		cpu_memory_rw_debug(env, base + DosHeader.e_lfanew,(uint8_t*)PeHeader, sizeof(*PeHeader), 0);
	}

	if(PeHeader->Signature != IMAGE_NT_SIGNATURE){  //get nt signature
		fprintf(stderr, "nt_sig Error -- Not a valid PE file!\n");
		return -1;
		}

	return 1;
}

uint32_t recon_getImageBase(IMAGE_NT_HEADERS *PeHeader){
	return PeHeader->OptionalHeader.ImageBase;
}

int readcstr(target_ulong addr, void *buf, CPUState *env, uint32_t cr3, int spaceType)
{
        //bytewise for now, perhaps block wise later.
        char *store = (char *) buf;
        int i = -1;
        int flag;
        do {
                if(++i == MAX_NAME_LENGTH)
                        break;

                if(spaceType) {
					flag = TEMU_memory_rw_with_cr3(cr3, addr+i, (void *)&store[i], 1, 0);
				}else{
					flag = cpu_memory_rw_debug(env, addr+i, (uint8_t *)&store[i], 1, 0);
				}

                if(flag < 0) {
                	store[i] = '\0';
                	return i;
                }
        } while (store[i] != '\0');

        if(i == MAX_NAME_LENGTH) {
                store[i-1] = '\0';
        }
        return i-1;
}

int get_export_section(
		IMAGE_EXPORT_DIRECTORY *tmp,
		DWORD *numOfExport,
		struct pe_entry *pef,
		int spaceType,
		uint32_t cr3,
		CPUState* env
		)
{
	DWORD *export_table, *ptr_to_table,*ptr_name_table;
	uint32_t image_base = pef->base;
	WORD *ptr_index_table;
	struct api_entry *api = NULL;

	ptr_to_table = (DWORD*)(tmp->AddressOfFunctions+image_base);
	ptr_name_table = (DWORD*)(tmp->AddressOfNames+image_base);
	ptr_index_table = (WORD*)(tmp->AddressOfNameOrdinals+image_base);

	uint32_t dllname = tmp->Name;
	char names[64];
	int m,i;
	if(spaceType){
		m = TEMU_memory_rw_with_cr3(cr3, dllname+image_base, (void *)names, 16, 0);
	}else{
		m = cpu_memory_rw_debug(env, dllname+image_base,(uint8_t*)names, 16, 0);
	}
	if(m == -1){
		//monitor_printf(default_mon,"error read name memory\n");
		return -1;
	}
	(*numOfExport) = tmp->NumberOfFunctions;
	DWORD num = tmp->NumberOfNames;
	WORD num1 = tmp->NumberOfNames;
	export_table = (DWORD*)malloc((*numOfExport)*sizeof(DWORD));
	DWORD *name_table = (DWORD*)malloc((num)*sizeof(DWORD));
	WORD *index_table = (WORD*)malloc((num1)*sizeof(WORD));
	if(spaceType){
		TEMU_memory_rw_with_cr3(cr3, ptr_to_table, export_table, (*numOfExport)*sizeof(DWORD), 0);
		TEMU_memory_rw_with_cr3(cr3, ptr_name_table, name_table, (num)*sizeof(DWORD), 0);
		TEMU_memory_rw_with_cr3(cr3, ptr_index_table, index_table, (num1)*sizeof(WORD), 0);
	}else{
		cpu_memory_rw_debug(env, ptr_to_table, export_table, (*numOfExport)*sizeof(DWORD), 0);
		cpu_memory_rw_debug(env, ptr_name_table, name_table, (num)*sizeof(DWORD), 0);
		cpu_memory_rw_debug(env, ptr_index_table, index_table, (num1)*sizeof(WORD), 0);
	}

	for(i=0;i<(num);i++){
		api = (struct api_entry *) malloc (sizeof(*api));
		memset(api, 0, sizeof(*api));
		char apiname[64] ={0};
		readcstr(name_table[i]+image_base, &apiname[0], env, cr3, spaceType);
		WORD k = index_table[i];
		//export_table[k] += image_base;

		api->base = export_table[k];
		strncpy(api->name, apiname, 63);
		api->name[63] = '\0';
		QLIST_INSERT_HEAD(&(pef->apilist_head), api, loadedlist_entry);
		/* put in map*/
	}
	free(name_table);
	free(index_table);
	free(export_table); //memleak
	return 0;
}

int getExportTable_with_pe (
		IMAGE_NT_HEADERS *PeHeader,
		DWORD *numOfExport,
		struct pe_entry *pef,
		uint32_t cr3,
		uint32_t spaceType )
{
	DWORD edt_va, edt_raw_offset;
	DWORD image_base = pef->base;
	IMAGE_EXPORT_DIRECTORY tmp;
	CPUState* env;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
				if (env->cpu_index == 0) {
					break;
				}
			}

	edt_va = PeHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

	if(!edt_va){
		//printf("this file does not have a export table\n");
		return -1;
	}

	edt_raw_offset = edt_va + image_base;
	if(!edt_raw_offset)
		return -1;
	int n;
	if(spaceType){
		n = TEMU_memory_rw_with_cr3(cr3, edt_raw_offset, (void*)&tmp, sizeof(tmp), 0);
	}else{
		n = cpu_memory_rw_debug(env, edt_raw_offset,(uint8_t*)&tmp, sizeof(tmp), 0);
	}

	if(n == -1){
			//monitor_printf(default_mon,"error read temp memory\n");
			return -1;
		}

	get_export_section(&tmp, numOfExport,pef, spaceType, cr3, env);

	return 0;
}

DWORD *recon_getExportTable(IMAGE_NT_HEADERS *PeHeader, DWORD *numOfExport, uint32_t image_base, uint32_t cr3, uint32_t spaceType ){
	DWORD edt_va, edt_raw_offset, *export_table, *ptr_to_table,*ptr_name_table;
	WORD *ptr_index_table;
	IMAGE_EXPORT_DIRECTORY tmp;
	CPUState* env;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
				if (env->cpu_index == 0) {
					break;
				}
			}
	edt_va = PeHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	monitor_printf(default_mon,"edt_va: 0x%08x\n", edt_va);

	if(!edt_va){
		printf("this file does not have a export table\n");
		return 0;
	}

	edt_raw_offset = edt_va + image_base;
	monitor_printf(default_mon,"edt_raw_offset: 0x%08x\n", edt_raw_offset);

	if(!edt_raw_offset)
		return 0;
	int n;
	if(spaceType){
		n = TEMU_memory_rw_with_cr3(cr3, edt_raw_offset, (void*)&tmp, sizeof(tmp), 0);
	}else{
		n = cpu_memory_rw_debug(env, edt_raw_offset,(uint8_t*)&tmp, sizeof(tmp), 0);
	}

	if(n == -1){
			//monitor_printf(default_mon,"error read temp memory\n");
			return 0;
		}
	ptr_to_table = (DWORD*)(tmp.AddressOfFunctions+image_base);
	ptr_name_table = (DWORD*)(tmp.AddressOfNames+image_base);
	ptr_index_table = (WORD*)(tmp.AddressOfNameOrdinals+image_base);

	uint32_t dllname = tmp.Name;
	//monitor_printf(default_mon,"dll: 0x%08x,ptr_table: 0x%08x,ptr_name: 0x%08x,ptr_index: 0x%08x\n", dllname,ptr_to_table,ptr_name_table,ptr_index_table);

	char names[64];
	int m;
	if(spaceType){
		m = TEMU_memory_rw_with_cr3(cr3, dllname+image_base, (void *)names, 16, 0);
	}else{
		m = cpu_memory_rw_debug(env, dllname+image_base,(uint8_t*)names, 16, 0);
	}
	if(m == -1){
		//monitor_printf(default_mon,"error read name memory\n");
		return 0;
	}
	//monitor_printf(default_mon,"name: %s\n", names);
	(*numOfExport) = tmp.NumberOfFunctions;
	DWORD num = tmp.NumberOfNames;
	WORD num1 = tmp.NumberOfNames;
	export_table = (DWORD*)malloc((*numOfExport)*sizeof(DWORD));
	DWORD *name_table = (DWORD*)malloc((num)*sizeof(DWORD));
	WORD *index_table = (WORD*)malloc((num1)*sizeof(WORD));
	if(spaceType){
		TEMU_memory_rw_with_cr3(cr3, ptr_to_table, export_table, (*numOfExport)*sizeof(DWORD), 0);
		TEMU_memory_rw_with_cr3(cr3, ptr_name_table, name_table, (num)*sizeof(DWORD), 0);
		TEMU_memory_rw_with_cr3(cr3, ptr_index_table, index_table, (num1)*sizeof(WORD), 0);
	}else{
		cpu_memory_rw_debug(env, ptr_to_table, export_table, (*numOfExport)*sizeof(DWORD), 0);
		cpu_memory_rw_debug(env, ptr_name_table, name_table, (num)*sizeof(DWORD), 0);
		cpu_memory_rw_debug(env, ptr_index_table, index_table, (num1)*sizeof(WORD), 0);
	}
	int i,j;
	for(i=0;i<(num);i++){
		char apiname[1024] ={0};
		for (j=0;j<1024;j++)
		{
			int flag;
			if(spaceType){
				flag = TEMU_memory_rw_with_cr3(cr3, name_table[i]+image_base+j*sizeof(char), (void *)apiname+j*sizeof(char), 1, 0);
			}else{
				flag = cpu_memory_rw_debug(env, name_table[i]+image_base+j*sizeof(char), (uint8_t *)apiname+j*sizeof(char), 1, 0);
			}
			 if(flag == -1){
				 strcpy(apiname,"read mem error!");
				 break;
			 }
			 if (apiname[j] == '/0')
			 {
				 break;
			 }
		}
		WORD k = index_table[i];
		export_table[k] += image_base;
		//monitor_printf(default_mon,"num: %d, 0x%08x, name:%s\n", i, export_table[k], apiname);
	}
	free(name_table);
	free(index_table);
	return export_table;
}
extern void vm_stop(int r);

int readustr_with_cr3(uint32_t addr, uint32_t cr3, void *buf, CPUState *env)
{
	uint32_t unicode_data[2];
	int i, j, unicode_len = 0;
	uint8_t unicode_str[MAX_UNICODE_LENGTH] = { '\0' };
	char *store = (char *) buf;

	if(cr3 != 0) {
		if (TEMU_memory_rw_with_cr3 (cr3, addr, (void *)&unicode_data, sizeof(unicode_data), 0) < 0) {
			//monitor_printf(default_mon,"TEMU_mem_rw_with_cr3(0x%08x, cr3=0x%08x, %d) returned non-zero.\n", addr, cr3, sizeof(unicode_data));
			store[0] = '\0';
			goto done;
		}
	} else {
		if (TEMU_memory_rw (addr, (void *)&unicode_data, sizeof(unicode_data), 0) < 0) {
			//monitor_printf(default_mon,"TEMU_mem_rw(0x%08x, %d) returned non-zero.\n", addr, sizeof(unicode_data));
			store[0] = '\0';
			goto done;
		}
	}

	unicode_len = (int) (unicode_data[0] & 0xFFFF);
	if (unicode_len > MAX_UNICODE_LENGTH)
			unicode_len = MAX_UNICODE_LENGTH;

	if(cr3 != 0) {
		if (TEMU_memory_rw_with_cr3 (cr3, unicode_data[1], (void *) unicode_str, unicode_len, 0) < 0) {
			store[0] = '\0';
			goto done;
		}
	} else {
		if (TEMU_memory_rw (unicode_data[1], (void *) unicode_str, unicode_len, 0) < 0) {
			store[0] = '\0';
			goto done;
		}
	}

	for (i = 0, j = 0; i < unicode_len; i += 2, j++) {
		if(unicode_str[i] < 0x20 || unicode_str[i] > 0x7e) //Non_printable character
			break;

		store[j] = unicode_str[i];
	}
	store[j] = '\0';

	done:
		return strlen(store);
}

int readustr(uint32_t addr, void *buf, CPUState *env)
{
	return readustr_with_cr3(addr, 0, buf, env);
}

uint32_t get_kpcr() {
	uint32_t kpcr, selfpcr;
	CPUState *env;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}

	kpcr = 0;
	cpu_memory_rw_debug(env, env->segs[R_FS].base + 0x1c, (uint8_t *) &selfpcr, 4, 0);

	if (selfpcr == env->segs[R_FS].base) {
		kpcr = selfpcr;
	}
	monitor_printf(default_mon, "KPCR at: 0x%08x\n", kpcr);

	return kpcr;
}

void get_os_version() {
	CPUState* env;
	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}
	uint32_t kdvb, CmNtCSDVersion, num_package;

	if (gkpcr == 0xffdff000) {
		cpu_memory_rw_debug(env, gkpcr + 0x34, (uint8_t *) &kdvb, 4, 0);
		cpu_memory_rw_debug(env, kdvb + 0x290, (uint8_t *) &CmNtCSDVersion, 4, 0); //CmNt version info
		cpu_memory_rw_debug(env, CmNtCSDVersion, (uint8_t *) &num_package, 4, 0);
		uint32_t num = num_package >> 8;
		if (num == 0x02) {
			GuestOS_index = 0; //winxpsp2
		} else if (num == 0x03) {
			GuestOS_index = 1; //winxpsp3
		}
	} else {
		GuestOS_index = 2; //win7
	}
	
}

static uint32_t get_ntoskrnl_internal(uint32_t curr_page, CPUState *env)
{
	IMAGE_DOS_HEADER *DosHeader = NULL;

	uint8_t page_data[4*1024] = {0}; //page_size
	uint16_t DOS_HDR = 0x5a4d;

	while(curr_page > 0x80000000) {
		if(cpu_memory_rw_debug(env, curr_page, (uint8_t *) page_data, 4*1024, 0) >= 0) { //This is paged out. Just continue
				if(memcmp(&page_data, &DOS_HDR, 2) == 0) {
					DosHeader = (IMAGE_DOS_HEADER *)&(page_data);
					if (DosHeader->e_magic != 0x5a4d)
						goto dec_continue;

					monitor_printf(default_mon, "DOS header matched at: 0x%08x\n", curr_page);

					if(*((uint32_t *)(&page_data[*((uint32_t *)&page_data[0x3c])])) != IMAGE_NT_SIGNATURE)
						goto dec_continue;

					return curr_page;
				}
		}
dec_continue:
		curr_page -= 1024*4;
	}
	return 0;
}

uint32_t  get_ntoskrnl(CPUState *env)
{
	uint32_t ntoskrnl_base = 0, exit_page = 0, cr3 = 0;
	struct cr3_info *cr3i = NULL;
	struct process_entry* procptr = NULL;
	monitor_printf(default_mon, "Trying by scanning back from sysenter_eip...\n");
	ntoskrnl_base = get_ntoskrnl_internal(env->sysenter_eip & 0xfffff000, env);
	if(ntoskrnl_base)
		goto found;
	monitor_printf(default_mon, "Trying by scanning back from eip that sets kpcr...\n");
	ntoskrnl_base = get_ntoskrnl_internal(env->eip & 0xfffff000, env);
	if(ntoskrnl_base)
		goto found;
	return 0;

found:
	cr3 = system_cr3 = env->cr[3];

	monitor_printf(default_mon, "OS base found at: 0x%08x\n", ntoskrnl_base);

	return ntoskrnl_base;
}

uint32_t get_cr3_from_proc_base(uint32_t base)
{
	CPUState* env;
	uint32_t cr3;

		for (env = first_cpu; env != NULL; env = env->next_cpu) {
			if (env->cpu_index == 0) {
				break;
			}
		}
	cpu_memory_rw_debug(env, base+0x18, (uint8_t *) &cr3, 4, 0);
	//vm_stop(0);

	return cr3;
}

// pass pe_entry structure
int update_api_with_pe(uint32_t cr3, struct pe_entry *pef, uint32_t spaceType){
	uint32_t numOfExport =0; //requested_base = 0;
	int i;
	IMAGE_NT_HEADERS PeHeader;

	i = NTHDR_from_image(pef->base, &PeHeader, cr3, spaceType);
	if(i ==-1){
		return 0;
	}
	recon_getImageBase(&PeHeader);
	getExportTable_with_pe(&PeHeader, &numOfExport, pef, cr3, spaceType);
	return numOfExport;
}

// pass base addr, get all api symbols
int update_api_with_base(uint32_t cr3, uint32_t base, uint32_t spaceType){

	uint32_t numOfExport =0; //requested_base = 0;
	int i;
	IMAGE_NT_HEADERS PeHeader;

	i = NTHDR_from_image(base, &PeHeader, cr3, spaceType);
	if(i ==-1){
		return 0;
	}
	recon_getImageBase(&PeHeader);
	recon_getExportTable(&PeHeader, &numOfExport,base, cr3, spaceType);
	return numOfExport;
}

// get all dlls of one process
int update_loaded_user_mods(struct process_entry *proc)
{
	uint32_t proc_addr = proc->EPROC_base_addr;
	uint32_t curr_cr3, peb, ldr, memlist, first_dll, curr_dll;
	struct pe_entry *curr_entry = NULL;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	int ret = 0, flag = 0;
	QLIST_INIT(&proc->modlist_head);
	curr_cr3 = get_cr3_from_proc_base(proc_addr);
	cpu_memory_rw_debug(env, proc_addr + 0x1b0, (uint8_t *) &peb, 4, 0);

	if(peb == 0x00)
		goto done;

	TEMU_memory_rw_with_cr3 (curr_cr3, peb+0xc, (void *)&ldr, 4, 0);
	monitor_printf(default_mon,"peb is: 0x%08x\n",peb);
	memlist = ldr + 0xc;
	TEMU_memory_rw_with_cr3 (curr_cr3, memlist, (void *) &first_dll, 4, 0);

	if(first_dll == 0)
		goto done;

	curr_dll = first_dll;

	do {
		curr_entry = (struct pe_entry *) malloc (sizeof(*curr_entry));
		memset(curr_entry, 0, sizeof(*curr_entry));
		QLIST_INIT(&curr_entry->apilist_head);

		TEMU_memory_rw_with_cr3 (curr_cr3, curr_dll+ 0x18, (void *) &(curr_entry->base), 4, 0);
		if(curr_entry->base == 0x0&& flag == 0){
					flag = 1;
					TEMU_memory_rw_with_cr3 (curr_cr3, curr_dll, (void *) &curr_dll, 4, 0);
					continue;
				}
		TEMU_memory_rw_with_cr3 (curr_cr3, curr_dll+ 0x20, (void *) &(curr_entry->size), 4, 0);
		readustr_with_cr3(curr_dll + 0x2c, curr_cr3, curr_entry->name, env);
		readustr_with_cr3(curr_dll + 0x24, curr_cr3, curr_entry->fullname, env);

		if((curr_entry->name)[0] == '\0')
			continue;

		update_api_with_pe(curr_cr3, curr_entry, 1);
		QLIST_INSERT_HEAD(&proc->modlist_head, curr_entry, loadedlist_entry);
		/* insert modules info, call function in procmod.h-- here need change */
//		procmod_insert_modinfo(proc->process_id, curr_cr3, curr_entry->name, curr_entry->base, curr_entry->size);

		ret++;
		TEMU_memory_rw_with_cr3 (curr_cr3, curr_dll, (void *) &curr_dll, 4, 0);
	} while (curr_dll != 0 && curr_dll != first_dll);

done:
	return ret;
}


void update_loaded_pefiles()
{
	int temp;
	struct process_entry *proc = NULL;
	clear_list(PROC);
	update_active_processlist();

	QLIST_FOREACH (proc, &processlist, loadedlist_entry) {
		temp = update_loaded_user_mods(proc);
		monitor_printf(default_mon, "%d entries loaded.\n", temp);
	}
}
//static void update_open_files();
//static void update_connections();
//static void update_open_registry_keys();
//static void update_arp_table();

int clear_list(int type){
	struct process_entry* proc = NULL;
	struct service_entry* se = NULL;
	struct file_entry* fe = NULL;
	struct thread_entry* te = NULL;
	struct pe_entry* pef = NULL;
	struct api_entry* api = NULL;

	if(type == 0 && !QLIST_EMPTY(&loadedlist)){
		QLIST_FOREACH(se, &loadedlist, loadedlist_entry){
				QLIST_REMOVE(se,loadedlist_entry);
				free(se);
			}
		return 0;
	}

	if(type == 1 && !QLIST_EMPTY(&processlist)){
		QLIST_FOREACH(proc, &processlist, loadedlist_entry){
			if(!QLIST_EMPTY(&proc->modlist_head)){
				QLIST_FOREACH(pef, &proc->modlist_head, loadedlist_entry){
					if(!QLIST_EMPTY(&pef->apilist_head)){
						QLIST_FOREACH(api, &pef->apilist_head, loadedlist_entry){
							QLIST_REMOVE(api,loadedlist_entry);
							free(api);
						}
					}
					QLIST_REMOVE(pef,loadedlist_entry);
					free(pef);
				}
			}
			QLIST_REMOVE(proc,loadedlist_entry);
			free(proc);
		}
		return 0;
	}

	if(type == 2 && !QLIST_EMPTY(&threadlist)  ){
		QLIST_FOREACH(te, &threadlist, loadedlist_entry){
				QLIST_REMOVE(te,loadedlist_entry);
				free(te);
			}
		return 0;
	}
	if(type == 3 && !QLIST_EMPTY(&filelist)){
		QLIST_FOREACH(fe, &filelist, loadedlist_entry){
				QLIST_REMOVE(fe,loadedlist_entry);
				free(fe);
			}
		return 0;
	}
	return -1;
}

static void update_loaded_kernel_modulelist() {
	uint32_t kdvb, psLM, curr_mod, next_mod;
	CPUState *env;
	struct service_entry *se;
	uint32_t holder;

	if (gkpcr == 0)
		return;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}
	// clear list
	clear_list(MODULES);

	cpu_memory_rw_debug(env, gkpcr + KDVB_OFFSET, (uint8_t *) &kdvb, 4, 0);
	cpu_memory_rw_debug(env, kdvb + PSLM_OFFSET, (uint8_t *) &psLM, 4, 0);
	cpu_memory_rw_debug(env, psLM, (uint8_t *) &curr_mod, 4, 0);

	while (curr_mod != 0 && curr_mod != psLM) {
		se = (struct service_entry *) malloc(sizeof(struct service_entry));
		cpu_memory_rw_debug(env, curr_mod + handle_funds[GuestOS_index].offset->DLLBASE_OFFSET, (uint8_t *) &(se->base), 4, 0); // dllbase  DLLBASE_OFFSET
		cpu_memory_rw_debug(env, curr_mod + handle_funds[GuestOS_index].offset->SIZE_OFFSET, (uint8_t *) &(se->size), 4, 0);           // dllsize  SIZE_OFFSET
		holder = readustr(curr_mod + handle_funds[GuestOS_index].offset->DLLNAME_OFFSET, se->name, env);

		QLIST_INSERT_HEAD(&loadedlist, se, loadedlist_entry);

		cpu_memory_rw_debug(env, curr_mod, (uint8_t *) &next_mod, 4, 0);
		cpu_memory_rw_debug(env, next_mod + 4, (uint8_t *) &holder, 4, 0);
		if (holder != curr_mod) {
			monitor_printf(default_mon,
					"Something is wrong. Next->prev != curr. curr_mod = 0x%08x\n",
					curr_mod);
			//vm_stop(0);
		}
		curr_mod = next_mod;
	}
}

static void update_active_processlist() {
	uint32_t kdvb, psAPH, curr_proc, next_proc, handle_table;
	CPUState *env;
	struct process_entry *pe;

	if (gkpcr == 0)
		return;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}
	clear_list(PROC);
	cpu_memory_rw_debug(env, gkpcr + KDVB_OFFSET, (uint8_t *) &kdvb, 4, 0);
	cpu_memory_rw_debug(env, kdvb + PSAPH_OFFSET, (uint8_t *) &psAPH, 4, 0);
	cpu_memory_rw_debug(env, psAPH, (uint8_t *) &curr_proc, 4, 0);

	while (curr_proc != 0 && curr_proc != psAPH) {
		pe = (struct process_entry *) malloc(sizeof(struct process_entry));
		memset(pe, 0, sizeof(*pe));

		pe->EPROC_base_addr = curr_proc - handle_funds[GuestOS_index].offset->PSAPL_OFFSET;
		pe->cr3 = get_cr3_from_proc_base(pe->EPROC_base_addr);
		uint32_t curr_proc_base = pe->EPROC_base_addr;

		cpu_memory_rw_debug(env, curr_proc_base + handle_funds[GuestOS_index].offset->PSAPNAME_OFFSET, (uint8_t *) &(pe->name), NAMESIZE, 0);
		cpu_memory_rw_debug(env, curr_proc_base + handle_funds[GuestOS_index].offset->PSAPID_OFFSET, (uint8_t *) &(pe->process_id), 4, 0);
		cpu_memory_rw_debug(env, curr_proc_base + handle_funds[GuestOS_index].offset->PSAPPID_OFFSET, (uint8_t *) &(pe->ppid), 4, 0);
		cpu_memory_rw_debug(env, curr_proc_base + handle_funds[GuestOS_index].offset->PSAPTHREADS_OFFSET, (uint8_t *) &(pe->number_of_threads), 4, 0);
		cpu_memory_rw_debug(env, curr_proc_base + handle_funds[GuestOS_index].offset->PSAPHANDLES_OFFSET, (uint8_t *) &(handle_table), 4, 0);
		cpu_memory_rw_debug(env, handle_table, (uint8_t *) &(pe->table_code), 4, 0);
		cpu_memory_rw_debug(env, handle_table + handle_funds[GuestOS_index].offset->HANDLE_COUNT_OFFSET, (uint8_t *) &(pe->number_of_handles), 4, 0);

		QLIST_INSERT_HEAD(&processlist, pe, loadedlist_entry);

		cpu_memory_rw_debug(env, curr_proc, (uint8_t *) &next_proc, 4, 0);
		curr_proc = next_proc;
	}

	/* Update the process data structures in procmod */

//	procmod_remove_all();
//	QLIST_FOREACH(pe, &processlist, loadedlist_entry) {
//		procmod_createproc(pe->process_id, pe->ppid,
//			       get_cr3_from_proc_base(pe->EPROC_base_addr), pe->name);
//	}


}

static void update_active_threadlist() {
	uint32_t kdvb, psAPH, thrdLH, curr_proc, next_proc;
	uint32_t curr_thrd, next_thrd, trapframe;
	CPUState *env;
	struct thread_entry *te = NULL;
	struct tcb* _tcb = NULL;

	if (gkpcr == 0)
		return;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}
	clear_list(THRD);
	cpu_memory_rw_debug(env, gkpcr + KDVB_OFFSET, (uint8_t *) &kdvb, 4, 0);
	cpu_memory_rw_debug(env, kdvb + PSAPH_OFFSET, (uint8_t *) &psAPH, 4, 0);
	cpu_memory_rw_debug(env, psAPH, (uint8_t *) &curr_proc, 4, 0);

	while (curr_proc != 0 && curr_proc != psAPH) {
		thrdLH = curr_proc - handle_funds[GuestOS_index].offset->PSAPL_OFFSET + handle_funds[GuestOS_index].offset->THREADLH_OFFSET;
		cpu_memory_rw_debug(env, thrdLH, (uint8_t *) &(curr_thrd), 4, 0);
		while (curr_thrd != 0 && curr_thrd != thrdLH) {

			te = (struct thread_entry *) malloc(sizeof(struct thread_entry));
			_tcb = (struct tcb *) malloc(sizeof(struct tcb));

			te->tcb = _tcb;
			te->ETHREAD_base_addr = curr_thrd - handle_funds[GuestOS_index].offset->THREADENTRY_OFFSET;
			cpu_memory_rw_debug(env, curr_thrd - handle_funds[GuestOS_index].offset->THREADENTRY_OFFSET + handle_funds[GuestOS_index].offset->TRAPFRAME_OFFSET, (uint8_t *) &(trapframe), 4, 0);
			cpu_memory_rw_debug(env, trapframe + 0x44, (uint8_t *) &(_tcb->_EAX), 4, 0);
			cpu_memory_rw_debug(env, trapframe + 0x5c, (uint8_t *) &(_tcb->_EBX), 4, 0);
			cpu_memory_rw_debug(env, trapframe + 0x40, (uint8_t *) &(_tcb->_ECX), 4, 0);
			cpu_memory_rw_debug(env, trapframe + 0x3c, (uint8_t *) &(_tcb->_EDX), 4, 0);
			cpu_memory_rw_debug(env, curr_thrd - handle_funds[GuestOS_index].offset->THREADENTRY_OFFSET + handle_funds[GuestOS_index].offset->THREADCID_OFFSET, (uint8_t *) &(te->owning_process_id), 4, 0);
			cpu_memory_rw_debug(env, curr_thrd - handle_funds[GuestOS_index].offset->THREADENTRY_OFFSET + handle_funds[GuestOS_index].offset->THREADCID_OFFSET + 0x04, (uint8_t *) &(te->thread_id), 4, 0);

			QLIST_INSERT_HEAD(&threadlist, te, loadedlist_entry);
			cpu_memory_rw_debug(env, curr_thrd, (uint8_t *) &next_thrd, 4, 0);
			curr_thrd = next_thrd;
		}

		cpu_memory_rw_debug(env, curr_proc, (uint8_t *) &next_proc, 4, 0);
		curr_proc = next_proc;
	}
}
// traverse handle table 1st level for win7
uint32_t w7_traverse_level1(uint32_t tableaddr, CPUState *env) {
	uint32_t count = 1;
	uint32_t object_body_addr, object_header_addr, info_mask;
	uint8_t object_type_index;
	W7TYPE_TABLE type = File;

	struct file_entry *fe;
	do {
		fe = (struct file_entry *) malloc(sizeof(struct file_entry));
		cpu_memory_rw_debug(env, tableaddr + count * 0x08, (uint8_t *) &object_header_addr, 4, 0);
		object_header_addr &= 0xffffffff8;

		if (object_header_addr == 0) {
			continue;
		}
		cpu_memory_rw_debug(env, object_header_addr + 0x0c, (uint8_t *) &object_type_index, 2, 0);
		cpu_memory_rw_debug(env, object_header_addr + 0xe,	(uint8_t *) &info_mask, 4, 0);
		object_body_addr = object_header_addr + 0x18;

		if (object_type_index == type) {
			char file_type[32] = "File";
			fe->file_object_base = object_header_addr;
			strcpy(fe->type, file_type);
			readustr(object_body_addr + 0x30, fe->filename, env);
			QLIST_INSERT_HEAD(&filelist, fe, loadedlist_entry);
		}
	} while (++count <= 511);
	return 0;
}

uint32_t traverse_level1(uint32_t tableaddr, CPUState *env) {
	uint32_t count = 1;
	uint32_t object_body_addr, object_header_addr, object_type_addr, nameinfo_offset;
	uint32_t handle_table;

	char type[1024];
	char file[] = "File";
	//char proc[] = "Process";

	struct file_entry *fe;
	do {
		fe = (struct file_entry *) malloc(sizeof(struct file_entry));
		cpu_memory_rw_debug(env, tableaddr + count * 0x08, (uint8_t *) &object_header_addr, 4, 0);
		object_header_addr &= 0xffffffff8;
		if (object_header_addr == 0) {
			continue;
		}
		cpu_memory_rw_debug(env, object_header_addr + 0x08, (uint8_t *) &object_type_addr, 4, 0);
		cpu_memory_rw_debug(env, object_header_addr + 0x0c,	(uint8_t *) &nameinfo_offset, 4, 0);
		object_body_addr = object_header_addr + 0x18;
		readustr(object_type_addr + 0x40, type, env);
		//monitor_printf(default_mon,"here we are%s\n", type);

		if (strcmp(type, file) == 0 ) {
			fe->file_object_base = object_header_addr;
			strcpy(fe->type, type);
			readustr(object_body_addr + 0x30, fe->filename, env);
			QLIST_INSERT_HEAD(&filelist, fe, loadedlist_entry);
		}
	} while (++count <= 511);
	return 0;
}

uint32_t traverse_level2(uint32_t tableaddr, CPUState *env) {
	uint32_t level2_table_ptr;
	uint32_t level2_next_table_ptr;
	do {
		cpu_memory_rw_debug(env, tableaddr, (uint8_t *) &level2_table_ptr, 4, 0);
		if(GuestOS_index < 2){
			traverse_level1(level2_table_ptr, env);
		}else{
			w7_traverse_level1(level2_table_ptr, env);
		}

		tableaddr += 0x04;
		cpu_memory_rw_debug(env, tableaddr, (uint8_t *) &level2_next_table_ptr, 4, 0);
	} while (level2_next_table_ptr != 0);
	return 0;
}

uint32_t traverse_level3(uint32_t tableaddr, CPUState *env) {
	uint32_t level3_table_ptr;
	uint32_t level3_next_table_ptr;
	do {
		cpu_memory_rw_debug(env, tableaddr, (uint8_t *) &level3_table_ptr, 4, 0);
		traverse_level2(level3_table_ptr, env);
		tableaddr += 0x04;
		cpu_memory_rw_debug(env, tableaddr, (uint8_t *) &level3_next_table_ptr,	4, 0);
	} while (level3_next_table_ptr != 0);
	return 0;
}



static void update_opened_filelist() {
	uint32_t kdvb, psAPH, curr_proc, next_proc, num_handles, handle_table,
			table_code;
	CPUState *env;
	uint32_t tablecode;
	uint32_t level;

	if (gkpcr == 0)
		return;

	for (env = first_cpu; env != NULL; env = env->next_cpu) {
		if (env->cpu_index == 0) {
			break;
		}
	}
	clear_list(FILE);
	cpu_memory_rw_debug(env, gkpcr + KDVB_OFFSET, (uint8_t *) &kdvb, 4, 0);
	cpu_memory_rw_debug(env, kdvb + PSAPH_OFFSET, (uint8_t *) &psAPH, 4, 0);
	cpu_memory_rw_debug(env, psAPH, (uint8_t *) &curr_proc, 4, 0);

	while (curr_proc != 0 && curr_proc != psAPH) {
		cpu_memory_rw_debug(env, curr_proc - handle_funds[GuestOS_index].offset->PSAPL_OFFSET + handle_funds[GuestOS_index].offset->PSAPHANDLES_OFFSET,	(uint8_t *) &(handle_table), 4, 0);
		cpu_memory_rw_debug(env, handle_table + handle_funds[GuestOS_index].offset->HANDLE_COUNT_OFFSET, (uint8_t *) &(num_handles), 4, 0);
		cpu_memory_rw_debug(env, handle_table, (uint8_t *) &(table_code), 4, 0);

		level = table_code & 3;
		tablecode = table_code & 0xfffffffc;
		//char* file = "File";
		switch (level) {
		case 0:
			// choose one func
			if(GuestOS_index < 2){
				traverse_level1(tablecode, env);
			}else{
				w7_traverse_level1(tablecode, env);
			}
			break;
		case 1:
			traverse_level2(tablecode, env);
			break;
		case 2:
			traverse_level3(tablecode, env);
			break;
		default:
			break;
		}
		cpu_memory_rw_debug(env, curr_proc, (uint8_t *) &next_proc, 4, 0);
		curr_proc = next_proc;
	}
}


static void update_symbolslist(Monitor *mon, const QDict *qdict){

	update_loaded_pefiles();
	function_map_remove();
	struct process_entry *proc = NULL;
	struct pe_entry *pef =NULL;
	struct api_entry *api = NULL;
	QLIST_FOREACH (proc, &processlist, loadedlist_entry) {
		monitor_printf(default_mon, "----------Getting loaded modules for %s. PID: %d--------\n\n",
			          proc->name, proc->process_id);
		uint32_t cr3 = get_cr3_from_proc_base(proc->EPROC_base_addr);

		QLIST_FOREACH(pef, &proc->modlist_head, loadedlist_entry){
			monitor_printf(default_mon, "----------Getting symbols for: 0x%08x, %s, %d, 0x%08x----------\n\n",
					      pef->base, pef->name, pef->size,pef->apilist_head);

			int i =1;
			QLIST_FOREACH(api, &pef->apilist_head, loadedlist_entry){
				monitor_printf(default_mon, "symbols %d: 0x%08x, %s \n", i++, api->base, api->name);
				// function_map
				function_map_create(pef->name, api->name, cr3, api->base);
			}
		}
	}
	hook_test();
}

void do_list_modules(Monitor *mon, const QDict *qdict) {
	struct service_entry *se = NULL;
	handle_funds[GuestOS_index].update_modulelist();
	monitor_printf(default_mon, "%d\tLoaded modules...\n", GuestOS_index);
	QLIST_FOREACH(se, &loadedlist, loadedlist_entry)
	{
		monitor_printf(default_mon, "0x%08x\t%d\t%s\n", se->base, se->size,
				se->name);
	}
}

struct process_entry* get_new_process() {
	struct process_entry *pe = NULL;
	handle_funds[GuestOS_index].update_processlist();
	monitor_printf(default_mon, "%d\tnew process...\n", GuestOS_index);
	QLIST_FOREACH(pe, &processlist, loadedlist_entry)
		{
			monitor_printf(default_mon, "0x%08x\t%d\t%s\t%d\t%d\t%d\t0x%08x\n",
					pe->EPROC_base_addr, pe->ppid, pe->name, pe->process_id,
					pe->number_of_threads, pe->number_of_handles, pe->cr3);
		}
	pe = QLIST_FIRST(&processlist);
	return pe;
}

void do_list_processes(Monitor *mon, const QDict *qdict) {
	struct process_entry *pe = NULL;
	handle_funds[GuestOS_index].update_processlist();
	monitor_printf(default_mon, "%d\tLoad process...\n", GuestOS_index);
	QLIST_FOREACH(pe, &processlist, loadedlist_entry)
	{
		monitor_printf(default_mon, "0x%08x\t%d\t%s\t%d\t%d\t%d\t0x%08x\n",
				pe->EPROC_base_addr, pe->ppid, pe->name, pe->process_id,
				pe->number_of_threads, pe->number_of_handles, pe->cr3);
//		procmod_remove_all();
//			//QLIST_FOREACH(pe, &processlist, loadedlist_entry) {
//		procmod_createproc(pe->process_id, pe->ppid,
//					       get_cr3_from_proc_base(pe->EPROC_base_addr), pe->name);
//			///}
	}
}
void do_list_threads(Monitor *mon, const QDict *qdict) {
	struct thread_entry *te = NULL;
	handle_funds[GuestOS_index].update_threadlist();
	monitor_printf(default_mon, "%d\tLoad threads...\n", GuestOS_index);
	QLIST_FOREACH(te, &threadlist, loadedlist_entry)
	{
		monitor_printf(default_mon,
				"0x%08x\t%d\t%d\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t\n",
				te->ETHREAD_base_addr, te->thread_id, te->owning_process_id,
				te->tcb->_EAX, te->tcb->_EBX, te->tcb->_ECX, te->tcb->_EDX);
	}
}
void do_list_files(Monitor *mon, const QDict *qdict) {
	struct file_entry *fe = NULL;
	handle_funds[GuestOS_index].update_filelist();
	monitor_printf(default_mon, "%d\tLoad files...\n", GuestOS_index);
	QLIST_FOREACH(fe, &filelist, loadedlist_entry)
	{
		monitor_printf(default_mon, "0x%08x\t%s\t%s\n", fe->file_object_base,
				fe->type, fe->filename);
	}
}

void remove_proc(struct process_entry *proc)
{
	struct pe_entry *mod, *next;
	struct api_entry *api, *next_api;
	struct thread_entry *thr, *next_thr;
	QLIST_FOREACH_SAFE(mod, &proc->modlist_head, loadedlist_entry, next) {
		QLIST_FOREACH_SAFE(api, &mod->apilist_head, loadedlist_entry, next_api) {
			QLIST_REMOVE(api, loadedlist_entry);
			free(api);
		}
		QLIST_REMOVE(mod, loadedlist_entry);
		free(mod);
	}
	QLIST_FOREACH_SAFE(thr, &threadlist, loadedlist_entry, next_thr) {
		if(thr->owning_process_id == proc->process_id) {
			QLIST_REMOVE(thr, loadedlist_entry);
			free(thr);
		}
	}
}

uint32_t exit_block_end_eip = 0;
void check_procexit()
{
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
//	if(!TEMU_is_in_kernel())
//		return;
//
//	if(exit_block_end_eip && env->eip != exit_block_end_eip)
//		return;

	qemu_mod_timer(recon_timer, qemu_get_clock_ns(vm_clock) + get_ticks_per_sec() * 30);

	monitor_printf(default_mon, "Checking for proc exits...\n");

	struct process_entry *proc = NULL, *next = NULL;
	uint32_t end_time[2];
	if(!QLIST_EMPTY(&processlist)){
         QLIST_FOREACH_SAFE(proc, &processlist, loadedlist_entry, next){
			if(proc->ppid == 0)
				continue;
			//0x78 for xp, 0x88 for win7
            cpu_memory_rw_debug(env, (proc->EPROC_base_addr)+handle_funds[GuestOS_index].offset->PEXIT_TIME, (uint8_t *)&end_time[0], 8, 0);
			if(end_time[0] | end_time[1]) {
				QLIST_REMOVE(proc, loadedlist_entry);
				remove_proc(proc);
				message_p(proc, 0);
				free(proc);
				exit_block_end_eip = env->eip;
				//return;
			}
		}
    }
}

void insn_end_cb(){

	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	struct cr3_info *cr3i = NULL;
	uint32_t cr3 = env->cr[3];
	uint32_t base;
	insn_counter++;

	if(env->eip > 0x80000000 && env->segs[R_FS].base > 0x80000000) {
			gkpcr = get_kpcr();
			if(gkpcr != 0){
				DECAF_unregister_callback(DECAF_INSN_END_CB, insn_handle);

				QLIST_INIT (&loadedlist);
				QLIST_INIT (&processlist);
				QLIST_INIT (&threadlist);
				QLIST_INIT (&filelist);
				cr3_hashtable = g_hash_table_new(0,0);
				eproc_ht = g_hash_table_new(0,0);

				get_os_version();
				base = get_ntoskrnl(env);
				if(!base) {
					monitor_printf(default_mon, "Unable to locate kernel base. Stopping VM...\n");
					vm_stop(0);
					return;
				}

				//////////////////////////////block_handle = DECAF_register_callback(DECAF_BLOCK_BEGIN_CB, block_begin_cb, NULL);
				qemu_mod_timer(recon_timer, qemu_get_clock_ns(vm_clock) + get_ticks_per_sec() * 30);
				TEMU_register_tlb_callback(tlb_call_back);
			}
	}
}

void recon_cleanup() {
	 if(insn_handle != 0)
		 DECAF_unregister_callback(DECAF_INSN_END_CB, insn_handle);
	 if(block_handle != 0)
		 DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB, block_handle);
	 qemu_del_timer(recon_timer);
}

void recon_init()
{
	insn_handle = DECAF_register_callback(DECAF_INSN_END_CB, insn_end_cb, NULL);
	recon_timer = qemu_new_timer_ns(vm_clock, check_procexit, 0);
}
