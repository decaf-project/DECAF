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
#include "android/skin/file.h"
#include "android/utils/path.h"
#include "android/charmap.h"
#include "android/utils/bufprint.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"

//#include "qemu-common.h"

/** UTILITY ROUTINES
 **/
static SkinImage*
skin_image_find_in( const char*  dirname, const char*  filename )
{
    char   buffer[1024];
    char*  p   = buffer;
    char*  end = p + sizeof(buffer);

    p = bufprint( p, end, "%s" PATH_SEP "%s", dirname, filename );
    if (p >= end)
        return SKIN_IMAGE_NONE;

    return skin_image_find_simple(buffer);
}

/** SKIN BACKGROUND
 **/

static void
skin_background_done( SkinBackground*  background )
{
    if (background->image)
        skin_image_unref(&background->image);
}

static int
skin_background_init_from( SkinBackground*  background,
                           AConfig*         node,
                           const char*      basepath )
{
    const char* img = aconfig_str(node, "image", NULL);
    int         x   = aconfig_int(node, "x", 0);
    int         y   = aconfig_int(node, "y", 0);

    background->valid = 0;

    if (img == NULL)   /* no background */
        return -1;

    background->image = skin_image_find_in( basepath, img );
    if (background->image == SKIN_IMAGE_NONE) {
        background->image = NULL;
        return -1;
    }

    background->rect.pos.x  = x;
    background->rect.pos.y  = y;
    background->rect.size.w = skin_image_w( background->image );
    background->rect.size.h = skin_image_h( background->image );

    background->valid = 1;

    return 0;
}

/** SKIN DISPLAY
 **/

static void
skin_display_done( SkinDisplay*  display )
{
    qframebuffer_done( display->qfbuff );
}

static int
skin_display_init_from( SkinDisplay*  display, AConfig*  node )
{
    display->rect.pos.x  = aconfig_int(node, "x", 0);
    display->rect.pos.y  = aconfig_int(node, "y", 0);
    display->rect.size.w = aconfig_int(node, "width", 0);
    display->rect.size.h = aconfig_int(node, "height", 0);
    display->rotation    = aconfig_unsigned(node, "rotation", SKIN_ROTATION_0);
    display->bpp         = aconfig_int(node, "bpp", 16);

    display->valid = ( display->rect.size.w > 0 && display->rect.size.h > 0 );

    if (display->valid) {
        SkinRect  r;
        skin_rect_rotate( &r, &display->rect, -display->rotation );
        qframebuffer_init( display->qfbuff,
                           r.size.w,
                           r.size.h,
                           0,
                           display->bpp == 32 ? QFRAME_BUFFER_RGBX_8888
                                              : QFRAME_BUFFER_RGB565 );

        qframebuffer_fifo_add( display->qfbuff );
    }
    return display->valid ? 0 : -1;
}

/** SKIN BUTTON
 **/

typedef struct
{
    const char*     name;
    AndroidKeyCode  code;
} KeyInfo;

