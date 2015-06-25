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
#include "android/skin/keyboard.h"
#include "android/utils/debug.h"
#include "android/utils/bufprint.h"
#include "android/utils/system.h"
#include "android/android.h"
#include "android/keycode-array.h"
#include "android/charmap.h"

#define  DEBUG  1

#if DEBUG
#  define  D(...)  VERBOSE_PRINT(keys,__VA_ARGS__)
#else
#  define  D(...)  ((void)0)
#endif

#define DEFAULT_ANDROID_CHARMAP  "qwerty2"

/** LAST PRESSED KEYS
 ** a small buffer of last pressed keys, this is used to properly
 ** implement the Unicode keyboard mode (SDL key up event always have
 ** their .unicode field set to 0
 **/
typedef struct {
    int  unicode;  /* Unicode of last pressed key        */
    int  sym;      /* SDL key symbol value (e.g. SDLK_a) */
    int  mod;      /* SDL key modifier value             */
} LastKey;

#define  MAX_LAST_KEYS  16

struct SkinKeyboard {
    const AKeyCharmap*  charmap;
    SkinKeyset*         kset;
    char                enabled;
    char                raw_keys;
    char                last_count;

    SkinRotation        rotation;

    SkinKeyCommandFunc  command_func;
    void*               command_opaque;
    SkinKeyEventFunc    press_func;
    void*               press_opaque;

    LastKey             last_keys[ MAX_LAST_KEYS ];

    AKeycodeBuffer      keycodes;
};


void
skin_keyboard_set_keyset( SkinKeyboard*  keyboard, SkinKeyset*  kset )
{
    if (kset == NULL)
        return;
    if (keyboard->kset && keyboard->kset != android_keyset) {
        skin_keyset_free(keyboard->kset);
    }
    keyboard->kset = kset;
}


void
skin_keyboard_set_rotation( SkinKeyboard*     keyboard,
                            SkinRotation      rotation )
{
    keyboard->rotation = (rotation & 3);
}

void
skin_keyboard_on_command( SkinKeyboard*  keyboard, SkinKeyCommandFunc  cmd_func, void*  cmd_opaque )
{
    keyboard->command_func   = cmd_func;
    keyboard->command_opaque = cmd_opaque;
}

void
skin_keyboard_on_key_press( SkinKeyboard*  keyboard, SkinKeyEventFunc  press_func, void*  press_opaque )
{
    keyboard->press_func   = press_func;
    keyboard->press_opaque = press_opaque;
}

void
skin_keyboard_add_key_event( SkinKeyboard*  kb,
                             unsigned       code,
                             unsigned       down )
{
    android_keycodes_add_key_event(&kb->keycodes, code, down);
}


void
skin_keyboard_flush( SkinKeyboard*  kb )
{
    android_keycodes_flush(&kb->keycodes);
}


static void
skin_keyboard_cmd( SkinKeyboard*   keyboard,
                   SkinKeyCommand  command,
                   int             param )
{
    if (keyboard->command_func) {
        keyboard->command_func( keyboard->command_opaque, command, param );
    }
}


static LastKey*
skin_keyboard_find_last( SkinKeyboard*  keyboard,
                         int            sym )
{
    LastKey*  k   = keyboard->last_keys;
    LastKey*  end = k + keyboard->last_count;

    for ( ; k < end; k++ ) {
        if (k->sym == sym)
            return k;
    }
    return NULL;
}

static void
skin_keyboard_add_last( SkinKeyboard*  keyboard,
                        int            sym,
                        int            mod,
                        int            unicode )
{
    LastKey*  k = keyboard->last_keys + keyboard->last_count;

    if (keyboard->last_count < MAX_LAST_KEYS) {
        k->sym     = sym;
        k->mod     = mod;
        k->unicode = unicode;

        keyboard->last_count += 1;
    }
}

static void
skin_keyboard_remove_last( SkinKeyboard*  keyboard,
                           int            sym )
{
    LastKey*  k   = keyboard->last_keys;
    LastKey*  end = k + keyboard->last_count;

    for ( ; k < end; k++ ) {
        if (k->sym == sym) {
           /* we don't need a sorted array, so place the last
            * element in place at the position of the removed
            * one... */
            k[0] = end[-1];
            keyboard->last_count -= 1;
            break;
        }
    }
}

static void
skin_keyboard_clear_last( SkinKeyboard*  keyboard )
{
    keyboard->last_count = 0;
}

