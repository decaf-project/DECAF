
#ifndef _LINUXDRV_H
#define _LINUXDRV_H

#define JPROBE_SUM    7
#define MESSAGE_MAX   512	/* max len of one message */
#define VIRTUAL_PORT "0x68"	/* virtual port in temu */

enum task_state {
    TASK_FORK,	/* task created */
    TASK_EXIT,	/* task exiting */
    TASK_EXEC,	/* task new binary loaded */
};

enum vma_state {
    VMA_CREATE,	/* create vma */
    VMA_REMOVE,	/* remove vma */
    VMA_MODIFY,	/* modify vma */
};

#endif
