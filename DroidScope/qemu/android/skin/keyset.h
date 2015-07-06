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
#ifndef _ANDROID_SKIN_KEYSET_H_
#define _ANDROID_SKIN_KEYSET_H_

#include "android/config.h"

/* A SkinKeySet maps keystrokes to specific commands. we have a few hard-coded
 * keysets in the emulator binary, and the user can define its own if he wants
 * to...
 */
typedef struct SkinKeyset  SkinKeyset;

#define  SKIN_KEY_COMMAND_LIST                  \
    _SKIN_KEY_COMMAND(NONE,"no key")            \
    _SKIN_KEY_COMMAND(BUTTON_HOME,"Home button")              \
    _SKIN_KEY_COMMAND(BUTTON_MENU,"Menu (Soft-Left) button")   \
    _SKIN_KEY_COMMAND(BUTTON_STAR,"Star (Soft-Right) button")  \
    _SKIN_KEY_COMMAND(BUTTON_BACK,"Back button")              \
    _SKIN_KEY_COMMAND(BUTTON_CALL,"Call/Dial button")              \
    _SKIN_KEY_COMMAND(BUTTON_HANGUP,"Hangup/EndCall button")            \
    _SKIN_KEY_COMMAND(BUTTON_POWER,"Power button")             \
    _SKIN_KEY_COMMAND(BUTTON_SEARCH,"Search button")            \
    _SKIN_KEY_COMMAND(BUTTON_VOLUME_UP,"Volume up button")         \
    _SKIN_KEY_COMMAND(BUTTON_VOLUME_DOWN,"Volume down button")       \
    _SKIN_KEY_COMMAND(BUTTON_CAMERA,"Camera button")            \
    _SKIN_KEY_COMMAND(CHANGE_LAYOUT_PREV,"Switch to previous layout")       \
    _SKIN_KEY_COMMAND(CHANGE_LAYOUT_NEXT,"Switch to next layout")       \
    _SKIN_KEY_COMMAND(TOGGLE_NETWORK,"Toggle cell network on/off")           \
    _SKIN_KEY_COMMAND(TOGGLE_TRACING,"Toggle code profiling")           \
    _SKIN_KEY_COMMAND(TOGGLE_FULLSCREEN,"Toggle fullscreen mode")        \
    _SKIN_KEY_COMMAND(TOGGLE_TRACKBALL,"Toggle trackball mode")         \
    _SKIN_KEY_COMMAND(SHOW_TRACKBALL,"Show trackball") \
    _SKIN_KEY_COMMAND(BUTTON_DPAD_CENTER,"DPad center")       \
    _SKIN_KEY_COMMAND(BUTTON_DPAD_LEFT,"DPad left") \
    _SKIN_KEY_COMMAND(BUTTON_DPAD_RIGHT,"DPad right")        \
    _SKIN_KEY_COMMAND(BUTTON_DPAD_UP,"DPad up")           \
    _SKIN_KEY_COMMAND(BUTTON_DPAD_DOWN,"DPad down")         \
    _SKIN_KEY_COMMAND(ONION_ALPHA_UP,"Increase onion alpha")           \
    _SKIN_KEY_COMMAND(ONION_ALPHA_DOWN,"Decrease onion alpha")         \
    _SKIN_KEY_COMMAND(BUTTON_TV,"TV button")         \
    _SKIN_KEY_COMMAND(BUTTON_EPG,"EPG button")         \
    _SKIN_KEY_COMMAND(BUTTON_DVR,"DVR button")         \
    _SKIN_KEY_COMMAND(BUTTON_PREV,"Previous button")         \
    _SKIN_KEY_COMMAND(BUTTON_NEXT,"Next button")         \
    _SKIN_KEY_COMMAND(BUTTON_PLAY,"Play button")         \
    _SKIN_KEY_COMMAND(BUTTON_PAUSE,"Pause button")         \
    _SKIN_KEY_COMMAND(BUTTON_STOP,"Stop button")         \
    _SKIN_KEY_COMMAND(BUTTON_REWIND,"Rewind button")         \
    _SKIN_KEY_COMMAND(BUTTON_FFWD,"Fast forward button")         \
    _SKIN_KEY_COMMAND(BUTTON_BOOKMARKS,"Bookmarks button")         \
    _SKIN_KEY_COMMAND(BUTTON_WINDOW,"Window button")         \
    _SKIN_KEY_COMMAND(BUTTON_CHANNELUP,"Channel up button")         \
    _SKIN_KEY_COMMAND(BUTTON_CHANNELDOWN,"Channel down button")         \


/* the list of commands in the emulator */
#define _SKIN_KEY_COMMAND(x,y)  SKIN_KEY_COMMAND_##x,
typedef enum {
    SKIN_KEY_COMMAND_LIST
    SKIN_KEY_COMMAND_MAX  // do not remove
} SkinKeyCommand;
#undef _SKIN_KEY_COMMAND

/* retrieve the textual name of a given command, this is the command name without
 * the "SKIN_KEY_COMMAND_" prefix. returns NULL if command is NONE or invalid
 * the result is a static constant string that must not be freed
 */
extern const char*      skin_key_command_to_str  ( SkinKeyCommand  command );

/* convert a string into a SkinKeyCommand. returns SKIN_COMMAND_NONE if the string
 * is unknown
 */
extern SkinKeyCommand   skin_key_command_from_str( const char*  str, int  len );

/* returns a short human-friendly description of the command */
extern const char*      skin_key_command_description( SkinKeyCommand  cmd );

/* returns the number of keysym string descriptors */
extern int              skin_keysym_str_count( void );

/* return the n-th keysym string descriptor */
extern const char*      skin_keysym_str( int  index );

/* convert a (sym,mod) pair into a descriptive string. e.g. "Ctrl-K" or "Alt-A", etc..
 * result is a static string that is overwritten on each call
 */
extern const char*      skin_key_symmod_to_str   ( int  sym, int  mod );

/* convert a key binding description into a (sym,mod) pair. returns 0 on success, -1
 * if the string cannot be parsed.
 */
extern int              skin_key_symmod_from_str ( const char*  str, int  *psym, int  *pmod );

/* create a new keyset from a configuration tree node */
extern SkinKeyset*      skin_keyset_new ( AConfig*  root );
extern SkinKeyset*      skin_keyset_new_from_text( const char*  text );

/* destroy a given keyset */
extern void             skin_keyset_free( SkinKeyset*  kset );

/* maximum number of key bindings per command. one command can be bound to several
 * key bindings for convenience
 */
#define  SKIN_KEY_COMMAND_MAX_BINDINGS  3

/* a structure that describe a key binding */
typedef struct {
    int  sym;   // really a SDL key symbol
    int  mod;   // really a SDL key modifier
} SkinKeyBinding;

/* return the number of keyboard bindings for a given command. results are placed in the 'bindings' array
 * which must have at least SKIN_KEY_MAX_BINDINGS items */
extern int              skin_keyset_get_bindings( SkinKeyset*      kset,
                                                  SkinKeyCommand   command,
                                                  SkinKeyBinding*  bindings );

/* return the command for a given keypress - SKIN_KEY_COMMAND_NONE is returned if unknown */
extern SkinKeyCommand   skin_keyset_get_command( SkinKeyset*  kset, int  sym, int  mod );

extern const char*      skin_keyset_get_default( void );

/* in android_main.c */
extern SkinKeyset*      android_keyset;

#endif /* _ANDROID_SKIN_KEYSET_H_ */