static int
skin_keyboard_rotate_sym( SkinKeyboard*  keyboard,
                          int            sym )
{
    switch (keyboard->rotation) {
        case SKIN_ROTATION_90:
            switch (sym) {
                case SDLK_LEFT:  sym = SDLK_DOWN; break;
                case SDLK_RIGHT: sym = SDLK_UP; break;
                case SDLK_UP:    sym = SDLK_LEFT; break;
                case SDLK_DOWN:  sym = SDLK_RIGHT; break;
            }
            break;

        case SKIN_ROTATION_180:
            switch (sym) {
                case SDLK_LEFT:  sym = SDLK_RIGHT; break;
                case SDLK_RIGHT: sym = SDLK_LEFT; break;
                case SDLK_UP:    sym = SDLK_DOWN; break;
                case SDLK_DOWN:  sym = SDLK_UP; break;
            }
            break;

        case SKIN_ROTATION_270:
            switch (sym) {
                case SDLK_LEFT:  sym = SDLK_UP; break;
                case SDLK_RIGHT: sym = SDLK_DOWN; break;
                case SDLK_UP:    sym = SDLK_RIGHT; break;
                case SDLK_DOWN:  sym = SDLK_LEFT; break;
            }
            break;

        default: ;
    }
    return  sym;
}

static AndroidKeyCode
skin_keyboard_key_to_code( SkinKeyboard*  keyboard,
                           unsigned       sym,
                           int            mod,
                           int            down )
{
    AndroidKeyCode  code   = 0;
    int             mod0   = mod;
    SkinKeyCommand  command;

    /* first, handle the arrow keys directly */
    /* rotate them if necessary */
    sym  = skin_keyboard_rotate_sym(keyboard, sym);
    mod &= (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT);

    switch (sym) {
        case SDLK_LEFT:       code = kKeyCodeDpadLeft; break;
        case SDLK_RIGHT:      code = kKeyCodeDpadRight; break;
        case SDLK_UP:         code = kKeyCodeDpadUp; break;
        case SDLK_DOWN:       code = kKeyCodeDpadDown; break;
        default: ;
    }

    if (code != 0) {
        D("handling arrow (sym=%d mod=%d)", sym, mod);
        if (!keyboard->raw_keys) {
            int  doCapL, doCapR, doAltL, doAltR;

            if (!down) {
                LastKey*  k = skin_keyboard_find_last(keyboard, sym);
                if (k != NULL) {
                    mod = k->mod;
                    skin_keyboard_remove_last( keyboard, sym );
                }
            } else {
                skin_keyboard_add_last( keyboard, sym, mod, 0);
            }

            doCapL = (mod & 0x7ff) & KMOD_LSHIFT;
            doCapR = (mod & 0x7ff) & KMOD_RSHIFT;
            doAltL = (mod & 0x7ff) & KMOD_LALT;
            doAltR = (mod & 0x7ff) & KMOD_RALT;

            if (down) {
                if (doAltL) skin_keyboard_add_key_event( keyboard, kKeyCodeAltLeft, 1 );
                if (doAltR) skin_keyboard_add_key_event( keyboard, kKeyCodeAltRight, 1 );
                if (doCapL) skin_keyboard_add_key_event( keyboard, kKeyCodeCapLeft, 1 );
                if (doCapR) skin_keyboard_add_key_event( keyboard, kKeyCodeCapRight, 1 );
            }
            skin_keyboard_add_key_event(keyboard, code, down);

            if (!down) {
                if (doCapR) skin_keyboard_add_key_event( keyboard, kKeyCodeCapRight, 0 );
                if (doCapL) skin_keyboard_add_key_event( keyboard, kKeyCodeCapLeft, 0 );
                if (doAltR) skin_keyboard_add_key_event( keyboard, kKeyCodeAltRight, 0 );
                if (doAltL) skin_keyboard_add_key_event( keyboard, kKeyCodeAltLeft, 0 );
            }
            code = 0;
        }
        return code;
    }

    /* special case for keypad keys, ignore them here if numlock is on */
    if ((mod0 & KMOD_NUM) != 0) {
        switch (sym) {
            case SDLK_KP0:
            case SDLK_KP1:
            case SDLK_KP2:
            case SDLK_KP3:
            case SDLK_KP4:
            case SDLK_KP5:
            case SDLK_KP6:
            case SDLK_KP7:
            case SDLK_KP8:
            case SDLK_KP9:
            case SDLK_KP_PLUS:
            case SDLK_KP_MINUS:
            case SDLK_KP_MULTIPLY:
            case SDLK_KP_DIVIDE:
            case SDLK_KP_EQUALS:
            case SDLK_KP_PERIOD:
            case SDLK_KP_ENTER:
                return 0;
        }
    }

    /* now try all keyset combos */
    command = skin_keyset_get_command( keyboard->kset, sym, mod );
    if (command != SKIN_KEY_COMMAND_NONE) {
        D("handling command %s from (sym=%d, mod=%d, str=%s)",
          skin_key_command_to_str(command), sym, mod, skin_key_symmod_to_str(sym,mod));
        skin_keyboard_cmd( keyboard, command, down );
        return 0;
    }
    D("could not handle (sym=%d, mod=%d, str=%s)", sym, mod,
      skin_key_symmod_to_str(sym,mod));
    return -1;
}

/* this gets called only if the reverse unicode mapping didn't work
 * or wasn't used (when in raw keys mode)
 */
static AndroidKeyCode
skin_keyboard_raw_key_to_code(SkinKeyboard*  kb, unsigned sym, int  down)
{
    switch(sym){
    case SDLK_1:          return kKeyCode1;
    case SDLK_2:          return kKeyCode2;
    case SDLK_3:          return kKeyCode3;
    case SDLK_4:          return kKeyCode4;
    case SDLK_5:          return kKeyCode5;
    case SDLK_6:          return kKeyCode6;
    case SDLK_7:          return kKeyCode7;
    case SDLK_8:          return kKeyCode8;
    case SDLK_9:          return kKeyCode9;
    case SDLK_0:          return kKeyCode0;

    case SDLK_q:          return kKeyCodeQ;
    case SDLK_w:          return kKeyCodeW;
    case SDLK_e:          return kKeyCodeE;
    case SDLK_r:          return kKeyCodeR;
    case SDLK_t:          return kKeyCodeT;
    case SDLK_y:          return kKeyCodeY;
    case SDLK_u:          return kKeyCodeU;
    case SDLK_i:          return kKeyCodeI;
    case SDLK_o:          return kKeyCodeO;
    case SDLK_p:          return kKeyCodeP;
    case SDLK_a:          return kKeyCodeA;
    case SDLK_s:          return kKeyCodeS;
    case SDLK_d:          return kKeyCodeD;
    case SDLK_f:          return kKeyCodeF;
    case SDLK_g:          return kKeyCodeG;
    case SDLK_h:          return kKeyCodeH;
    case SDLK_j:          return kKeyCodeJ;
    case SDLK_k:          return kKeyCodeK;
    case SDLK_l:          return kKeyCodeL;
    case SDLK_z:          return kKeyCodeZ;
    case SDLK_x:          return kKeyCodeX;
    case SDLK_c:          return kKeyCodeC;
    case SDLK_v:          return kKeyCodeV;
    case SDLK_b:          return kKeyCodeB;
    case SDLK_n:          return kKeyCodeN;
    case SDLK_m:          return kKeyCodeM;
    case SDLK_COMMA:      return kKeyCodeComma;
    case SDLK_PERIOD:     return kKeyCodePeriod;
    case SDLK_SPACE:      return kKeyCodeSpace;
    case SDLK_SLASH:      return kKeyCodeSlash;
    case SDLK_RETURN:     return kKeyCodeNewline;
    case SDLK_BACKSPACE:  return kKeyCodeDel;

/* these are qwerty keys not on a device keyboard */
    case SDLK_TAB:        return kKeyCodeTab;
    case SDLK_BACKQUOTE:  return kKeyCodeGrave;
    case SDLK_MINUS:      return kKeyCodeMinus;
    case SDLK_EQUALS:     return kKeyCodeEquals;
    case SDLK_LEFTBRACKET: return kKeyCodeLeftBracket;
    case SDLK_RIGHTBRACKET: return kKeyCodeRightBracket;
    case SDLK_BACKSLASH:  return kKeyCodeBackslash;
    case SDLK_SEMICOLON:  return kKeyCodeSemicolon;
    case SDLK_QUOTE:      return kKeyCodeApostrophe;

    case SDLK_RSHIFT:     return kKeyCodeCapRight;
    case SDLK_LSHIFT:     return kKeyCodeCapLeft;
    case SDLK_RMETA:      return kKeyCodeSym;
    case SDLK_LMETA:      return kKeyCodeSym;
    case SDLK_RALT:       return kKeyCodeAltRight;
    case SDLK_LALT:       return kKeyCodeAltLeft;
    case SDLK_RCTRL:      return kKeyCodeSym;
    case SDLK_LCTRL:      return kKeyCodeSym;

    default:
        /* fprintf(stderr,"* unknown sdl keysym %d *\n", sym); */
        return -1;
    }
}


static void
skin_keyboard_do_key_event( SkinKeyboard*   kb,
                            AndroidKeyCode  code,
                            int             down )
{
    if (kb->press_func) {
        kb->press_func( kb->press_opaque, code, down );
    }
    skin_keyboard_add_key_event(kb, code, down);
}


