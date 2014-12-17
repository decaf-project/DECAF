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

#ifndef _ANDROID_PROTOCOL_UI_COMMON_H
#define _ANDROID_PROTOCOL_UI_COMMON_H

/*
 * Contains declarations for UI control protocol used by both the Core,
 * and the UI.
 */

/* UI control command header.
 * Every UI control command sent by the Core, or by the UI begins with this
 * header, immediately followed by the command parameters (if there are any).
 * Command type is defined by cmd_type field of this header. If command doesn't
 * have any command-specific parameters, cmd_param_size field of this header
 * must be 0.
 */
typedef struct UICmdHeader {
    /* Command type. */
    uint8_t     cmd_type;

    /* Byte size of the buffer containing parameters for the comand defined by
     * the cmd_type field. The buffer containing parameters must immediately
     * follow this header. If command doesn't have any parameters, this field
     * must be 0 */
    uint32_t    cmd_param_size;
} UICmdHeader;

/* UI control command response header.
 * If UI control command assumes a response from the remote end, the response
 * must start with this header, immediately followed by the response data buffer.
 */
typedef struct UICmdRespHeader {
    /* Result of the command handling. */
    int         result;

    /* Byte size of the buffer containing response data immediately following
     * this header. If there are no response data for the command, this field
     * must be 0. */
    uint32_t    resp_data_size;
} UICmdRespHeader;

#endif /* _ANDROID_PROTOCOL_UI_COMMON_H */
