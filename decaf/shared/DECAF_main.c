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
 * DECAF_main.c
 *
 *  Created on: Oct 14, 2012
 *      Author: lok
 */

#include <dlfcn.h>
#include "sysemu.h"

#include "shared/DECAF_main.h"
#include "shared/DECAF_main_internal.h"
#include "shared/DECAF_vm_compress.h"
#include "shared/DECAF_cmds.h"
#include "shared/DECAF_callback_to_QEMU.h"
#include "shared/hookapi.h"
#include "DECAF_target.h"

// AVB, Add only when file analysis is needed
#include "shared/DECAF_fileio.h"


#include "procmod.h"
#include "block_int.h"
#ifdef CONFIG_TCG_TAINT
#include "tainting/taint_memory.h"
#include "tainting/taintcheck_opt.h"
#endif /* CONFIG_TCG_TAINT */

#ifdef CONFIG_VMI_ENABLE
extern void VMI_init(void);
#endif

int DECAF_kvm_enabled = 0;

plugin_interface_t *decaf_plugin = NULL;
static void *plugin_handle = NULL;
static char decaf_plugin_path[PATH_MAX] = "";
static FILE *decaflog = NULL;

int should_monitor = 1;

static int devices=0;

struct __flush_list flush_list_internal;




mon_cmd_t DECAF_mon_cmds[] = {
#include "DECAF_mon_cmds.h"
		{ NULL, NULL , }, };

mon_cmd_t DECAF_info_cmds[] = {
#include "DECAF_info_cmds.h"
		{ NULL, NULL , }, };


int g_bNeedFlush = 0;
disk_info_t disk_info_internal[5];

static void convert_endian_4b(uint32_t *data);


static gpa_t _DECAF_get_phys_addr(CPUState* env, gva_t addr) {
	int mmu_idx, index;
	uint32_t phys_addr;

	index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
	mmu_idx = cpu_mmu_index(env);
	if (__builtin_expect(
			env->tlb_table[mmu_idx][index].addr_read
					!= (addr & TARGET_PAGE_MASK), 0)) {
		if (__builtin_expect(
				env->tlb_table[mmu_idx][index].addr_code
						!= (addr & TARGET_PAGE_MASK), 0)) {
			phys_addr = cpu_get_phys_page_debug(env, addr & TARGET_PAGE_MASK);
			if (phys_addr == -1)
				return -1;
			phys_addr += addr & (TARGET_PAGE_SIZE - 1);
			return phys_addr;
		}
	}

	void *p = (void *) (addr+ env->tlb_table[mmu_idx][index].addend);

//			(void *) ((addr & TARGET_PAGE_MASK)+ env->tlb_table[mmu_idx][index].addend);
	return (gpa_t) qemu_ram_addr_from_host_nofail(p);
}

gpa_t DECAF_get_phys_addr(CPUState* env, gva_t addr)
{
	gpa_t phys_addr;
	if (env == NULL )
	{
#ifdef DECAF_NO_FAIL_SAFE
		return(INV_ADDR);
#else
		env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
#endif
	}

#ifdef TARGET_MIPS
	uint32_t ori_hflags = env->hflags;
	env->hflags &= ~MIPS_HFLAG_UM;
	env->hflags &= ~MIPS_HFLAG_SM;
#endif

	phys_addr = _DECAF_get_phys_addr(env, addr);

	// restore hflags
#ifdef TARGET_MIPS
	env->hflags = ori_hflags;
#endif
	return phys_addr;

}

DECAF_errno_t DECAF_memory_rw(CPUState* env, /*uint32_t*/target_ulong addr, void *buf, int len,
		int is_write) {
	int l;
	target_ulong page, phys_addr;

	if (env == NULL ) {
#ifdef DECAF_NO_FAIL_SAFE
		return(INV_ADDR);
#else
		env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
#endif
	}

	int ret = 0;

	while (len > 0) {
		page = addr & TARGET_PAGE_MASK;
		phys_addr = DECAF_get_phys_addr(env, page);
		if (phys_addr == -1 || phys_addr > ram_size) {
			ret = -1;
			break;
		}
		l = (page + TARGET_PAGE_SIZE) - addr;
		if (l > len)
			l = len;

		cpu_physical_memory_rw(phys_addr + (addr & ~TARGET_PAGE_MASK), buf, l,
		is_write);

		len -= l;
		buf += l;
		addr += l;
	}

	return ret;
}