int
skin_keyboard_process_unicode_event( SkinKeyboard*  kb,  unsigned int  unicode, int  down )
{
    return android_charmap_reverse_map_unicode(kb->charmap, unicode, down,
                                               &kb->keycodes);
}


void
skin_keyboard_enable( SkinKeyboard*  keyboard,
                      int            enabled )
{
    keyboard->enabled = enabled;
    if (enabled) {
        SDL_EnableUNICODE(!keyboard->raw_keys);
        SDL_EnableKeyRepeat(0,0);
    }
}

void
skin_keyboard_process_event( SkinKeyboard*  kb, SDL_Event*  ev, int  down )
{
    unsigned         code;
    int              unicode = ev->key.keysym.unicode;
    int              sym     = ev->key.keysym.sym;
    int              mod     = ev->key.keysym.mod;

    /* ignore key events if we're not enabled */
    if (!kb->enabled) {
        printf( "ignoring key event sym=%d mod=0x%x unicode=%d\n",
                sym, mod, unicode );
        return;
    }

    /* first, try the keyboard-mode-independent keys */
    code = skin_keyboard_key_to_code( kb, sym, mod, down );
    if (code == 0)
        return;

    if ((int)code > 0) {
        skin_keyboard_do_key_event(kb, code, down);
        skin_keyboard_flush(kb);
        return;
    }

    /* Ctrl-K is used to switch between 'unicode' and 'raw' modes */
    if (sym == SDLK_k)
    {
        int  mod2 = mod & 0x7ff;

        if ( mod2 == KMOD_LCTRL || mod2 == KMOD_RCTRL ) {
            if (down) {
                skin_keyboard_clear_last(kb);
                kb->raw_keys = !kb->raw_keys;
                SDL_EnableUNICODE(!kb->raw_keys);
                D( "switching keyboard to %s mode", kb->raw_keys ? "raw" : "unicode" );
            }
            return;
        }
    }

    if (!kb->raw_keys) {
       /* ev->key.keysym.unicode is only valid on keydown events, and will be 0
        * on the corresponding keyup ones, so remember the set of last pressed key
        * syms to "undo" the job
        */
        if ( !down && unicode == 0 ) {
            LastKey*  k = skin_keyboard_find_last(kb, sym);
            if (k != NULL) {
                unicode = k->unicode;
                skin_keyboard_remove_last(kb, sym);
            }
        }
    }
    if (!kb->raw_keys &&
        skin_keyboard_process_unicode_event( kb, unicode, down ) > 0)
    {
        if (down)
            skin_keyboard_add_last( kb, sym, mod, unicode );

        skin_keyboard_flush( kb );
        return;
    }

    code = skin_keyboard_raw_key_to_code( kb, sym, down );

    if ( !kb->raw_keys &&
         (code == kKeyCodeAltLeft  || code == kKeyCodeAltRight ||
          code == kKeyCodeCapLeft  || code == kKeyCodeCapRight ||
          code == kKeyCodeSym) )
        return;

    if (code == -1) {
        D("ignoring keysym %d", sym );
    } else if (code > 0) {
        skin_keyboard_do_key_event(kb, code, down);
        skin_keyboard_flush(kb);
    }
}

static SkinKeyboard*
skin_keyboard_create_from_charmap_name(const char*  charmap_name,
                                       int  use_raw_keys)
{
    SkinKeyboard*  kb;

    ANEW0(kb);

    kb->charmap = android_get_charmap_by_name(charmap_name);
    if (!kb->charmap) {
        // Charmap name was not found. Default to "qwerty2" */
        kb->charmap = android_get_charmap_by_name(DEFAULT_ANDROID_CHARMAP);
        fprintf(stderr, "### warning, skin requires unknown '%s' charmap, reverting to '%s'\n",
                charmap_name, kb->charmap->name );
    }
    kb->raw_keys = use_raw_keys;
    kb->enabled  = 0;

    /* add default keyset */
    if (android_keyset)
        kb->kset = android_keyset;
    else
        kb->kset = skin_keyset_new_from_text( skin_keyset_get_default() );

    return kb;
}

SkinKeyboard*
skin_keyboard_create( const char*  kcm_file_path, int  use_raw_keys )
{
    const char* charmap_name = DEFAULT_ANDROID_CHARMAP;
    char        cmap_buff[AKEYCHARMAP_NAME_SIZE];

    if (kcm_file_path != NULL) {
        kcm_extract_charmap_name(kcm_file_path, cmap_buff, sizeof cmap_buff);
        charmap_name = cmap_buff;
    }
    return skin_keyboard_create_from_charmap_name(charmap_name, use_raw_keys);
}

void
skin_keyboard_free( SkinKeyboard*  keyboard )
{
    if (keyboard) {
        AFREE(keyboard);
    }
}
