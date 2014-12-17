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
#include "android/skin/keyset.h"
#include "android/utils/debug.h"
#include "android/utils/bufprint.h"
#include "android/android.h"
#include <SDL.h>

#define  DEBUG  1

#if 1
#  define  D_ACTIVE  VERBOSE_CHECK(keys)
#else
#  define  D_ACTIVE  DEBUG
#endif

#if DEBUG
#  define  D(...)   VERBOSE_PRINT(keys,__VA_ARGS__)
#else
#  define  D(...)   ((void)0)
#endif

#define _SKIN_KEY_COMMAND(x,y)    #x ,
static const char* const command_strings[ SKIN_KEY_COMMAND_MAX ] = {
    SKIN_KEY_COMMAND_LIST
};
#undef _SKIN_KEY_COMMAND

const char*
skin_key_command_to_str( SkinKeyCommand  cmd )
{
    if (cmd > SKIN_KEY_COMMAND_NONE && cmd < SKIN_KEY_COMMAND_MAX)
        return  command_strings[cmd];

    return NULL;
}

SkinKeyCommand
skin_key_command_from_str( const char*  str, int  len )
{
    int  nn;
    if (len < 0)
        len = strlen(str);
    for (nn = 0; nn < SKIN_KEY_COMMAND_MAX; nn++) {
        const char*  cmd = command_strings[nn];

        if ( !memcmp( cmd, str, len ) && cmd[len] == 0 )
            return (SkinKeyCommand) nn;
    }
    return SKIN_KEY_COMMAND_NONE;
}


#define _SKIN_KEY_COMMAND(x,y)    y ,
static const char* const command_descriptions[ SKIN_KEY_COMMAND_MAX ] = {
    SKIN_KEY_COMMAND_LIST
};
#undef _SKIN_KEY_COMMAND

const char*
skin_key_command_description( SkinKeyCommand  cmd )
{
    if (cmd > SKIN_KEY_COMMAND_NONE && cmd < SKIN_KEY_COMMAND_MAX)
        return command_descriptions[cmd];

    return NULL;
}

#define  _KEYSYM1_(x)  _KEYSYM_(x,x)

