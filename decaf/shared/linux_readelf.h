
#ifndef LINUX_READELF_H_
#define LINUX_READELF_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int read_elf_info(const char * mod_name, target_ulong start_addr, unsigned int inode_number);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif
