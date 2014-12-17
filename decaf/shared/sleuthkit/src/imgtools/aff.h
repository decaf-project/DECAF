#ifndef _AFF_H
#define _AFF_H

#include <afflib.h>

extern IMG_INFO *aff_open(const char **, IMG_INFO *);

typedef struct IMG_AFF_INFO IMG_AFF_INFO;

struct IMG_AFF_INFO {
    IMG_INFO img_info;
    AFFILE *af_file;
    off_t seek_pos;
    uint16_t type;		/* TYPE - uses AF_IDENTIFY_x values */
};

#endif