#define _KEYSYM_LIST  \
    _KEYSYM1_(BACKSPACE)   \
    _KEYSYM1_(TAB)         \
    _KEYSYM1_(CLEAR)       \
    _KEYSYM_(RETURN,ENTER) \
    _KEYSYM1_(PAUSE)       \
    _KEYSYM1_(ESCAPE)      \
    _KEYSYM1_(SPACE)       \
    _KEYSYM_(EXCLAIM,EXCLAM)    \
    _KEYSYM_(QUOTEDBL,DOUBLEQUOTE)   \
    _KEYSYM_(HASH,HASH)    \
    _KEYSYM1_(DOLLAR)      \
    _KEYSYM1_(AMPERSAND)   \
    _KEYSYM1_(QUOTE)       \
    _KEYSYM_(LEFTPAREN,LPAREN)  \
    _KEYSYM_(RIGHTPAREN,RPAREN) \
    _KEYSYM1_(ASTERISK) \
    _KEYSYM1_(PLUS) \
    _KEYSYM1_(COMMA) \
    _KEYSYM1_(MINUS) \
    _KEYSYM1_(PERIOD) \
    _KEYSYM1_(SLASH) \
    _KEYSYM1_(0) \
    _KEYSYM1_(1) \
    _KEYSYM1_(2) \
    _KEYSYM1_(3) \
    _KEYSYM1_(4) \
    _KEYSYM1_(5) \
    _KEYSYM1_(6) \
    _KEYSYM1_(7) \
    _KEYSYM1_(8) \
    _KEYSYM1_(9) \
    _KEYSYM1_(COLON) \
    _KEYSYM1_(SEMICOLON) \
    _KEYSYM1_(LESS) \
    _KEYSYM_(EQUALS,EQUAL) \
    _KEYSYM1_(GREATER) \
    _KEYSYM1_(QUESTION) \
    _KEYSYM1_(AT) \
    _KEYSYM1_(LEFTBRACKET) \
    _KEYSYM1_(BACKSLASH) \
    _KEYSYM1_(RIGHTBRACKET) \
    _KEYSYM1_(CARET) \
    _KEYSYM1_(UNDERSCORE) \
    _KEYSYM1_(BACKQUOTE) \
    _KEYSYM_(a,A) \
    _KEYSYM_(b,B) \
    _KEYSYM_(c,C) \
    _KEYSYM_(d,D) \
    _KEYSYM_(e,E) \
    _KEYSYM_(f,F) \
    _KEYSYM_(g,G) \
    _KEYSYM_(h,H) \
    _KEYSYM_(i,I) \
    _KEYSYM_(j,J) \
    _KEYSYM_(k,K) \
    _KEYSYM_(l,L) \
    _KEYSYM_(m,M) \
    _KEYSYM_(n,N) \
    _KEYSYM_(o,O) \
    _KEYSYM_(p,P) \
    _KEYSYM_(q,Q) \
    _KEYSYM_(r,R) \
    _KEYSYM_(s,S) \
    _KEYSYM_(t,T) \
    _KEYSYM_(u,U) \
    _KEYSYM_(v,V) \
    _KEYSYM_(w,W) \
    _KEYSYM_(x,X) \
    _KEYSYM_(y,Y) \
    _KEYSYM_(z,Z) \
    _KEYSYM1_(DELETE) \
    _KEYSYM_(KP_PLUS,KEYPAD_PLUS)     \
    _KEYSYM_(KP_MINUS,KEYPAD_MINUS)    \
    _KEYSYM_(KP_MULTIPLY,KEYPAD_MULTIPLY) \
    _KEYSYM_(KP_DIVIDE,KEYPAD_DIVIDE)   \
    _KEYSYM_(KP_ENTER,KEYPAD_ENTER)    \
    _KEYSYM_(KP_PERIOD,KEYPAD_PERIOD)   \
    _KEYSYM_(KP_EQUALS,KEYPAD_EQUALS)   \
    _KEYSYM_(KP1,KEYPAD_1)         \
    _KEYSYM_(KP2,KEYPAD_2)         \
    _KEYSYM_(KP3,KEYPAD_3)         \
    _KEYSYM_(KP4,KEYPAD_4)         \
    _KEYSYM_(KP5,KEYPAD_5)         \
    _KEYSYM_(KP6,KEYPAD_6)         \
    _KEYSYM_(KP7,KEYPAD_7)         \
    _KEYSYM_(KP8,KEYPAD_8)         \
    _KEYSYM_(KP9,KEYPAD_9)         \
    _KEYSYM_(KP0,KEYPAD_0)         \
    _KEYSYM1_(UP)  \
    _KEYSYM1_(DOWN) \
    _KEYSYM1_(RIGHT) \
    _KEYSYM1_(LEFT) \
    _KEYSYM1_(INSERT) \
    _KEYSYM1_(HOME) \
    _KEYSYM1_(END) \
    _KEYSYM1_(PAGEUP) \
    _KEYSYM1_(PAGEDOWN) \
    _KEYSYM1_(F1) \
    _KEYSYM1_(F2) \
    _KEYSYM1_(F3) \
    _KEYSYM1_(F4) \
    _KEYSYM1_(F5) \
    _KEYSYM1_(F6) \
    _KEYSYM1_(F7) \
    _KEYSYM1_(F8) \
    _KEYSYM1_(F9) \
    _KEYSYM1_(F10) \
    _KEYSYM1_(F11) \
    _KEYSYM1_(F12) \
    _KEYSYM1_(F13) \
    _KEYSYM1_(F14) \
    _KEYSYM1_(F15) \
    _KEYSYM1_(SCROLLOCK) \
    _KEYSYM1_(SYSREQ) \
    _KEYSYM1_(PRINT) \
    _KEYSYM1_(BREAK) \

#define _KEYSYM_(x,y)   { SDLK_##x, #y },
static const struct { int  _sym; const char*  _str; }  keysym_names[] =
{
    _KEYSYM_LIST
    { 0, NULL }
};
#undef _KEYSYM_

int
skin_keysym_str_count( void )
{
    return sizeof(keysym_names)/sizeof(keysym_names[0])-1;
}

const char*
skin_keysym_str( int  index )
{
    if (index >= 0 && index < skin_keysym_str_count())
        return keysym_names[index]._str;

    return NULL;
}

const char*
skin_key_symmod_to_str( int  sym, int  mod )
{
    static char  temp[32];
    char*        p = temp;
    char*        end = p + sizeof(temp);
    int          nn;

    if ((mod & KMOD_LCTRL) != 0) {
        p = bufprint(p, end, "Ctrl-");
    }
    if ((mod & KMOD_RCTRL) != 0) {
        p = bufprint(p, end, "RCtrl-");
    }
    if ((mod & KMOD_LSHIFT) != 0) {
        p = bufprint(p, end, "Shift-");
    }
    if ((mod & KMOD_RSHIFT) != 0) {
        p = bufprint(p, end, "RShift-");
    }
    if ((mod & KMOD_LALT) != 0) {
        p = bufprint(p, end, "Alt-");
    }
    if ((mod & KMOD_RALT) != 0) {
        p = bufprint(p, end, "RAlt-");
    }
    for (nn = 0; keysym_names[nn]._sym != 0; nn++) {
        if (keysym_names[nn]._sym == sym) {
            p = bufprint(p, end, "%s", keysym_names[nn]._str);
            return temp;;
        }
    }

    if (sym >= 32 && sym <= 127) {
        p = bufprint(p, end, "%c", sym);
        return temp;
    }

    return NULL;
}


int
skin_key_symmod_from_str( const char*  str, int  *psym, int  *pmod )
{
    int          mod = 0;
    int          match = 1;
    int          nn;
    const char*  s0 = str;
    static const struct { const char*  prefix; int  mod; }  mods[] =
    {
        { "^",      KMOD_LCTRL },
        { "Ctrl",   KMOD_LCTRL },
        { "ctrl",   KMOD_LCTRL },
        { "RCtrl",  KMOD_RCTRL },
        { "rctrl",  KMOD_RCTRL },
        { "Alt",    KMOD_LALT },
        { "alt",    KMOD_LALT },
        { "RAlt",   KMOD_RALT },
        { "ralt",   KMOD_RALT },
        { "Shift",  KMOD_LSHIFT },
        { "shift",  KMOD_LSHIFT },
        { "RShift", KMOD_RSHIFT },
        { "rshift", KMOD_RSHIFT },
        { NULL, 0 }
    };

    while (match) {
        match = 0;
        for (nn = 0; mods[nn].prefix != NULL; nn++) {
            const char*  prefix = mods[nn].prefix;
            int          len    = strlen(prefix);

            if ( !memcmp(str, prefix, len) ) {
                str  += len;
                match = 1;
                mod  |= mods[nn].mod;
                if (str[0] == '-' && str[1] != 0)
                    str++;
                break;
            }
        }
    }

    for (nn = 0; keysym_names[nn]._sym; nn++) {
#ifdef _WIN32
        if ( !stricmp(str, keysym_names[nn]._str) )
#else
        if ( !strcasecmp(str, keysym_names[nn]._str) )
#endif
        {
            *psym = keysym_names[nn]._sym;
            *pmod = mod;
            return 0;
        }
    }

    D("%s: can't find sym value for '%s' (mod=%d, str=%s)", __FUNCTION__, s0, mod, str);
    return -1;
}


typedef struct {
    int             sym;
    int             mod;
    SkinKeyCommand  command;
} SkinKeyItem;


struct SkinKeyset {
    int           num_items;
    int           max_items;
    SkinKeyItem*  items;
};


static int
skin_keyset_add( SkinKeyset*  kset, int  sym, int  mod, SkinKeyCommand  command )
{
    SkinKeyItem*  item = kset->items;
    SkinKeyItem*  end  = item + kset->num_items;
    SkinKeyItem*  first = NULL;
    int           count = 0;

    D( "adding binding %s to %s", skin_key_command_to_str(command), skin_key_symmod_to_str(sym,mod));
    for ( ; item < end; item++) {
        if (item->command == command) {
            if (!first)
                first = item;
            if (++count == SKIN_KEY_COMMAND_MAX_BINDINGS) {
                /* replace the first (oldest) one in the list */
                first->sym = sym;
                first->mod = mod;
                return 0;
            }
            continue;
        }
        if (item->sym == sym && item->mod == mod) {
            /* replace a (sym,mod) binding */
            item->command = command;
            return 0;
        }
    }
    if (kset->num_items >= kset->max_items) {
        int           old_size  = kset->max_items;
        int           new_size  = old_size + (old_size >> 1) + 4;
        SkinKeyItem*  new_items = realloc( kset->items, new_size*sizeof(SkinKeyItem) );
        if (new_items == NULL) {
            return -1;
        }
        kset->items     = new_items;
        kset->max_items = new_size;
    }
    item = kset->items + kset->num_items++;
    item->command = command;
    item->sym     = sym;
    item->mod     = mod;
    return 1;
}


