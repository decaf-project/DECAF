/* Copyright (C) 2010 The Android Open Source Project
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

/*
 * Contains the UI-side implementation of the "ui-core-control" service that is
 * part of the UI control protocol. Here we send UI control commands to the Core.
 */

#include "console.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/debug.h"
#include "android/utils/panic.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/core-commands.h"
#include "android/protocol/core-commands-proxy.h"
#include "android/protocol/core-commands-api.h"

/* Descriptor for the UI-side "ui-core-control" service. */
typedef struct CoreCmdProxy {
    /* Core connection established for this service. */
    CoreConnection*     core_connection;

    /* Socket descriptor for the UI service. */
    int                 sock;

    /* Socket wrapper for sync srites. */
    SyncSocket*         sync_writer;

    /* Socket wrapper for sync reads. */
    SyncSocket*         sync_reader;
} CoreCmdProxy;

/* One and only one CoreCmdProxy instance. */
static CoreCmdProxy  _coreCmdProxy = { 0 };

/* Sends UI command to the core.
 * Param:
 *  cmd_type, cmd_param, cmd_param_size - Define the command.
 * Return:
 *  0 On success, or < 0 on failure.
 */
static int
_coreCmdProxy_send_command(uint8_t cmd_type,
                           void* cmd_param,
                           uint32_t cmd_param_size)
{
    int status;
    UICmdHeader header;

    // Prepare the command header.
    header.cmd_type = cmd_type;
    header.cmd_param_size = cmd_param_size;
    status = syncsocket_start_write(_coreCmdProxy.sync_writer);
    if (!status) {
        // Send the header.
        status = syncsocket_write(_coreCmdProxy.sync_writer, &header,
                                  sizeof(header),
                                  core_connection_get_timeout(sizeof(header)));
        // If there is request data, send it too.
        if (status > 0 && cmd_param != NULL && cmd_param_size > 0) {
            status = syncsocket_write(_coreCmdProxy.sync_writer, cmd_param,
                                      cmd_param_size,
                                      core_connection_get_timeout(cmd_param_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(_coreCmdProxy.sync_writer);
    }
    if (status < 0) {
        derror("Unable to send UI control command %d (size %u): %s\n",
                cmd_type, cmd_param_size, errno_str);
    }
    return status;
}

/* Reads UI control command response from the core.
 * Param:
 *  resp - Upon success contains command response header.
 *  resp_data - Upon success contains allocated reponse data (if any). The caller
 *      is responsible for deallocating the memory returned here.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_coreCmdProxy_get_response(UICmdRespHeader* resp, void** resp_data)
{
    int status =  syncsocket_start_read(_coreCmdProxy.sync_reader);
    if (!status) {
        // Read the header.
        status = syncsocket_read(_coreCmdProxy.sync_reader, resp,
                                 sizeof(UICmdRespHeader),
                                 core_connection_get_timeout(sizeof(UICmdRespHeader)));
        // Read response data (if any).
        if (status > 0 && resp->resp_data_size) {
            *resp_data = malloc(resp->resp_data_size);
            if (*resp_data == NULL) {
                APANIC("_coreCmdProxy_get_response is unable to allocate response data buffer.\n");
            }
            status = syncsocket_read(_coreCmdProxy.sync_reader, *resp_data,
                                     resp->resp_data_size,
                                     core_connection_get_timeout(resp->resp_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_read(_coreCmdProxy.sync_reader);
    }
    if (status < 0) {
        derror("Unable to get UI command response from the Core: %s\n",
               errno_str);
    }
    return status;
}

int
corecmd_set_coarse_orientation(AndroidCoarseOrientation orient)
{
    UICmdSetCoarseOrientation cmd;
    cmd.orient = orient;
    return _coreCmdProxy_send_command(AUICMD_SET_COARSE_ORIENTATION,
                                      &cmd, sizeof(cmd));
}

int
corecmd_toggle_network()
{
    return _coreCmdProxy_send_command(AUICMD_TOGGLE_NETWORK, NULL, 0);
}

int
corecmd_trace_control(int start)
{
    UICmdTraceControl cmd;
    cmd.start = start;
    return _coreCmdProxy_send_command(AUICMD_TRACE_CONTROL,
                                      &cmd, sizeof(cmd));
}

int
corecmd_is_network_disabled()
{
    UICmdRespHeader resp;
    void* tmp = NULL;
    int status;

    status = _coreCmdProxy_send_command(AUICMD_CHK_NETWORK_DISABLED, NULL, 0);
    if (status < 0) {
        return status;
    }
    status = _coreCmdProxy_get_response(&resp, &tmp);
    if (status < 0) {
        return status;
    }
    return resp.result;
}

int
corecmd_get_netspeed(int index, NetworkSpeed** netspeed)
{
    UICmdGetNetSpeed req;
    UICmdRespHeader resp;
    UICmdGetNetSpeedResp* resp_data = NULL;
    int status;

    // Initialize and send the query.
    req.index = index;
    status = _coreCmdProxy_send_command(AUICMD_GET_NETSPEED, &req, sizeof(req));
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _coreCmdProxy_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result) {
        NetworkSpeed* ret;
        // Allocate memory for the returning NetworkSpeed instance.
        // It includes: NetworkSpeed structure +
        // size of zero-terminated "name" and "display" strings saved in
        // resp_data.
        *netspeed = malloc(sizeof(NetworkSpeed) + 1 +
                           resp.resp_data_size - sizeof(UICmdGetNetSpeedResp));
        ret = *netspeed;

        // Copy data obtained from the core to the returning NetworkSpeed
        // instance.
        ret->upload = resp_data->upload;
        ret->download = resp_data->download;
        ret->name = (char*)ret + sizeof(NetworkSpeed);
        strcpy((char*)ret->name, resp_data->name);
        ret->display = ret->name + strlen(ret->name) + 1;
        strcpy((char*)ret->display, resp_data->name + strlen(resp_data->name) + 1);
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
corecmd_get_netdelay(int index, NetworkLatency** netdelay)
{
    UICmdGetNetDelay req;
    UICmdRespHeader resp;
    UICmdGetNetDelayResp* resp_data = NULL;
    int status;

    // Initialize and send the query.
    req.index = index;
    status = _coreCmdProxy_send_command(AUICMD_GET_NETDELAY, &req, sizeof(req));
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _coreCmdProxy_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result) {
        NetworkLatency* ret;
        // Allocate memory for the returning NetworkLatency instance.
        // It includes: NetworkLatency structure +
        // size of zero-terminated "name" and "display" strings saved in
        // resp_data.
        *netdelay = malloc(sizeof(NetworkLatency) + 1 +
                           resp.resp_data_size - sizeof(UICmdGetNetDelayResp));
        ret = *netdelay;

        // Copy data obtained from the core to the returning NetworkLatency
        // instance.
        ret->min_ms = resp_data->min_ms;
        ret->max_ms = resp_data->max_ms;
        ret->name = (char*)ret + sizeof(NetworkLatency);
        strcpy((char*)ret->name, resp_data->name);
        ret->display = ret->name + strlen(ret->name) + 1;
        strcpy((char*)ret->display, resp_data->name + strlen(resp_data->name) + 1);
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
corecmd_get_qemu_path(int type,
                      const char* filename,
                      char* path,
                      size_t path_buf_size)
{
    UICmdRespHeader resp;
    char* resp_data = NULL;
    int status;

    // Initialize and send the query.
    uint32_t cmd_data_size = sizeof(UICmdGetQemuPath) + strlen(filename) + 1;
    UICmdGetQemuPath* req = (UICmdGetQemuPath*)malloc(cmd_data_size);
    if (req == NULL) {
        APANIC("corecmd_get_qemu_path is unable to allocate %u bytes\n",
               cmd_data_size);
    }
    req->type = type;
    strcpy(req->filename, filename);
    status = _coreCmdProxy_send_command(AUICMD_GET_QEMU_PATH, req,
                                        cmd_data_size);
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _coreCmdProxy_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result && resp_data != NULL) {
        strncpy(path, resp_data, path_buf_size);
        path[path_buf_size - 1] = '\0';
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
corecmd_get_hw_lcd_density(void)
{
    UICmdRespHeader resp;
    void* tmp = NULL;
    int status;

    status = _coreCmdProxy_send_command(AUICMD_GET_LCD_DENSITY, NULL, 0);
    if (status < 0) {
        return status;
    }
    status = _coreCmdProxy_get_response(&resp, &tmp);
    if (status < 0) {
        return status;
    }
    return resp.result;
}

int
coreCmdProxy_create(SockAddress* console_socket)
{
    char* handshake = NULL;

    // Connect to the ui-core-control service.
    _coreCmdProxy.core_connection =
        core_connection_create_and_switch(console_socket, "ui-core-control",
                                          &handshake);
    if (_coreCmdProxy.core_connection == NULL) {
        derror("Unable to connect to the ui-core-control service: %s\n",
               errno_str);
        return -1;
    }

    // Initialze command writer and response reader.
    _coreCmdProxy.sock = core_connection_get_socket(_coreCmdProxy.core_connection);
    _coreCmdProxy.sync_writer = syncsocket_init(_coreCmdProxy.sock);
    if (_coreCmdProxy.sync_writer == NULL) {
        derror("Unable to initialize CoreCmdProxy writer: %s\n", errno_str);
        coreCmdProxy_destroy();
        return -1;
    }
    _coreCmdProxy.sync_reader = syncsocket_init(_coreCmdProxy.sock);
    if (_coreCmdProxy.sync_reader == NULL) {
        derror("Unable to initialize CoreCmdProxy reader: %s\n", errno_str);
        coreCmdProxy_destroy();
        return -1;
    }


    fprintf(stdout, "ui-core-control is now connected to the core at %s.",
            sock_address_to_string(console_socket));
    if (handshake != NULL) {
        if (handshake[0] != '\0') {
            fprintf(stdout, " Handshake: %s", handshake);
        }
        free(handshake);
    }
    fprintf(stdout, "\n");

    return 0;
}

/* Destroys CoreCmdProxy instance. */
void
coreCmdProxy_destroy(void)
{
    if (_coreCmdProxy.sync_writer != NULL) {
        syncsocket_close(_coreCmdProxy.sync_writer);
        syncsocket_free(_coreCmdProxy.sync_writer);
        _coreCmdProxy.sync_writer = NULL;
    }
    if (_coreCmdProxy.sync_reader != NULL) {
        syncsocket_close(_coreCmdProxy.sync_reader);
        syncsocket_free(_coreCmdProxy.sync_reader);
        _coreCmdProxy.sync_reader = NULL;
    }
    if (_coreCmdProxy.core_connection != NULL) {
        core_connection_close(_coreCmdProxy.core_connection);
        core_connection_free(_coreCmdProxy.core_connection);
        _coreCmdProxy.core_connection = NULL;
    }
}