DECAF_errno_t DECAF_memory_rw_with_pgd(CPUState* env, target_ulong pgd,
		gva_t addr, void *buf, int len, int is_write) {
	if (env == NULL ) {
#ifdef DECAF_NO_FAIL_SAFE
		return (INV_ADDR);
#else
		env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
#endif
	}

	int l;
	gpa_t page, phys_addr;

	while (len > 0) {
		page = addr & TARGET_PAGE_MASK;
		phys_addr = DECAF_get_phys_addr_with_pgd(env, pgd, page);
		if (phys_addr == -1)
			return -1;
		l = (page + TARGET_PAGE_SIZE) - addr;
		if (l > len)
			l = len;
		cpu_physical_memory_rw(phys_addr + (addr & ~TARGET_PAGE_MASK), buf, l,
		is_write);
		len -= l;
		buf += l;
		addr += l;
	}
	return 0;
}

DECAF_errno_t DECAF_read_mem(CPUState* env, gva_t vaddr, int len, void *buf) {
	return DECAF_memory_rw(env, vaddr, buf, len, 0);
}

DECAF_errno_t DECAF_write_mem(CPUState* env, gva_t vaddr, int len, void *buf) {
	return DECAF_memory_rw(env, vaddr, buf, len, 1);
}

DECAF_errno_t DECAF_read_mem_with_pgd(CPUState* env, target_ulong cr3,
		gva_t vaddr, int len, void *buf) {
	return DECAF_memory_rw_with_pgd(env, cr3, vaddr, buf, len, 0);
}

DECAF_errno_t DECAF_write_mem_with_pgd(CPUState* env, target_ulong cr3,
		gva_t vaddr, int len, void *buf) {
	return DECAF_memory_rw_with_pgd(env, cr3, vaddr, buf, len, 1);
}

//Modified from tb_find_slow
static TranslationBlock *DECAF_tb_find_slow(CPUState *env, target_ulong pc) {
	TranslationBlock *tb, **ptb1;
	unsigned int h;

	tb_invalidated_flag = 0;

//DECAF_printf("DECAF_tb_find_slow: phys_pc=%08x\n", phys_pc);

	for (h = 0; h < CODE_GEN_PHYS_HASH_SIZE; h++) {
		ptb1 = &tb_phys_hash[h];
		for (;;) {
			tb = *ptb1;
			if (!tb)
				break;
			if (tb->pc + tb->cs_base == pc) {
				goto found;
			}
			ptb1 = &tb->phys_hash_next;
		}
	}

	//DECAF_printf("DECAF_tb_find_slow: not found!\n");
	return NULL ;

	found:
	//DECAF_printf("DECAF_tb_find_slow: found! pc=%08x size=%08x\n",tb->pc, tb->size);
	return tb;
}

// This is the same as tb_find_fast except we invalidate at the end
void DECAF_flushTranslationBlock_env(CPUState *env, /*uint32_t*/target_ulong addr) {
	TranslationBlock *tb;

	if (env == NULL ) {
#ifdef DECAF_NO_FAIL_SAFE
		return;
#else
		env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
#endif

	}

//	tb = env->tb_jmp_cache[tb_jmp_cache_hash_func(addr)];
//	if (unlikely(!tb || tb->pc+tb->cs_base != addr)) {
	tb = DECAF_tb_find_slow(env, addr);
//	}
	if (tb == NULL ) {
		return;
	}
	tb_phys_invalidate(tb, -1);
}

void DECAF_flushTranslationPage_env(CPUState* env, /*uint32_t*/target_ulong addr)
{
	if (env == NULL ) {
#ifdef DECAF_NO_FAIL_SAFE
		return;
#else
		env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
#endif
	}

	TranslationBlock *tb = DECAF_tb_find_slow(env, addr);
	if (tb) {
		tb_invalidate_phys_page_range(tb->page_addr[0],
				tb->page_addr[0] + TARGET_PAGE_SIZE, 0);
	}
}

