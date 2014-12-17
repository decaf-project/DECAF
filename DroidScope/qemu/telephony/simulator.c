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
#include "android_modem.h"
#include "sysdeps.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define  DEFAULT_PORT  6703

static AModem  modem;

typedef struct {
    SysChannel   channel;
    char         in_buff[ 128 ];
    int          in_pos;

    char         out_buff[ 128 ];
    int          out_pos;
    int          out_size;
} ClientRec, *Client;

static Client
client_alloc( SysChannel  channel )
{
    Client  client = calloc( sizeof(*client), 1 );

    client->channel = channel;
    return client;
}

static void
client_free( Client  client )
{
    sys_channel_close( client->channel );
    client->channel = NULL;
    free( client );
}

static void
client_append( Client  client, const char*  str, int len );

static void
dump_line( const char*  line, const char*  prefix )
{
    if (prefix)
        printf( "%s", prefix );

    for ( ; *line; line++ ) {
        int  c = line[0];

        if (c >= 32 && c < 127)
            printf( "%c", c );
        else if (c == '\r')
            printf( "<CR>" );
        else if (c == '\n')
            printf( "<LF>" );
        else
            printf( "\\x%02x", c );
    }
    printf( "\n" );
}

static void
client_handle_line( Client  client, const char*  cmd )
{
    const char*  answer;

    dump_line( cmd, "<< " );
    answer = amodem_send( modem, cmd );
    if (answer == NULL)  /* not an AT command, ignored */ {
        printf( "-- NO ANSWER\n" );
        return;
    }

    dump_line( answer, ">> " );
    client_append( client, answer, -1 );
    client_append( client, "\r", 1 );
}

static void
client_handler( void* _client, int  events )
{
    Client  client = _client;

    if (events & SYS_EVENT_READ) {
        int  ret;
        /* read into buffer, one character at a time */
        ret = sys_channel_read( client->channel, client->in_buff + client->in_pos, 1 );
        if (ret != 1) {
            fprintf(stderr, "client %p could not read byte, result = %d, error: %s\n",
                    client, ret, strerror(errno) );
            goto ExitClient;
        }
        if (client->in_buff[client->in_pos] == '\r' ||
            client->in_buff[client->in_pos] == '\n' ) {
            const char*  cmd = client->in_buff;
            client->in_buff[client->in_pos] = 0;

            if (client->in_pos > 0) {
                client_handle_line( client, cmd );
                client->in_pos = 0;
            }
        } else
            client->in_pos += 1;
    }

    if (events & SYS_EVENT_WRITE) {
        int  ret;
        /* write from output buffer, one char at a time */
        ret = sys_channel_write( client->channel, client->out_buff + client->out_pos, 1 );
        if (ret != 1) {
            fprintf(stderr, "client %p could not write byte, result = %d, error: %s\n",
                    client, ret, strerror(errno) );
            goto ExitClient;
        }
        client->out_pos += 1;
        if (client->out_pos == client->out_size) {
            client->out_size = 0;
            client->out_pos  = 0;
            /* we don't need to write */
            sys_channel_on( client->channel, SYS_EVENT_READ, client_handler, client );
        }
    }
    return;

ExitClient:
    printf( "client %p exiting\n", client );
    client_free( client );
}


static void
client_append( Client  client, const char*  str, int len )
{
    int  avail;

    if (len < 0)
        len = strlen(str);

    avail = sizeof(client->out_buff) - client->out_size;
    if (len > avail)
        len = avail;

    memcpy( client->out_buff + client->out_size, str, len );
    if (client->out_size == 0) {
        sys_channel_on( client->channel, SYS_EVENT_READ | SYS_EVENT_WRITE, client_handler, client );
    }
    client->out_size += len;
}


static void
accept_func( void*  _server, int  events )
{
    SysChannel  server  = _server;
    SysChannel  handler;
    Client      client;

    printf( "connection accepted for server channel, getting handler socket\n" );
    handler = sys_channel_create_tcp_handler( server );
    client  = client_alloc( handler );
    printf( "got one. created client %p\n", client );

    events=events;
    sys_channel_on( handler, SYS_EVENT_READ, client_handler, client );
}


int  main( void )
{
    int  port = DEFAULT_PORT;
    SysChannel  server;

    sys_main_init();
    modem = amodem_create( NULL, NULL );

    server = sys_channel_create_tcp_server( port );
    printf( "GSM simulator listening on local port %d\n", port );

    sys_channel_on( server, SYS_EVENT_READ, accept_func, server );
    sys_main_loop();
    printf( "GSM simulator exiting\n" );
    return 0;
}
