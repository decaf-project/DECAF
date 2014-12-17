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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "android/config.h"
#include "android/utils/path.h"

AConfig*
aconfig_node(const char *name, const char *value)
{
    AConfig *n;

    n = (AConfig*) calloc(sizeof(AConfig), 1);
    n->name = name ? name : "";
    n->value = value ? value : "";

    return n;
}

static AConfig*
_aconfig_find(AConfig *root, const char *name, int create)
{
    AConfig *node;

    for(node = root->first_child; node; node = node->next) {
        if(!strcmp(node->name, name)) return node;
    }

    if(create) {
        node = (AConfig*) calloc(sizeof(AConfig), 1);
        node->name = name;
        node->value = "";

        if(root->last_child) {
            root->last_child->next = node;
        } else {
            root->first_child = node;
        }
        root->last_child = node;
    }

    return node;
}

AConfig*
aconfig_find(AConfig *root, const char *name)
{
    return _aconfig_find(root, name, 0);
}

int
aconfig_bool(AConfig *root, const char *name, int _default)
{
    AConfig *n = _aconfig_find(root, name, 0);
    if(n == 0) {
        return _default;
    } else {
        switch(n->value[0]){
        case 'y':
        case 'Y':
        case '1':
            return 1;
        default:
            return 0;
        }
    }
}

unsigned
aconfig_unsigned(AConfig *root, const char *name, unsigned _default)
{
    AConfig *n = _aconfig_find(root, name, 0);
    if(n == 0) {
        return _default;
    } else {
        return strtoul(n->value, 0, 0);
    }
}

int
aconfig_int(AConfig *root, const char *name, int _default)
{
    AConfig *n = _aconfig_find(root, name, 0);
    if(n == 0) {
        return _default;
    } else {
        return strtol(n->value, 0, 0);
    }
}


const char*
aconfig_str(AConfig *root, const char *name, const char *_default)
{
    AConfig *n = _aconfig_find(root, name, 0);
    if(n == 0) {
        return _default;
    } else {
        return n->value;
    }
}

void
aconfig_set(AConfig *root, const char *name, const char *value)
{
    AConfig *node = _aconfig_find(root, name, 1);
    node->value = value;
}

#define T_EOF 0
#define T_TEXT 1
#define T_DOT 2
#define T_OBRACE 3
#define T_CBRACE 4

typedef struct
{
    char *data;
    char *text;
    int len;
    char next;
} cstate;


static int _lex(cstate *cs, int value)
{
    char c;
    char *s;
    char *data;

    data = cs->data;

    if(cs->next != 0) {
        c = cs->next;
        cs->next = 0;
        goto got_c;
    }

restart:
    for(;;) {
        c = *data++;
    got_c:
        if(isspace(c)) continue;

        switch(c) {
        case 0:
            return T_EOF;

        /* a sharp sign (#) starts a line comment and everything
         * behind that is ignored until the end of line
         */
        case '#':
            for(;;) {
                switch(*data) {
                case 0:
                    cs->data = data;
                    return T_EOF;
                case '\n':
                    cs->data = data + 1;
                    goto restart;
                default:
                    data++;
                }
            }
            break;

        case '.':
            cs->data = data;
            return T_DOT;

        case '{':
            cs->data = data;
            return T_OBRACE;

        case '}':
            cs->data = data;
            return T_CBRACE;

        default:
            s = data - 1;

            if(value) {
               /* if we're looking for a value, then take anything
                * until the end of line. note that sharp signs do
                * not start comments then. the result will be stripped
                * from trailing whitespace.
                */
                for(;;) {
                    if(*data == 0) {
                        cs->data = data;
                        break;
                    }
                    if(*data == '\n') {
                        cs->data = data + 1;
                        *data-- = 0;
                        break;
                    }
                    data++;
                }

                    /* strip trailing whitespace */
                while(data > s){
                    if(!isspace(*data)) break;
                    *data-- = 0;
                }

                goto got_text;
            } else {
               /* looking for a key name. we stop at whitspace,
                * EOF, of T_DOT/T_OBRACE/T_CBRACE characters.
                * note that the name can include sharp signs
                */
                for(;;) {
                    if(isspace(*data)) {
                        *data = 0;
                        cs->data = data + 1;
                        goto got_text;
                    }
                    switch(*data) {
                    case 0:
                        cs->data = data;
                        goto got_text;
                    case '.':
                    case '{':
                    case '}':
                        cs->next = *data;
                        *data = 0;
                        cs->data = data + 1;
                        goto got_text;
                    default:
                        data++;
                    }
                }
            }
        }
    }

got_text:
    cs->text = s;
    return T_TEXT;
}