static KeyInfo  _keyinfo_table[] = {
    { "dpad-up",      kKeyCodeDpadUp },
    { "dpad-down",    kKeyCodeDpadDown },
    { "dpad-left",    kKeyCodeDpadLeft },
    { "dpad-right",   kKeyCodeDpadRight },
    { "dpad-center",  kKeyCodeDpadCenter },
    { "soft-left",    kKeyCodeSoftLeft },
    { "soft-right",   kKeyCodeSoftRight },
    { "search",       kKeyCodeSearch },
    { "camera",       kKeyCodeCamera },
    { "volume-up",    kKeyCodeVolumeUp },
    { "volume-down",  kKeyCodeVolumeDown },
    { "power",        kKeyCodePower },
    { "home",         kKeyCodeHome },
    { "back",         kKeyCodeBack },
    { "del",          kKeyCodeDel },
    { "0",            kKeyCode0 },
    { "1",            kKeyCode1 },
    { "2",            kKeyCode2 },
    { "3",            kKeyCode3 },
    { "4",            kKeyCode4 },
    { "5",            kKeyCode5 },
    { "6",            kKeyCode6 },
    { "7",            kKeyCode7 },
    { "8",            kKeyCode8 },
    { "9",            kKeyCode9 },
    { "star",         kKeyCodeStar },
    { "pound",        kKeyCodePound },
    { "phone-dial",   kKeyCodeCall },
    { "phone-hangup", kKeyCodeEndCall },
    { "q",            kKeyCodeQ },
    { "w",            kKeyCodeW },
    { "e",            kKeyCodeE },
    { "r",            kKeyCodeR },
    { "t",            kKeyCodeT },
    { "y",            kKeyCodeY },
    { "u",            kKeyCodeU },
    { "i",            kKeyCodeI },
    { "o",            kKeyCodeO },
    { "p",            kKeyCodeP },
    { "a",            kKeyCodeA },
    { "s",            kKeyCodeS },
    { "d",            kKeyCodeD },
    { "f",            kKeyCodeF },
    { "g",            kKeyCodeG },
    { "h",            kKeyCodeH },
    { "j",            kKeyCodeJ },
    { "k",            kKeyCodeK },
    { "l",            kKeyCodeL },
    { "DEL",          kKeyCodeDel },
    { "z",            kKeyCodeZ },
    { "x",            kKeyCodeX },
    { "c",            kKeyCodeC },
    { "v",            kKeyCodeV },
    { "b",            kKeyCodeB },
    { "n",            kKeyCodeN },
    { "m",            kKeyCodeM },
    { "COMMA",        kKeyCodeComma },
    { "PERIOD",       kKeyCodePeriod },
    { "ENTER",        kKeyCodeNewline },
    { "AT",           kKeyCodeAt },
    { "SPACE",        kKeyCodeSpace },
    { "SLASH",        kKeyCodeSlash },
    { "CAP",          kKeyCodeCapLeft },
    { "SYM",          kKeyCodeSym },
    { "ALT",          kKeyCodeAltLeft },
    { "ALT2",         kKeyCodeAltRight },
    { "CAP2",         kKeyCodeCapRight },
    { "tv",           kKeyCodeTV },
    { "epg",          kKeyCodeEPG },
    { "dvr",          kKeyCodeDVR },
    { "prev",         kKeyCodePrevious },
    { "next",         kKeyCodeNext },
    { "play",         kKeyCodePlay },
    { "pause",        kKeyCodePause },
    { "stop",         kKeyCodeStop },
    { "rev",          kKeyCodeRewind },
    { "ffwd",         kKeyCodeFastForward },
    { "bookmarks",    kKeyCodeBookmarks },
    { "window",       kKeyCodeCycleWindows },
    { "channel-up",   kKeyCodeChannelUp },
    { "channel-down", kKeyCodeChannelDown },
    { 0, 0 },
};

static unsigned
keyinfo_lookup_code(const char *name)
{
    KeyInfo *ki = _keyinfo_table;
    while(ki->name) {
        if(!strcmp(name, ki->name))
            return ki->code;
        ki++;
    }
    return 0;
}


static void
skin_button_free( SkinButton*  button )
{
    if (button) {
        skin_image_unref( &button->image );
        AFREE(button);
    }
}

static SkinButton*
skin_button_create_from( AConfig*   node, const char*  basepath )
{
    SkinButton*  button;
    ANEW0(button);
    if (button) {
        const char*  img = aconfig_str(node, "image", NULL);
        int          x   = aconfig_int(node, "x", 0);
        int          y   = aconfig_int(node, "y", 0);

        button->name       = node->name;
        button->rect.pos.x = x;
        button->rect.pos.y = y;

        if (img != NULL)
            button->image = skin_image_find_in( basepath, img );

        if (button->image == SKIN_IMAGE_NONE) {
            skin_button_free(button);
            return NULL;
        }

        button->rect.size.w = skin_image_w( button->image );
        button->rect.size.h = skin_image_h( button->image );

        button->keycode = keyinfo_lookup_code( button->name );
        if (button->keycode == 0) {
            dprint( "Warning: skin file button uses unknown key name '%s'", button->name );
        }
    }
    return button;
}

/** SKIN PART
 **/