/* Method to insert into the flush linked list, 
   It holds a list of all the flushes that need to be performed
   Flushed can be BLOCK_LEVEL, PAGE_LEVEL or ALL_CACHE, which is a flush
	of the complete cache */

void flush_list_insert(flush_list *list, int type, unsigned int addr )  {
		
	++list->size;
	flush_node *temp=list->head;
	flush_node *to_insert=(flush_node *)malloc(sizeof(flush_node));
	to_insert->type=type;
	to_insert->next=NULL;
	to_insert->addr=addr;
	
	if(temp==NULL) {
		list->head=to_insert;
		return;
	}

	while(temp->next !=NULL) {
		temp=temp->next;
	}

	temp->next=to_insert;
}

/* Method to perform flush, this is performed right before TB_fast_lookup()
	in the main loop of QEMU */

void DECAF_perform_flush(CPUState* env)
{
	//TO DO, Empty this list as its traversed,
	flush_node *prev,*temp=flush_list_internal.head;
	while(temp!=NULL) {
		switch (temp->type) {
			case BLOCK_LEVEL: 
				DECAF_flushTranslationBlock(temp->addr);
				break;
			case PAGE_LEVEL: 
				DECAF_flushTranslationPage(temp->addr);
				break;
			case ALL_CACHE:
				tb_flush(env);
				break;
		}
		prev=temp;
		temp=temp->next;
		prev->next=NULL;
		free(prev);
	}
	flush_list_internal.head=NULL;
	flush_list_internal.size=0;
}

void DECAF_flushTranslationCache(int type,target_ulong addr)
{
	flush_list_insert(&flush_list_internal,type,addr);
}


int do_load_plugin(Monitor *mon, const QDict *qdict, QObject **ret_data) {
	do_load_plugin_internal(mon, qdict_get_str(qdict, "filename"));
	return (0);
}

void do_load_plugin_internal(Monitor *mon, const char *plugin_path) {
	plugin_interface_t *(*init_plugin)(void);
	char *error;

	if (decaf_plugin_path[0]) {
		monitor_printf(mon, "%s has already been loaded! \n", plugin_path);
		return;
	}

	plugin_handle = dlopen(plugin_path, RTLD_NOW);
	if (NULL == plugin_handle) {
		// AWH
		char tempbuf[128];
		strncpy(tempbuf, dlerror(), 127);
		monitor_printf(mon, "%s\n", tempbuf);
		fprintf(stderr, "%s COULD NOT BE LOADED - ERR = [%s]\n", plugin_path,
				tempbuf);
		//assert(0);
		return;
	}

	dlerror();

	init_plugin = dlsym(plugin_handle, "init_plugin");
	if ((error = dlerror()) != NULL ) {
		fprintf(stderr, "%s\n", error);
		dlclose(plugin_handle);
		plugin_handle = NULL;
		return;
	}

	decaf_plugin = init_plugin();

	if (NULL == decaf_plugin) {
		monitor_printf(mon, "fail to initialize the plugin!\n");
		dlclose(plugin_handle);
		plugin_handle = NULL;
		return;
	}
#if defined(_REPLAY_) && !defined(_RECORD_)
	do_enable_emulation(); //emulation is automatically enabled while replay.
#endif
#if 0 // AWH TAINT_ENABLED
	plugin_taint_record_size = decaf_plugin->taint_record_size;
#endif
	decaflog = fopen("decaf.log", "w");
	assert(decaflog != NULL);
#if 0 //LOK: Removed // AWH TAINT_ENABLED
	taintcheck_create();
#endif

#if 0 //TO BE REMOVED -heng
	if (decaf_plugin->bdrv_open) {
#if 0 // AWH - uses blockdev.c "drives" queue now
		int i;
		for (i = 0; i <= nb_drives; ++i) {
			if (drives_table[i].bdrv)
			decaf_plugin->bdrv_open(i, drives_table[i].bdrv);
		}
#else
		BlockInterfaceType interType = IF_NONE;
		int index = 0;
		DriveInfo *drvInfo = NULL;
		for (interType = IF_NONE; interType < IF_COUNT; interType++) {
			index = 0;
			do {
				drvInfo = drive_get_by_index(interType, index);
				if (drvInfo && drvInfo->bdrv)
				decaf_plugin->bdrv_open(interType, index, drvInfo->bdrv);
				index++;
			}while (drvInfo);
		}
#endif // AWH
	}
#endif // Heng
	strncpy(decaf_plugin_path, plugin_path, PATH_MAX);
	monitor_printf(mon, "%s is loaded successfully!\n", plugin_path);
}

