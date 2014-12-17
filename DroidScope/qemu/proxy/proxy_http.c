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
#include "proxy_int.h"
#include "proxy_http_int.h"
#include "qemu-common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define  HTTP_VERSION  "1.1"

static void
http_service_free( HttpService*  service )
{
    PROXY_LOG("%s", __FUNCTION__);
    if (service->footer != service->footer0)
        qemu_free(service->footer);
    qemu_free(service);
}


static ProxyConnection*
http_service_connect( HttpService*  service,
                      SocketType    sock_type,
                      SockAddress*  address )
{
    /* the HTTP proxy can only handle TCP connections */
    if (sock_type != SOCKET_STREAM)
        return NULL;

    /* if the client tries to directly connect to the proxy, let it do so */
    if (sock_address_equal( address, &service->server_addr ))
        return NULL;

    PROXY_LOG("%s: trying to connect to %s",
              __FUNCTION__, sock_address_to_string(address));

    if (sock_address_get_port(address) == 80) {
        /* use the rewriter for HTTP */
        PROXY_LOG("%s: using HTTP rewriter", __FUNCTION__);
        return http_rewriter_connect(service, address);
    } else {
        PROXY_LOG("%s: using HTTP rewriter", __FUNCTION__);
        return http_connector_connect(service, address);
    }
}


int
proxy_http_setup( const char*         servername,
                  int                 servernamelen,
                  int                 serverport,
                  int                 num_options,
                  const ProxyOption*  options )
{
    HttpService*        service;
    SockAddress         server_addr;
    const ProxyOption*  opt_nocache   = NULL;
    const ProxyOption*  opt_keepalive = NULL;
    const ProxyOption*  opt_auth_user = NULL;
    const ProxyOption*  opt_auth_pass = NULL;
    const ProxyOption*  opt_user_agent = NULL;

    if (servernamelen < 0)
        servernamelen = strlen(servername);

    PROXY_LOG( "%s: creating http proxy service connecting to: %.*s:%d",
               __FUNCTION__, servernamelen, servername, serverport );

    /* resolve server address */
    if (proxy_resolve_server(&server_addr, servername,
                             servernamelen, serverport) < 0)
    {
        return -1;
    }

    /* create service object */
    service = qemu_mallocz(sizeof(*service));
    if (service == NULL) {
        PROXY_LOG("%s: not enough memory to allocate new proxy service", __FUNCTION__);
        return -1;
    }

    service->server_addr = server_addr;

    /* parse options */
    {
        const ProxyOption*  opt = options;
        const ProxyOption*  end = opt + num_options;

        for ( ; opt < end; opt++ ) {
            switch (opt->type) {
                case PROXY_OPTION_HTTP_NOCACHE:     opt_nocache    = opt; break;
                case PROXY_OPTION_HTTP_KEEPALIVE:   opt_keepalive  = opt; break;
                case PROXY_OPTION_AUTH_USERNAME:    opt_auth_user  = opt; break;
                case PROXY_OPTION_AUTH_PASSWORD:    opt_auth_pass  = opt; break;
                case PROXY_OPTION_HTTP_USER_AGENT:  opt_user_agent = opt; break;
                default: ;
            }
        }
    }

    /* prepare footer */
    {
        int    wlen;
        char*  p    = service->footer0;
        char*  end  = p + sizeof(service->footer0);

        /* no-cache */
        if (opt_nocache) {
            p += snprintf(p, end-p, "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
            if (p >= end) goto FooterOverflow;
        }
        /* keep-alive */
        if (opt_keepalive) {
            p += snprintf(p, end-p, "Connection: Keep-Alive\r\nProxy-Connection: Keep-Alive\r\n");
            if (p >= end) goto FooterOverflow;
        }
        /* authentication */
        if (opt_auth_user && opt_auth_pass) {
            char  user_pass[256];
            char  encoded[512];
            int   uplen;

            uplen = snprintf( user_pass, sizeof(user_pass), "%.*s:%.*s",
                              opt_auth_user->string_len, opt_auth_user->string,
                              opt_auth_pass->string_len, opt_auth_pass->string );

            if (uplen >= sizeof(user_pass)) goto FooterOverflow;

            wlen = proxy_base64_encode(user_pass, uplen, encoded, (int)sizeof(encoded));
            if (wlen < 0) {
                PROXY_LOG( "could not base64 encode '%.*s'", uplen, user_pass);
                goto FooterOverflow;
            }

            p += snprintf(p, end-p, "Proxy-authorization: Basic %.*s\r\n", wlen, encoded);
            if (p >= end) goto FooterOverflow;
        }
        /* user agent */
        if (opt_user_agent) {
            p += snprintf(p, end-p, "User-Agent: %.*s\r\n",
                          opt_user_agent->string_len,
                          opt_user_agent->string);
            if (p >= end) goto FooterOverflow;
        }

        p += snprintf(p, end-p, "\r\n");

        if (p >= end) {
        FooterOverflow:
            PROXY_LOG( "%s: buffer overflow when creating connection footer",
                       __FUNCTION__);
            http_service_free(service);
            return -1;
        }

        service->footer     = service->footer0;
        service->footer_len = (p - service->footer);
    }

    PROXY_LOG( "%s: creating HTTP Proxy Service Footer is (len=%d):\n'%.*s'",
               __FUNCTION__, service->footer_len,
               service->footer_len, service->footer );

    service->root->opaque       = service;
    service->root->serv_free    = (ProxyServiceFreeFunc)    http_service_free;
    service->root->serv_connect = (ProxyServiceConnectFunc) http_service_connect;

    if (proxy_manager_add_service( service->root ) < 0) {
        PROXY_LOG("%s: could not register service ?", __FUNCTION__);
        http_service_free(service);
        return -1;
    }
    return 0;
}

