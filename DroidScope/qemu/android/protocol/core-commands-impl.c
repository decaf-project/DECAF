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
 * Contains the Core-side implementation of the "ui-core-control" service that is
 * part of the UI control protocol. Here we handle UI control commands sent by
 * the UI to the Core.
 */

#include "android/android.h"
#include "android/globals.h"
#include "telephony/modem_driver.h"
#include "android-trace.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/debug.h"
#include "android/protocol/core-commands.h"
#include "android/protocol/core-commands-impl.h"

/* Enumerates state values for the command reader in the CoreCmdImpl descriptor.
 */
typedef enum CoreCmdImplState {
    /* The reader is waiting on command header. */
    EXPECTS_HEADER,

    /* The reader is waiting on command parameters. */
    EXPECTS_PARAMETERS,
} CoreCmdImplState;

/* Descriptor for the Core-side implementation of the "ui-core-control" service.
 */
typedef struct CoreCmdImpl {
    /* Reader to detect UI disconnection. */
    AsyncReader         async_reader;

    /* I/O associated with this descriptor. */
    LoopIo              io;

    /* Looper used to communicate with the UI. */
    Looper*             looper;

    /* Writer to send responses to the UI commands. */
    SyncSocket*         sync_writer;

    /* Socket descriptor for this service. */
    int                 sock;

    /* Command reader state. */
    CoreCmdImplState    cmd_state;

    /* Incoming command header. */
    UICmdHeader         cmd_header;

    /* A small preallocated buffer for command parameters. */
    uint8_t             cmd_param[256];

    /* Buffer to use for reading command parameters. Depending on expected size
     * of the parameters this buffer can point to cmd_param field of this
     * structure (for small commands), or can be allocated for large commands. */
    void*               cmd_param_buf;
} CoreCmdImpl;

/* One and only one CoreCmdImpl instance. */
static CoreCmdImpl    _coreCmdImpl;

/* Implemented in android/console.c */
extern void destroy_corecmd_client(void);
/* Implemented in vl-android.c */
extern char* qemu_find_file(int type, const char* filename);

/* Properly initializes cmd_param_buf field in CoreCmdImpl instance to receive
 * the expected command parameters.
 */
static uint8_t*
_alloc_cmd_param_buf(CoreCmdImpl* corecmd, uint32_t size)
{
    if (size < sizeof(corecmd->cmd_param)) {
        // cmd_param can contain all request data.
        corecmd->cmd_param_buf = &corecmd->cmd_param[0];
    } else {
        // Expected request us too large to fit into preallocated buffer.
        corecmd->cmd_param_buf = qemu_malloc(size);
    }
    return corecmd->cmd_param_buf;
}

/* Properly frees cmd_param_buf field in CoreCmdImpl instance.
 */
static void
_free_cmd_param_buf(CoreCmdImpl* corecmd)
{
    if (corecmd->cmd_param_buf != &corecmd->cmd_param[0]) {
        qemu_free(corecmd->cmd_param_buf);
        corecmd->cmd_param_buf = &corecmd->cmd_param[0];
    }
}

/* Calculates timeout for transferring the given number of bytes via socket.
 * Return:
 *  Number of milliseconds during which the entire number of bytes is expected
 *  to be transferred via socket for this service.
 */
static int
_coreCmdImpl_get_timeout(size_t data_size)
{
    // Min 2 seconds + 10 millisec for each transferring byte.
    // TODO: Come up with a better arithmetics here.
    return 2000 + data_size * 10;
}

/* Sends command response back to the UI.
 * Param:
 *  corecmd - CoreCmdImpl instance to use to send the response.
 *  resp - Response header.
 *  resp_data - Response data. Data size is defined by the header.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_coreCmdImpl_respond(CoreCmdImpl* corecmd, UICmdRespHeader* resp, void* resp_data)
{
    int status = syncsocket_start_write(corecmd->sync_writer);
    if (!status) {
        // Write the header
        status = syncsocket_write(corecmd->sync_writer, resp,
                                  sizeof(UICmdRespHeader),
                                  _coreCmdImpl_get_timeout(sizeof(UICmdRespHeader)));
        // Write response data (if any).
        if (status > 0 && resp_data != NULL && resp->resp_data_size != 0) {
            status = syncsocket_write(corecmd->sync_writer, resp_data,
                                      resp->resp_data_size,
                                      _coreCmdImpl_get_timeout(resp->resp_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(corecmd->sync_writer);
    }
    if (status < 0) {
        derror("Core is unable to respond with %u bytes to the UI control command: %s\n",
               resp->resp_data_size, errno_str);
    }
    return status;
}

/* Handles UI control command received from the UI.
 * Param:
 *  corecmd - CoreCmdImpl instance that received the command.
 *  cmd_header - Command header.
 *  cmd_param - Command data.
 */