int do_unload_plugin(Monitor *mon, const QDict *qdict, QObject **ret_data) {
	if (decaf_plugin_path[0]) {
		decaf_plugin->plugin_cleanup();
		fclose(decaflog);
		decaflog = NULL;

		//Flush all the callbacks that the plugin might have registered for
		hookapi_flush_hooks(decaf_plugin_path);

		dlclose(plugin_handle);
		plugin_handle = NULL;
		decaf_plugin = NULL;

		monitor_printf(default_mon, "%s is unloaded!\n", decaf_plugin_path);
		decaf_plugin_path[0] = 0;
	} else {
		monitor_printf(default_mon,
				"Can't unload plugin because no plugin was loaded!\n");
	}

	return (0);
}

/* AWH - Fix for bugzilla bug #9 */
static int runningState = 0;
void DECAF_stop_vm(void) {
	if (runstate_is_running()) {
        vm_stop(RUN_STATE_PAUSED);
    }
	// if (runstate_check(RUN_STATE_RUNNING)) {
	// 	runningState = 1;
	// 	vm_stop(RUN_STATE_PAUSED);
	// } else
	// 	runningState = 0;
}

void DECAF_start_vm(void) {
	// if (runningState) vm_start();
    if (!runstate_is_running()) {
        vm_start();
    }	
}

void DECAF_loadvm(void *opaque) {
	char **loadvm_args = opaque;
	if (loadvm_args[0]) {
		/* AWH do_loadvm*/load_vmstate(loadvm_args[0]);
		free(loadvm_args[0]);
		loadvm_args[0] = NULL;
	}

	if (loadvm_args[1]) {
		do_load_plugin_internal(default_mon, loadvm_args[1]);
		free(loadvm_args[1]);
		loadvm_args[1] = NULL;
	}

	if (loadvm_args[2]) {
		DECAF_after_loadvm(loadvm_args[2]);
		free(loadvm_args[2]);
		loadvm_args[2] = NULL;
	}
}

FILE *guestlog = NULL;

static void DECAF_save(QEMUFile * f, void *opaque) {
	uint32_t len = strlen(decaf_plugin_path) + 1;
	qemu_put_be32(f, len);
	qemu_put_buffer(f, (const uint8_t *) decaf_plugin_path, len); // AWH - cast

	//save guest.log
	//we only save guest.log when no plugin is loaded
	if (len == 1) {
		FILE *fp = fopen("guest.log", "r");
		uint32_t size;
		if (!fp) {
			fprintf(stderr, "cannot open guest.log!\n");
			return;
		}

		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		qemu_put_be32(f, size);
		rewind(fp);
		if (size > 0) {
			DECAF_CompressState_t state;
			if (DECAF_compress_open(&state, f) < 0)
				return;

			while (!feof(fp)) {
				uint8_t buf[4096];
				size_t res = fread(buf, 1, sizeof(buf), fp);
				DECAF_compress_buf(&state, buf, res);
			}

			DECAF_compress_close(&state);
		}
		fclose(fp);
	}

	qemu_put_be32(f, 0x12345678);       //terminator
}

static int DECAF_load(QEMUFile * f, void *opaque, int version_id) {
	uint32_t len = qemu_get_be32(f);
	char tmp_plugin_path[PATH_MAX];

	if (plugin_handle) {
		do_unload_plugin(NULL, NULL, NULL ); // AWH - Added NULLs
	}
	qemu_get_buffer(f, (uint8_t *) tmp_plugin_path, len); // AWH - cast
	if (tmp_plugin_path[len - 1] != 0)
		return -EINVAL;

	//load guest.log
	if (len == 1) {
		fclose(guestlog);
		if (!(guestlog = fopen("guest.log", "w"))) {
			fprintf(stderr, "cannot open guest.log for write!\n");
			return -EINVAL;
		}

		uint32_t file_size = qemu_get_be32(f);
		uint8_t buf[4096];
		uint32_t i;
		DECAF_CompressState_t state;
		if (DECAF_decompress_open(&state, f) < 0)
			return -EINVAL;

		for (i = 0; i < file_size;) {
			uint32_t len =
					(sizeof(buf) < file_size - i) ? sizeof(buf) : file_size - i;
			if (DECAF_decompress_buf(&state, buf, len) < 0)
				return -EINVAL;

			fwrite(buf, 1, len, guestlog);
			i += len;
		}
		DECAF_decompress_close(&state);
		fflush(guestlog);
	}

	if (len > 1)
		do_load_plugin_internal(default_mon, tmp_plugin_path);

	uint32_t terminator = qemu_get_be32(f);
	if (terminator != 0x12345678)
		return -EINVAL;

	return 0;
}

extern void tainting_init(void);
extern void function_map_init(void);

void DECAF_init(void) {
	DECAF_callback_init();
	tainting_init();
	DECAF_virtdev_init();
	// AWH - change in API, added NULL as first parm
	/* Aravind - NOTE: DECAF_save *must* be called before function_map_save and DECAF_load must be called
	 * before function_map_load for function maps to load properly during loadvm.
	 * This is because, TEMU_load restores guest.log, which is read into function map.
	 */
	register_savevm(NULL, "DECAF", 0, 1, DECAF_save, DECAF_load, NULL );
	DECAF_vm_compress_init();
	function_map_init();
	init_hookapi();
#ifndef CONFIG_VMI_ENABLE
	procmod_init();
#else
	VMI_init();
#endif
}

/*
 * NIC related functions
 */
static void DECAF_virtdev_write_data(void *opaque, uint32_t addr, uint32_t val) {



	static char syslogline[GUEST_MESSAGE_LEN];
	static int pos = 0;

	if (pos >= GUEST_MESSAGE_LEN - 2)
		pos = GUEST_MESSAGE_LEN - 2;

	if ((syslogline[pos++] = (char) val) == 0) {
#ifndef CONFIG_VMI_ENABLE
		
		handle_guest_message(syslogline);
		// fprintf(guestlog, "%s", syslogline);
		// fflush(guestlog);
#endif
		pos = 0;
	}

}

void DECAF_virtdev_init(void) {
	int res = register_ioport_write(0x68, 1, 1, DECAF_virtdev_write_data,
			NULL );
	if (res) {
		fprintf(stderr, "failure on initializing DECAF virtual device\n");
		exit(-1);
	}
	if (!(guestlog = fopen("guest.log", "w"))) {
		fprintf(stderr, "failure on opening guest.log \n");
		exit(-1);
	}
}

void DECAF_after_loadvm(const char *param) {
	if (decaf_plugin && decaf_plugin->after_loadvm)
		decaf_plugin->after_loadvm(param);
}



/*
 * NIC related functions
 */

void DECAF_nic_receive(const uint8_t * buf, const int size, const int cur_pos,
		const int start, const int stop) {
	if (DECAF_is_callback_needed(DECAF_NIC_REC_CB))
		helper_DECAF_invoke_nic_rec_callback(buf, size, cur_pos, start, stop);

}

void DECAF_nic_send(const uint32_t addr, const int size, const uint8_t * buf) {
	if (DECAF_is_callback_needed(DECAF_NIC_SEND_CB))
		helper_DECAF_invoke_nic_send_callback(addr, size, buf);
}

