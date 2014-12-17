#ifdef __cplusplus
extern "C" {
#endif

    extern char *progname;
    extern int verbose;

#define TSK_ERRSTR_L	512
    extern uint32_t tsk_errno;
    extern char tsk_errstr[TSK_ERRSTR_L];
    extern char tsk_errstr2[TSK_ERRSTR_L];

    extern void tsk_error_print(FILE *);

#define TSK_ERR_AUX	0x01000000
#define TSK_ERR_IMG	0x02000000
#define TSK_ERR_MM	0x04000000
#define TSK_ERR_FS	0x08000000
#define TSK_ERR_MASK	0x00ffffff

#define TSK_ERR_AUX_MALLOC	(TSK_ERR_AUX | 0)
#define TSK_ERR_AUX_MAX		2

#define TSK_ERR_IMG_NOFILE	(TSK_ERR_IMG | 0)
#define TSK_ERR_IMG_OFFSET	(TSK_ERR_IMG | 1)
#define TSK_ERR_IMG_UNKTYPE	(TSK_ERR_IMG | 2)
#define TSK_ERR_IMG_UNSUPTYPE 	(TSK_ERR_IMG | 3)
#define TSK_ERR_IMG_OPEN 	(TSK_ERR_IMG | 4)
#define TSK_ERR_IMG_STAT	(TSK_ERR_IMG | 5)
#define TSK_ERR_IMG_SEEK	(TSK_ERR_IMG | 6)
#define TSK_ERR_IMG_READ	(TSK_ERR_IMG | 7)
#define TSK_ERR_IMG_READ_OFF	(TSK_ERR_IMG | 8)
#define TSK_ERR_IMG_LAYERS	(TSK_ERR_IMG | 9)
#define TSK_ERR_IMG_MAGIC	(TSK_ERR_IMG | 10)
#define TSK_ERR_IMG_WRITE	(TSK_ERR_IMG | 11)
#define TSK_ERR_IMG_MAX		12

#define TSK_ERR_MM_UNKTYPE	(TSK_ERR_MM | 0)
#define TSK_ERR_MM_UNSUPTYPE	(TSK_ERR_MM | 1)
#define TSK_ERR_MM_READ		(TSK_ERR_MM | 2)
#define TSK_ERR_MM_MAGIC	(TSK_ERR_MM | 3)
#define TSK_ERR_MM_WALK_RNG	(TSK_ERR_MM | 4)
#define TSK_ERR_MM_BUF		(TSK_ERR_MM | 5)
#define TSK_ERR_MM_BLK_NUM	(TSK_ERR_MM | 6)
#define TSK_ERR_MM_MAX		7

#define TSK_ERR_FS_UNKTYPE	(TSK_ERR_FS | 0)
#define TSK_ERR_FS_UNSUPTYPE	(TSK_ERR_FS | 1)
#define TSK_ERR_FS_FUNC		(TSK_ERR_FS | 2)
#define TSK_ERR_FS_WALK_RNG	(TSK_ERR_FS | 3)
#define TSK_ERR_FS_READ		(TSK_ERR_FS | 4)
#define TSK_ERR_FS_ARG		(TSK_ERR_FS | 5)
#define TSK_ERR_FS_BLK_NUM	(TSK_ERR_FS | 6)
#define TSK_ERR_FS_INODE_NUM	(TSK_ERR_FS | 7)
#define TSK_ERR_FS_INODE_INT	(TSK_ERR_FS | 8)
#define TSK_ERR_FS_MAGIC	(TSK_ERR_FS | 9)
#define TSK_ERR_FS_FWALK	(TSK_ERR_FS | 10)
#define TSK_ERR_FS_WRITE	(TSK_ERR_FS | 11)
#define TSK_ERR_FS_UNICODE	(TSK_ERR_FS | 12)
#define TSK_ERR_FS_RECOVER	(TSK_ERR_FS | 13)
#define TSK_ERR_FS_GENFS	(TSK_ERR_FS | 14)
#define TSK_ERR_FS_MAX		15


#ifdef __cplusplus
}
#endif