#if 0
char *TOKENNAMES[] = { "EOF", "TEXT", "DOT", "OBRACE", "CBRACE" };

static int lex(cstate *cs, int value)
{
    int tok = _lex(cs, value);
    printf("TOKEN(%d) %s %s\n", value, TOKENNAMES[tok],
           tok == T_TEXT ? cs->text : "");
    return tok;
}
#else
#define lex(cs,v) _lex(cs,v)
#endif

static int parse_expr(cstate *cs, AConfig *node);

static int
parse_block(cstate *cs, AConfig *node)
{
    for(;;){
        switch(lex(cs, 0)){
        case T_TEXT:
            if(parse_expr(cs, node)) return -1;
            continue;

        case T_CBRACE:
            return 0;

        default:
            return -1;
        }
    }
}

static int
parse_expr(cstate *cs, AConfig *node)
{
        /* last token was T_TEXT */
    node = _aconfig_find(node, cs->text, 1);

    for(;;) {
        switch(lex(cs, 1)) {
        case T_DOT:
            if(lex(cs, 0) != T_TEXT) return -1;
            node = _aconfig_find(node, cs->text, 1);
            continue;

        case T_TEXT:
            node->value = cs->text;
            return 0;

        case T_OBRACE:
            return parse_block(cs, node);

        default:
            return -1;
        }
    }
}

void
aconfig_load(AConfig *root, char *data)
{
    if(data != 0) {
        cstate cs;
        cs.data = data;
        cs.next = 0;

        for(;;) {
            switch(lex(&cs, 0)){
            case T_TEXT:
                if(parse_expr(&cs, root)) return;
                break;
            default:
                return;
            }
        }
    }
}

int
aconfig_load_file(AConfig *root, const char *fn)
{
    char *data;
    data = path_load_file(fn, NULL);
    if (data == NULL)
        return -1;

    aconfig_load(root, data);
    return 0;
}


typedef struct
{
    char   buff[1024];
    char*  p;
    char*  end;
    int    fd;
} Writer;

static int
writer_init( Writer*  w, const char*  fn )
{
    w->p   = w->buff;
    w->end = w->buff + sizeof(w->buff);

    w->fd  = creat( fn, 0755 );
    if (w->fd < 0)
        return -1;

#ifdef _WIN32
    _setmode( w->fd, _O_BINARY );
#endif
    return 0;
}

static void
writer_write( Writer*  w, const char*  src, int  len )
{
    while (len > 0) {
        int  avail = w->end - w->p;

        if (avail > len)
            avail = len;

        memcpy( w->p, src, avail );
        src += avail;
        len -= avail;

        w->p += avail;
        if (w->p == w->end) {
            int ret;
            do {
                ret = write( w->fd, w->buff, w->p - w->buff );
            } while (ret < 0 && errno == EINTR);
            if (ret < 0)
                break;
            w->p = w->buff;
        }
    }
}

static void
writer_done( Writer*  w )
{
    if (w->p > w->buff) {
        int ret;
        do {
            ret = write( w->fd, w->buff, w->p - w->buff );
        } while (ret < 0 && errno == EINTR);
    }
    close( w->fd );
}

static void
writer_margin( Writer*  w, int  margin)
{
    static const char  spaces[10] = "          ";
    while (margin >= 10) {
        writer_write(w,spaces,10);
        margin -= 10;
    }
    if (margin > 0)
        writer_write(w,spaces,margin);
}

static void
writer_c(Writer*  w, char  c)
{
    writer_write(w, &c, 1);
}

static void
writer_str(Writer*  w, const char*  str)
{
    writer_write(w, str, strlen(str));
}

static void
writer_node(Writer*  w, AConfig*  node, int  margin)
{
    writer_margin(w,margin);
    writer_str(w, node->name);
    writer_c(w,' ');

    if (node->value[0]) {
        writer_str(w, node->value);
        writer_c(w,'\n');
    }
    else
    {
        AConfig*  child;

        writer_c(w, '{');
        writer_c(w, '\n');

        for (child = node->first_child; child; child = child->next)
            writer_node(w,child,margin+4);

        writer_margin(w,margin);
        writer_c(w,'}');
        writer_c(w,'\n');
    }
}

int
aconfig_save_file(AConfig *root, const char *fn)
{
    AConfig*  child;
    Writer    w[1];

    if (writer_init(w,fn) < 0)
        return -1;

    for (child = root->first_child; child; child = child->next)
        writer_node(w,child,0);

    writer_done(w);
    return 0;
}
