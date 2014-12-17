/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#ifndef _ANDROID_SKIN_FILE_H
#define _ANDROID_SKIN_FILE_H

#include "android/skin/image.h"
#include "android/config.h"
#include "android/framebuffer.h"

/**  Layout
 **/

typedef struct SkinBackground {
    SkinImage*  image;
    SkinRect    rect;
    char        valid;
} SkinBackground;

typedef struct SkinDisplay {
    SkinRect      rect;      /* display rectangle    */
    SkinRotation  rotation;  /* framebuffer rotation */
    int           bpp;       /* bits per pixel, 32 or 16 */
    char          valid;
    QFrameBuffer  qfbuff[1];
} SkinDisplay;

typedef struct SkinButton {
    struct SkinButton*  next;
    const char*         name;
    SkinImage*          image;
    SkinRect            rect;
    unsigned            keycode;
} SkinButton;

typedef struct SkinPart {
    struct SkinPart*   next;
    const char*        name;
    SkinBackground     background[1];
    SkinDisplay        display[1];
    SkinButton*        buttons;
    SkinRect           rect;    /* bounding box of all parts */
} SkinPart;

#define  SKIN_PART_LOOP_BUTTONS(part,button)              \
    do {                                                  \
        SkinButton*  __button = (part)->buttons;          \
        while (__button != NULL) {                        \
            SkinButton*  __button_next = __button->next;  \
            SkinButton*  button        = __button;

#define   SKIN_PART_LOOP_END             \
            __button = __button_next;    \
        }                                \
    } while (0);

typedef struct SkinLocation {
    SkinPart*             part;
    SkinPos               anchor;
    SkinRotation          rotation;
    struct SkinLocation*  next;
} SkinLocation;

typedef struct SkinLayout {
    struct SkinLayout*  next;
    const char*         name;
    unsigned            color;
    int                 event_type;
    int                 event_code;
    int                 event_value;
    char                has_dpad_rotation;
    SkinRotation        dpad_rotation;
    SkinSize            size;
    SkinLocation*       locations;
} SkinLayout;

#define  SKIN_LAYOUT_LOOP_LOCS(layout,loc)               \
    do {                                                 \
        SkinLocation*  __loc = (layout)->locations;      \
        while (__loc != NULL) {                          \
            SkinLocation*  __loc_next = (__loc)->next;   \
            SkinLocation*  loc        = __loc;

#define  SKIN_LAYOUT_LOOP_END   \
            __loc = __loc_next; \
        }                       \
    } while (0);

extern SkinDisplay*   skin_layout_get_display( SkinLayout*  layout );

extern SkinRotation   skin_layout_get_dpad_rotation( SkinLayout*  layout );

typedef struct SkinFile {
    int             version;  /* 1, 2 or 3 */
    SkinPart*       parts;
    SkinLayout*     layouts;
    int             num_parts;
    int             num_layouts;
} SkinFile;

#define  SKIN_FILE_LOOP_LAYOUTS(file,layout)             \
    do {                                                 \
        SkinLayout*  __layout = (file)->layouts;         \
        while (__layout != NULL) {                       \
            SkinLayout*  __layout_next = __layout->next; \
            SkinLayout*  layout        = __layout;

#define  SKIN_FILE_LOOP_END_LAYOUTS       \
            __layout = __layout_next;     \
        }                                 \
    } while (0);

#define  SKIN_FILE_LOOP_PARTS(file,part)                 \
    do {                                                 \
        SkinPart*   __part = (file)->parts;              \
        while (__part != NULL) {                         \
            SkinPart*  __part_next = __part->next;       \
            SkinPart*  part        = __part;

#define  SKIN_FILE_LOOP_END_PARTS  \
            __part = __part_next;  \
        }                          \
    } while (0);

extern SkinFile*  skin_file_create_from_aconfig( AConfig*   aconfig, const char*  basepath );
extern void       skin_file_free( SkinFile*  file );

#endif /* _ANDROID_SKIN_FILE_H */
