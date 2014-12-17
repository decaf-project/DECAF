
#ifndef LINUX_READELF_H_
#define LINUX_READELF_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int read_elf_info(CPUState *env, uint32_t cr3, const char * mod_name, target_ulong start_addr, uint64_t size);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif
