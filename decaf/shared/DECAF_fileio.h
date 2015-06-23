#include "tsk/libtsk.h"


// AVB, this struct is used to store info about the disk images opened by qemu
// We use this to open the disk using Sleuthkit and read files from it
typedef struct {
  TSK_FS_INFO *fs;
  TSK_IMG_INFO *img;
  void *bs;
} disk_info_t;


// List of loaded disk images
// This could be more than 5. We just assume 5 for now
extern disk_info_t disk_info_internal[5];