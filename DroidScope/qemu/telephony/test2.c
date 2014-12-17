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
#include "sysdeps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define  PORT          8000
#define  MAX_COUNTER   30
#define  INITIAL_DELAY 1000
#define  DELAY         5000

static int  counter = 0;

static void
timer_func( void*  _timer )
{
    SysTimer  timer = _timer;
    SysTime   now   = sys_time_ms();

    ++counter;
    printf( "tick %d/%d a %.2fs\n", counter, MAX_COUNTER, now/1000. );
    if (counter < MAX_COUNTER)
        sys_timer_set( timer, now + DELAY, timer_func, timer );
    else
        sys_timer_destroy( timer );
}

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
client_handle_line( Client  client, const char*  cmd )
{
    char temp[256];
    int  nn, mm = 0;

    for (nn = 0; cmd[nn] != 0; nn++) {
        int  c = cmd[nn];
        if (c >= 32 && c <= 127)
            temp[mm++] = c;
        else if (c == '\n') {
            strcat( temp+mm, "<LF>" );
            mm += 4;
        }
        else if (c == '\r') {
            strcat( temp+mm, "<CR>" );
            mm += 4;
        }
        else {
            sprintf( temp+mm, "\\x%02x", c );
            mm += strlen( temp+mm );
        }
    }
    temp[mm] = 0;
    printf( "%p: << %s\n", client, temp );

    if ( !strcmp( cmd, "quit" ) ) {
        printf( "client %p quitting\n", client );
        client_free( client );
        return;
    }
    client_append( client, "type 'quit' to quit\n", -1 );
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

            /* eat leading cr and lf, maybe left-overs from previous line */
            while (*cmd == '\r' || *cmd =='\n')
                cmd++;

            client_handle_line( client, cmd );
            client->in_pos = 0;
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
    printf( "got one. creating client\n" );
    client  = client_alloc( handler );

    events=events;
    sys_channel_on( handler, SYS_EVENT_READ, client_handler, client );
    client_append( client, "Welcome !\n", -1 );
}


int  main( void )
{
    SysTimer    timer;
    SysChannel  server_channel;

    /* initialize event subsystem */
    sys_main_init();

    /* create timer and register it */
    timer = sys_timer_create();
    sys_timer_set( timer, sys_time_ms() + INITIAL_DELAY, timer_func, timer );

    server_channel = sys_channel_create_tcp_server( PORT );
    printf( "listening on port %d with %p\n", PORT, server_channel );

    sys_channel_on( server_channel, SYS_EVENT_READ, accept_func, server_channel );

    printf("entering event loop\n");
    sys_main_loop();
    printf("exiting event loop\n" );
    return 0;
}
