/*
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
*/

#ifndef _FS_DATA_H
#define _FS_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

    extern FS_DATA *fs_data_alloc(uint8_t);
    extern FS_DATA_RUN *fs_data_run_alloc();
    extern FS_DATA *fs_data_getnew_attr(FS_DATA *, uint8_t);
    extern void fs_data_clear_list(FS_DATA *);

    extern FS_DATA *fs_data_put_str(FS_DATA *, char *, uint32_t, uint16_t,
	void *, unsigned int);

    extern FS_DATA *fs_data_put_run(FS_DATA *, DADDR_T, OFF_T,
	FS_DATA_RUN *, char *, uint32_t, uint16_t, OFF_T, uint8_t);

    extern FS_DATA *fs_data_lookup(FS_DATA *, uint32_t, uint16_t);
    extern FS_DATA *fs_data_lookup_noid(FS_DATA *, uint32_t);

    extern void fs_data_run_free(FS_DATA_RUN *);
    extern void fs_data_free(FS_DATA *);

#ifdef __cplusplus
}
#endif
#endif