void DECAF_nic_out(const uint32_t addr, const int size) {
#ifdef CONFIG_TCG_TAINT
	taintcheck_nic_writebuf(addr, size, (uint8_t *) &(cpu_single_env->tempidx));
#endif
}

void DECAF_nic_in(const uint32_t addr, const int size) {
#ifdef CONFIG_TCG_TAINT
	taintcheck_nic_readbuf(addr, size, (uint8_t *) &(cpu_single_env->tempidx));
#endif
}
/*
 * Keystroke related functions
 *
 */
uint32_t taint_keystroke_enabled = 0;
void DECAF_keystroke_place(int keycode) {
	if (DECAF_is_callback_needed(DECAF_KEYSTROKE_CB))
		helper_DECAF_invoke_keystroke_callback(keycode,
				&taint_keystroke_enabled);

}
void DECAF_keystroke_read(uint8_t taint_status) {
#ifdef CONFIG_TCG_TAINT
	if (taint_keystroke_enabled) {
		cpu_single_env->tempidx = taint_status;
		cpu_single_env->tempidx = cpu_single_env->tempidx & 0xFF;
	}
#endif /*CONFIG_TCG_TAINT*/
}


DECAF_errno_t DECAF_read_ptr(CPUState* env, gva_t vaddr, gva_t *pptr)
{
	int ret = DECAF_read_mem(env, vaddr, sizeof(gva_t), pptr);
	if(0 == ret)
	{
#ifdef TARGET_WORDS_BIGENDIAN
		convert_endian_4b(pptr);
#endif
	}
	return ret;
}

static void convert_endian_4b(uint32_t *data)
{
   *data = ((*data & 0xff000000) >> 24)
         | ((*data & 0x00ff0000) >>  8)
         | ((*data & 0x0000ff00) <<  8)
         | ((*data & 0x000000ff) << 24);
}

// AVB, This function is used to read 'n' bytes off the disk images give by `opaque' 
// at an offset
int DECAF_bdrv_pread(void *opaque, int64_t offset, void *buf, int count) {

	return bdrv_pread((BlockDriverState *)opaque, offset, buf, count);

}

// AVB, This function is used to open the disk on sleuthkit by calling `tsk_fs_open_img'.
void DECAF_bdrv_open(int index, void *opaque) {

  TSK_FS_INFO *fs=NULL;
  TSK_OFF_T a_offset = 0;
  unsigned long img_size = ((BlockDriverState *)opaque)->total_sectors * 512;

  if(!qemu_pread)
	  qemu_pread=(qemu_pread_t)DECAF_bdrv_pread;

  monitor_printf(default_mon, "inside bdrv open, drv addr= 0x%0x, size= %lu\n", opaque, img_size);
  
  disk_info_internal[devices].bs = opaque;
  disk_info_internal[devices].img = tsk_img_open(1, (const char **) &opaque, QEMU_IMG, 0);
  disk_info_internal[devices].img->size = img_size;
	

  if (disk_info_internal[devices].img==NULL)
  {
    monitor_printf(default_mon, "img_open error! \n",opaque);
  }

  // TODO: AVB, also add an option of 56 as offset with sector size of 4k, Sector size is now assumed to be 512 by default
  if(!(disk_info_internal[devices].fs = tsk_fs_open_img(disk_info_internal[devices].img, 0 ,TSK_FS_TYPE_EXT_DETECT)) &&
  	    !(disk_info_internal[devices].fs = tsk_fs_open_img(disk_info_internal[devices].img, 63 * (disk_info_internal[devices].img)->sector_size, TSK_FS_TYPE_EXT_DETECT)) &&
  	    	!(disk_info_internal[devices].fs = tsk_fs_open_img(disk_info_internal[devices].img, 2048 * (disk_info_internal[devices].img)->sector_size , TSK_FS_TYPE_EXT_DETECT)) )
  {
	monitor_printf(default_mon, "fs_open error! \n",opaque);
  }	
  else
  {
  	monitor_printf(default_mon, "fs_open = %s \n",(disk_info_internal[devices].fs)->duname);
  }

  ++devices;
}