SkinKeyset*
skin_keyset_new ( AConfig*  root )
{
    SkinKeyset*  kset = calloc(1, sizeof(*kset));
    AConfig*     node = root->first_child;;

    if (kset == NULL)
        return NULL;

    for ( ; node != NULL; node = node->next )
    {
        SkinKeyCommand  command;
        int             sym, mod;
        char*           p;

        command = skin_key_command_from_str( node->name, -1 );
        if (command == SKIN_KEY_COMMAND_NONE) {
            D( "ignoring unknown keyset command '%s'", node->name );
            continue;
        }
        p = (char*)node->value;
        while (*p) {
            char*  q = strpbrk( p, " \t,:" );
            if (q == NULL)
                q = p + strlen(p);

            if (q > p) {
                int   len = q - p;
                char  keys[24];
                if (len+1 >= (int)sizeof(keys)) {
                    D("key binding too long: '%s'", p);
                }
                else {
                    memcpy( keys, p, len );
                    keys[len] = 0;
                    if ( skin_key_symmod_from_str( keys, &sym, &mod ) < 0 ) {
                        D( "ignoring unknown keys '%s' for command '%s'",
                                keys, node->name );
                    } else {
                        skin_keyset_add( kset, sym, mod, command );
                    }
                }
            } else if (*q)
                q += 1;

            p = q;
        }
    }
    return  kset;
}


SkinKeyset*
skin_keyset_new_from_text( const char*  text )
{
    AConfig*     root = aconfig_node("","");
    char*        str = strdup(text);
    SkinKeyset*  result;

    D("kset new from:\n%s", text);
    aconfig_load( root, str );
    result = skin_keyset_new( root );
    free(str);
    D("kset done result=%p", result);
    return result;
}


void
skin_keyset_free( SkinKeyset*  kset )
{
    if (kset) {
        free(kset->items);
        kset->items     = NULL;
        kset->num_items = 0;
        kset->max_items = 0;
        free(kset);
    }
}


extern int
skin_keyset_get_bindings( SkinKeyset*      kset,
                          SkinKeyCommand   command,
                          SkinKeyBinding*  bindings )
{
    if (kset) {
        int     count = 0;
        SkinKeyItem*  item = kset->items;
        SkinKeyItem*  end  = item + kset->num_items;

        for ( ; item < end; item++ ) {
            if (item->command == command) {
                bindings->sym = item->sym;
                bindings->mod = item->mod;
                bindings ++;
                if ( ++count >= SKIN_KEY_COMMAND_MAX_BINDINGS ) {
                    /* shouldn't happen, but be safe */
                    break;
                }
            }
        }
        return count;
    }
    return -1;
}


/* retrieve the command corresponding to a given (sym,mod) pair. returns SKIN_KEY_COMMAND_NONE if not found */
SkinKeyCommand
skin_keyset_get_command( SkinKeyset*  kset, int  sym, int mod )
{
    if (kset) {
        SkinKeyItem*  item = kset->items;
        SkinKeyItem*  end  = item + kset->num_items;

        for ( ; item < end; item++ ) {
            if (item->sym == sym && item->mod == mod) {
                return item->command;
            }
        }
    }
    return SKIN_KEY_COMMAND_NONE;
}


const char*
skin_keyset_get_default( void )
{
    return
    "BUTTON_CALL         F3\n"
    "BUTTON_HANGUP       F4\n"
    "BUTTON_HOME         Home\n"
    "BUTTON_BACK         Escape\n"
    "BUTTON_MENU         F2, PageUp\n"
    "BUTTON_STAR         Shift-F2, PageDown\n"
    "BUTTON_POWER        F7\n"
    "BUTTON_SEARCH       F5\n"
    "BUTTON_CAMERA       Ctrl-Keypad_5, Ctrl-F3\n"
    "BUTTON_VOLUME_UP    Keypad_Plus, Ctrl-F5\n"
    "BUTTON_VOLUME_DOWN  Keypad_Minus, Ctrl-F6\n"

    "TOGGLE_NETWORK      F8\n"
    "TOGGLE_TRACING      F9\n"
    "TOGGLE_FULLSCREEN   Alt-Enter\n"

    "BUTTON_DPAD_CENTER  Keypad_5\n"
    "BUTTON_DPAD_UP      Keypad_8\n"
    "BUTTON_DPAD_LEFT    Keypad_4\n"
    "BUTTON_DPAD_RIGHT   Keypad_6\n"
    "BUTTON_DPAD_DOWN    Keypad_2\n"

    "TOGGLE_TRACKBALL    F6\n"
    "SHOW_TRACKBALL      Delete\n"

    "CHANGE_LAYOUT_PREV  Keypad_7, Ctrl-F11\n"
    "CHANGE_LAYOUT_NEXT  Keypad_9, Ctrl-F12\n"
    "ONION_ALPHA_UP      Keypad_Multiply\n"
    "ONION_ALPHA_DOWN    Keypad_Divide\n"
    ;
}