static void
_coreCmdImpl_handle_command(CoreCmdImpl* corecmd,
                            const UICmdHeader* cmd_header,
                            const uint8_t* cmd_param)
{
    switch (cmd_header->cmd_type) {
        case AUICMD_SET_COARSE_ORIENTATION:
        {
            UICmdSetCoarseOrientation* cmd =
                (UICmdSetCoarseOrientation*)cmd_param;
            android_sensors_set_coarse_orientation(cmd->orient);
            break;
        }

        case AUICMD_TOGGLE_NETWORK:
            qemu_net_disable = !qemu_net_disable;
            if (android_modem) {
                amodem_set_data_registration(
                        android_modem,
                qemu_net_disable ? A_REGISTRATION_UNREGISTERED
                    : A_REGISTRATION_HOME);
            }
            break;

        case AUICMD_TRACE_CONTROL:
        {
            UICmdTraceControl* cmd = (UICmdTraceControl*)cmd_param;
            if (cmd->start) {
                start_tracing();
            } else {
                stop_tracing();
            }
            break;
        }

        case AUICMD_CHK_NETWORK_DISABLED:
        {
            UICmdRespHeader resp;
            resp.resp_data_size = 0;
            resp.result = qemu_net_disable;
            _coreCmdImpl_respond(corecmd, &resp, NULL);
            break;
        }

        case AUICMD_GET_NETSPEED:
        {
            UICmdRespHeader resp;
            UICmdGetNetSpeedResp* resp_data = NULL;
            UICmdGetNetSpeed* cmd = (UICmdGetNetSpeed*)cmd_param;

            resp.resp_data_size = 0;
            resp.result = 0;

            if (cmd->index >= android_netspeeds_count ||
                android_netspeeds[cmd->index].name == NULL) {
                resp.result = -1;
            } else {
                const NetworkSpeed* netspeed = &android_netspeeds[cmd->index];
                // Calculate size of the response data:
                // fixed header + zero-terminated netspeed name.
                resp.resp_data_size = sizeof(UICmdGetNetSpeedResp) +
                                      strlen(netspeed->name) + 1;
                // Count in zero-terminated netspeed display.
                if (netspeed->display != NULL) {
                    resp.resp_data_size += strlen(netspeed->display) + 1;
                } else {
                    resp.resp_data_size++;
                }
                // Allocate and initialize response data buffer.
                resp_data =
                    (UICmdGetNetSpeedResp*)qemu_malloc(resp.resp_data_size);
                resp_data->upload = netspeed->upload;
                resp_data->download = netspeed->download;
                strcpy(resp_data->name, netspeed->name);
                if (netspeed->display != NULL) {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1,
                           netspeed->display);
                } else {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1, "");
                }
            }
            _coreCmdImpl_respond(corecmd, &resp, resp_data);
            if (resp_data != NULL) {
                qemu_free(resp_data);
            }
            break;
        }

        case AUICMD_GET_NETDELAY:
        {
            UICmdRespHeader resp;
            UICmdGetNetDelayResp* resp_data = NULL;
            UICmdGetNetDelay* cmd = (UICmdGetNetDelay*)cmd_param;

            resp.resp_data_size = 0;
            resp.result = 0;

            if (cmd->index >= android_netdelays_count ||
                android_netdelays[cmd->index].name == NULL) {
                resp.result = -1;
            } else {
                const NetworkLatency* netdelay = &android_netdelays[cmd->index];
                // Calculate size of the response data:
                // fixed header + zero-terminated netdelay name.
                resp.resp_data_size = sizeof(UICmdGetNetDelayResp) +
                                      strlen(netdelay->name) + 1;
                // Count in zero-terminated netdelay display.
                if (netdelay->display != NULL) {
                    resp.resp_data_size += strlen(netdelay->display) + 1;
                } else {
                    resp.resp_data_size++;
                }
                // Allocate and initialize response data buffer.
                resp_data =
                    (UICmdGetNetDelayResp*)qemu_malloc(resp.resp_data_size);
                resp_data->min_ms = netdelay->min_ms;
                resp_data->max_ms = netdelay->max_ms;
                strcpy(resp_data->name, netdelay->name);
                if (netdelay->display != NULL) {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1,
                           netdelay->display);
                } else {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1, "");
                }
            }
            _coreCmdImpl_respond(corecmd, &resp, resp_data);
            if (resp_data != NULL) {
                qemu_free(resp_data);
            }
            break;
        }

        case AUICMD_GET_QEMU_PATH:
        {
            UICmdRespHeader resp;
            UICmdGetQemuPath* cmd = (UICmdGetQemuPath*)cmd_param;
            char* filepath = NULL;

            resp.resp_data_size = 0;
            resp.result = -1;
            filepath = qemu_find_file(cmd->type, cmd->filename);
            if (filepath != NULL) {
                resp.resp_data_size = strlen(filepath) + 1;
            }
            _coreCmdImpl_respond(corecmd, &resp, filepath);
            if (filepath != NULL) {
                qemu_free(filepath);
            }
            break;
        }

        case AUICMD_GET_LCD_DENSITY:
        {
            UICmdRespHeader resp;
            resp.resp_data_size = 0;
            resp.result = android_hw->hw_lcd_density;
            _coreCmdImpl_respond(corecmd, &resp, NULL);
            break;
        }

        default:
            derror("Unknown UI control command %d is received by the Core.\n",
                   cmd_header->cmd_type);
            break;
    }
}

/* Asynchronous I/O callback reading UI control commands.
 * Param:
 *  opaque - CoreCmdImpl instance.
 *  events - Lists I/O event (read or write) this callback is called for.
 */
static void
_coreCmdImpl_io_func(void* opaque, int fd, unsigned events)
{
    AsyncStatus status;
    CoreCmdImpl* corecmd;

    if (events & LOOP_IO_WRITE) {
        // We don't use async writer here, so we don't expect
        // any write callbacks.
        derror("Unexpected LOOP_IO_WRITE in _coreCmdImpl_io_func\n");
        return;
    }

    corecmd = (CoreCmdImpl*)opaque;

    // Read whatever is expected from the socket.
    status = asyncReader_read(&corecmd->async_reader);
    switch (status) {
        case ASYNC_COMPLETE:
            switch (corecmd->cmd_state) {
                case EXPECTS_HEADER:
                    // We just read the command  header. Now we expect the param.
                    if (corecmd->cmd_header.cmd_param_size != 0) {
                        corecmd->cmd_state = EXPECTS_PARAMETERS;
                        // Setup the reader to read expected amount of data.
                        _alloc_cmd_param_buf(corecmd,
                                             corecmd->cmd_header.cmd_param_size);
                        asyncReader_init(&corecmd->async_reader,
                                         corecmd->cmd_param_buf,
                                         corecmd->cmd_header.cmd_param_size,
                                         &corecmd->io);
                    } else {
                        // Command doesn't have param. Go ahead and handle it.
                        _coreCmdImpl_handle_command(corecmd, &corecmd->cmd_header,
                                                NULL);
                        // Prepare for the next header.
                        corecmd->cmd_state = EXPECTS_HEADER;
                        asyncReader_init(&corecmd->async_reader,
                                         &corecmd->cmd_header,
                                         sizeof(corecmd->cmd_header),
                                         &corecmd->io);
                    }
                    break;

                case EXPECTS_PARAMETERS:
                    // Entore command is received. Handle it.
                    _coreCmdImpl_handle_command(corecmd, &corecmd->cmd_header,
                                            corecmd->cmd_param_buf);
                    _free_cmd_param_buf(corecmd);
                    // Prepare for the next command.
                    corecmd->cmd_state = EXPECTS_HEADER;
                    asyncReader_init(&corecmd->async_reader, &corecmd->cmd_header,
                                     sizeof(corecmd->cmd_header), &corecmd->io);
                    break;
            }
            break;

        case ASYNC_ERROR:
            loopIo_dontWantRead(&corecmd->io);
            if (errno == ECONNRESET) {
                // UI has exited. We need to destroy the service.
                destroy_corecmd_client();
            }
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

int
coreCmdImpl_create(int fd)
{
    _coreCmdImpl.sock = fd;
    _coreCmdImpl.looper = looper_newCore();
    loopIo_init(&_coreCmdImpl.io, _coreCmdImpl.looper, _coreCmdImpl.sock,
                _coreCmdImpl_io_func, &_coreCmdImpl);
    _coreCmdImpl.cmd_state = EXPECTS_HEADER;
    _coreCmdImpl.cmd_param_buf = &_coreCmdImpl.cmd_param[0];
    asyncReader_init(&_coreCmdImpl.async_reader, &_coreCmdImpl.cmd_header,
                     sizeof(_coreCmdImpl.cmd_header), &_coreCmdImpl.io);
    _coreCmdImpl.sync_writer = syncsocket_init(fd);
    if (_coreCmdImpl.sync_writer == NULL) {
        derror("Unable to create writer for CoreCmdImpl instance: %s\n",
               errno_str);
        coreCmdImpl_destroy();
        return -1;
    }
    return 0;
}

void
coreCmdImpl_destroy()
{
    // Destroy the writer
    if (_coreCmdImpl.sync_writer != NULL) {
        syncsocket_close(_coreCmdImpl.sync_writer);
        syncsocket_free(_coreCmdImpl.sync_writer);
    }
    if (_coreCmdImpl.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_coreCmdImpl.io);
        looper_free(_coreCmdImpl.looper);
        _coreCmdImpl.looper = NULL;
    }
    // Free allocated memory.
    _free_cmd_param_buf(&_coreCmdImpl);
}