static void
skin_part_free( SkinPart*  part )
{
    if (part) {
        skin_background_done( part->background );
        skin_display_done( part->display );

        SKIN_PART_LOOP_BUTTONS(part,button)
            skin_button_free(button);
        SKIN_PART_LOOP_END
        part->buttons = NULL;
        AFREE(part);
    }
}

static SkinLocation*
skin_location_create_from_v2( AConfig*  node, SkinPart*  parts )
{
    const char*    partname = aconfig_str(node, "name", NULL);
    int            x        = aconfig_int(node, "x", 0);
    int            y        = aconfig_int(node, "y", 0);
    SkinRotation   rot      = aconfig_int(node, "rotation", SKIN_ROTATION_0);
    SkinPart*      part;
    SkinLocation*  location;

    if (partname == NULL) {
        dprint( "### WARNING: ignoring part location without 'name' element" );
        return NULL;
    }

    for (part = parts; part; part = part->next)
        if (!strcmp(part->name, partname))
            break;

    if (part == NULL) {
        dprint( "### WARNING: ignoring part location with unknown name '%s'", partname );
        return NULL;
    }

    ANEW0(location);
    location->part     = part;
    location->anchor.x = x;
    location->anchor.y = y;
    location->rotation = rot;

    return location;
}

static SkinPart*
skin_part_create_from_v1( AConfig*  root, const char*  basepath )
{
    SkinPart*  part;
    AConfig*  node;
    SkinBox   box;

    ANEW0(part);
    part->name = root->name;

    node = aconfig_find(root, "background");
    if (node)
        skin_background_init_from(part->background, node, basepath);

    node = aconfig_find(root, "display");
    if (node)
        skin_display_init_from(part->display, node);

    node = aconfig_find(root, "button");
    if (node) {
        for (node = node->first_child; node != NULL; node = node->next)
        {
            SkinButton*  button = skin_button_create_from(node, basepath);

            if (button != NULL) {
                button->next  = part->buttons;
                part->buttons = button;
            }
        }
    }

    skin_box_minmax_init( &box );

    if (part->background->valid)
        skin_box_minmax_update( &box, &part->background->rect );

    if (part->display->valid)
        skin_box_minmax_update( &box, &part->display->rect );

    SKIN_PART_LOOP_BUTTONS(part, button)
        skin_box_minmax_update( &box, &button->rect );
    SKIN_PART_LOOP_END

    if ( !skin_box_minmax_to_rect( &box, &part->rect ) ) {
        skin_part_free(part);
        part = NULL;
    }

    return part;
}

static SkinPart*
skin_part_create_from_v2( AConfig*  root, const char*  basepath )
{
    SkinPart*  part;
    AConfig*  node;
    SkinBox   box;

    ANEW0(part);
    part->name = root->name;

    node = aconfig_find(root, "background");
    if (node)
        skin_background_init_from(part->background, node, basepath);

    node = aconfig_find(root, "display");
    if (node)
        skin_display_init_from(part->display, node);

    node = aconfig_find(root, "buttons");
    if (node) {
        for (node = node->first_child; node != NULL; node = node->next)
        {
            SkinButton*  button = skin_button_create_from(node, basepath);

            if (button != NULL) {
                button->next  = part->buttons;
                part->buttons = button;
            }
        }
    }

    skin_box_minmax_init( &box );

    if (part->background->valid)
        skin_box_minmax_update( &box, &part->background->rect );

    if (part->display->valid)
        skin_box_minmax_update( &box, &part->display->rect );

    SKIN_PART_LOOP_BUTTONS(part, button)
        skin_box_minmax_update( &box, &button->rect );
    SKIN_PART_LOOP_END

    if ( !skin_box_minmax_to_rect( &box, &part->rect ) ) {
        skin_part_free(part);
        part = NULL;
    }
    return part;
}

/** SKIN LAYOUT
 **/

static void
skin_layout_free( SkinLayout*  layout )
{
    if (layout) {
        SKIN_LAYOUT_LOOP_LOCS(layout,loc)
            AFREE(loc);
        SKIN_LAYOUT_LOOP_END
        layout->locations = NULL;
        AFREE(layout);
    }
}

SkinDisplay*
skin_layout_get_display( SkinLayout*  layout )
{
    SKIN_LAYOUT_LOOP_LOCS(layout,loc)
        SkinPart*  part = loc->part;
        if (part->display->valid) {
            return part->display;
        }
    SKIN_LAYOUT_LOOP_END
    return NULL;
}

SkinRotation
skin_layout_get_dpad_rotation( SkinLayout*  layout )
{
    if (layout->has_dpad_rotation)
        return layout->dpad_rotation;

    SKIN_LAYOUT_LOOP_LOCS(layout, loc)
        SkinPart*  part = loc->part;
        SKIN_PART_LOOP_BUTTONS(part,button)
            if (button->keycode == kKeyCodeDpadUp)
                return loc->rotation;
        SKIN_PART_LOOP_END
    SKIN_LAYOUT_LOOP_END

    return SKIN_ROTATION_0;
}


static int
skin_layout_event_decode( const char*  event, int  *ptype, int  *pcode, int *pvalue )
{
    typedef struct {
        const char*  name;
        int          value;
    } EventName;

    static const EventName  _event_names[] = {
        { "EV_SW", 0x05 },
        { NULL, 0 },
    };

    const char*       x = strchr(event, ':');
    const char*       y = NULL;
    const EventName*  ev = _event_names;

    if (x != NULL)
        y = strchr(x+1, ':');

    if (x == NULL || y == NULL) {
        dprint( "### WARNING: invalid skin layout event format: '%s', should be '<TYPE>:<CODE>:<VALUE>'", event );
        return -1;
    }

    for ( ; ev->name != NULL; ev++ )
        if (!memcmp( event, ev->name, x - event ) && ev->name[x-event] == 0)
            break;

    if (!ev->name) {
        dprint( "### WARNING: unrecognized skin layout event name: %.*s", x-event, event );
        return -1;
    }

    *ptype  = ev->value;
    *pcode  = strtol(x+1, NULL, 0);
    *pvalue = strtol(y+1, NULL, 0);
    return 0;
}

static SkinLayout*
skin_layout_create_from_v2( AConfig*  root, SkinPart*  parts )
{
    SkinLayout*    layout;
    int            width, height;
    SkinLocation** ptail;
    AConfig*       node;

    ANEW0(layout);

    width  = aconfig_int( root, "width", 400 );
    height = aconfig_int( root, "height", 400 );

    node = aconfig_find( root, "event" );
    if (node != NULL) {
        skin_layout_event_decode( node->value,
                                  &layout->event_type,
                                  &layout->event_code,
                                  &layout->event_value );
    } else {
        layout->event_type  = 0x05;  /* close keyboard by default */
        layout->event_code  = 0;
        layout->event_value = 1;
    }

    layout->name  = root->name;
    layout->color = aconfig_unsigned( root, "color", 0x808080 ) | 0xff000000;
    ptail         = &layout->locations;

    node = aconfig_find( root, "dpad-rotation" );
    if (node != NULL) {
        layout->dpad_rotation     = aconfig_int( root, "dpad-rotation", 0 );
        layout->has_dpad_rotation = 1;
    }

    for (node = root->first_child; node; node = node->next)
    {
        if (!memcmp(node->name, "part", 4)) {
            SkinLocation*  location = skin_location_create_from_v2( node, parts );
            if (location == NULL) {
                continue;
            }
            *ptail = location;
            ptail  = &location->next;
        }
    }

    if (layout->locations == NULL)
        goto Fail;

    layout->size.w = width;
    layout->size.h = height;

    return layout;

Fail:
    skin_layout_free(layout);
    return NULL;
}

/** SKIN FILE
 **/

static int
skin_file_load_from_v1( SkinFile*  file, AConfig*  aconfig, const char*  basepath )
{
    SkinPart*      part;
    SkinLayout*    layout;
    SkinLayout**   ptail = &file->layouts;
    SkinLocation*  location;
    int            nn;

    file->parts = part = skin_part_create_from_v1( aconfig, basepath );
    if (part == NULL)
        return -1;

    for (nn = 0; nn < 2; nn++)
    {
        ANEW0(layout);

        layout->color = 0xff808080;

        ANEW0(location);

        layout->event_type  = 0x05;  /* close keyboard by default */
        layout->event_code  = 0;
        layout->event_value = 1;

        location->part     = part;
        switch (nn) {
            case 0:
                location->anchor.x = 0;
                location->anchor.y = 0;
                location->rotation = SKIN_ROTATION_0;
                layout->size       = part->rect.size;
                break;

#if 0
            case 1:
                location->anchor.x = part->rect.size.h;
                location->anchor.y = 0;
                location->rotation = SKIN_ROTATION_90;
                layout->size.w     = part->rect.size.h;
                layout->size.h     = part->rect.size.w;
                layout->event_value = 0;
                break;

            case 2:
                location->anchor.x = part->rect.size.w;
                location->anchor.y = part->rect.size.h;
                location->rotation = SKIN_ROTATION_180;
                layout->size       = part->rect.size;
                break;
#endif
            default:
                location->anchor.x = 0;
                location->anchor.y = part->rect.size.w;
                location->rotation = SKIN_ROTATION_270;
                layout->size.w     = part->rect.size.h;
                layout->size.h     = part->rect.size.w;
                layout->event_value = 0;
                break;
        }
        layout->locations = location;

        *ptail = layout;
        ptail  = &layout->next;
    }
    file->version = 1;
    return 0;
}

static int
skin_file_load_from_v2( SkinFile*  file, AConfig*  aconfig, const char*  basepath )
{
    AConfig*  node;

    /* first, load all parts */
    node = aconfig_find(aconfig, "parts");
    if (node == NULL)
        return -1;
    else
    {
        SkinPart**  ptail = &file->parts;
        for (node = node->first_child; node != NULL; node = node->next)
        {
            SkinPart*  part = skin_part_create_from_v2( node, basepath );
            if (part == NULL) {
                dprint( "## WARNING: can't load part '%s' from skin\n", node->name ? "<NULL>" : node->name );
                continue;
            }
            part->next = NULL;
            *ptail     = part;
            ptail      = &part->next;
        }
    }

    if (file->parts == NULL)
        return -1;

    /* then load all layouts */
    node = aconfig_find(aconfig, "layouts");
    if (node == NULL)
        return -1;
    else
    {
        SkinLayout**  ptail = &file->layouts;
        for (node = node->first_child; node != NULL; node = node->next)
        {
            SkinLayout*  layout = skin_layout_create_from_v2( node, file->parts );
            if (layout == NULL) {
                dprint( "## WARNING: ignoring layout in skin file" );
                continue;
            }
            *ptail = layout;
            layout->next = NULL;
            ptail        = &layout->next;
        }
    }
    if (file->layouts == NULL)
        return -1;

    file->version = 2;
    return 0;
}

SkinFile*
skin_file_create_from_aconfig( AConfig*   aconfig, const char*  basepath )
{
    SkinFile*  file;

    ANEW0(file);

    if ( aconfig_find(aconfig, "parts") != NULL) {
        if (skin_file_load_from_v2( file, aconfig, basepath ) < 0) {
            goto BAD_FILE;
        }
        file->version = aconfig_int(aconfig, "version", 2);
        /* The file version must be 1 or higher */
        if (file->version <= 0) {
            dprint( "## WARNING: invalid skin version: %d", file->version);
            goto BAD_FILE;
        }
    }
    else {
        if (skin_file_load_from_v1( file, aconfig, basepath ) < 0) {
            goto BAD_FILE;
        }
        file->version = 1;
    }
    return file;

BAD_FILE:
    skin_file_free( file );
    return NULL;
}

void
skin_file_free( SkinFile*  file )
{
    if (file) {
        SKIN_FILE_LOOP_LAYOUTS(file,layout)
            skin_layout_free(layout);
        SKIN_FILE_LOOP_END_LAYOUTS
        file->layouts = NULL;

        SKIN_FILE_LOOP_PARTS(file,part)
            skin_part_free(part);
        SKIN_FILE_LOOP_END_PARTS
        file->parts = NULL;

        AFREE(file);
    }
}
